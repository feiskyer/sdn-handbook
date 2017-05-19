#!/bin/bash

# VERSION=v3.0.3
# VERSION=v2.3.4
VERSION=v2.3.3

#
# Download and unpack etcd.
#
if [[ $(uname) == "Linux" ]]; then
    if [[ ! -f etcd-$VERSION-linux-amd64.tar.gz ]]; then
	echo "Downloading etcd for Linux ..."
	wget https://github.com/coreos/etcd/releases/download/$VERSION/etcd-$VERSION-linux-amd64.tar.gz
    fi
    tar xzvf etcd-$VERSION-linux-amd64.tar.gz
    rm etcd && ln -s etcd-$VERSION-linux-amd64 etcd
elif [[ $(uname) == "Darwin" ]]; then
    if [[ ! -f etcd-$VERSION-darwin-amd64.zip ]]; then
	echo "Downloading etcd for Mac OS X ..."
	wget https://github.com/coreos/etcd/releases/download/$VERSION/etcd-$VERSION-darwin-amd64.zip
    fi
    unzip etcd-$VERSION-darwin-amd64.zip 
    rm etcd && ln -s etcd-$VERSION-darwin-amd64 etcd
else
    echo "Unknow OS: " $(uanme)
    exit 1
fi

# 
# Create TLS assets
# 
../bidirectional/create_tls_asserts.bash
