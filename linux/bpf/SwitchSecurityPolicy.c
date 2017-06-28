
#include <bcc/proto.h>
#include <bcc/helpers.h>
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/in.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#define BPF_TRACE
#define IP_SECURITY_INGRESS
#define MAC_SECURITY_INGRESS
#define MAC_SECURITY_EGRESS
#define MAX_PORTS 32
#define PRINT_MAC(x) (bpf_htonll(x)>>16)
/*
  Fowarding Table: MAC Address to port association.  This table is filled
  in the learning phase and then is used in the forwarding phase to decide
  where to send a packet
*/
BPF_TABLE("hash", __be64, u32, fwdtable, 1024);
/*
  The Security Mac Table (securitymac) associate to each port the allowed mac
  address. If no entry is associated with the port, the port security is not
  applied to the port.
*/
BPF_TABLE("hash", u32, __be64, securitymac, MAX_PORTS);
/*
  The Security Ip Table (securityip) associates to each port the allowed ip
  address. If no entry is associated with the port, the port security is not
  applied to the port.
*/
BPF_TABLE("hash", u32, __be32, securityip, MAX_PORTS);
struct eth_hdr {
  __be64   dst:48;
  __be64   src:48;
  __be16   proto;
} __attribute__((packed));
static int handle_rx(void *skb, struct metadata *md) {
  struct __sk_buff *skb2 = (struct __sk_buff *)skb;
  void *data = (void *)(long)skb2->data;
  void *data_end = (void *)(long)skb2->data_end;
  struct eth_hdr *eth = data;
  if (data + sizeof(*eth) > data_end)
    return RX_DROP;
  u32 in_ifc = md->in_ifc;
  #ifdef BPF_TRACE
    bpf_trace_printk("[switch-%d]: in_ifc=%d\n", md->module_id, in_ifc);
  #endif
  // port security on source mac
  #ifdef MAC_SECURITY_INGRESS
  __be64 *mac_lookup = securitymac.lookup(&in_ifc);
  if (mac_lookup)
    if (eth->src != *mac_lookup) {
      #ifdef BPF_TRACE
        bpf_trace_printk("[switch-%d]: mac INGRESS %lx mismatch %lx -> DROP\n",
          md->module_id, PRINT_MAC(eth->src), PRINT_MAC(*mac_lookup));
      #endif
      return RX_DROP;
    }
  #endif
  // port security on source ip
  #ifdef IP_SECURITY_INGRESS
  if (eth->proto == bpf_htons(ETH_P_IP)) {
    __be32 *ip_lookup = securityip.lookup(&in_ifc);
    if (ip_lookup) {
      struct ip_t *ip = data + sizeof(*eth);
      if (data + sizeof(*eth) + sizeof(*ip) > data_end)
        return RX_DROP;
      if (ip->src != *ip_lookup) {
        #ifdef BPF_TRACE
          bpf_trace_printk("[switch-%d]: IP INGRESS %x mismatch %x -> DROP\n",
            md->module_id, bpf_htonl(ip->src), bpf_htonl(*ip_lookup));
        #endif
        return RX_DROP;
      }
    }
  }
  #endif
  #ifdef BPF_TRACE
    bpf_trace_printk("[switch-%d]: mac src:%lx dst:%lx\n",
      md->module_id, PRINT_MAC(eth->src), PRINT_MAC(eth->dst));
  #endif
  //LEARNING PHASE: mapping in_ifc with src_interface
  __be64 src_key = eth->src;
  //lookup in fwdtable. if no key present initialize with interface
  u32 *interface_lookup = fwdtable.lookup_or_init(&src_key, &in_ifc);
  //if the same mac has changed interface, update it
  if (*interface_lookup != in_ifc)
    *interface_lookup = in_ifc;
  //FORWARDING PHASE: select interface(s) to send the packet
  __be64 dst_mac = eth->dst;
  //lookup in forwarding table fwdtable
  u32 *dst_interface = fwdtable.lookup(&dst_mac);
  if (dst_interface) {
    //HIT in forwarding table
    //redirect packet to dst_interface
    #ifdef MAC_SECURITY_EGRESS
    u32 out_iface = *dst_interface;
    __be64 *mac_lookup = securitymac.lookup(&out_iface);
    if (mac_lookup)
      if (eth->dst != *mac_lookup){
        #ifdef BPF_TRACE
          bpf_trace_printk("[switch-%d]: mac EGRESS %lx mismatch %lx -> DROP\n",
            md->module_id, PRINT_MAC(eth->dst), PRINT_MAC(*mac_lookup));
        #endif
        return RX_DROP;
      }
    #endif
    /* do not send packet back on the ingress interface */
    if (*dst_interface == in_ifc)
      return RX_DROP;
    pkt_redirect(skb, md, *dst_interface);
    #ifdef BPF_TRACE
      bpf_trace_printk("[switch-%d]: redirect out_ifc=%d\n", md->module_id, *dst_interface);
    #endif
    return RX_REDIRECT;
  } else {
    #ifdef BPF_TRACE
      bpf_trace_printk("[switch-%d]: Broadcast\n", md->module_id);
    #endif
    pkt_controller(skb, md, PKT_BROADCAST);
    return RX_CONTROLLER;
  }
}
