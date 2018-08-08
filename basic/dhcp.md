# DHCP和DNS

## DHCP

DHCP（Dynamic Host Configuration Protocol）是一个用于主机动态获取IP地址的配置解析，使用UDP报文传送，端口号为67和68。

DHCP使用了租约的概念，或称为计算机IP地址的有效期。租用时间是不定的，主要取决于用户在某地连接Internet需要多久，这对于教育行业和其它用户频繁改变的环境是很实用的。通过较短的租期，DHCP能够在一个计算机比可用IP地址多的环境中动态地重新配置网络。DHCP支持为计算机分配静态地址，如需要永久性IP地址的Web服务器。

![](https://upload.wikimedia.org/wikipedia/commons/2/28/DHCP_session_en.svg)

## DNS

DNS（Domain Name System）是一个解析域名和IP地址对应关系以及电子邮件选路信息的服务。它以递归的方式运行：首先访问最近的DNS服务器，如果查询到域名对应的IP地址则直接返回，否则的话再向上一级查询。DNS通常以UDP报文来传送，并使用端口号53。

从应用的角度来看，其实就是两个库函数gethostbyname()和gethostbyaddr()。

> FQDN：全域名(FQDN，Fully Qualified Domain Name)是指主机名加上全路径，全路径中列出了序列中所有域成员(包括root)。全域名可以从逻辑上准确地表示出主机在什么地方，也可以说全域名是主机名的一种完全表示形式。

**资源记录（RR）**

- A记录：  用于查询IP地址
- PTR记录：  逆向查询记录，用于从IP地址查询域名
- CNAME：  表示“规范名字”，用来表示一个域名，也通常称为别名
- HINFO：  表示主机信息，包括主机CPU和操作系统的两个字符串
- MX：  邮件交换记录
- NS：  名字服务器记录，即下一级域名信息的服务器地址，只能设置为域名，不能是IP

**高速缓存**

为了减少DNS的通信量，所有的名字服务器均使用高速缓存。在标准Unix是实现中，高速缓存是由名字服务器而不是名字解释器来维护的。

**用UDP还是TCP**

DNS服务器支持TCP和UDP两种协议的查询方式，而且端口都是53。而大多数的查询都是UDP查询的，一般需要TCP查询的有两种情况：

1. 当查询数据多大以至于产生了数据截断(TC标志为1)，这时，需要利用TCP的分片能力来进行数据传输（看TCP的相关章节）。 
2. 当主（master）服务器和辅（slave）服务器之间通信，辅服务器要拿到主服务器的zone信息的时候。

### 示例

```sh
$ dig k8s.io
; <<>> DiG 9.10.3-P4-Ubuntu <<>> k8s.io
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 37946
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 512
;; QUESTION SECTION:
;k8s.io.				IN	A

;; ANSWER SECTION:
k8s.io.			299	IN	A	23.236.58.218

;; Query time: 392 msec
;; SERVER: 169.254.169.254#53(169.254.169.254)
;; WHEN: Mon Sep 11 05:50:37 UTC 2017
;; MSG SIZE  rcvd: 51


# 反向查询
$ dig -x 23.236.58.218
; <<>> DiG 9.10.3-P4-Ubuntu <<>> -x 23.236.58.218
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 7130
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 512
;; QUESTION SECTION:
;218.58.236.23.in-addr.arpa.	IN	PTR

;; ANSWER SECTION:
218.58.236.23.in-addr.arpa. 119	IN	PTR	218.58.236.23.bc.googleusercontent.com.

;; Query time: 158 msec
;; SERVER: 169.254.169.254#53(169.254.169.254)
;; WHEN: Mon Sep 11 05:50:45 UTC 2017
;; MSG SIZE  rcvd: 107
```

## FAQ

### dnsmasq bad DHCP host name 问题

这个问题是由于hostname是数字前缀，并且dnsmasq对版本低于2.67，这个问题在[2.67版本中修复](http://www.thekelleys.org.uk/dnsmasq/CHANGELOG):

>   Allow hostnames to start with a number, as allowed in
>   RFC-1123. Thanks to Kyle Mestery for the patch. 
