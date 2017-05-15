# OVN Kubernetes

VN provides network virtualization to containers. OVN's integration with containers currently works in two modes - the "underlay" mode or the "overlay" mode.

* In the "underlay" mode, one can create logical networks and can have containers running inside VMs, standalone VMs (without having any containers running inside them) and physical machines connected to the same logical network. In this mode, OVN programs the Open vSwitch instances running in hypervisors with containers running inside the VMs.
* In the "overlay" mode, OVN can create a logical network amongst containers running on multiple hosts. In this mode, OVN programs the Open vSwitch instances running inside your VMs or directly on bare metal hosts.

## Overlay mode

![](Vp0TCil.png)

### Setup master

```
ovs-vsctl set Open_vSwitch . external_ids:k8s-api-server="127.0.0.1:8080"
ovn-k8s-overlay master-init \
  --cluster-ip-subnet="192.168.0.0/16" \
  --master-switch-subnet="192.168.1.0/24" \
  --node-name="kube-master"
```

### Setup nodes

```
ovs-vsctl set Open_vSwitch . \
  external_ids:k8s-api-server="$K8S_API_SERVER_IP:8080"

ovs-vsctl set Open_vSwitch . \
  external_ids:k8s-api-server="https://$K8S_API_SERVER_IP" \
  external_ids:k8s-ca-certificate="$CA_CRT" \
  external_ids:k8s-api-token="$API_TOKEN"

ovn-k8s-overlay minion-init \
  --cluster-ip-subnet="192.168.0.0/16" \
  --minion-switch-subnet="192.168.2.0/24" \
  --node-name="kube-minion1"
```

### Gateway nodes (minions or separate nodes)

```
ovs-vsctl set Open_vSwitch . \
  external_ids:k8s-api-server="$K8S_API_SERVER_IP:8080"

ovn-k8s-overlay gateway-init \
  --cluster-ip-subnet="192.168.0.0/16" \
  --physical-interface eth1 \
  --physical-ip 10.33.74.138/24 \
  --node-name="kube-minion2" \
  --default-gw 10.33.74.253
```

The second option is to share a single network interface for both your management traffic (e.g ssh) as well as the cluster's North-South traffic. To do this, you need to attach your physical interface to a OVS bridge and move its IP address and routes to that bridge. For e.g., if 'eth0' is your primary network interface with IP address of "$PHYSICAL_IP", you create a bridge called 'breth0', add 'eth0' as a port of that bridge and then move "$PHYSICAL_IP" to 'breth0'. You also need to move the routing table entries to 'breth0'. The following helper script does the same

```
ovn-k8s-util nics-to-bridge eth0
```

```
ovn-k8s-overlay gateway-init \
  --cluster-ip-subnet="$CLUSTER_IP_SUBNET" \
  --bridge-interface breth0 \
  --physical-ip "$PHYSICAL_IP" \
  --node-name="$NODE_NAME" \
  --default-gw "$EXTERNAL_GATEWAY"
```

Since you share a NIC for both mgmt and North-South connectivity, you will have to start a separate daemon to de-multiplex the traffic.

```
ovn-k8s-gateway-helper --physical-bridge=breth0 --physical-interface=eth0 \
    --pidfile --detach
```

### Start watcher

Finally, start a watcher on the master node to listen for Kubernetes events. This watcher is responsible to create logical ports and load-balancer entries.

```
ovn-k8s-watcher \
  --overlay \
  --pidfile \
  --log-file \
  -vfile:info \
  -vconsole:emer \
  --detach
```

### CNI plugin

#### ADD

* Get ip/mac/gateway from 'ovn' annotation
* Setup interface and routes inside container's netns
* Add ovs port

```
ovs-vsctl add-port br-int veth_outside \
  --set interface veth_outside \
    external_ids:attached_mac=mac_address \
    external_ids:iface-id=namespace_pod \
    external_ids:ip_address=ip_address
```

#### DEL

```
ovs-vsctl del-port br-int port
```

## Underlay mode

Not supported yet.


**Reference**

* <https://github.com/openvswitch/ovn-kubernetes>
