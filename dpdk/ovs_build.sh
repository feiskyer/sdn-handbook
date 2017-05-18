#!/bin/bash
set -x
source dpdk.rc

# download ovs
curl -sSl http://openvswitch.org/releases/openvswitch-${OVS_VERSION}.tar.gz | tar -xz && mv openvswitch-${OVS_VERSION} ${OVS_DIR}

# Build ovs
cd ${OVS_DIR}
./boot.sh
./configure --with-dpdk=/usr/local/share/dpdk/${RTE_TARGET} --prefix=${OVS_INSTALL_DIR} --localstatedir=/var --enable-ssl --with-linux=/lib/modules/$(uname -r)/build
make -j `nproc`
make install
