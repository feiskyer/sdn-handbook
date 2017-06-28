#include <bcc/proto.h>
#include <bcc/helpers.h>
#include <uapi/linux/if_arp.h>
#include <uapi/linux/bpf.h>
#include <uapi/linux/icmp.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/in.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#include <uapi/linux/udp.h>
/* The following flags control the debubbing output of the eBPF program.
 * Please note that because of some eBPF limitations all of them cannot
 * be activated at the same time
 */
//#define BPF_TRACE // global trace control (should be comment to get better performance)
#ifdef BPF_TRACE
//#define BPF_TRACE_TTL
//#define BPF_TRACE_INPUT
//#define BPF_TRACE_OUTPUT
//#define BPF_TRACE_ROUTING
//#define BPF_TRACE_ARP
//#define BPF_TRACE_ICMP_ECHO_REPLY
#endif
//#define CHECK_MAC_DST
#define ROUTING_TABLE_DIM  6
#define ROUTER_PORT_N     32
#define ARP_TABLE_DIM     32
#define IP_CSUM_OFFSET (sizeof(struct eth_hdr) + offsetof(struct iphdr, check))
#define ICMP_CSUM_OFFSET (sizeof(struct eth_hdr) + sizeof(struct iphdr) + offsetof(struct icmphdr, checksum))
#define MAC_MULTICAST_MASK 0x10ULL // network byte order
enum {
  SLOWPATH_ARP_REPLY = 1,
  SLOWPATH_ARP_LOOKUP_MISS,
  SLOWPATH_TTL_EXCEEDED
};
/* Routing Table Entry */
struct rt_entry {
  __be32 network;
  __be32 netmask;
  u16 port;
  __be32 nexthop;  // ip address of next hop, 0 is locally reachable
};
/* Router Port */
struct r_port {
  __be32 ip;
  __be32 netmask;
  __be64 mac:48;
};
/*
  The Routing table is implemented as an array of struct rt_entry (Routing Table Entry)
  the longest prefix matching algorithm (at least a simplified version)
  is implemented performing a bounded loop over the entries of the routing table.
  We assume that the control plane puts entry ordered from the longest netmask
  to the shortest one.
*/
BPF_TABLE("array", u32, struct rt_entry, routing_table, ROUTING_TABLE_DIM);
/*
  Router Port table provides a way to simulate the physical interface of the router
  The ip address is used to answer to the arp request (TO IMPLEMENT)
  The mac address is used as mac_scr for the outcoming packet on that interface,
  and as mac address contained in the arp reply
*/
BPF_TABLE("hash", u16, struct r_port, router_port, ROUTER_PORT_N);
/*
  Arp Table implements a mapping between ip and mac addresses.
*/
BPF_TABLE("hash", u32, u64, arp_table, ARP_TABLE_DIM);
#define PRINT_MAC(x) (bpf_htonll(x)>>16)
struct eth_hdr {
  __be64   dst:48;
  __be64   src:48;
  __be16   proto;
} __attribute__((packed));
struct arp_hdr {
  __be16          ar_hrd;     /* format of hardware address	*/
  __be16          ar_pro;     /* format of protocol address	*/
  unsigned char   ar_hln;     /* length of hardware address	*/
  unsigned char   ar_pln;     /* length of protocol address	*/
  __be16          ar_op;      /* ARP opcode (command)		*/
  __be64          ar_sha:48;  /* sender hardware address	*/
  __be32          ar_sip;     /* sender IP address		*/
  __be64          ar_tha:48;  /* target hardware address	*/
  __be32          ar_tip;     /* target IP address		*/
} __attribute__((packed));
static int handle_rx(void *skb, struct metadata *md) {
  struct __sk_buff *skb2 = (struct __sk_buff *)skb;
  void *data = (void *)(long)skb2->data;
  void *data_end = (void *)(long)skb2->data_end;
  struct eth_hdr *eth = data;
  if (data + sizeof(*eth) > data_end)
    goto DROP;
  #ifdef BPF_TRACE_INPUT
  bpf_trace_printk("[router-%d]: in_ifc:%d\n", md->module_id, md->in_ifc);
  //bpf_trace_printk("[router-%d]: eth_type:%x mac_scr:%lx mac_dst:%lx\n",
  //  md->module_id, bpf_htons(eth->proto), PRINT_MAC(eth->src), PRINT_MAC(eth->dst));
  #endif
  struct r_port *in_port = router_port.lookup(&md->in_ifc);
  if (!in_port) {
    #ifdef BPF_TRACE_INPUT
    bpf_trace_printk("[router-%d]: received packet from non valid port : '%d'\n",
      md->module_id, md->in_ifc);
    #endif
    goto DROP;
  }
  /*
    Check if the mac destination of the packet is multicast, broadcast, or the
    unicast address of the router port.  If not, drop the packet.
  */
  #ifdef CHECK_MAC_DST
  if (eth->dst != in_port->mac && !(eth->dst & (__be64) MAC_MULTICAST_MASK)) {
      #ifdef BPF_TRACE_INPUT
      bpf_trace_printk("[router-%d]: mac destination %lx MISMATCH %lx\n",
        md->module_id, PRINT_MAC(eth->dst), PRINT_MAC(in_port->mac));
      #endif
      goto DROP;
  }
  #endif
  switch (eth->proto) {
    case htons(ETH_P_IP): goto IP;   // ipv4 packet
    case htons(ETH_P_ARP): goto ARP; // arp packet
    default: goto DROP;
  }
IP: ; // ipv4 packet
  __be32 l3sum = 0;
  struct iphdr *ip = data + sizeof(*eth);
  if (data + sizeof(*eth) + sizeof(*ip) > data_end)
    goto DROP;
  #ifdef BPF_TRACE_TTL
  bpf_trace_printk("[router-%d]: ttl: %u\n", md->module_id, ip->ttl);
  #endif
  /* ICMP Echo Responder for router ports */
  if (ip->protocol == IPPROTO_ICMP) {
    __be32 l4sum = 0;
    struct icmphdr *icmp = data + sizeof(*eth) + sizeof(*ip);
    if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*icmp) > data_end)
      goto DROP;
    /* Only manage ICMP Request */
    if (icmp->type == ICMP_ECHO && in_port->ip == ip->daddr) {
      // Reply to ICMP Echo request
      __u32 old_type = icmp->type;
      __u32 new_type = ICMP_ECHOREPLY;
      l4sum = bpf_csum_diff(&old_type, 4, &new_type, 4, l4sum);
      icmp->type = (__u8) new_type;
      __be32 old_src = ip->saddr;
      __be32 old_dst = ip->daddr;
      ip->daddr = old_src;
      ip->saddr = old_dst;
      __be64 old_src_mac = eth->src;
      __be64 old_dst_mac = eth->dst;
      eth->src = old_dst_mac;
      eth->dst = old_src_mac;
      #ifdef BPF_TRACE_ICMP_ECHO_REPLY
      bpf_trace_printk("[router-%d]: ICMP ECHO Request from 0x%x port %d. Generating Reply ...\n",
        md->module_id, bpf_htonl(ip->saddr), md->in_ifc);
      #endif
      bpf_l4_csum_replace(skb2, ICMP_CSUM_OFFSET, 0, l4sum, 0);
      pkt_redirect(skb, md, md->in_ifc);
      return RX_REDIRECT;
    }
  }
  if (ip->ttl == 1) {
    #ifdef BPF_TRACE_TTL
    bpf_trace_printk("[router-%d]: packet DROP (ttl = 0)\n", md->module_id);
    #endif
    // Set router port ip address as metadata[0]
    u32 mdata[3];
    mdata[0] = bpf_htonl(in_port->ip);
    pkt_set_metadata(skb, mdata);
    // Send packet to slowpath
    pkt_controller(skb, md, SLOWPATH_TTL_EXCEEDED);
    return RX_CONTROLLER;
  }
  /*
    ROUTING ALGORITHM (simplified)
    for each item in the routing table (upbounded loop)
    apply the netmask on dst_ip_address
    (possible optimization, not recompute if at next iteration the netmask is the same)
    if masked address == network in the routing table
      1- change src mac to otuput port mac
      2- change dst mac to lookup arp table (or send to fffffffffffff)
      3- forward the packet to dst port
  */
  int i = 0;
  struct rt_entry *rt_entry_p = 0;
  #pragma unroll
  for (i = 0; i < ROUTING_TABLE_DIM; i++) {
    u32 t = i;
    rt_entry_p = routing_table.lookup(&t);
      if (rt_entry_p) {
      if ((ip->daddr & rt_entry_p->netmask) == rt_entry_p->network) {
        goto FORWARD;
      }
    }
  }
  #ifdef BPF_TRACE_ROUTING
  bpf_trace_printk("[router-%d]: no routing table match for %x\n",
    md->module_id, bpf_htonl(ip->daddr));
  #endif
  goto DROP;
