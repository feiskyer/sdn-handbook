# 负载均衡

## lvs

Linux Virtual Server (lvs) 是Linux内核自带的负载均衡器，也是目前性能最好的软件负载均衡器之一。lvs包括ipvs内核模块和ipvsadm用户空间命令行工具两部分。

在lvs中，节点分为Director Server和Real Server两个角色，其中Director Server是负载均衡器所在节点，而Real Server则是后端服务节点。当用户的请求到达Director Server时，内核netfilter机制的PREROUTING链会将发往本地IP的包转发给INPUT链（也就是ipvs的工作链），在INPUT链上，ipvs根据用户定义的规则对数据包进行处理（如修改目的IP和端口等），并把新的包发送到POSTROUTING链，进而再转发给Real Server。

### 转发模式

**NAT**

NAT模式通过修改数据包的目的IP和目的端口来将包转发给Real Server。它的特点包括

- Director Server必须作为Real Server的网关，并且它们必须处于同一个网段内
- 不需要Real Server做任何特殊配置
- 支持端口映射
- 请求和响应都需要经过Director Server，易成为性能瓶颈

![](images/lvs-nat.png)

**DR**

DR（Direct Route）模式通过修改数据包的目的MAC地址将包转发给Real Server。它的特点包括

- 需要在Real Server的lo上配置vip，并配置arp_ignore和arp_announce忽略对vip的ARP解析请求
- Director Server和Real Server必须在同一个物理网络内，二层可达
- 虽然所有请求包都会经过Director Server，但响应报文不经过，有性能上的优势

![](images/lvs-dr.png)

**TUN**

TUN模式通过将数据包封装在另一个IP包中（源地址为DIP，目的为RIP）将包转发给Real Server。它的特点包括

- Real Server需要在lo上配置vip，但不需要Director Server作为网关
- 不支持端口映射

![](images/lvs-tun.png)

**FULLNAT**

FULLNAT是阿里在NAT基础上增加的一个新转发模式，通过引入local IP（CIP-VIP转换为LIP->RIP，而LIP和RIP均为IDC内网IP）使得物理网络可以跨越不同vlan，代码维护在<https://github.com/alibaba/LVS>上面。其特点是

- 物理网络仅要求三层可达
- Real Server不需要任何特殊配置
- SYNPROXY防止synflooding攻击
- 未进入内核主线，维护复杂

![](images/lvs-fullnat.png)

### 调度算法

- 轮叫调度（Round-Robin Scheduling）
- 加权轮叫调度（Weighted Round-Robin Scheduling）
- 最小连接调度（Least-Connection Scheduling）
- 加权最小连接调度（Weighted Least-Connection Scheduling）
- 基于局部性的最少链接（Locality-Based Least Connections Scheduling）
- 带复制的基于局部性最少链接（Locality-Based Least Connections with Replication Scheduling）
- 目标地址散列调度（Destination Hashing Scheduling）
- 源地址散列调度（Source Hashing Scheduling）
- 最短预期延时调度（Shortest Expected Delay Scheduling）
- 不排队调度（Never Queue Scheduling）

### lvs配置示例

安装ipvs包并开启ip转发

```sh
yum -y install ipvsadm keepalived
sysctl -w net.ipv4.ip_forward=1
```

修改/etc/keepalived/keepalived.conf，增加vip和lvs的配置

```
vrrp_instance VI_3 {
    state MASTER   # 另一节点为BACKUP
    interface eth0
    virtual_router_id 11
    priority 100   # 另一节点为50
    advert_int 1
    authentication {
        auth_type PASS
        auth_pass PASSWORD
    }

    track_script {
        chk_http_port
    }

    virtual_ipaddress {
        192.168.0.100
    }
}

virtual_server 192.168.0.100 9696 {
    delay_loop 30
    lb_algo rr
    lb_kind DR
    persistence_timeout 30
    protocol TCP

    real_server 192.168.0.101 9696 {
        weight 3
        TCP_CHECK {
            connect_timeout 10
            nb_get_retry 3
            delay_before_retry 3
            connect_port 9696
        }
    }

    real_server 192.168.0.102 9696 {
        weight 3
        TCP_CHECK {
            connect_timeout 10
            nb_get_retry 3
            delay_before_retry 3
            connect_port 9696
        }
    }
}
```

