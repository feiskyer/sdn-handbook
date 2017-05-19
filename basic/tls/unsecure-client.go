package main

import (
	"crypto/tls"
	"io"
	"log"
	"net/http"
	"os"
)

func main() {
	c := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		}}

	if resp, e := c.Get("https://localhost"); e != nil {
		log.Fatal("http.Client.Get: ", e)
	} else {
		defer resp.Body.Close()
		io.Copy(os.Stdout, resp.Body)
	}
}
