# 内核编译

## 编译Linux内核

### 下载内核源码

```sh
apt-get source linux-image-$(uname -r)
apt-get build-dep linux-image-$(uname -r)
apt-get install -y libncurses5 libncurses5-dev
```

### 修改内核配置并编译

```sh
chmod a+x debian/rules
chmod a+x debian/scripts/*
chmod a+x debian/scripts/misc/*
fakeroot debian/rules clean
fakeroot debian/rules editconfigs

# build kernel
fakeroot debian/rules clean
# quicker build:
fakeroot debian/rules binary-headers binary-generic binary-perarch
# if you need linux-tools or lowlatency kernel, run instead:
fakeroot debian/rules binary

# Only build kernel modules for network drivers
# make prepare
# make modules_prepare
# make modules SUBDIRS=drivers/net
# make M=drivers/net
# make modules
```

### 安装并重启

```sh
dpkg -i linux*4.8.0-17.19*.deb
reboot
```
