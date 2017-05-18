# DPDK Ring and ivshmem

## DPDK Ring

DPDK Ring提供了一个FIFO无锁队列，支持丰富的队列操作，比如

* Multi-consumer or single-consumer dequeue
* Multi-producer or single-producer enqueue
* Bulk dequeue - Dequeues the specified count of objects if successful; otherwise fails
* Bulk enqueue - Enqueues the specified count of objects if successful; otherwise fails
* Burst dequeue - Dequeue the maximum available objects if the specified count cannot be fulfilled
* Burst enqueue - Enqueue the maximum available objects if the specified count cannot be fulfilled

![](images/14889749801109.jpg)

## ivshmem

ivshmem则通过把内存映射成虚拟机PCI设备提供了虚拟机间(host-to-guest or guest-to-guest)共享内存的机制。

![](images/14889755522615.png)

DPDK ivshmem:

![](images/14889757073018.jpg)


### ivshmem使用示例

```sh
# on the host
mount tmpfs /dev/shm -t tmpfs -osize=32m
ivshmem_server -m 64 -p/tmp/nahanni &

# start VM
qemu-system-x86_64  -hda mg -L /pc-bios/ --smp 4 –chardev socket,path=/tmp/nahanni,id=nahanni-device ivshmem,chardev=nahanni,size=32m,msi=off -serial telnet:0.0.0.0:4000,server,nowait,nodelay-enable-kvm&

# inside VM
modprobe kvm_ivshmem
cat/proc/devices | grep kvm_ivshmem
mknod-mode=666 /dev/ivshmem c 245 0
```

**参考文档**

- [DPDK Ring Library](http://dpdk.org/doc/guides/prog_guide/ring_lib.html)
- [DPDK IVSHMEM Library](http://dpdk.org/doc/guides-16.04/prog_guide/ivshmem_lib.html)

