package main

import (
	"encoding/hex"
	"flag"
	"fmt"
	"github.com/influxdata/influxdb/client/v2"
	"github.com/tarm/serial"
	"log"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"
)

var influx_url, influx_db, influx_user, influx_pass string
var serial_dev string
var cnf_logfile string
var cnf_daemon bool
var logfile *os.File

func doLog(format string, a ...interface{}) {
	out := os.Stdout
	t := time.Now()
	var err error
	if cnf_logfile != "" {
		if logfile == nil {
			fmt.Printf("Creating log file '%s'\n", cnf_logfile)
			logfile, err = os.OpenFile(cnf_logfile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
			if err != nil {
				//		out = bufio.NewWriter(f)
				log.Fatal(err)
			}
		}
		out = logfile
	}

	fmt.Fprintf(out, "%s ", string(t.Format("20060102 150405")))
	fmt.Fprintf(out, format, a...)
}

type Packet struct {
	raw        string
	smac, dmac string
	span, dpan string
	payload    string
	lqi, rssi  int64
	datamap    map[string]interface{}
}

func dump_packet(p *Packet) {
	//doLog("Dump: Raw:'%s'\n", hex.Dump( []byte(p.raw)  ) );
	doLog("Dump: Payload:'%s'\n", p.payload)
	doLog("Dump: Smac:'%s' Dmac: '%s'\n", p.smac, p.dmac)
	doLog("Dump: Span:'%s' Dpan: '%s'\n", p.span, p.dpan)
	doLog("Dump: lqi:'%d' rssi: '%d'\n", p.lqi, p.rssi)
	for k, v := range p.datamap {
		doLog("Dump: key/values: '%s' value: '%v'\n", k, v)
	}
}

func decode_packet(p *Packet) {

	doLog("RAW MSG: %s", p.raw)

	p.datamap = make(map[string]interface{})

	// Decore RAW
	ss := strings.Split(p.raw, ";")
	for _, pair := range ss {
		z := strings.Split(pair, "=")
		if len(z) == 2 {
			if z[0] == "PAYLOAD" {
				p.payload = z[1]
			}
			if z[0] == "SMAC" {
				p.smac = z[1]
			}
			if z[0] == "LQI" {
				p.lqi, _ = strconv.ParseInt(z[1], 10, 64)
			}
			if z[0] == "RSSI" {
				p.rssi, _ = strconv.ParseInt(z[1], 10, 64)
			}
		}
	}

	k, err := hex.DecodeString(p.payload) // Skip header (macs)
	if err != nil {
		log.Fatal(err)
	}
	p.payload = string(k)

	// Decore Payload
	ss = strings.Split(p.payload, ";")
	for _, pair := range ss {
		z := strings.Split(pair, ":")
		if len(z) == 2 {
			if z[0] == "TEMP" || z[0] == "HUMI" {
				//doLog ("This is float");
				f, _ := strconv.ParseFloat(z[1], 64)
				p.datamap[z[0]] = f
			} else {
				i, _ := strconv.ParseInt(z[1], 10, 64)
				p.datamap[z[0]] = i
			}

		}
	}

}

/*


 */

func push_influx(p *Packet) {
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
	tags := map[string]string{"ID": p.smac}
	p.datamap["RSSI"] = p.rssi
	p.datamap["LQI"] = p.lqi
	fields := p.datamap

	for k, v := range p.datamap {
		doLog("Dump_send_influx: key/values: '%s' value: '%v'\n", k, v)
	}

	if len(p.datamap) > 0 {
		doLog("Sending to influx server\n")
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

	// <PAYLOAD:46575645523A303130343B434150413A303030343B4241544C45563A323636313B4157414B455F5345433A303B4D41494E5F4C4F4F503A303B4552524F523A303031323B4800; SMAC:00:00:00:00:00:00:00:25;LQI:120;RSSI:61>
	r, _ := regexp.Compile("<PAYLOAD=[^>]+>\n")
	for {
		n, _ = s.Read(buf)
		if n > 0 {
			buftotal = append(buftotal, buf[:n]...)

			m := r.FindIndex(buftotal)
			if m != nil {
				//		fmt.Println("OK\n")

				pkt := Packet{
					raw: (string)(buftotal[m[0]+1 : m[1]-2]),
				}
				buftotal = buftotal[m[1]:]

				// Send signal
				sig <- &pkt
			}

		}
	} // Endless loop for
}

func parseArgs() {
	flag.StringVar(&influx_url, "influx_url", "", "InfluxDB serveur URL")
	flag.StringVar(&influx_db, "influx_db", "", "InfluxDB database")
	flag.StringVar(&influx_user, "influx_user", "", "InfluxDB user")
	flag.StringVar(&influx_pass, "influx_pass", "", "InfluxDB password")
	flag.StringVar(&serial_dev, "dev", "", "/dev/ttyUSB1")
	flag.StringVar(&cnf_logfile, "log", "", "Path to log file")
	flag.BoolVar(&cnf_daemon, "daemon", false, "Run in background")
	flag.Parse()

	doLog("InfluxDB backend: Url:%s Db:%s\n", influx_url, influx_db)
	doLog("InfluxDB backend: User:%s Pass:<hidden>\n", influx_user)

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
		decode_packet(p)
		dump_packet(p)
		push_influx(p)
	}

}
