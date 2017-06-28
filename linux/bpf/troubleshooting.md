# eBPF故障排查

## 内存限制

eBPF map使用固定的内存（locked memory），但默认非常小，可以通过调用[setrlimit(2)](http://man7.org/linux/man-pages/man2/setrlimit.2.html)来增大`RLIMIT_MEMLOCK`。如果内存不足，`bpf_create_map`会返回`EPERM (Operation not permitted)`错误。

## 开启BPF JIT

开启方法为

```sh
$ sysctl net/core/bpf_jit_enable=1
net.core.bpf_jit_enable = 1
```

## ELF二进制文件

eBPF通过LLVM编译器生成的程序就是一个普通的ELF二进制文件，可以使用`readelf`或者`llvm-objdump`分析该文件，如

```sh
$ llvm-objdump -h xdp_ddos01_blacklist_kern.o

xdp_ddos01_blacklist_kern.o:	file format ELF64-unknown

Sections:
Idx Name          Size      Address          Type
  0               00000000 0000000000000000 
  1 .strtab       00000072 0000000000000000 
  2 .text         00000000 0000000000000000 TEXT DATA 
  3 xdp_prog      000001b8 0000000000000000 TEXT DATA 
  4 .relxdp_prog  00000020 0000000000000000 
  5 maps          00000028 0000000000000000 DATA 
  6 license       00000004 0000000000000000 DATA 
  7 .symtab       000000d8 0000000000000000 
```

## 提取eBPF-JIT代码

在调试eBPF程序时，有时需要提取eBPF-JIT代码

```sh
$ sysctl net.core.bpf_jit_enable=2
```

输出如下所示：

```
 flen=55 proglen=335 pass=4 image=ffffffffa0006820 from=xdp_ddos01_blac pid=13333
 JIT code: 00000000: 55 48 89 e5 48 81 ec 28 02 00 00 48 89 9d d8 fd
 JIT code: 00000010: ff ff 4c 89 ad e0 fd ff ff 4c 89 b5 e8 fd ff ff
 JIT code: 00000020: 4c 89 bd f0 fd ff ff 31 c0 48 89 85 f8 fd ff ff
 JIT code: 00000030: bb 02 00 00 00 48 8b 77 08 48 8b 7f 00 48 89 fa
 JIT code: 00000040: 48 83 c2 0e 48 39 f2 0f 87 e1 00 00 00 48 0f b6
 JIT code: 00000050: 4f 0c 48 0f b6 57 0d 48 c1 e2 08 48 09 ca 48 89
 JIT code: 00000060: d1 48 81 e1 ff 00 00 00 41 b8 06 00 00 00 49 39
 JIT code: 00000070: c8 0f 87 b7 00 00 00 48 81 fa 88 a8 00 00 74 0e
 JIT code: 00000080: b9 0e 00 00 00 48 81 fa 81 00 00 00 75 1a 48 89
 JIT code: 00000090: fa 48 83 c2 12 48 39 f2 0f 87 90 00 00 00 b9 12
 JIT code: 000000a0: 00 00 00 48 0f b7 57 10 bb 02 00 00 00 48 81 e2
 JIT code: 000000b0: ff ff 00 00 48 83 fa 08 75 49 48 01 cf 31 db 48
 JIT code: 000000c0: 89 fa 48 83 c2 14 48 39 f2 77 38 8b 7f 0c 89 7d
 JIT code: 000000d0: fc 48 89 ee 48 83 c6 fc 48 bf 00 9c 24 5f 07 88
 JIT code: 000000e0: ff ff e8 29 cd 13 e1 bb 02 00 00 00 48 83 f8 00
 JIT code: 000000f0: 74 11 48 8b 78 00 48 83 c7 01 48 89 78 00 bb 01
 JIT code: 00000100: 00 00 00 89 5d f8 48 89 ee 48 83 c6 f8 48 bf c0
 JIT code: 00000110: 76 12 13 04 88 ff ff e8 f4 cc 13 e1 48 83 f8 00
 JIT code: 00000120: 74 0c 48 8b 78 00 48 83 c7 01 48 89 78 00 48 89
 JIT code: 00000130: d8 48 8b 9d d8 fd ff ff 4c 8b ad e0 fd ff ff 4c
 JIT code: 00000140: 8b b5 e8 fd ff ff 4c 8b bd f0 fd ff ff c9 c3
```

其中，`proglen`是opcode sequence的长度，`flen`是bpf insns的个数。可以使用`bpf_jit_disasm`工具来生成相关的opcodes。
