#!/bin/bash

# 1. 创建我们自己CA的私钥：

openssl genrsa -out ca.key 2048

# 创建我们自己CA的CSR，并且用自己的私钥自签署之，得到CA的身份证：

openssl req -x509 -new -nodes -key ca.key -days 10000 -out ca.crt -subj "/CN=we-as-ca"

# 2. 创建server的私钥，CSR，并且用CA的私钥自签署server的身份证：

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

# 3. 创建client的私钥，CSR，以及用`ca.key`签署client的身份证：

openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -subj "/CN=localhost"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365
