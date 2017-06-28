#include <bcc/proto.h>
#include <bcc/helpers.h>
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/in.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#include <uapi/linux/udp.h>
//#define BPF_TRACE
#define ETHERNET_BROADCAST 0xffffffffffffULL
/* See RFC 2131 */
struct dhcp_packet {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr_nip;
  uint32_t gateway_nip;
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint32_t cookie;
} __attribute__((packed));
struct config_t {
  __be32 server_ip;
  __be64 server_mac;
};
/* config: contains the configuration of the module */
BPF_TABLE("array", unsigned int, struct config_t, config, 1);
struct eth_hdr {
  __be64   dst:48;
  __be64   src:48;
  __be16   proto;
} __attribute__((packed));
static int handle_rx(void *skb, struct metadata *md) {
  struct __sk_buff *skb2 = (struct __sk_buff *)skb;
  void *data = (void *)(long)skb2->data;
  void *data_end = (void *)(long)skb2->data_end;
  unsigned int zero = 0;
  /* get server configuration */
  struct config_t *cfg = config.lookup(&zero);
  if (!cfg) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] no config found\n");
    #endif
    goto DROP;
  }
  struct eth_hdr *eth = data;
  if (data + sizeof(*eth) > data_end)
    goto DROP;
  /* check that the packet is a dhcp packet*/
  if (eth->dst != ETHERNET_BROADCAST && eth->dst != cfg->server_mac) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] no dst mac\n");
    #endif
    goto DROP;
  }
  if (eth->proto != bpf_htons(ETH_P_IP)) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] no IP\n");
    #endif
    goto DROP;
  }
  struct iphdr *ip = data + sizeof(*eth);
  if (data + sizeof(*eth) + sizeof(*ip) > data_end)
    goto DROP;
  if (ip->daddr != INADDR_BROADCAST  && ip->daddr != cfg->server_ip) {
    goto DROP;
  }
  // is the packet UDP?
  if (ip->protocol != IPPROTO_UDP) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] packet is not udp\n");
    #endif
    goto DROP;
  }
  struct udphdr *udp = data + sizeof(*eth) + sizeof(*ip);
  if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) > data_end)
    goto DROP;
  if (udp->dest != bpf_htons(67) || udp->source != bpf_htons(68)) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] packet has invalid port numbers\n");
    #endif
    goto DROP;
  }
  struct dhcp_packet *dhcp = data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);
  if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + sizeof(*dhcp) > data_end)
    goto DROP;
  if (dhcp->op != 1) {
    #ifdef BPF_TRACE
    bpf_trace_printk("[dhcp] dhcp packet has dhcp->op != 1\n");
    #endif
    goto DROP;
  }
  #ifdef BPF_TRACE
  bpf_trace_printk("[dhcp] send packet to controller\n");
  #endif
  return RX_CONTROLLER;
DROP:
  #ifdef BPF_TRACE
  bpf_trace_printk("[dhcp] dropping packet\n");
  #endif
  return RX_DROP;
}