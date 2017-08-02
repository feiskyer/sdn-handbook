# Ryu

[Ryu](https://osrg.github.io/ryu/)是日本NTT公司推出的SDN控制器框架，它基于Python开发，模块清晰，可扩展性好，逐步取代了早期的NOX和POX。

- Ryu支持OpenFlow 1.0到1.5版本，也支持Netconf，OF-CONIFG等其他南向协议
- Ryu可以作为OpenStack的插件，见[Dragonflow](https://github.com/openstack/dragonflow)
- Ryu提供了丰富的组件，便于开发者构建SDN应用

## 示例

Ryu的安装非常简单，直接用pip就可以安装

```sh
pip install ryu
```

安装完成后，就可以用python来开发SDN应用了。比如下面的例子构建了一个L2Switch应用：

```python
from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_0

class L2Switch(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_0.OFP_VERSION]

    def __init__(self, *args, **kwargs):
        super(L2Switch, self).__init__(*args, **kwargs)

    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def packet_in_handler(self, ev):
        msg = ev.msg
        dp = msg.datapath
        ofp = dp.ofproto
        ofp_parser = dp.ofproto_parser

        actions = [ofp_parser.OFPActionOutput(ofp.OFPP_FLOOD)]
        out = ofp_parser.OFPPacketOut(
            datapath=dp, buffer_id=msg.buffer_id, in_port=msg.in_port,
            actions=actions)
        dp.send_msg(out)
```

最后可以使用`ryu-manager`启动应用：

```sh
ryu-manager L2Switch.py
```

## 参考文档

- [Ryu官网](https://osrg.github.io/ryu/)
- [Ryu源码](https://github.com/osrg/ryu)
- [Ryu Book](http://osrg.github.io/ryu-book/en/html/)
- [RYU 控制器性能测试报告](http://www.sdnctc.com/public/download/RYU.pdf)
