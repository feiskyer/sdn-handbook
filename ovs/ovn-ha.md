# OVN HA

目前，OVS支持主从模式的高可用。

## Active-Backup

在启动ovsdb-server时，可以设置[主从同步选项](http://openvswitch.org/support/dist-docs/ovsdb-server.1.html)：

    Syncing Options

       The following options allow ovsdb-server to synchronize  its  databases
       with another running ovsdb-server.

       --sync-from=server
              Sets up ovsdb-server to synchronize its databases with the data‐
              bases  in  server,  which must be an active connection method in
              one of the forms documented in ovsdb-client(1).  Every  transac‐
              tion  committed  by  server  will be replicated to ovsdb-server.
              This option makes ovsdb-server start as  a  backup  server;  add
              --active to make it start as an active server.

       --sync-exclude-tables=db:table[,db:table]...
              Causes the specified tables to be excluded from replication.

       --active
              By  default, --sync-from makes ovsdb-server start up as a backup
              for server.  With --active, however, ovsdb-server starts  as  an
              active  server.  Use this option to allow the syncing options to
              be specified using command line options, yet start  the  server,
              as  the default, active server.  To switch the running server to
              backup mode, use ovs-appctl(1) to execute the  ovsdb-server/con‐
              nect-active-ovsdb-server command.

注意，这里的配置是静态的，主ovsdb-server出现问题时，从并不会自动恢复。这时可以借助Pacemaker来实现自动故障恢复：

After creating a pacemaker cluster, use the following commands to create one active and multiple backup servers for OVN databases:

```
$ pcs resource create ovndb_servers ocf:ovn:ovndb-servers \
     master_ip=x.x.x.x \
     ovn_ctl=<path of the ovn-ctl script> \
     op monitor interval="10s" \
     op monitor role=Master interval="15s"
$ pcs resource master ovndb_servers-master ovndb_servers \
    meta notify="true"
```

The `master_ip` and `ovn_ctl` are the parameters that will be used by the OCF script.

* `ovn_ctl` is optional, if not given, it assumes a default value of /usr/share/openvswitch/scripts/ovn-ctl.
* `master_ip` is the IP address on which the active database server is expected to be listening, the slave node uses it to connect to the master node. You can add the optional parameters ‘nb_master_port’, ‘nb_master_protocol’, ‘sb_master_port’, ‘sb_master_protocol’ to set the protocol and port.

Whenever the active server dies, pacemaker is responsible to promote one of the backup servers to be active. Both ovn-controller and ovn-northd needs the ip-address at which the active server is listening. With pacemaker changing the node at which the active server is run, it is not efficient to instruct all the ovn-controllers and the ovn-northd to listen to the latest active server’s ip-address.

This problem can be solved by using a native ocf resource agent ocf:heartbeat:IPaddr2. The IPAddr2 resource agent is just a resource with an ip-address. When we colocate this resource with the active server, pacemaker will enable the active server to be connected with a single ip-address all the time. This is the ip-address that needs to be given as the parameter while creating the ovndb_servers resource.

Use the following command to create the IPAddr2 resource and colocate it with the active server:

```
$ pcs resource create VirtualIP ocf:heartbeat:IPaddr2 ip=x.x.x.x \
    op monitor interval=30s
$ pcs constraint order promote ovndb_servers-master then VirtualIP
$ pcs constraint colocation add VirtualIP with master ovndb_servers-master \
    score=INFINITY
```

主从同步的实现方法可见[OVSDB Replication Implementation](http://docs.openvswitch.org/en/latest/topics/ovsdb-replication/)。

## Active-Active

OVN控制平面的Active-Active高可用还在开发中，预计会借鉴etcd的方式，基于Raft算法实现。

* <https://github.com/blp/ovs-reviews/tree/raft3>
* <http://docs.openvswitch.org/en/latest/topics/high-availability/>
* <http://galsagie.github.io/2015/08/03/df-distributed-db/>

