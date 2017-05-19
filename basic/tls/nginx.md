---
title: "nginx tls配置"
---

## 单向ssl

```
server {
        listen 443;
        server_name example.com;
        root /apps/www;
        index index.html index.htm;
        ssl on;
        ssl_certificate      /usr/local/nginx/ca/server/server.crt; 
        ssl_certificate_key  /usr/local/nginx/ca/server/server.key; 
        ssl_client_certificate /usr/local/nginx/ca/private/ca.crt; 
        ssl_session_timeout  5m; 
        ssl_verify_client on;  #开户客户端证书验证 

        ssl_protocols  SSLv2 SSLv3 TLSv1; 
        ssl_ciphers ALL:!ADH:!EXPORT56:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv2:+EXP; 
        ssl_prefer_server_ciphers   on; 
}
```

http的请求强制转到https

```
server {
  listen      80;
  server_name example.me;
  rewrite     ^   https://$server_name$request_uri? permanent;
### 使用return的效率会更高 
#  return 301 https://$server_name$request_uri;
}
```

## 双向ssl

```
    proxy_ignore_client_abort on；
    ssl on;
    ...
    ssl_verify_client on;
    ssl_verify_depth 2;
    ssl_client_certificate ../SSL/ca-chain.pem;
    # 在双向location下加入：
    proxy_set_header X-SSL-Client-Cert $ssl_client_cert;

```