FORWARD: ;
  #ifdef BPF_TRACE_ROUTING
  bpf_trace_printk("[router-%d]: routing table match (#%d) network: %x\n",
    md->module_id, i, bpf_htonl(rt_entry_p->network));
  #endif
  // Select out interface
  u16 out_port = rt_entry_p->port;
  struct r_port *r_port_p = router_port.lookup(&out_port);
  if (!r_port_p) {
    #ifdef BPF_TRACE_ROUTING
    bpf_trace_printk("[router-%d]: Out port '%d' not found\n",
      md->module_id, out_port);
    #endif
    goto DROP;
  }
  __be32 dst_ip = 0;
  if (rt_entry_p->nexthop == 0) {
    // Next Hop is local, directly lookup in arp table for the destination ip.
    dst_ip = ip->daddr;
  } else {
    // Next Hop not local, lookup in arp table for the next hop ip address.
    dst_ip = rt_entry_p->nexthop;
  }
  __be64 *mac_entry = arp_table.lookup(&dst_ip);
  if (!mac_entry) {
    #ifdef BPF_TRACE_ARP
    bpf_trace_printk("[router-%d]: arp lookup failed. Send to controller", md->module_id);
    #endif
    // Set metadata and send packet to slowpath
    u32 mdata[3];
    mdata[0] = bpf_htonl(dst_ip);
    mdata[1] = out_port;
    mdata[2] = bpf_htonl(r_port_p->ip);
    pkt_set_metadata(skb, mdata);
    pkt_controller(skb, md, SLOWPATH_ARP_LOOKUP_MISS);
    return RX_CONTROLLER;
  }
  #ifdef BPF_TRACE_OUTPUT
  bpf_trace_printk("[router-%d]: in: %d out: %d REDIRECT\n",
    md->module_id, md->in_ifc, out_port);
  #endif
  eth->dst = *mac_entry;
  eth->src = r_port_p->mac;
  /* Decrement TTL and update checksum */
  __u32 old_ttl = ip->ttl;
  __u32 new_ttl = ip->ttl - 1;
  l3sum = bpf_csum_diff(&old_ttl, 4, &new_ttl, 4, l3sum);
  ip->ttl = (__u8) new_ttl;
  bpf_l3_csum_replace(skb2, IP_CSUM_OFFSET, 0, l3sum, 0);
  pkt_redirect(skb,md,out_port);
  return RX_REDIRECT;
