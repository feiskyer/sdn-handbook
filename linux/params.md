# 内核中的网络参数

## nf_conntrack

`nf_conntrack`是Linux内核连接跟踪的模块，常用在iptables中，比如

```
-A INPUT -m state --state RELATED,ESTABLISHED  -j RETURN
-A INPUT -m state --state INVALID -j DROP
```

可以通过`cat /proc/net/nf_conntrack`来查看当前跟踪的连接信息，这些信息以哈希形式（用链地址法处理冲突）存在内存中，并且每条记录大约占300B空间。

与`nf_conntrack`相关的内核参数有三个：

- `nf_conntrack_max`：连接跟踪表的大小，建议根据内存计算该值`CONNTRACK_MAX = RAMSIZE (in bytes) / 16384 / (x / 32)`，并满足`nf_conntrack_max=4*nf_conntrack_buckets`，默认262144
- `nf_conntrack_buckets`：哈希表的大小，(`nf_conntrack_max/nf_conntrack_buckets`就是每条哈希记录链表的长度)，默认65536
- `nf_conntrack_tcp_timeout_established`：tcp会话的超时时间，默认是432000 (5天)

比如，对64G内存的机器，推荐配置：

```
net.netfilter.nf_conntrack_max=4194304
net.netfilter.nf_conntrack_tcp_timeout_established=300
net.netfilter.nf_conntrack_buckets=1048576
```

## TCP相关

**参数**| **描述**| **默认值**| **优化值**
-------| ------- | -------- | --------
`net.core.rmem_default` | 默认的TCP数据接收窗口大小（字节）| 212992| 
`net.core.rmem_max` | 最大的TCP数据接收窗口（字节）。     | 212992 | 
`net.core.wmem_default` | 默认的TCP数据发送窗口大小（字节）。 | 212992   | 
`net.core.wmem_max` | 最大的TCP数据发送窗口（字节）。     | 212992   | 
`net.core.netdev_max_backlog` | 在每个网络接口接收数据包的速率比内核处理这些包的速率快时，允许送到队列的数据包的最大数目。| 1000 | 10000 
`net.core.somaxconn` | 定义了系统中每一个端口最大的监听队列的长度，这是个全局的参数。 | 128 | 2048 
`net.core.optmem_max`| 表示每个套接字所允许的最大缓冲区的大小。 | 20480 | 81920 
`net.ipv4.tcp_mem` | 确定TCP栈应该如何反映内存使用，每个值的单位都是内存页（通常是4KB）。第一个值是内存使用的下限；第二个值是内存压力模式开始对缓冲区使用应用压力的上限；第三个值是内存使用的上限。在这个层次上可以将报文丢弃，从而减少对内存的使用。对于较大的BDP可以增大这些值（注意，其单位是内存页而不是字节）。 | 5814 7754 11628 | 
`net.ipv4.tcp_rmem` | 为自动调优定义socket使用的内存。第一个值是为socket接收缓冲区分配的最少字节数；第二个值是默认值（该值会被`rmem_default`覆盖），缓冲区在系统负载不重的情况下可以增长到这个值；第三个值是接收缓冲区空间的最大字节数（该值会被`rmem_max`覆盖）。    | 4096  87380  3970528  | 
`net.ipv4.tcp_wmem` | 为自动调优定义socket使用的内存。第一个值是为socket发送缓冲区分配的最少字节数；第二个值是默认值（该值会被`wmem_default`覆盖），缓冲区在系统负载不重的情况下可以增长到这个值；第三个值是发送缓冲区空间的最大字节数（该值会被`wmem_max`覆盖）。    | 4096  16384  3970528  | 
`net.ipv4.tcp_keepalive_time` | TCP发送keepalive探测消息的间隔时间（秒），用于确认TCP连接是否有效。 | 7200     | 1800 
`net.ipv4.tcp_keepalive_intvl`  | 探测消息未获得响应时，重发该消息的间隔时间（秒） | 75 | 30 
`net.ipv4.tcp_keepalive_probes` | 在认定TCP连接失效之前，最多发送多少个keepalive探测消息。 | 9 | 3 
`net.ipv4.tcp_sack` | 启用有选择的应答（1表示启用），通过有选择地应答乱序接收到的报文来提高性能，让发送者只发送丢失的报文段，（对于广域网通信来说）这个选项应该启用，但是会增加对CPU的占用。 | 1 | 1 
`net.ipv4.tcp_fack` | 启用转发应答，可以进行有选择应答（SACK）从而减少拥塞情况的发生，这个选项也应该启用。 | 1 | 1 
`net.ipv4.tcp_timestamps` | TCP时间戳（会在TCP包头增加12个字节），以一种比重发超时更精确的方法（参考RFC 1323）来启用对RTT 的计算，为实现更好的性能应该启用这个选项。 | 1 | 1 
`net.ipv4.tcp_window_scaling`   | 启用RFC 1323定义的window scaling，要支持超过64KB的TCP窗口，必须启用该值（1表示启用），TCP窗口最大至1GB，TCP连接双方都启用时才生效。 | 1 | 1 
`net.ipv4.tcp_syncookies` | 表示是否打开TCP同步标签（syncookie），内核必须打开了`CONFIG_SYN_COOKIES`项进行编译，同步标签可以防止一个套接字在有过多试图连接到达时引起过载。 | 1 | 1 
`net.ipv4.tcp_tw_reuse` | 表示是否允许将处于TIME-WAIT状态的socket（TIME-WAIT的端口）用于新的TCP连接 。 | 0 | 1 
`net.ipv4.tcp_tw_recycle` | 能够更快地回收TIME-WAIT套接字。 | 0 | 1 
`net.ipv4.tcp_fin_timeout` | 对于本端断开的socket连接，TCP保持在FIN-WAIT-2状态的时间（秒）。对方可能会断开连接或一直不结束连接或不可预料的进程死亡。  | 60 | 30 
`net.ipv4.ip_local_port_range`  | 表示TCP/UDP协议允许使用的本地端口号 | 32768  60999 | 1024  65000 
`net.ipv4.tcp_max_syn_backlog`  | 对于还未获得对方确认的连接请求，可保存在队列中的最大数目。如果服务器经常出现过载，可以尝试增加这个数字。 | 128 | 
`net.ipv4.tcp_low_latency` | 允许TCP/IP栈适应在高吞吐量情况下低延时的情况，这个选项应该禁用。 | 0 | 0 


