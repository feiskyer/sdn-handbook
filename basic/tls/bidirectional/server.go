package main

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
)

func main() {
	s := &http.Server{
		Addr: ":443",
		Handler: http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			fmt.Fprintf(w, "Hello World!\n")
		}),
		TLSConfig: &tls.Config{
			ClientCAs:  loadCA("ca.crt"),
			ClientAuth: tls.RequireAndVerifyClientCert,
		},
	}

	e := s.ListenAndServeTLS("server.crt", "server.key")
	if e != nil {
		log.Fatal("ListenAndServeTLS: ", e)
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
