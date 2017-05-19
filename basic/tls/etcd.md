---
title: "安全etcd机群"
---

了解TLS的机制，不仅可以配置HTTPS服务，也可以配置很多分布式系统的安全通
信。关于分布式系统中各个进程之间通过TLS身份证互相验证的过程，可以参见
[这个图](./tls.html#bidirectional)。

一个典型的分布式系统是etcd机群<sup>[cluster](#cluster)</sup>。一个etcd
机群里，各个成员（member）之间可以用TLS身份证互相验证身份
<sup>[security](#security)</sup>。此外，客户端程序和成员之间也可以利用
TLS身份证互相验证身份。前一种验证可以保证没有恶意etcd进程鱼目混珠地加
入机群，窃取机群里的内容。后一种验证可以确定对etcd数据库的访问者都是合
法的。

## 机群成员之间的认证

etcd每个成员都可以监听一个peer端口——只要互相知道对方的host域名和peer端
口，就可以互相确认身份，组成一个机群。此外，每个成员都可以监听一个
client端口，和客户端通信。

在下面这个例子里，我们启动一个三个成员的etcd机群。这三个进程都运行来本
地，而且我们准备客户端也运行在本地，所以etcd机群的host域名可以就是
localhost。这样，这三个etcd成员可以公用一个TLS身份证，其中CSR里的域名
就是 localhost。我们用一个自签署的CA身份证来签署这个etcd成员公用的身份
证，并且称之为 `server.csr`。

实际情况通常比这个复杂：因为每个etcd成员运行在不同的机器上，所以三个成
员应该有三个不同的TLS身份证，每个身份证里的CSR里的域名不同。但是都可以
用自签署的CA身份证来签署<sup>[real](#real)</sup>。


## 客户端和机群之间的认证

此外，我们为etcd客户端（kubectl或者curl）准备一个身份证，这个身份证也
是用CA签署的。这样etcd机群和客户端之间可以互相验证身份。我们给这个身份
证取名叫 `client.csr`。

要生成自签署的CA身份证，以及一对儿server和client身份证，我们可以复用
[《用Go语言写HTTPS程序》](./golang.html)一文中
[双方认证对方身份](./golang.html#双方认证对方身份)一节中的脚本
`bidirectional/create_tls_asserts.bash`。


## 启动机群

我们接下来启动一个etcd机群，其中各个成员的端口号如下：

| 进程 | peer 端口 | client 端口 | 
|-----|-----------|------------|
|成员1| 23801     | 23791      |
|成员2| 23802     | 23792      |
|成员3| 23803     | 23793      |


我们可以运行附带的 [`init.bash`](./etcd/init.bash) 来下载和安装 etcd，
然后运行 [`etcd.bash`](./etcd/etcd.bash) 来在本机启动etcd机群。

```
cd etcd
./init.bash
./etcd.bash
```

机群启动脚本[`etcd.bash`](./etcd/etcd.bash)的主要内容如下。注意，其中
每个成员都携带 `server.crt`, `server.key`, `ca.crt`。其中 `server.crt`
是所有成员公用的身份证，即在互相验证身份时使用，也在和客户端互相验证身
份时使用。

```
for (( i = 1; i <= 3; i++ )); do
    ./etcd/etcd \
	--name infra$i \
	--advertise-client-urls https://localhost:2379$i \
	--listen-client-urls https://localhost:2379$i \
	--cert-file ./server.crt \
	--key-file ./server.key \
	--client-cert-auth \
	--ca-file ca.crt \
	\
	--initial-advertise-peer-urls https://localhost:2380$i \
	--listen-peer-urls https://localhost:2380$i \
	--peer-cert-file ./server.crt \
	--peer-key-file ./server.key \
	--peer-ca-file ./ca.crt \
	\
	--initial-cluster 'infra1=https://localhost:23801,infra2=https://localhost:23802,infra3=https://localhost:23803' \
	--initial-cluster-token etcd-for-k8sp-tls-demo \
	--initial-cluster-state new 2>&1 | tee /tmp/$i.log &
done
```


然后我们可以用如下命令验证对这个机群的访问：

```
$ ./etcd/etcdctl -C https://localhost:23791 --ca-file ca.crt --key-file client.key --cert-file client.crt set hello world
world
$ ./etcd/etcdctl -C https://localhost:23791 --ca-file ca.crt --key-file client.key --cert-file client.crt get hello
world
```

如果不告诉客户端它的身份证，那么etcd机群应该因为无法验证客户端身份而报错：

```
$ ./etcd/etcdctl -C https://localhost:23791 --ca-file ca.crt  set get foo
Error:  remote error: bad certificate
```

结束试验只需要

```
killall etcd
```

即可杀掉etcd机群。


## 参考文献

- <a name=cluster>cluster</a> https://coreos.com/etcd/docs/latest/clustering.html

- <a name=security>security</a> https://coreos.com/etcd/docs/latest/security.html

- <a name=real>real</a> https://github.com/coreos/etcd/blob/master/Documentation/op-guide/clustering.md
