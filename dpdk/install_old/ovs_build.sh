#!/bin/bash
export OVS_VERSION=2.6.1
export OVS_CTL_OPTS=""

apt-get update 
apt-get install -y build-essential curl dh-autoreconf libssl-dev python python-six 
apt-get install -y linux-headers-$(uname -r) kmod linux-image-$(uname -r)

curl -sSL http://openvswitch.org/releases/openvswitch-${OVS_VERSION}.tar.gz | tar -xz 
cd openvswitch-${OVS_VERSION} 
./configure --prefix=/usr --localstatedir=/var --sysconfdir=/etc --enable-ssl --with-linux=/lib/modules/$(uname -r)/build 

make -j `nproc` && make install && make modules_install && /bin/rm -rf openvswitch-${OVS_VERSION} 
# apt-get clean && rm -rf /var/lib/apt/lists/*

/usr/share/openvswitch/scripts/ovs-ctl start --system-id=random ${OVS_CTL_OPTS}
