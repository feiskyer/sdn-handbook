# 虚拟网络设备

Linux提供了许多虚拟设备，这些虚拟设备有助于构建复杂的网络拓扑，满足各种网络需求。

## 网桥（bridge）

网桥是一个二层设备，工作在链路层，主要是根据MAC学习来转发数据到不同的port。

```sh
# 创建网桥
brctl addbr br0
# 添加设备到网桥
brctl addif br0 eth1
# 查询网桥mac表
brctl showmacs br0
```

## veth

veth pair是一对虚拟网络设备，一端发送的数据会由另外一端接受，常用于不同的网络命名空间。

```sh
# 创建veth pair
ip link add veth0 type veth peer name veth1

# 将veth1放入另一个netns
ip link set veth1 netns newns
```

## TAP/TUN

TAP/TUN设备是一种让用户态程序向内核协议栈注入数据的设备，TAP等同于一个以太网设备，工作在二层；而TUN则是一个虚拟点对点设备，工作在三层。

```sh
ip tuntap add tap0 mode tap
ip tuntap add tun0 mode tun
```