## ARP表回收

- `gc_stale_time` 每次检查neighbour记录的有效性的周期。当neighbour记录失效时，将在给它发送数据前再解析一次。缺省值是60秒。
- `gc_thresh1` 存在于ARP高速缓存中的最少记录数，如果少于这个数，垃圾收集器将不会运行。缺省值是128。
- `gc_thresh2` 存在 ARP 高速缓存中的最多的记录软限制。垃圾收集器在开始收集前，允许记录数超过这个数字 5 秒。缺省值是 512。
- `gc_thresh3` 保存在 ARP 高速缓存中的最多记录的硬限制，一旦高速缓存中的数目高于此，垃圾收集器将马上运行。缺省值是1024。

比如可以增大为

```
net.ipv4.neigh.default.gc_thresh1=1024
net.ipv4.neigh.default.gc_thresh2=4096
net.ipv4.neigh.default.gc_thresh3=8192
```

### 其他ARP相关参数

arp_filter - BOOLEAN

    1 - Allows you to have multiple network interfaces on the same
    subnet, and have the ARPs for each interface be answered
    based on whether or not the kernel would route a packet from
    the ARP'd IP out that interface (therefore you must use source
    based routing for this to work). In other words it allows control
    of which cards (usually 1) will respond to an arp request.

    0 - (default) The kernel can respond to arp requests with addresses
    from other interfaces. This may seem wrong but it usually makes
    sense, because it increases the chance of successful communication.
    IP addresses are owned by the complete host on Linux, not by
    particular interfaces. Only for more complex setups like load-
    balancing, does this behaviour cause problems.

    arp_filter for the interface will be enabled if at least one of
    conf/{all,interface}/arp_filter is set to TRUE,
    it will be disabled otherwise

arp_announce - INTEGER

    Define different restriction levels for announcing the local
    source IP address from IP packets in ARP requests sent on
    interface:
    0 - (default) Use any local address, configured on any interface
    1 - Try to avoid local addresses that are not in the target's
    subnet for this interface. This mode is useful when target
    hosts reachable via this interface require the source IP
    address in ARP requests to be part of their logical network
    configured on the receiving interface. When we generate the
    request we will check all our subnets that include the
    target IP and will preserve the source address if it is from
    such subnet. If there is no such subnet we select source
    address according to the rules for level 2.
    2 - Always use the best local address for this target.
    In this mode we ignore the source address in the IP packet
    and try to select local address that we prefer for talks with
    the target host. Such local address is selected by looking
    for primary IP addresses on all our subnets on the outgoing
    interface that include the target IP address. If no suitable
    local address is found we select the first local address
    we have on the outgoing interface or on all other interfaces,
    with the hope we will receive reply for our request and
    even sometimes no matter the source IP address we announce.

    The max value from conf/{all,interface}/arp_announce is used.

    Increasing the restriction level gives more chance for
    receiving answer from the resolved target while decreasing
    the level announces more valid sender's information.

arp_ignore - INTEGER

    Define different modes for sending replies in response to
    received ARP requests that resolve local target IP addresses:
    0 - (default): reply for any local target IP address, configured
    on any interface
    1 - reply only if the target IP address is local address
    configured on the incoming interface
    2 - reply only if the target IP address is local address
    configured on the incoming interface and both with the
    sender's IP address are part from same subnet on this interface
    3 - do not reply for local addresses configured with scope host,
    only resolutions for global and link addresses are replied
    4-7 - reserved
    8 - do not reply for all local addresses

    The max value from conf/{all,interface}/arp_ignore is used
    when ARP request is received on the {interface}

arp_notify - BOOLEAN

    Define mode for notification of address and device changes.
    0 - (default): do nothing
    1 - Generate gratuitous arp requests when device is brought up
        or hardware address changes.

arp_accept - BOOLEAN

    Define behavior for gratuitous ARP frames who's IP is not
    already present in the ARP table:
    0 - don't create new entries in the ARP table
    1 - create new entries in the ARP table

    Both replies and requests type gratuitous arp will trigger the
    ARP table to be updated, if this setting is on.

    If the ARP table already contains the IP address of the
    gratuitous arp frame, the arp table will be updated regardless
    if this setting is on or off.
