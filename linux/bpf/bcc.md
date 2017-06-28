# BCC (BPF Compiler Collection)

BPF Compiler Collection (BCC)是基于eBPF的Linux内核分析、跟踪、网络监控工具。其源码存放于<https://github.com/iovisor/bcc>。

BCC包括一些列的工具

![](https://github.com/iovisor/bcc/raw/master/images/bcc_tracing_tools_2016.png)

## 安装BCC

Ubuntu：

```sh
echo "deb [trusted=yes] https://repo.iovisor.org/apt/xenial xenial-nightly main" | sudo tee /etc/apt/sources.list.d/iovisor.list
sudo apt-get update
sudo apt-get install -y bcc-tools libbcc-examples python-bcc
```

CentOS：

```sh
echo -e '[iovisor]\nbaseurl=https://repo.iovisor.org/yum/nightly/f23/$basearch\nenabled=1\ngpgcheck=0' | sudo tee /etc/yum.repos.d/iovisor.repo
yum install -y bcc-tools
```

安装完成后，bcc工具会放到`/usr/share/bcc/tools`目录中

```sh
$ ls /usr/share/bcc/tools
argdist       cachestat  ext4dist        hardirqs        offwaketime  softirqs    tcpconnect  vfscount
bashreadline  cachetop   ext4slower      killsnoop       old          solisten    tcpconnlat  vfsstat
biolatency    capable    filelife        llcstat         oomkill      sslsniff    tcplife     wakeuptime
biosnoop      cpudist    fileslower      mdflush         opensnoop    stackcount  tcpretrans  xfsdist
biotop        dcsnoop    filetop         memleak         pidpersec    stacksnoop  tcptop      xfsslower
bitesize      dcstat     funccount       mountsnoop      profile      statsnoop   tplist      zfsdist
btrfsdist     doc        funclatency     mysqld_qslower  runqlat      syncsnoop   trace       zfsslower
btrfsslower   execsnoop  gethostlatency  offcputime      slabratetop  tcpaccept   ttysnoop
```

## 常用工具示例

### capable

capable检查Linux进程的security capabilities：

```sh
$ capable
TIME      UID    PID    COMM             CAP  NAME                 AUDIT
22:11:23  114    2676   snmpd            12   CAP_NET_ADMIN        1
22:11:23  0      6990   run              24   CAP_SYS_RESOURCE     1
22:11:23  0      7003   chmod            3    CAP_FOWNER           1
22:11:23  0      7003   chmod            4    CAP_FSETID           1
22:11:23  0      7005   chmod            4    CAP_FSETID           1
22:11:23  0      7005   chmod            4    CAP_FSETID           1
22:11:23  0      7006   chown            4    CAP_FSETID           1
22:11:23  0      7006   chown            4    CAP_FSETID           1
22:11:23  0      6990   setuidgid        6    CAP_SETGID           1
22:11:23  0      6990   setuidgid        6    CAP_SETGID           1
22:11:23  0      6990   setuidgid        7    CAP_SETUID           1
22:11:24  0      7013   run              24   CAP_SYS_RESOURCE     1
22:11:24  0      7026   chmod            3    CAP_FOWNER           1
[...]
```

### tcpconnect

tcpconnect检查活跃的TCP连接，并输出源和目的地址：

```sh
$ ./tcpconnect
 PID    COMM         IP SADDR            DADDR            DPORT
 2462   curl         4  192.168.1.99       74.125.23.138    80
```

### tcptop

tcptop统计TCP发送和接受流量：

```sh
$ ./tcptop -C 1 3
Tracing... Output every 1 secs. Hit Ctrl-C to end

08:06:45 loadavg: 0.04 0.01 0.00 2/174 3099

PID    COMM         LADDR                 RADDR                  RX_KB  TX_KB
1740   sshd         192.168.1.99:22         192.168.0.29:60315         0      0

08:06:46 loadavg: 0.04 0.01 0.00 2/174 3099

PID    COMM         LADDR                 RADDR                  RX_KB  TX_KB
1740   sshd         192.168.1.99:22         192.168.0.29:60315         0      0

08:06:47 loadavg: 0.04 0.01 0.00 2/174 3099

PID    COMM         LADDR                 RADDR                  RX_KB  TX_KB
1740   sshd         192.168.1.99:22         192.168.0.29:60315         0      0
```

## 扩展工具

基于eBPF和bcc，可以很方便的扩展功能。bcc目前支持以下事件

* `kprobe__kernel_function_name` (`BPF.attach_kprobe()`)
* `kretprobe__kernel_function_name` (`BPF.attach_kretprobe()`)
* `TRACEPOINT_PROBE(category, event)`，支持的event列表参见`/sys/kernel/debug/tracing/events/category/event/format`
* `BPF.attach_uprobe()`和B`PF.attach_uretprobe()`
* 用户自定义探针(USDT) `USDT.enable_probe()`

### 简单示例

```python
#!/usr/bin/env python
from __future__ import print_function
from bcc import BPF

text='int kprobe__sys_sync(void *ctx) { bpf_trace_printk("Hello, World!\\n"); return 0; }'
prog="""
int hello(void *ctx) {
        bpf_trace_printk("Hello, World!\\n");
        return 0;
}
"""

b = BPF(text=prog)
b.attach_kprobe(event="sys_clone", fn_name="hello")

print("%-18s %-16s %-6s %s" % ("TIME(s)", "COMM", "PID", "MESSAGE"))
while True:
        try:
                (task, pid, cpu, flags, ts, msg) = b.trace_fields()
        except ValueError:
                continue
        print("%-18.9f %-16s %-6d %s" % (ts, task, pid, msg))
```

### 使用`BPF_PERF_OUTPUT`

```python
from __future__ import print_function
from bcc import BPF
import ctypes as ct

# load BPF program
b = BPF(text="""
struct data_t {
    u64 ts;
};

BPF_PERF_OUTPUT(events);

void kprobe__sys_sync(void *ctx) {
    struct data_t data = {};
    data.ts = bpf_ktime_get_ns() / 1000;
    events.perf_submit(ctx, &data, sizeof(data));
};
""")

class Data(ct.Structure):
    _fields_ = [
        ("ts", ct.c_ulonglong)
    ]

# header
print("%-18s %s" % ("TIME(s)", "CALL"))

# process event
def print_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    print("%-18.9f sync()" % (float(event.ts) / 1000000))

# loop with callback to print_event
b["events"].open_perf_buffer(print_event)
while True:
    b.kprobe_poll()
```

更多的示例参考<https://github.com/iovisor/bcc/blob/master/docs/tutorial_bcc_python_developer.md>。

### 用户自定义探针示例

```python
from __future__ import print_function
from bcc import BPF
from time import strftime
import ctypes as ct

# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>

struct str_t {
    u64 pid;
    char str[80];
};

BPF_PERF_OUTPUT(events);

int printret(struct pt_regs *ctx) {
    struct str_t data  = {};
    u32 pid;
    if (!PT_REGS_RC(ctx))
        return 0;
    pid = bpf_get_current_pid_tgid();
    data.pid = pid;
    bpf_probe_read(&data.str, sizeof(data.str), (void *)PT_REGS_RC(ctx));
    events.perf_submit(ctx,&data,sizeof(data));

    return 0;
};
"""
STR_DATA = 80

class Data(ct.Structure):
    _fields_ = [
        ("pid", ct.c_ulonglong),
        ("str", ct.c_char * STR_DATA)
    ]

b = BPF(text=bpf_text)
b.attach_uretprobe(name="/bin/bash", sym="readline", fn_name="printret")

# header
print("%-9s %-6s %s" % ("TIME", "PID", "COMMAND"))

def print_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    print("%-9s %-6d %s" % (strftime("%H:%M:%S"), event.pid, event.str))

b["events"].open_perf_buffer(print_event)
while 1:
    b.kprobe_poll()
```

```sh
# ./bashreadline
TIME      PID    COMMAND
08:22:44  2070   ls /
08:22:56  2070   ping -c3 google.com

# ./gethostlatency
TIME      PID    COMM                  LATms HOST
08:23:26  3370   ping                   2.00 google.com
08:23:37  3372   ping                  56.00 baidu.com
```

## 参考文档

- <https://www.iovisor.org/>
- <https://www.iovisor.org/technology/ebpf>
- <https://www.iovisor.org/technology/xdp>
- <https://github.com/iovisor/bpf-docs>
- <https://www.kernel.org/doc/Documentation/networking/filter.txt>
- <https://events.linuxfoundation.org/sites/events/files/slides/iovisor-lc-bof-2016.pdf>
- <https://suchakra.wordpress.com/2015/08/12/bpf-internals-ii/>
- <https://qmonnet.github.io/whirl-offload/2016/09/01/dive-into-bpf/>
- <https://github.com/cilium/cilium>
