# Open vSwitch

## OVS安装

### CentOS

```sh
yum install centos-release-openstack-newton
yum install openvswitch
systemctl enable openvswitch
systemctl start openvswitch
```

如果想要安装master版本，可以使用<https://copr.fedorainfracloud.org/coprs/leifmadsen/ovs-master/>的BUILD：

```sh
wget -o /etc/yum.repos.d/ovs-master.repo https://copr.fedorainfracloud.org/coprs/leifmadsen/ovs-master/repo/epel-7/leifmadsen-ovs-master-epel-7.repo
yum install openvswitch openvswitch-ovn-*
```

注：DPDK版本见<https://copr.fedorainfracloud.org/coprs/pmatilai/dpdk-snapshot/>。

## OVS常用命令参考

**如何添加bridge和port**

    ovs-vsctl add-br br0
    ovs-vsctl del-br br0
    ovs-vsctl list-br
    ovs-vsctl add-port br0 eth0
    ovs-vsctl set port eth0 tag=1 #vlan id
    ovs-vsctl del-port br0 eth0
    ovs-vsctl list-ports br0
    ovs-vsctl show

**给OVS端口配置IP**

    ovs−vsctl add−port br-ex port tag=10 −− set Interface port type=internal # default is access
    ifconfig port 192.168.100.1

**如何配置流镜像**

    ovs-vsctl -- set Bridge br-int mirrors=@m -- --id=@tap6a094914-cd get Port tap6a094914-cd -- --id=@tap73e945b4-79 get Port tap73e945b4-79 -- --id=@tapa6cd1168-a2 get Port tapa6cd1168-a2 -- --id=@m create Mirror name=mymirror select-dst-port=@tap6a094914-cd,@tap73e945b4-79 select-src-port=@tap6a094914-cd,@tap73e945b4-79 output-port=@tapa6cd1168-a2

    # clear
    ovs-vsctl remove Bridge br0 mirrors mymirror
    ovs-vsctl clear Bridge br-int mirrors

**利用mirror特性对ovs端口patch-tun抓包**

    ip link add name snooper0 type dummy
    ip link set dev snooper0 up
    ovs-vsctl add-port br-int snooper0
    ovs-vsctl -- set Bridge br-int mirrors=@m  \
    -- --id=@snooper0 get Port snooper0  \
    -- --id=@patch-tun get Port patch-tun  \
    -- --id=@m create Mirror name=mymirror select-dst-port=@patch-tun \
    select-src-port=@patch-tun output-port=@snooper0

    # capture
    tcpdump -i snooper0

    # clear
    ovs-vsctl clear Bridge br-int mirrors
    ip link delete dev snooper0

**如何配置QOS，比如队列和限速**

    ovs-vsctl -- set Port eth2 qos=@newqos  \
    -- --id=@newqos create QoS type=linux-htb other-config:max-rate=1000000000 queues=0=@q0,1=@q1 \
    -- --id=@q0 create  Queue dscp =1 other-config:min-rate=100000000 other-config:max-rate=100000000 \
    -- --id=@q1 create Queue other-config:min-rate=500000000

    # clear
    ovs-vsctl clear Port eth2 qos
    ovs-vsctl list qos
    ovs-vsctl destroy qos _uuid
    ovs-vsctl list qos
    ovs-vsctl destroy queue _uuid

**如何配置流监控sflow**

    ovs-vsctl -- --id=@s create sFlow agent=eth2 target=\"10.0.0.1:6343\" header=128 sampling=64 polling=10  -- set Bridge br-int sflow=@s
    ovs-vsctl -- clear Bridge br-int sflow

**如何配置流规则**
    
    ovs-ofctl add-flow br-int idle_timeout=0,in_port=2,dl_type=0x0800,dl_src=00:88:77:66:55:44,dl_dst=11:22:33:44:55:66,nw_src=1.2.3.4,nw_dst=5.6.7.8,nw_proto=1,tp_src=1,tp_dst=2,actions=drop
    ovs-ofctl del-flows br-int in_port=2 //in_port=2的所有流规则被删除
    ovs-ofctl  dump-ports br-int
    ovs-ofctl  dump-flows br-int
    ovs-ofctl show br-int //查看端口号

