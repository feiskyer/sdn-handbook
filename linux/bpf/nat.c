#include <bcc/proto.h>
#include <bcc/helpers.h>
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_arp.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/udp.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/icmp.h>
#include <uapi/linux/in.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
// the following flags control the debubbing output of the eBPF program.
// please note that because of some eBPF limitations all of them cannot
// be activated at the same time
// #define BPF_TRACE  /* global trace control (should be commented to get better performance) */
#ifdef BPF_TRACE
// #define BPF_TRACE_INGRESS
// #define BPF_TRACE_UDP
// #define BPF_TRACE_EGRESS_UDP
// #define BPF_TRACE_REVERSE_UDP
// #define BPF_TRACE_TCP
// #define BPF_TRACE_EGRESS_TCP
// #define BPF_TRACE_REVERSE_TCP
// #define BPF_TRACE_ICMP
// #define BPF_TRACE_EGRESS_ICMP
// #define BPF_TRACE_REVERSE_ICMP
// #define BPF_TRACE_DROP
#endif
#define EGRESS_NAT_TABLE_DIM  1024
#define REVERSE_NAT_TABLE_DIM 1024
#define IN_IFC  1
#define OUT_IFC 2
#define IP_CSUM_OFFSET (sizeof(struct eth_hdr) + offsetof(struct iphdr, check))
#define UDP_CSUM_OFFSET (sizeof(struct eth_hdr) + sizeof(struct iphdr) + offsetof(struct udphdr, check))
#define TCP_CSUM_OFFSET (sizeof(struct eth_hdr) + sizeof(struct iphdr) + offsetof(struct tcphdr, check))
#define IS_PSEUDO 0x10
#define PRINT_MAC(x) (bpf_htonll(x)>>16)
// egress nat key
struct egress_nat_key{
  __be32 ip_src;
};
// egress nat value
struct egress_nat_value{
  __be32 ip_src_new;
};
// reverse nat key
struct reverse_nat_key{
  __be32 ip_dst;
};
// reverse nat value
struct reverse_nat_value{
  __be32 ip_dst_new;
};
// egress nat table
BPF_TABLE("hash", struct egress_nat_key, struct egress_nat_value, egress_nat_table, EGRESS_NAT_TABLE_DIM);
// reverse nat table
BPF_TABLE("hash", struct reverse_nat_key, struct reverse_nat_value, reverse_nat_table, REVERSE_NAT_TABLE_DIM);
struct eth_hdr {
  __be64   dst:48;
  __be64   src:48;
  __be16   proto;
} __attribute__((packed));
// given ip_src returns the correspondent association in
// the nat table
static inline struct egress_nat_value * get_egress_value(__be32 ip_src) {
  // lookup in the egress_nat_table
  struct egress_nat_key egress_key = {};
  egress_key.ip_src = ip_src;
  struct egress_nat_value *egress_value_p = egress_nat_table.lookup(&egress_key);
  return egress_value_p;
}
static int handle_rx(void *skb, struct metadata *md) {
  struct __sk_buff *skb2 = (struct __sk_buff *)skb;
  void *data = (void *)(long)skb2->data;
  void *data_end = (void *)(long)skb2->data_end;
  struct eth_hdr *eth = data;
  if (data + sizeof(*eth) > data_end)
    goto DROP;
#ifdef BPF_TRACE_INGRESS
  bpf_trace_printk("[nat-%d]: in_ifc:%d eth_type:%x\n", md->module_id, md->in_ifc, bpf_ntohs(eth->proto));
  bpf_trace_printk("[nat-%d]: mac_src:%lx mac_dst:%lx\n", md->module_id, PRINT_MAC(eth->src), PRINT_MAC(eth->dst));
#endif
  switch (eth->proto) {
    case htons(ETH_P_IP): goto ip;
    case htons(ETH_P_ARP): goto arp;
    default: goto EOP;
  }
  ip: ;
  __be32 l3sum = 0;
  struct iphdr *ip = data + sizeof(*eth);
  if (data + sizeof(*eth) + sizeof(*ip) > data_end)
    goto DROP;
  switch (ip->protocol) {
    case IPPROTO_UDP: goto udp;
    case IPPROTO_TCP: goto tcp;
    case IPPROTO_ICMP: goto icmp;
    default: goto EOP;
  }
  // nat is transparent for arp packets, this is the reason a nat must always
  // be deployed attached to a Router, that manages layer 2
  arp: {
    if (md->in_ifc == OUT_IFC) {
      pkt_redirect(skb, md, IN_IFC);
      return RX_REDIRECT;
    } else if (md->in_ifc == IN_IFC) {
      pkt_redirect(skb, md, OUT_IFC);
      return RX_REDIRECT;
    }
    return RX_DROP;
  }
  icmp: {
    // BEGIN ICMP
    __be32 l4sum = 0;
    struct icmphdr *icmp = data + sizeof(*eth) + sizeof(*ip);
    if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*icmp) > data_end)
      return RX_DROP;
  #ifdef BPF_TRACE_ICMP
    bpf_trace_printk("[nat-%d]: ICMP packet type: %d code: %d\n", md->module_id, icmp->type, icmp->code);
    bpf_trace_printk("[nat-%d]: ICMP packet id: %d seq: %d\n", md->module_id, icmp->un.echo.id, icmp->un.echo.sequence);
  #endif
    switch (md->in_ifc) {
      case IN_IFC: goto EGRESS_ICMP;
      case OUT_IFC: goto REVERSE_ICMP;
    }
    goto DROP;
    EGRESS_ICMP: ;
      // packet exiting the nat, apply nat translation
      struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr);
      if (egress_value_p) {
        #ifdef BPF_TRACE_EGRESS_ICMP
          bpf_trace_printk("[nat-%d]: EGRESS NAT ICMP. ip->saddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->saddr), bpf_ntohl(egress_value_p->ip_src_new));
        #endif
          // change ip saddr
          __be32 old_ip = ip->saddr;
          __be32 new_ip = egress_value_p->ip_src_new;
          l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
          ip->saddr = new_ip;
          // update checksum(s)
          bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET ,0, l3sum, 0);
          // redirect packet
          pkt_redirect(skb, md, OUT_IFC);
          return RX_REDIRECT;
        } else {
          goto DROP;
        }
    REVERSE_ICMP: ;
      // packet coming back, apply reverse nat translaton
      struct reverse_nat_key reverse_key = {};
      reverse_key.ip_dst = ip->daddr;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_ICMP
        bpf_trace_printk("[nat-%d]: REVERSE NAT ICMP. ip->daddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->daddr), bpf_ntohl(reverse_value_p->ip_dst_new));
      #endif
        // change ip daddr
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET ,0, l3sum, 0);
        // redirect packet
        pkt_redirect(skb, md, IN_IFC);
        return RX_REDIRECT;
      } else {
        goto DROP;
      }
    // END ICMP
    goto EOP;
  }
  udp: {
  // BEGIN UDP
    __be32 l4sum = 0;
    struct udphdr *udp = data + sizeof(*eth) + sizeof(*ip);
    if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) > data_end)
      return RX_DROP;
  #ifdef BPF_TRACE_UDP
    bpf_trace_printk("[nat-%d]: UDP packet. source:%d dest:%d\n", md->module_id, bpf_ntohs(udp->source), bpf_ntohs(udp->dest));
  #endif
    switch (md->in_ifc) {
      case IN_IFC: goto EGRESS_UDP;
      case OUT_IFC: goto REVERSE_UDP;
    }
    goto DROP;
    EGRESS_UDP: ;
    // packet exiting the nat, apply nat translation
    struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr);
    if (egress_value_p) {
    #ifdef BPF_TRACE_EGRESS_UDP
      bpf_trace_printk("[nat-%d]: EGRESS NAT UDP. ip->saddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->saddr), bpf_ntohl(egress_value_p->ip_src_new));
    #endif
      // change ip
      __be32 old_ip = ip->saddr;
      __be32 new_ip = egress_value_p->ip_src_new;
      l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
      ip->saddr = new_ip;
      // update checksum(s)
      bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
      bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET , 0, l3sum, 0);
      // redirect packet
      pkt_redirect(skb, md, OUT_IFC);
      return RX_REDIRECT;
      } else {
        goto DROP;
      }
    REVERSE_UDP: ;
      // packet coming back, apply reverse nat translaton
      struct reverse_nat_key reverse_key = {};
      reverse_key.ip_dst = ip->daddr;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_UDP
        bpf_trace_printk("[nat-%d]: REVERSE NAT UDP. ip->daddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->daddr), bpf_ntohl(reverse_value_p->ip_dst_new));
      #endif
        // change ip
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET , 0, l3sum, 0);
        // redirect packet
        pkt_redirect(skb, md, IN_IFC);
        return RX_REDIRECT;
      } else {
        goto DROP;
      }
      // END UDP
      goto EOP;
    }
    tcp: {
      // BEGIN TCP
      __be32 l4sum = 0;
      struct tcphdr *tcp = data + sizeof(*eth) + sizeof(*ip);
      if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*tcp) > data_end)
        return RX_DROP;
    #ifdef BPF_TRACE_TCP
      bpf_trace_printk("[nat-%d]: TCP packet. source:%d dest:%d\n", md->module_id, bpf_ntohs(tcp->source), bpf_ntohs(tcp->dest));
    #endif
      switch (md->in_ifc) {
        case IN_IFC: goto EGRESS_TCP;
        case OUT_IFC: goto REVERSE_TCP;
      }
      goto DROP;
      EGRESS_TCP: ;
      // packet exiting the nat, apply nat translation
      struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr);
      if (egress_value_p) {
      #ifdef BPF_TRACE_EGRESS_TCP
        bpf_trace_printk("[nat-%d]: EGRESS NAT TCP. ip->saddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->saddr), bpf_ntohl(egress_value_p->ip_src_new));
      #endif
      // change ip
      __be32 old_ip = ip->saddr;
      __be32 new_ip = egress_value_p->ip_src_new;
      l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
      ip->saddr = new_ip;
      // update checksum(s)
      bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
      bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET , 0, l3sum, 0);
      // redirect packet
      pkt_redirect(skb, md, OUT_IFC);
      return RX_REDIRECT;
    } else {
      goto DROP;
    }
    REVERSE_TCP: ;
      // packet coming back, apply reverse nat translaton
      struct reverse_nat_key reverse_key = {};
      reverse_key.ip_dst = ip->daddr;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_TCP
        bpf_trace_printk("[nat-%d]: REVERSE NAT TCP. ip->daddr translation: 0x%x->0x%x\n", md->module_id, bpf_ntohl(ip->daddr), bpf_ntohl(reverse_value_p->ip_dst_new));
      #endif
        // change ip
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET , 0, l3sum, 0);
        // redirect packet
        pkt_redirect(skb, md, IN_IFC);
        return RX_REDIRECT;
      } else {
        goto DROP;
      }
      // END TCP
      goto EOP;
    }
DROP: ;
EOP: ;
#ifdef BPF_TRACE_DROP
  bpf_trace_printk("[nat-%d]: DROP packet.\n", md->module_id);
#endif
  return RX_DROP;
}