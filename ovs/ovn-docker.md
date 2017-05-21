# OVN docker plugin

```sh
# start docker
docker daemon --cluster-store=consul://127.0.0.1:8500 \
    --cluster-advertise=$HOST_IP:0

# start north
/usr/share/openvswitch/scripts/ovn-ctl start_northd
ovn-nbctl set-connection ptcp:6641
ovn-sbctl set-connection ptcp:6642

# start south
ovs-vsctl set Open_vSwitch . \
    external_ids:ovn-remote="tcp:$CENTRAL_IP:6642" \
    external_ids:ovn-nb="tcp:$CENTRAL_IP:6641" \
    external_ids:ovn-encap-ip=$LOCAL_IP \
    external_ids:ovn-encap-type="$ENCAP_TYPE"
/usr/share/openvswitch/scripts/ovn-ctl start_controller

# start openvswitch plugin
pip install Flask
PYTHONPATH=$OVS_PYTHON_LIBS_PATH ovn-docker-overlay-driver --detach

# create docker network
docker network create -d openvswitch --subnet=192.168.1.0/24 foo
```

## Workflow

### Initialize ovn bridge

```sh
ovs-vsctl --timeout=5 -vconsole:off -- --may-exist add-br br-int \
          -- set bridge br-int external_ids:bridge-id=br-int \
             other-config:disable-in-band=true fail-mode=secure

ovs-vsctl --timeout=5 -vconsole:off -- get Open_vSwitch . external_ids:ovn-nb
ovs-vsctl --timeout=5 -vconsole:off -- set open_vswitch . external_ids:ovn-bridge=br-int
```

### Create network

```sh
nid="red-net"
ovn-nbctl ls-add $nid -- set Logical_Switch $nid external_ids:subnet=10.160.0.0/24 external_ids:gateway_ip=10.160.0.1
ovn-nbctl show
```

### Create container

```sh
nid="red-net"
eid="blue-container"
ip="10.160.0.2"
mac="02:38:e1:a2:28:38"
ovn-nbctl lsp-add $nid $eid
ovn-nbctl lsp-set-addresses $eid "$mac $ip"

ip netns add $eid
ip link add veth_inside type veth peer name veth_outside
ip link set dev veth_inside address $mac
ip link set veth_inside netns $eid
ip link set veth_outside up
ip netns exec $eid ip addr add 10.160.0.2/24 dev veth_inside
ip netns exec $eid ip route add default via 10.160.0.1

ovs-vsctl --timeout=5 -vconsole:off \
          -- add-port br-int veth_outside \
          -- set interface veth_outside \
             external_ids:attached-mac=$mac \
             external_ids:iface-id=$eid \
             external_ids:vm-id=$eid \
             external_ids:iface-status=active
```

Get endpoint status

```sh
ovn-nbctl --if-exists get Logical_Switch_Port $eid addresses
```

### Delete container

```sh
ip netns del $eid
ip link delete veth_outside
ovs-vsctl --if-exists del-port veth_outside
ovn-nbctl lsp-del $eid
```

### Delete network

```sh
ovn-nbctl ls-del red-net
```

**References**

* <http://docs.openvswitch.org/en/latest/howto/docker/>
* <http://dockone.io/article/1200>
