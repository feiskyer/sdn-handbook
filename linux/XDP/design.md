# XDP架构

XDP基于一系列的技术来实现高性能和可编程性，包括

- 基于eBPF
- Capabilities negotiation：通过协商确定网卡驱动支持的特性，XDP尽量利用新特性，但网卡驱动不需要支持所有的特性
- 在网络协议栈前处理
- 无锁设计
- 批量I/O操作
- 轮询式
- 直接队列访问
- 不需要分配skbuff
- 支持网络卸载
- DDIO
- XDP程序快速执行并结束，没有循环
- Packeting steering

## 包处理逻辑

如下图所示，基于内核的eBPF程序处理包，每个RX队列分配一个CPU，且以每个网络包一个Page（packet-page）的方式避免分配skbuff。

![](packet-processor.png)