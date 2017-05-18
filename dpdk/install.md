# DPDK安装

## 源码安装

```sh
export RTE_SDK="/usr/src/dpdk"
export DPDK_VERSION="2.2.0"
export RTE_TARGET="x86_64-native-linuxapp-gcc"

######################## ubuntu ######################################
apt-get install -y vim gcc-multilib libfuse-dev linux-source libssl-dev llvm-dev python autoconf libtool libpciaccess-dev make libcunit1-dev libaio-dev

######################## centos ######################################
yum -y install git cmake gcc autoconf automake device-mapper-devel \
   sqlite-devel pcre-devel libsepol-devel libselinux-devel \
   automake autoconf gcc make glibc-devel glibc-devel.i686 kernel-devel \
   fuse-devel pciutils libtool openssl-devel libpciaccess-devel CUnit-devel libaio-devel
mkdir -p /lib/modules/$(uname -r)
ln -sf /usr/src/kernels/$(uname -r) /lib/modules/$(uname -r)/build

# download dpdk
curl -sSL http://dpdk.org/browse/dpdk/snapshot/dpdk-${DPDK_VERSION}.tar.gz | tar -xz && mv dpdk-${DPDK_VERSION} ${RTE_SDK}

# install dpdk
cd ${RTE_SDK}
sed -ri 's,(CONFIG_RTE_BUILD_COMBINE_LIBS=).*,\1y,' config/common_linuxapp
sed -ri 's,(CONFIG_RTE_LIBRTE_VHOST=).*,\1y,' config/common_linuxapp
sed -ri 's,(CONFIG_RTE_LIBRTE_VHOST_USER=).*,\1n,' config/common_linuxapp
sed -ri '/CONFIG_RTE_LIBNAME/a CONFIG_RTE_BUILD_FPIC=y' config/common_linuxapp
make config CC=gcc T=$RTE_TARGET
make -j `nproc` RTE_KERNELDIR=/lib/modules/$(uname -r)/build
make install
depmod
```

配置Hugepages: 添加`"default_hugepagesz=1G hugepagesz=1G hugepages=4"`到`/etc/grub/default`的`GRUB_CMDLINE_LINUX`，并执行（测试环境可以配置`GRUB_CMDLINE_LINUX="crashkernel=auto rhgb quiet transparent_hugepage=never default_hugepagesz=2MB hugepagesz=2MB hugepages=512"`）

```sh
grub2-mkconfig -o /boot/grub2/grub.cfg
systemctl reboot
```

加载内核模块

```sh
modprobe uio
insmod kmod/igb_uio.ko
./tools/dpdk-devbind.py --bind=igb_uio <Device BDF>
```

## REHL 7.2

```sh
subscription-manager repos --enable rhel-7-server-extras-rpms
yum install dpdk dpdk-tools
```

## Ubuntu 15+

```sh
apt-get install dpdk
```
