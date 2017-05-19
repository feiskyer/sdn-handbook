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
	c := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{RootCAs: loadCA("server.crt")},
		}}

	if resp, e := c.Get("https://localhost"); e != nil {
		log.Fatal("http.Client.Get: ", e)
	} else {
		defer resp.Body.Close()
		io.Copy(os.Stdout, resp.Body)
	}
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
