#!/bin/bash
set -x
source dpdk.rc

######################## ubuntu ######################################
# sudo -E apt-get update -qq
# sudo -E apt-get install -qq gcc-multilib libfuse-dev linux-source libssl-dev llvm-dev
#
# git clone git://git.kernel.org/pub/scm/devel/sparse/chrisl/sparse.git
# cd sparse && make && sudo -E make install PREFIX=/usr && cd ..
# install kernel source
# KERNELSRC="/usr/src/linux-source-3.13.0/linux-source-3.13.0"
# CFLAGS="-Werror"
# SPARSE_FLAGS=""
# EXTRA_OPTS="--with-linux=$KERNELSRC"
# sudo -E apt-get install -qq linux-source
# cd /usr/src/linux-source-3.13.0
# tar jxf linux-source-3.13.0.tar.bz2
# cd linux-source-3.13.0
# make allmodconfig
# make net/openvswitch/
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

# update grub (need reboot)
# Recommand "default_hugepagesz=1G hugepagesz=1G hugepages=8" for real server
# grep GRUB_CMDLINE_LINUX /etc/default/grub
GRUB_CMDLINE_LINUX="crashkernel=auto rhgb quiet transparent_hugepage=never default_hugepagesz=2MB hugepagesz=2MB hugepages=512"
update-grub
