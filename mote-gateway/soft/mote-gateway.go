package main

import (
	"encoding/hex"
	"fmt"
	"github.com/tarm/serial"
	"log"
	"regexp"
	"time"
	"strings"
	"github.com/influxdata/influxdb/client/v2"
	"flag"
)

var influx_url, influx_db,influx_user,influx_pass string;
var serial_dev string;

func process_data(s string) {

	fmt.Println("process_data:",s);

	// FWVER:0102;CAPA:0004;BATLEV:2764;AWAKE_SEC:0;MAIN_LOOP:0;TEMP:+0.00;
    ss := strings.Split(s,";")
	m := make(map[string]string)
	for  _, pair := range  ss {
		z:=strings.Split(pair,":")
		if len(z)==2 {
			m[z[0]]=z[1]
		}
	}

	for  k,v := range m {
		fmt.Println("Key:", k, "Val:", v)
	}

	// Create a new HTTPClient
	c, err := client.NewHTTPClient(client.HTTPConfig{
		Addr:     influx_url,
		Username: influx_user,
		Password: influx_pass,
	})
	if err != nil {
		log.Fatal(err)
	}

	// Create a new point batch
	bp, err := client.NewBatchPoints(client.BatchPointsConfig{
		Database:  influx_db,
		Precision: "s",
	})
	if err != nil {
		log.Fatal(err)
	}

	// Create a point and add to batch
	tags := map[string]string{"mote": "mote1"}
	fields := map[string]interface{}{
		"temp":   20.1,
		"pres": 0,
		"lux":   0,
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
	c := &serial.Config{Name: serial_dev, Baud: 115200, ReadTimeout: time.Second * 1}
	s, err := serial.OpenPort(c)
	if err != nil {
		log.Fatal(err)
	}

	n, err := s.Write([]byte("net chan 11\n"))
	if err != nil {
		log.Fatal(err)
	}
	// Fixme : Handle Write eror
	n, _ = s.Write([]byte("net mon\n"))

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


func parseArgs(){
	flag.StringVar(&influx_url, "influx_url", "", "InfluxDB serveur URL")
	flag.StringVar(&influx_db, "influx_db", "", "InfluxDB database")
	flag.StringVar(&influx_user, "influx_user", "", "InfluxDB user")
	flag.StringVar(&influx_pass, "influx_pass", "", "InfluxDB password")
	flag.StringVar(&serial_dev, "dev", "", "/dev/ttyUSB1")
	flag.Parse()

	fmt.Println("InfluxDB backend: Url:",influx_url, " Db", influx_db, " User", influx_user, " Pass:xxx");

}

func main() {

	fmt.Println("OLFmotes gateway server\r\n")
	fmt.Println("Written in GO !\r\n")

	parseArgs()

	// Inter routines communicatin
	messages := make(chan string)
	signals := make(chan string)

	// Deal with serial in a job !
	go serialjob(messages, signals)

	// Loop until someting happens
	for {
		s := <-signals
		process_data(s);
	}

}
