# 南向接口

南向接口负责控制器与数据平面的通信，可以理解成数据平面的编程接口，直接决定了SDN架构的可编程能力。

主流的南向接口协议包括

- OpenFlow：第一个开放的南向接口协议，也是目前最流行的协议。它提出了控制与转发分离的架构，规定了SDN转发设备的基本组件和功能要求，以及与控制器通信的协议。
- OF-Config：提供开放接口用于控制和配置OpenFlow交换机，但不影响流表的内容和数据转发行为。OF-CONFIG在OpenFlow架构上增加了一个被称作OpenFlow Configuration Point的配置节点。这个节点既可以是控制器上的一个软件进程，也可以是传统的网管设备。OF-Config基于NET-CONF与设备通信。
- [P4](http://p4.org/)：协议无关的数据包处理编程语言，提供了比OpenFlow更出色的编程能力。它不仅可以指导数据流进行转发，还可以对交换机等转发设备的数据处理流程进行软件编程定义。
- OVSDB：Open vSwitch数据库协议。
- NET-CONF：用于替代CLI、SNMP等配置交换机。
- OpFlex：思科ACI使用的一种声明式南向接口协议。

## 参考文档

- <https://www.opennetworking.org/sdn-resources/openflow>
- <http://p4.org/>
- [P4:真正的SDN还遥远吗](http://www.muzixing.com/pages/2016/03/23/p4zhen-zheng-de-sdnhuan-yao-yuan-ma.html)