重启keepalived：

    systemctl reload keepalived

最后在neutron-server所在机器上为lo配置vip，并抑制ARP响应：

```sh
vip=192.168.0.100
ifconfig lo:1 ${vip} broadcast ${vip} netmask 255.255.255.255
route add -host ${vip} dev lo:1
echo "1" >/proc/sys/net/ipv4/conf/lo/arp_ignore
echo "2" >/proc/sys/net/ipv4/conf/lo/arp_announce
echo "1" >/proc/sys/net/ipv4/conf/all/arp_ignore
echo "2" >/proc/sys/net/ipv4/conf/all/arp_announce
```

### LVS缺点

- Keepalived主备模式设备利用率低；不能横向扩展；VRRP协议，有脑裂的风险。
- ECMP的方式需要了解动态路由协议，LVS和交换机均需要较复杂配置；交换机的HASH算法一般比较简单，增加删除节点会造成HASH重分布，可能导致当前TCP连接全部中断；部分交换机的ECMP在处理分片包时会有BUG。

## Haproxy

Haproxy也是Linux最常用的负载均衡软件之一，兼具性能和功能的组合，同时支持TCP和HTTP负载均衡。

配置和使用方法请见[官网](http://www.haproxy.org/)。

## Nginx

Nginx也是Linux最常用的负载均衡软件之一，常用作反向代理和HTTP负载均衡（当然也支持TCP和UDP负载均衡）。

配置和使用方法请见[官网](https://nginx.org/en/)。

## 自研负载均衡

### Google Maglev

[Maglev](https://research.google.com/pubs/pub44824.html)是Google自研的负载均衡方案，在2008年就已经开始用于生产环境。Maglev安装后不需要预热5秒内就能处理每秒100万次请求。谷歌的性能基准测试中，Maglev实例运行在一个8核CPU下，网络吞吐率上限为12M PPS（数据包每秒）。如果Maglev使用Linux内核网络堆栈则速度会慢下来，吞吐率小于4M PPS。

![](images/maglev.png)

- 路由器ECMP (Equal Cost Multipath) 转发包到Maglev（而不是传统的主从结构)
- Kernel Bypass, CPU绑定，共享内存
- 一致性哈希保证连接不中断

### UCloud Vortex

Vortex参考了Maglev，大致的架构和实现跟Maglev类似：

- ECMP实现集群的负载均衡
- 一致性哈希保证连接不中断
    - 即使是不同的Vortex服务器收到了数据包，仍然能够将该数据包转发到同一台后端服务器
    - 后端服务器变化时，通过连接追踪机制保证当前活动连接的数据包被送往之前选择的服务器，而所有新建连接则会在变化后的服务器集群中进行负载分担
- DPDK提升单机性能 (14M PPS，10G, 64字节线速)
    - 通过RSS直接将网卡队列和CPU Core绑定，消除线程的上下文切换带来的开销
    - Vortex线程间采用高并发无锁的消息队列通信
- DR模式避免额外开销

## 参考文档

- <http://www.linuxvirtualserver.org/>
- <http://www.haproxy.org/>
- [揭秘100G＋线速云负载均衡的设计与实现：从Maglev到Vortex](https://mp.weixin.qq.com/s?src=3&timestamp=1495372816&ver=1&signature=ifj0PRCsXKHVPiVcl-dNxhSlKKKcX6hwO1rz-hbipIrL2weMxHv0bSysMyY-yB-AXJrUZix9kjQCpvsRJnxF1grXi*O6nZZjaUFFEdA6ROfgicdAvfEFDM4-i42kY*58X1UmOW8WUoQqc6b8iEuUVw==)
- [你真的掌握lvs工作原理吗](https://mp.weixin.qq.com/s?__biz=MzA3OTgyMDcwNg%3D%3D&idx=1&mid=2650625837&sn=2b86df07eabba8ff2035583913a0ef41)
- [lvs 负载均衡fullnat 模式clientip 怎样传递给 realserver](http://wangxuemin.github.io/2015/07/26/lvs%20%E8%B4%9F%E8%BD%BD%E5%9D%87%E8%A1%A1fullnat%20%E6%A8%A1%E5%BC%8Fclientip%20%E6%80%8E%E6%A0%B7%E4%BC%A0%E9%80%92%E7%BB%99%20realserver/)
