oflmote-gateway 

# How it works

This is the linux  part of the Gateway.
Basicaly, this server connects to the serial port and connects the mote gateway board.

Then, it sends initialisation sequence: 

``` 
net chan 11
net pan 61453
net addr 00:00:00:00:00:00:00:01^M
```

before starting packet dump mode : 

```
net pktdump
```

# Run

First, you need to install required libraries: 

```
go get github.com/tarm/serial
go get github.com/influxdata/influxdb/client/v2
```

Then edit and run start.sh wrapper: 

```
./start.sh
```

you'll find the logs in logs/


