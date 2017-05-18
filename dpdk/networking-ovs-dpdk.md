# DPDK accelerated Open vSwitch with neutron

## Source code

https://github.com/openstack/networking-ovs-dpdk

## Install

    yum install -y kernel-headers kernel-core kernel-modules kernel kernel-devel kernel-modules-extra

* https://github.com/openstack/networking-ovs-dpdk/blob/master/doc/source/_downloads/local.conf.single_node
* https://github.com/openstack/networking-ovs-dpdk/blob/master/doc/source/getstarted/devstack/centos.rst

## Usage

Note: Virtual Machines using vhost-user need to be backed by hugepages.


Example::

    nova flavor-key m1.tiny set "hw:mem_page_size=large"

