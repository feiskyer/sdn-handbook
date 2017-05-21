# scapy

scapy是一个强大的python网络数据包处理库，它可以生成或解码网络协议数据包，可以用来端口扫描、探测、网络测试等。

## scapy安装

    pip install scapy

## 简单使用

scapy提供了一个简单的交互式界面，直接运行scapy命令即可进入。当然，也可以在python交互式命令行中导入scapy包进入

```python
from scapy.all import *
```

查看所有支持的协议和预制工具：

    ls()
    lsc()

构造IP数据包

    pkt=IP(dst="8.8.8.8")
    pkt.show()
    print pkt.dst  # 8.8.8.8
    str(pkt)       # hex string
    hexdump(pkt)   # hex dump

输出HEX格式的数据包

	import binascii
	from scapy.all import *
	a=Ether(dst="02:ac:10:ff:00:22",src="02:ac:10:ff:00:11")/IP(dst="172.16.255.22",src="172.16.255.11", ttl=10)/ICMP()
	print binascii.hexlify(str(a))


TCP/IP协议的四层模型都可以分别构造，并通过`/`连接

    Ether()/IP()/TCP()
    IP()/TCP()
    IP()/TCP()/"GET / HTTP/1.0\r\n\r\n"
    Ether()/IP()/IP()/UDP()
    Ether()/IP(dst="www.slashdot.org")/TCP()/"GET /index.html HTTP/1.0 \n\n"

从PCAP文件读入数据

    a = rdpcap("test.cap")

发送数据包

    # 三层发送，不接收
    send(IP(dst="8.8.8.8")/ICMP())  
    # 二层发送，不接收
    sendp(Ether()/IP(dst="8.8.8.8",ttl=10)/ICMP())
    # 三层发送并接收
    # 二层可以用srp, srp1和srploop 
    result, unanswered = sr(IP(dst="8.8.8.8")/ICMP())
    # 发送并只接收第一个包
    sr1(IP(dst="8.8.8.8")/ICMP())
    # 发送多个数据包
    result=srloop(IP(dst="8.8.8.8")/ICMP(), inter=1, count=2)

嗅探数据包

    sniff(filter="icmp", count=3, timeout=5, prn=lambda x:x.summary())
    a=sniff(filter="tcp and ( port 25 or port 110 )",
     prn=lambda x: x.sprintf("%IP.src%:%TCP.sport% -> %IP.dst%:%TCP.dport%  %2s,TCP.flags% : %TCP.payload%"))

SYN扫描

    sr1(IP(dst="172.217.24.14")/TCP(dport=80,flags="S"))
    ans,unans = sr(IP(dst=["192.168.1.1","yahoo.com","slashdot.org"])/TCP(dport=[22,80,443],flags="S"))

TCP traceroute

    ans,unans=sr(IP(dst="www.baidu.com",ttl=(2,25),id=RandShort())/TCP(flags=0x2),timeout=50) 
    for snd,rcv in ans: 
        print snd.ttl,rcv.src,isinstance(rcv.payload,TCP)

ARP Ping

    ans,unans=srp(Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(pdst="192.168.1.0/24"),timeout=2)
    ans.summary(lambda (s,r): r.sprintf("%Ether.src% %ARP.psrc%") )

ICMP Ping

    ans,unans=sr(IP(dst="192.168.1.1-254")/ICMP())
    ans.summary(lambda (s,r): r.sprintf("%IP.src% is alive") )

TCP Ping

    ans,unans=sr( IP(dst="192.168.1.*")/TCP(dport=80,flags="S") )
    ans.summary( lambda(s,r) : r.sprintf("%IP.src% is alive") )

