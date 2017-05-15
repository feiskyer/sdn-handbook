# OVN OpenStack

* http://networkop.co.uk/blog/2016/12/10/ovn-part2/
* https://docs.openstack.org/developer/networking-ovn/refarch/refarch.html
* https://docs.openstack.org/developer/networking-ovn/refarch/routers.html


OpenStack [networking-ovn](https://github.com/openstack/networking-ovn) project contains an ML2 driver for OpenStack Neutron that provides integration with OVN.  It differs from Neutron’s original OVS integration in some significant ways.  It no longer makes use of the Neutron Python agents as all equivalent functionality has been moved into OVN.  As a result, it no longer uses RabbitMQ.  Neutron’s use of RabbitMQ for RPC has been replaced by OVN’s database driven control plane.  The following diagram gives a visual representation of the architecture of Neutron using OVN.  Even more detail can be found in our documented [reference architecture](http://docs.openstack.org/developer/networking-ovn/refarch/refarch.html).

![](/images/14878643863580.png)

