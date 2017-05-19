package main

import (
	"crypto/tls"
	"crypto/x509"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
)

func main() {
	pair, e := tls.LoadX509KeyPair("client.crt", "client.key")
	if e != nil {
		log.Fatal("LoadX509KeyPair:", e)
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{
				RootCAs:      loadCA("ca.crt"),
				Certificates: []tls.Certificate{pair},
			},
		}}

	resp, e := client.Get("https://localhost")
	if e != nil {
		log.Fatal("http.Client.Get: ", e)
	}
	defer resp.Body.Close()
	io.Copy(os.Stdout, resp.Body)
}

func loadCA(caFile string) *x509.CertPool {
	pool := x509.NewCertPool()

	if ca, e := ioutil.ReadFile(caFile); e != nil {
		log.Fatal("ReadFile: ", e)
	} else {
		pool.AppendCertsFromPEM(ca)
	}
	return pool
}
