package main

import (
	"container/list"
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
	t := time.Now()

	if cnf_logfile != "" && logfile == os.Stdout {
		fmt.Printf("INFO: Creating log file '%s'\n", cnf_logfile)
		tf, err := os.OpenFile(cnf_logfile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			fmt.Printf("ERROR: Can't open log file '%s' for writing, logging to Stdout\n", cnf_logfile)
			fmt.Printf("ERROR: %s\n", err)
		} else {
			fmt.Printf("INFO: log file '%s' is ready for writing\n", cnf_logfile)
			logfile = tf
		}
	}

	// logfile default is os.Stdout
	fmt.Fprintf(logfile, "%s ", string(t.Format("20060102 150405")))
	fmt.Fprintf(logfile, format, a...)
}

type Packet struct {
	raw        string
	smac, dmac string
	span, dpan string
	payload    string
	lqi, rssi  int64
	datamap    map[string]interface{}
	timestamp  time.Time
}

func dump_packet(p *Packet) {
	//doLog("Dump: Raw:'%s'\n", hex.Dump( []byte(p.raw)  ) );
	doLog("Dump: Timestamp:'%s'\n", p.timestamp)
	doLog("Dump: Payload:'%s'\n", p.payload)
	doLog("Dump: Smac:'%s' Dmac: '%s'\n", p.smac, p.dmac)
	//doLog("Dump: Span:'%s' Dpan: '%s'\n", p.span, p.dpan)
	doLog("Dump: lqi:'%d' rssi: '%d'\n", p.lqi, p.rssi)
	for k, v := range p.datamap {
		doLog("Dump: '%s' => '%v'\n", k, v)
	}
}

func decode_packet(p *Packet) {

	doLog("RECEIVED: %s\n", p.raw)

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

	k, err := hex.DecodeString(p.payload)
	if err != nil {
		doLog("ERROR: Can't decode HEX payload '%s'\n", p.payload)
		return
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

func push_influx(ps *list.List) {
	// Create a new HTTPClient
	c, err := client.NewHTTPClient(client.HTTPConfig{
		Addr:     influx_url,
		Username: influx_user,
		Password: influx_pass,
		Timeout:  10 * time.Second,
	})
	if err != nil {
		doLog("ERROR: Can't create HTTP client to influxdb :%s\n", err)
	}
	defer c.Close()

	doLog("Buffer contains %d elements\n", ps.Len())
	for ps.Len() > 0 {
		// Get First In, and remove it
		e := ps.Front()
		p := e.Value.(*Packet)
		dump_packet(p)

		// Create a new point batch
		bp, err := client.NewBatchPoints(client.BatchPointsConfig{
			Database:  influx_db,
			Precision: "s",
		})
		if err != nil {
			doLog("ERROR: Can't create batchpoint : %s\n", err)
			return
		}

		// Create a point and add to batch
		tags := map[string]string{"ID": p.smac}
		p.datamap["RSSI"] = p.rssi
		p.datamap["LQI"] = p.lqi
		fields := p.datamap

		if len(p.datamap) > 0 {
			//		doLog("Sending %d metrics to influx server\n", len(p.datamap) )
			pt, err := client.NewPoint("metrics", tags, fields, p.timestamp)
			if err != nil {
				doLog("ERROR: Can't create InfluxDB points: %s\n", err)
				return
			}
			bp.AddPoint(pt)
		}
		// Write the batch
		if err := c.Write(bp); err != nil {
			doLog("ERROR: Can't send batch '%s'\n", err)
			return
		}
		doLog("Transmission OK\n")

		ps.Remove(e)
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
	//n, _ = s.Write([]byte("net mon\n"))

	buftotal := make([]byte, 1)
	buf := make([]byte, 1024)

	// <PAYLOAD:46575645523A303130343B434150413A303030343B4241544C45563A323636313B4157414B455F5345433A303B4D41494E5F4C4F4F503A303B4552524F523A303031323B4800; SMAC:00:00:00:00:00:00:00:25;LQI:120;RSSI:61>
	r, _ := regexp.Compile("<PAYLOAD=[^>]+>\n")
	doLog("Start reading UART, forever\n")
	for {
		n, _ = s.Read(buf)
		if n > 0 {
			buftotal = append(buftotal, buf[:n]...)
			doLog("Serial: Read %d bytes [%s]\n", n, buftotal );

			m := r.FindIndex(buftotal)
			if m != nil {
				//		fmt.Println("OK\n")

				pkt := Packet{
					raw:       (string)(buftotal[m[0]+1 : m[1]-2]),
					timestamp: time.Now(),
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

	doLog("Conf : InfluxDB backend: Url:%s Db:%s\n", influx_url, influx_db)
	doLog("Conf : InfluxDB backend: User:%s Pass:<hidden>\n", influx_user)

}

func main() {

	logfile = os.Stdout
	parseArgs()
	doLog("Starting OLFmotes gateway \n")

	// Inter routines communicatin
	signals := make(chan *Packet)

	// Deal with serial in a job !
	go serialworker(signals)

	var p *Packet
	ps := list.New()
	// Loop until someting happens
	for {
		p = <-signals
		decode_packet(p)
		ps.PushBack(p)

		push_influx(ps)
	}

}
