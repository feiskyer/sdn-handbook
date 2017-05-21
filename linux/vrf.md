# Virtual Routing and Forwarding (VRF)

Linux内核的Virtual Routing and Forwarding (VRF) 是由路由表和一组网络设备组成的路由实例。

## VRF安装

Ubuntu默认不包括vrf内核模块，需要额外安装:

```sh
apt-get install linux-headers-4.10.0-14-generic linux-image-extra-4.10.0-14-generic
reboot
apt-get install linux-image-extra-$(uname -r)
modprobe vrf
```

## VRF示例

```sh
# create vrf device
ip link add vrf-blue type vrf table 10
ip link set dev vrf-blue up

# An l3mdev FIB rule directs lookups to the table associated with the device.
# A single l3mdev rule is sufficient for all VRFs.
# Prior to the v4.8 kernel iif and oif rules are needed for each VRF device:
ip ru add oif vrf-blue table 10
ip ru add iif vrf-blue table 10

#Set the default route for the table (and hence default route for the VRF).
ip route add table 10 unreachable default

# Enslave L3 interfaces to a VRF device.
# Local and connected routes for enslaved devices are automatically moved to
# the table associated with VRF device. Any additional routes depending on
# the enslaved device are dropped and will need to be reinserted to the VRF
# FIB table following the enslavement.
ip link set dev eth1 master vrf-blue

# The IPv6 sysctl option keep_addr_on_down can be enabled to keep IPv6 global
# addresses as VRF enslavement changes.
sysctl -w net.ipv6.conf.all.keep_addr_on_down=1

# Additional VRF routes are added to associated table.
ip route add table 10 ...
```

## 进程绑定VRF

Linux进程可以通过在VRF设备上监听socket来绑定VRF：

```c
setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, dev, strlen(dev)+1);
```

TCP & UDP services running in the default VRF context (ie., not bound
to any VRF device) can work across all VRF domains by enabling the
`tcp_l3mdev_accept` and `udp_l3mdev_accept` sysctl options:

    sysctl -w net.ipv4.tcp_l3mdev_accept=1
    sysctl -w net.ipv4.udp_l3mdev_accept=1

## VRF操作

### 创建VRF

    ip link add dev NAME type vrf table ID

### 查询VRF列表

```
# ip -d link show type vrf
16: vrf-blue: <NOARP,MASTER,UP,LOWER_UP> mtu 65536 qdisc noqueue state UP mode DEFAULT group default qlen 1000
    link/ether 9e:9c:8e:7b:32:a4 brd ff:ff:ff:ff:ff:ff promiscuity 0
    vrf table 10 addrgenmode eui64
```

### 添加网卡到VRF

    ip link set dev eth0 master vrf-blue

### 查询VRF邻接表和路由

    ip neigh show vrf vrf-blue
    ip addr show vrf vrf-blue
    ip -br addr show vrf vrf-blue
    ip route show vrf vrf-blue

## 从VRF中删除网卡

    ip link set dev eth0 nomaster

**参考文档**

- [Linux kernel documentation](https://www.kernel.org/doc/Documentation/networking/vrf.txt)