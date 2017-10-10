# FAQ

## 如何定位丢包问题

* 如何知道哪个网卡在丢包：`netstat -i`
* 如何知道什么时候丢包：`perf record -g -a -e skb:kfree_skb`
* 如何知道哪里丢包了：<https://github.com/pavel-odintsov/drop_watch>

## 如何查看Linux系统的带宽流量

* 按网卡查看流量：`ifstat`、`dstat -nf`或`sar -n DEV 1 2`
* 按进程查看流量：`nethogs`
* 按连接查看流量：`iptraf`、`iftop`或`tcptrack`
* 查看流量最大的进程：`sysdig -c topprocs_net`
* 查看流量最大的端口：`sysdig -c topports_server`
* 查看连接最多的服务器端口：`sysdig -c fdbytes_by fd.sport`

## 参考文档

- [Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/)
- [Monitoring and Tuning the Linux Networking Stack: Sending Data](https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data/)

