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
#define ICMP_CSUM_OFFSET (sizeof(struct eth_hdr) + sizeof(struct iphdr) + offsetof(struct icmphdr, checksum))
#define IS_PSEUDO 0x10
#define PRINT_MAC(x) (bpf_htonll(x)>>16)
// egress nat key
struct egress_nat_key {
  __be32 ip_src;
  __be32 ip_dst;
  __be16 port_src;
  __be16 port_dst;
};
// egress nat value
struct egress_nat_value {
  __be32 ip_src_new;
  __be16 port_src_new;
};
// reverse nat key
struct reverse_nat_key {
  __be32 ip_src;
  __be32 ip_dst;
  __be16 port_src;
  __be16 port_dst;
};
// reverse nat value
struct reverse_nat_value {
  __be32 ip_dst_new;
  __be16 port_dst_new;
};
// egress nat table. this table translate and mantains the association between
// (ip_src, ip_dst, port_src, port_dst) and the correspondent mapping into
// new (new_ip_src, new_port_src)
BPF_TABLE("hash", struct egress_nat_key, struct egress_nat_value, egress_nat_table, EGRESS_NAT_TABLE_DIM);
// reverse nat table. this table contains the association for the reverse
// translation of packet of a session coming back to the nat
BPF_TABLE("hash", struct reverse_nat_key, struct reverse_nat_value, reverse_nat_table, REVERSE_NAT_TABLE_DIM);
// first implementation of a pool of ports. (incremental counter)
BPF_TABLE("array", u32, u16, first_free_port, 1);
// public ip
BPF_TABLE("array", u32, __be32, public_ip, 1);
struct eth_hdr {
  __be64   dst:48;
  __be64   src:48;
  __be16   proto;
} __attribute__((packed));
// returns the public ip address set by control plane
static inline __be32 get_public_ip() {
  u32 index = 0;
  __be32 * public_ip_p = public_ip.lookup(&index);
  if (public_ip_p)
    return *public_ip_p;
  return 0;
}
// allocates and returns the first free port for instantiate a new session
static inline __be16 get_free_port() {
  u32 i = 0;
  u16 *new_port_p = first_free_port.lookup(&i);
  if (new_port_p) {
    if(*new_port_p < 1024)
      *new_port_p = 1024;
    *new_port_p = (*new_port_p + 1) % 65535 ;
    return bpf_htons(*new_port_p);
  }
  return 0;
}
// given ip(src,dst) and port(src,dst) returns the correspondent association in
// the nat table
// if no association is present, allocates egress and reverse nat table entries,
// and returns the new association just created
static inline struct egress_nat_value * get_egress_value(__be32 ip_src, __be32 ip_dst, __be16 port_src, __be16 port_dst) {
  // lookup in the egress_nat_table
  struct egress_nat_key egress_key = {};
  egress_key.ip_src = ip_src;
  egress_key.ip_dst = ip_dst;
  egress_key.port_src = port_src;
  egress_key.port_dst = port_dst;
  struct egress_nat_value *egress_value_p = egress_nat_table.lookup(&egress_key);
  if (!egress_value_p) {
    // create rule for egress
    struct egress_nat_value egress_value = {};
    egress_value.ip_src_new = get_public_ip();
    egress_value.port_src_new = get_free_port();
    // push egress rule
    egress_nat_table.lookup_or_init(&egress_key, &egress_value);
    egress_value_p = &egress_value;
    // create rule for reverse
    struct reverse_nat_key reverse_key= {};
    reverse_key.ip_src = egress_key.ip_dst;
    reverse_key.ip_dst = egress_value.ip_src_new;
    reverse_key.port_src = egress_key.port_dst;
    reverse_key.port_dst = egress_value.port_src_new;
    struct reverse_nat_value reverse_value= {};
    reverse_value.ip_dst_new = egress_key.ip_src;
    reverse_value.port_dst_new = egress_key.port_src;
    // push reverse nat rule
    reverse_nat_table.lookup_or_init(&reverse_key, &reverse_value);
  }
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
    // Only manage ICMP Request and Reply
    if (!((icmp->type == ICMP_ECHO) || (icmp->type == ICMP_ECHOREPLY)))
      goto DROP;
    switch (md->in_ifc) {
      case IN_IFC: goto EGRESS_ICMP;
      case OUT_IFC: goto REVERSE_ICMP;
    }
    goto DROP;
    EGRESS_ICMP: ;
      // packet exiting the nat, apply nat translation
      struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr, ip->daddr, icmp->un.echo.id, 0);
      if (egress_value_p) {
      #ifdef BPF_TRACE_EGRESS_ICMP
        bpf_trace_printk("[nat-%d]: EGRESS NAT ICMP icmp->id translation: %d->%d\n", md->module_id, bpf_htons(icmp->un.echo.id), bpf_htons(egress_value_p->port_src_new));
      #endif
        // change icmp id
        __be32 old_icmp_id = icmp->un.echo.id;
        __be32 new_icmp_id = egress_value_p->port_src_new;
        l4sum = bpf_csum_diff(&old_icmp_id, 4, &new_icmp_id, 4, l4sum);
        icmp->un.echo.id = (__be16) new_icmp_id;
        // change ip saddr
        __be32 old_ip = ip->saddr;
        __be32 new_ip = egress_value_p->ip_src_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->saddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, ICMP_CSUM_OFFSET, 0, l4sum, 0);
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
      reverse_key.ip_src = ip->saddr;
      reverse_key.ip_dst = ip->daddr;
      reverse_key.port_src = 0;
      reverse_key.port_dst = icmp->un.echo.id;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_ICMP
        bpf_trace_printk("[nat-%d]: REVERSE NAT ICMP icmp->id translation: %d->%d\n", md->module_id , bpf_htons(icmp->un.echo.id) , bpf_htons(reverse_value_p->port_dst_new));
      #endif
        // change icmp id
        __be32 old_icmp_id = icmp->un.echo.id;
        __be32 new_icmp_id = reverse_value_p->port_dst_new;
        l4sum = bpf_csum_diff(&old_icmp_id, 4, &new_icmp_id, 4, l4sum);
        icmp->un.echo.id = (__be16) new_icmp_id;
        // change ip daddr
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, ICMP_CSUM_OFFSET, 0, l4sum, 0);
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
      struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr, ip->daddr, udp->source, udp->dest);
      if(egress_value_p){
      #ifdef BPF_TRACE_EGRESS_UDP
        bpf_trace_printk("[nat-%d]: EGRESS NAT UDP port->source translation: %d->%d\n", md->module_id, bpf_ntohs(udp->source), bpf_ntohs(egress_value_p->port_src_new));
      #endif
        // change udp source
        __be32 old_udp_source = udp->source;
        __be32 new_udp_source = egress_value_p->port_src_new;
        l4sum = bpf_csum_diff(&old_udp_source, 4, &new_udp_source, 4, l4sum);
        udp->source = (__be16) new_udp_source;
        // change ip
        __be32 old_ip = ip->saddr;
        __be32 new_ip = egress_value_p->ip_src_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->saddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l4sum, 0);
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
      reverse_key.ip_src = ip->saddr;
      reverse_key.ip_dst = ip->daddr;
      reverse_key.port_src = udp->source;
      reverse_key.port_dst = udp->dest;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_UDP
        bpf_trace_printk("[nat-%d]: REVERSE NAT UDP port->dest translation: %d->%d\n", md->module_id, bpf_ntohs(udp->dest), bpf_ntohs(reverse_value_p->port_dst_new));
      #endif
        // change udp dest
        __be32 old_udp_dest = udp->dest;
        __be32 new_udp_dest = reverse_value_p->port_dst_new;
        l4sum = bpf_csum_diff(&old_udp_dest, 4, &new_udp_dest, 4, l4sum);
        udp->dest = (__be16) new_udp_dest;
        // change ip
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l4_csum_replace(skb2, UDP_CSUM_OFFSET, 0, l4sum, 0);
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
      struct egress_nat_value *egress_value_p = get_egress_value(ip->saddr, ip->daddr, tcp->source, tcp->dest);
      if (egress_value_p) {
      #ifdef BPF_TRACE_EGRESS_TCP
        bpf_trace_printk("[nat-%d]: EGRESS NAT TCP port->source translation: %d->%d\n", md->module_id, bpf_ntohs(tcp->source), bpf_ntohs(egress_value_p->port_src_new));
      #endif
        // change tcp source
        __be32 old_tcp_source = tcp->source;
        __be32 new_tcp_source = egress_value_p->port_src_new;
        l4sum = bpf_csum_diff(&old_tcp_source, 4, &new_tcp_source, 4, l4sum);
        tcp->source = (__be16) new_tcp_source;
        // change ip
        __be32 old_ip = ip->saddr;
        __be32 new_ip = egress_value_p->ip_src_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->saddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l4sum, 0);
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
      reverse_key.ip_src = ip->saddr;
      reverse_key.ip_dst = ip->daddr;
      reverse_key.port_src = tcp->source;
      reverse_key.port_dst = tcp->dest;
      struct reverse_nat_value *reverse_value_p = reverse_nat_table.lookup(&reverse_key);
      if (reverse_value_p) {
      #ifdef BPF_TRACE_REVERSE_TCP
        bpf_trace_printk("[nat-%d]: REVERSE NAT TCP port->dest translation: %d->%d\n", md->module_id, bpf_ntohs(tcp->dest), bpf_ntohs(reverse_value_p->port_dst_new));
      #endif
        // change udp dest
        __be32 old_tcp_dest = tcp->dest;
        __be32 new_tcp_dest = reverse_value_p->port_dst_new;
        l4sum = bpf_csum_diff(&old_tcp_dest, 4, &new_tcp_dest, 4, l4sum);
        tcp->dest = (__be16) new_tcp_dest;
        // change ip
        __be32 old_ip = ip->daddr;
        __be32 new_ip = reverse_value_p->ip_dst_new;
        l3sum = bpf_csum_diff(&old_ip, 4, &new_ip, 4, l3sum);
        ip->daddr = new_ip;
        // update checksum(s)
        bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l3sum, IS_PSEUDO | 0);
        bpf_l4_csum_replace(skb2, TCP_CSUM_OFFSET, 0, l4sum, 0);
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