- 支持字段还有nw_tos,nw_ecn,nw_ttl,dl_vlan,dl_vlan_pcp,ip_frag，arp_sha,arp_tha,ipv6_src,ipv6_dst等;
- 支持流动作还有output：port，mod_dl_src/mod_dl_dst,set field等;


**如何查看OVS的配置**

    ovs-vsctl list/set/get/add/remove/clear/destroy table record column [VALUE]
    
其中，TABLE名支持bridge,controller,interface,mirror,netflow,open_vswitch,port,qos,queue,ssl,sflow

**配置VXLAN/GRE**

    ovs-vsctl add-port br-ex port -- set interface port type=vxlan options:remote_ip=192.168.100.3
    ovs−vsctl add−port br-ex port −− set Interface port type=gre options:remote_ip=192.168.100.3
    ovs-vsctl set interface vxlan type=vxlan option:remote_ip=140.113.215.200 option:key=flow ofport_request=9

**显示并学习MAC**

    ovs-appctl fdb/show br-ex

**设置控制器地址**

    ovs-vsctl set-controller br-ex tcp:192.168.100.1:6633
    ovs-vsctl get-controller br0

## 流表管理

### 流规则组成

每条流规则由一系列字段组成，分为基本字段、条件字段和动作字段三部分：

- 基本字段包括生效时间duration_sec、所属表项table_id、优先级priority、处理的数据包数n_packets，空闲超时时间idle_timeout等，空闲超时时间idle_timeout以秒为单位，超过设置的空闲超时时间后该流规则将被自动删除，空闲超时时间设置为0表示该流规则永不过期，idle_timeout将不包含于ovs-ofctl dump-flows brname的输出中。
- 条件字段包括输入端口号in_port、源目的mac地址dl_src/dl_dst、源目的ip地址nw_src/nw_dst、数据包类型dl_type、网络层协议类型nw_proto等，可以为这些字段的任意组合，但在网络分层结构中底层的字段未给出确定值时上层的字段不允许给确定值，即一条流规则中允许底层协议字段指定为确定值，高层协议字段指定为通配符(不指定即为匹配任何值)，而不允许高层协议字段指定为确定值，而底层协议字段却为通配符(不指定即为匹配任何值)，否则，ovs-vswitchd 中的流规则将全部丢失，网络无法连接。
- 动作字段包括正常转发normal、定向到某交换机端口output：port、丢弃drop、更改源目的mac地址mod_dl_src/mod_dl_dst等，一条流规则可有多个动作，动作执行按指定的先后顺序依次完成。

流规则中可包含通配符和简写形式，任何字段都可等于*或ANY，如丢弃所有收到的数据包

    ovs-ofctl add-flow xenbr0 dl_type=*,nw_src=ANY,actions=drop

简写形式为将字段组简写为协议名，目前支持的简写有ip，arp，icmp，tcp，udp，与流规则条件字段的对应关系如下:

    dl_type=0x0800 <=>ip
    dl_type=0x0806 <=>arp
    dl_type=0x0800，nw_proto=1 <=> icmp
    dl_type=0x0800，nw_proto=6 <=> tcp
    dl_type=0x0800，nw_proto=17 <=> udp
    dl_type=0x86dd. <=> ipv6
    dl_type=0x86dd,nw_proto=6. <=> tcp6
    dl_type=0x86dd,nw_proto=17. <=> udp6
    dl_type=0x86dd,nw_proto=58. <=> icmp6

屏蔽某个IP

    ovs-ofctl add-flow xenbr0 idle_timeout=0,dl_type=0x0800,nw_src=119.75.213.50,actions=drop


数据包重定向

    ovs-ofctl add-flow xenbr0 idle_timeout=0,dl_type=0x0800,nw_proto=1,actions=output:4


