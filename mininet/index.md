# Mininet

Mininet是一个由Stanford大学Nick研究小组开发的网络虚拟化平台，可以用来方便的测试、验证和研究OpenFlow和SDN网络。

Mininet使用Linux Network Namespaces来创建虚拟节点，默认情况下，Mininet会为每一个host创建一个新的网络命名空间，同时在root Namespace（根进程命名空间）运行交换机和控制器的进程，因此这两个进程就共享host网络命名空间。由于每个主机都有各自独立的网络命名空间，我们就可以进行个性化的网络配置和网络程序部署。

Mininet提供了命令行工具`mn`和Python API，用来构建网络拓扑。

## 安装Mininet

在Ubuntu系统上可以使用通过下面的命令来安装mininet：

```sh
sudo apt-get install -y mininet openvswitch-testcontroller
sudo /usr/bin/ovs-testcontroller /usr/bin/ovs-controller
sudo service openvswitch-switch start
```

然后运行下面的命令验证安装是否正常

```sh
sudo mn --test pingall
```

其他系统上，可以参考[这里](https://github.com/mininet/mininet/wiki/Mininet-VM-Images)下载预置mininet的虚拟机镜像。

## mn命令行

直接运行mn命令可以进入mininet控制台，默认创建一个`minimal`拓扑，即一个控制器、一台OpenFlow交换机并连着两台host。

```sh
$ sudo mn
mininet> nodes
available nodes are:
h1 h2 s1
mininet> dump
<Host h1: h1-eth0:10.0.0.1 pid=21291>
<Host h2: h2-eth0:10.0.0.2 pid=21293>
<OVSBridge s1: lo:127.0.0.1,s1-eth1:None,s1-eth2:None pid=21298>
```

以节点名字开始的命令在节点内运行

```sh
mininet> h1 ifconfig -a
h1-eth0   Link encap:Ethernet  HWaddr aa:7d:6a:7f:b5:52
          inet addr:10.0.0.1  Bcast:10.255.255.255  Mask:255.0.0.0
          inet6 addr: fe80::a87d:6aff:fe7f:b552/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:15 errors:0 dropped:0 overruns:0 frame:0
          TX packets:8 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:1206 (1.2 KB)  TX bytes:648 (648.0 B)

lo        Link encap:Local Loopback
          inet addr:127.0.0.1  Mask:255.0.0.0
          inet6 addr: ::1/128 Scope:Host
          UP LOOPBACK RUNNING  MTU:65536  Metric:1
          RX packets:0 errors:0 dropped:0 overruns:0 frame:0
          TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1
          RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)

mininet> h1 ping -c1 h2
PING 10.0.0.2 (10.0.0.2) 56(84) bytes of data.
64 bytes from 10.0.0.2: icmp_seq=1 ttl=64 time=0.040 ms

--- 10.0.0.2 ping statistics ---
1 packets transmitted, 1 received, 0% packet loss, time 0ms
rtt min/avg/max/mdev = 0.040/0.040/0.040/0.000 ms
```

#### 命令和选项

命令列表

- py：执行python命令，如`py h1.IP()`
- dump：查看各节点的信息
- nodes：查看所有节点
- net：查看链路信息
- links：查看网络接口连接拓扑
- link：开启或关闭网络接口，如`link s1 h1 up`
- xterm：开启终端
- dpctl：操作OpenFlow流表
- pingall：自动ping测试

选项列表

- `--topo`：自定义拓扑，如`linear|minimal|reversed|single|torus|tree`
- `--link`：自定义网络参数，如`default|ovs|tc`
- `--switch`：自定义虚拟交换机，如`default|ivs|lxbr|ovs|ovsbr|ovsk|user`
- `--controller`：自定义控制器，如`default|none|nox|ovsc|ref|remote|ryu`
- `--nat`：自动设置NAT
- `--cluster`：集群模式，将网络拓扑运行在多台机器上
- `--mac`：自动设置主机mac
- `--arp`：自动设置arp表项

#### 自定义网络拓扑

默认的`minimal`拓扑比较简单，可以使用`--topo`选项设置网络拓扑。

```sh
$ sudo mn --topo single,3
mininet> dump
<Host h1: h1-eth0:10.0.0.1 pid=22193>
<Host h2: h2-eth0:10.0.0.2 pid=22195>
<Host h3: h3-eth0:10.0.0.3 pid=22197>
<OVSBridge s1: lo:127.0.0.1,s1-eth1:None,s1-eth2:None,s1-eth3:None pid=22202>
```

#### 自定义网络参数

可以使用`--link`选项设置网络参数。

```sh
$ sudo mn --link tc,bw=10,delay=10ms
mininet> iperf
*** Iperf: testing TCP bandwidth between h1 and h2
*** Results: ['9.50 Mbits/sec', '12.0 Mbits/sec']
```

#### 自定义独立命名空间

默认情况下，host在独立的netns中，而switch和controller都还是使用host netns，可以使用`--innamespace`选项将它们也放到独立的netns中。

```sh
$ sudo mn --innamespace --switch user
```

## Python API

对于复杂的网络，需要使用[Python API](http://mininet.org/api/annotated.html)来构建。

比如，使用python API构造一个单switch接4台虚拟节点的示例

```python
#!/usr/bin/python
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel
from mininet.node import OVSController

class SingleSwitchTopo(Topo):
    "Single switch connected to n hosts."
    def build(self, n=2):
        switch = self.addSwitch('s1')
        # Python's range(N) generates 0..N-1
        for h in range(n):
            host = self.addHost('h%s' % (h + 1))
            self.addLink(host, switch)

def simpleTest():
    "Create and test a simple network"
    topo = SingleSwitchTopo(n=4)
    net = Mininet(topo=topo, controller = OVSController)
    net.start()
    print "Dumping host connections"
    dumpNodeConnections(net.hosts)
    print "Testing network connectivity"
    net.pingAll()
    net.stop()

topos = {"mytopo": SingleSwitchTopo}

if __name__ == '__main__':
    # Tell mininet to print useful information
    setLogLevel('info')
    simpleTest()
```

这个脚本可以直接运行，也可以使用mn命令启动

```sh
$ sudo mn --custom a.py --topo mytopo,3 --test pingall
```

Mininet v2.2.0+还提供了一个miniedit的可视化界面，更直观的编辑和查看网络拓扑。miniedit实际上是一个python脚本，存放在mininet安装目录的examples中，如`/usr/lib/python2.7/dist-packages/mininet/examples/miniedit.py`。

## 参考文档

- [Mininet官方网站](http://mininet.org/)
- [Mininet Github](https://github.com/mininet/mininet)
- [REPRODUCING NETWORK RESEARCH](https://reproducingnetworkresearch.wordpress.com/)
