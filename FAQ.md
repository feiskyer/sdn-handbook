# FAQ

## 如何定位丢包问题

* 如何知道哪个网卡在丢包：`netstat -i`
* 如何知道什么时候丢包：`perf record -g -a -e skb:kfree_skb`
* 如何知道哪里丢包了：<https://github.com/pavel-odintsov/drop_watch>


## 参考文档

- [Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/)
- [Monitoring and Tuning the Linux Networking Stack: Sending Data](https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data/)