去除VLAN tag

    ovs-ofctl add-flow xenbr0 idle_timeout=0,in_port=3,actions=strip_vlan,normal


更改数据包源IP地址后转发

    ovs-ofctl add-flow xenbr0 idle_timeout=0,in_port=3,actions=mod_nw_src:211.68.52.32,normal

注包

    # 格式为：ovs-ofctl packet-out switch in_port actions packet
    # 其中，packet为hex格式数据包
    ovs-ofctl packet-out br2 none output:2 040815162342FFFFFFFFFFFF07C30000

**流表常用字段**

- in_port=port	传递数据包的端口的 OpenFlow 端口编号
- dl_vlan=vlan	数据包的 VLAN Tag 值，范围是 0-4095，0xffff 代表不包含 VLAN Tag 的数据包
- dl_src=<MAC>和dl_dst=<MAC> 匹配源或者目标的 MAC 地址
    01:00:00:00:00:00/01:00:00:00:00:00 代表广播地址
    00:00:00:00:00:00/01:00:00:00:00:00 代表单播地址
- dl_type=ethertype	匹配以太网协议类型，其中：
    dl_type=0x0800 代表 IPv4 协议
    dl_type=0x086dd 代表 IPv6 协议
    dl_type=0x0806 代表 ARP 协议
- nw_src=ip[/netmask]和nw_dst=ip[/netmask]
    当 dl_typ=0x0800 时，匹配源或者目标的 IPv4 地址，可以使 IP 地址或者域名
- nw_proto=proto	和 dl_type 字段协同使用。当 dl_type=0x0800 时，匹配 IP 协议编号；当 dl_type=0x086dd 代表 IPv6 协议编号
- table=number	指定要使用的流表的编号，范围是 0-254。在不指定的情况下，默认值为 0。通过使用流表编号，可以创建或者修改多个 Table 中的 Flow
- reg<idx>=value[/mask]	交换机中的寄存器的值。当一个数据包进入交换机时，所有的寄存器都被清零，用户可以通过 Action 的指令修改寄存器中的值

**常见的操作**

- output:port: 输出数据包到指定的端口。port 是指端口的 OpenFlow 端口编号
- mod_vlan_vid: 修改数据包中的 VLAN tag
- strip_vlan: 移除数据包中的 VLAN tag
- mod_dl_src/ mod_dl_dest: 修改源或者目标的 MAC 地址信息
- mod_nw_src/mod_nw_dst: 修改源或者目标的 IPv4 地址信息
- resubmit:port: 替换流表的 in_port 字段，并重新进行匹配
- load:value−>dst[start..end]: 写数据到指定的字段

**跟踪数据包的处理过程**

```
ovs-appctl ofproto/trace br0 in_port=3,tcp,nw_src=10.0.0.2,tcp_dst=22

ovs-appctl ofproto/trace br-int \
 in_port=1,dl_src=00:00:00:00:00:01,\
  dl_dst=00:00:00:00:00:02 -generate
```

## Packet out (注包)

```
import binascii
from scapy.all import *
a=Ether(dst="02:ac:10:ff:00:22",src="02:ac:10:ff:00:11")/IP(dst="172.16.255.22",src="172.16.255.11", ttl=10)/ICMP()
print binascii.hexlify(str(a))

ovs-ofctl packet-out br-int 5 "normal" 02AC10FF002202AC10FF001108004500001C000100000A015A9DAC10FF0BAC10FF160800F7FF00000000
```

## OVS文档链接

- [http://openvswitch.org/](http://openvswitch.org/)
- [http://blog.scottlowe.org/](http://blog.scottlowe.org/)
- [https://blog.russellbryant.net/](https://blog.russellbryant.net/)
- [Comparing OpenStack Neutron ML2+OVS and OVN – Control Plane](https://blog.russellbryant.net/2016/12/19/comparing-openstack-neutron-ml2ovs-and-ovn-control-plane/)

