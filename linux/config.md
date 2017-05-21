# Linux网络配置

Linux网络配置方法简介。

## 配置IP地址

```sh
# 使用ifconfig
ifconfig eth0 192.168.1.3 netmask 255.255.255.0

# 使用用ip命令增加一个IP
ip addr add 192.168.1.4/24 dev eth0

# 使用ifconfig增加网卡别名
ifconfig eth0:0 192.168.1.10
```

这样配置的IP地址重启机器后会丢失，所以一般应该把网络配置写入文件中。如Ubuntu可以将网卡配置写入`/etc/network/interfaces`（Redhat和CentOS则需要写入` /etc/sysconfig/network-scripts/ifcfg-eth0`中）：

```
auto lo
iface lo inet loopback

iface eth0 inet static
    address 192.168.1.3
    netmask 255.255.255.0
    gateway 192.168.1.1

auto eth1
iface eth1 inet dhcp
```

## 配置默认路由

```sh
# 使用route命令
route add default gw 192.168.1.1
# 也可以使用ip命令
ip route add default via 192.168.1.1
```

## 配置VLAN

```sh
# 安装并加载内核模块
apt-get install vlan
modprobe 8021q

# 添加vlan
vconfig add eth0 100
ifconfig eth0.100 192.168.100.2 netmask 255.255.255.0

# 删除vlan
vconfig rem eth0.100
```

## 配置硬件选项

```sh
# 改变speed
ethtool -s eth0 speed 1000 duplex full

# 关闭GRO
ethtool -K eth0 gro off

# 开启网卡多队列
ethtool -L eth0 combined 4

# 开启vxlan offload
ethtool -K ens2f0 rx-checksum on
ethtool -K ens2f0 tx-udp_tnl-segmentation on

# 查询网卡统计
ethtool -S eth0
```
