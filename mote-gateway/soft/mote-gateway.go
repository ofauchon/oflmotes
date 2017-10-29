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

type Packet struct {
	raw	string
	payload	string
	smac,dmac string
	span,dpan string
	datamap map[string]interface{}
}

func dump_packet(p *Packet){
	doLog("Dump: Raw:'%s'\n", hex.Dump( []byte(p.raw)  ) );
	doLog("Dump: Payload:'%s'\n", p.payload );
	doLog("Dump: Smac:'%s' Dmac: '%s'\n", p.smac, p.dmac);
	doLog("Dump:X Span:'%s' Dpab: '%s'\n", p.span, p.dpan);
	for  k,v := range p.datamap {
		doLog("Dump: key/values: '%s' value: '%s'\n", k, v)
	}
}

func decode_packet(p *Packet){

	fmt.Println(p.raw)
	r, err := hex.DecodeString(p.raw[18:]) // Skip header (macs)
	if err != nil {
		log.Fatal(err)
	}
	p.dpan=p.raw[6:10]
	p.dmac=p.raw[10:26]
	p.span=p.raw[26:30]
	p.smac=p.raw[30:46]

	// Flip source/destination bytes
	cp1:="";
	cp2:="";
	for i:=7; i>=0; i--{
	fmt.Println( p.smac[(2*i):(2*i+2)]);
		cp1= cp1 + p.smac[2*i:2*i+2];
		cp2= cp2 + p.dmac[2*i:2*i+2];
	}
	p.smac=cp1; 
	p.dmac=cp2; 

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
	n, err = s.Write([]byte("net pan 61453\n"))
	if err != nil {
		log.Fatal(err)
	}
	n, err = s.Write([]byte("net addr 00:00:00:00:00:00:00:01\n"))
	if err != nil {
		log.Fatal(err)
	}
	n, err = s.Write([]byte("net pktdump\n"))
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
				fmt.Println("OK\n"); 

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

	doLog("InfluxDB backend: Url:%s Db:%s\n", influx_url, influx_db);
	doLog("InfluxDB backend: User:%s Pass:<hidden>\n", influx_user);

}


func main() {

	doLog("--------------------------------------)\n")
	doLog("OLFmotes gateway server (I love golang)\n")
	doLog("--------------------------------------)\n")

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
