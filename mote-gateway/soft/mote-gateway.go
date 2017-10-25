package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"time"
	"regexp"
	"strings"
	"flag"
	"strconv"
	"github.com/influxdata/influxdb/client/v2"
	"github.com/tarm/serial"
)

var influx_url, influx_db,influx_user,influx_pass string;
var serial_dev string;




func doLog(format string, a ...interface{}) {
	t := time.Now()
	fmt.Printf("%s ", string(t.Format("20060102 150405")) )
	fmt.Printf(format, a...)
}

type  Packet struct {
	// FWVER:0102;CAPA:0004;BATLEV:2764;AWAKE_SEC:0;MAIN_LOOP:0;TEMP:+0.00;
	raw	string
	payload	string
	smac,dmac string
	datamap map[string]interface{}
}

func dump_packet(p *Packet){
	doLog("Dump:\nX Raw:'%' '\n", p.raw);
	doLog("Dump:\nX Payload:'%' '\n", p.payload);
	doLog("Dump:\nX Smac:'%s' Dmac: '%s'\n", p.smac, p.dmac);
	for  k,v := range p.datamap {
		doLog("Dump key/values: '%s' value: '%s'\n", k, v)
	}
}

func decode_packet(p *Packet){

	r, err := hex.DecodeString(p.raw[18:]) // Skip header (macs)
	if err != nil {
		log.Fatal(err)
	}
	p.smac=p.raw[2:10]
	p.dmac=p.raw[10:18]
	p.payload = (string)(r); 
    ss := strings.Split(p.payload, ";")
	p.datamap = make(map[string]interface{})
	for  _, pair := range ss {
		z:=strings.Split(pair,":")
		if len(z)==2 {
			if(z[0]=="TEMP") {
				//doLog ("This is float"); 
				f, _  := strconv.ParseFloat(z[1],64)
				p.datamap[z[0]]=f
			} else {
				i, _  := strconv.ParseInt(z[1],10,64)
				p.datamap[z[0]]=i
			}
		}
	}
}

func push_influx(p *Packet){
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
	tags := map[string]string{"id": p.smac}
	fields := p.datamap

	if (len(p.datamap)>0){
		doLog("Sending to influx server\n");
		pt, err := client.NewPoint("metrics", tags, fields, time.Now())
		if err != nil {
			log.Fatal(err)
		}
		bp.AddPoint(pt)
	}
	// Write the batch
	if err := c.Write(bp); err != nil {
		log.Fatal(err)
	}

}


func serialworker(sig chan *Packet) {
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

				pkt :=  Packet {
					raw: (string)(buftotal[m[0]+1:m[1]-1]),
				}
				buftotal = buftotal[m[1]:]

				// Send signal
				sig <- &pkt
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

	doLog("InfluxDB backend: Url:",influx_url, " Db", influx_db, " User", influx_user, " Pass:xxx\n");

}


func main() {

	doLog("OLFmotes gateway server (golang)\n")

	parseArgs()

	// Inter routines communicatin
	//messages := make(chan *Packet)
	signals := make(chan *Packet)

	// Deal with serial in a job !
	go serialworker(signals)

	// Loop until someting happens
	for {
		p := <-signals
		decode_packet(p);
		dump_packet(p);
		push_influx(p)
	}

}