ARP: ; // arp packet
  struct arp_hdr *arp = data + sizeof(*eth);
  if (data + sizeof(*eth) + sizeof(*arp) > data_end)
    goto DROP;
  if (arp->ar_op == bpf_htons(ARPOP_REQUEST) && arp->ar_tip == in_port->ip) { // arp request?
    #ifdef BPF_TRACE_ARP
    bpf_trace_printk("[arp]: Somebody is asking for my address\n");
    #endif
    __be64 remotemac = arp->ar_sha;
    __be32 remoteip = arp->ar_sip;
    arp->ar_op = bpf_htons(ARPOP_REPLY);
    arp->ar_tha = remotemac;
    arp->ar_sha = in_port->mac;
    arp->ar_tip = remoteip;
    arp->ar_sip = in_port->ip;
    eth->dst = remotemac;
    eth->src = in_port->mac;
    /* register the requesting mac and ip */
    arp_table.update(&remoteip, &remotemac);
    pkt_redirect(skb, md, md->in_ifc);
    return RX_REDIRECT;
  } else if (arp->ar_op == bpf_htons(ARPOP_REPLY)) { //arp reply
    #ifdef BPF_TRACE_ARP
    bpf_trace_printk("[router-%d]: packet is arp reply\n", md->module_id);
    #endif
    __be64 mac_ = arp->ar_sha;
    __be32 ip_  = arp->ar_sip;
    arp_table.update(&ip_, &mac_);
    // notify the slowpath. New arp reply received.
    pkt_controller(skb, md, SLOWPATH_ARP_REPLY);
    return RX_CONTROLLER;
  }
  return RX_DROP;
DROP:
  #ifdef BPF_TRACE_OUTPUT
  bpf_trace_printk("[router-%d]: in: %d out: -- DROP\n", md->module_id, md->in_ifc);
  #endif
  return RX_DROP;
}
