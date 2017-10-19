package main

import (
	"encoding/hex"
	"fmt"
	"github.com/tarm/serial"
	"log"
	"regexp"
	"time"
	"github.com/influxdata/influxdb/client/v2"
)

const (
	iUrl = "http://localhost:8086"
	iDb = "oflmotes"
	iUser = "dev"
	iPass = "dev"
)



func write_data() {

	// Create a new HTTPClient
	c, err := client.NewHTTPClient(client.HTTPConfig{
		Addr:     iUrl,
		Username: iUser,
		Password: iPass,
	})
	if err != nil {
		log.Fatal(err)
	}

	// Create a new point batch
	bp, err := client.NewBatchPoints(client.BatchPointsConfig{
		Database:  iDb,
		Precision: "s",
	})
	if err != nil {
		log.Fatal(err)
	}

	// Create a point and add to batch
	tags := map[string]string{"cpu": "cpu-total"}
	fields := map[string]interface{}{
		"idle":   10.1,
		"system": 53.3,
		"user":   46.6,
	}

	pt, err := client.NewPoint("metrics", tags, fields, time.Now())
	if err != nil {
		log.Fatal(err)
	}
	bp.AddPoint(pt)

	// Write the batch
	if err := c.Write(bp); err != nil {
		log.Fatal(err)
	}

}


func serialjob(msg chan string, sig chan string) {
	c := &serial.Config{Name: "/dev/ttyUSB1", Baud: 115200, ReadTimeout: time.Second * 1}
	s, err := serial.OpenPort(c)
	if err != nil {
		log.Fatal(err)
	}

	n, err := s.Write([]byte("net mon\n"))
	if err != nil {
		log.Fatal(err)
	}

	buftotal := make([]byte, 1)
	buf := make([]byte, 1024)

	for {
		n, _ = s.Read(buf)
		if n > 0 {
			buftotal = append(buftotal, buf[:n]...)

			r, _ := regexp.Compile("<[0-9A-Z]+\n")
			m := r.FindIndex(buftotal)
			if m != nil {
				mat := buftotal[m[0]+1+18 : m[1]-4-1] /// Extract only playload (no '<', \n,macs
				//fmt.Printf("Match %s\r\n", mat)
				decoded, err := hex.DecodeString(string(mat))
				if err != nil {
					log.Fatal(err)
				}

				//fmt.Printf("%s\r\n", decoded)
				buftotal = buftotal[m[1]:]
				sig <- string(decoded)
			}

		}
	} // Endless loop for
}

func main() {

	fmt.Println("OLFmotes gateway server\r\n")
	fmt.Println("Made with GOLANG !\r\n")

	messages := make(chan string)
	signals := make(chan string)

	// Deal with serial in a job !
	go serialjob(messages, signals)

	// Loop until someting happens
	for {
		s := <-signals
		fmt.Println("Input :", s)
		write_data();
	}

}
