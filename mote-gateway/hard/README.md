oflmote-gateway

This is the firmware for the OFLmote Gateway. 

  * Build and flash

```
make clean
make 
make flash
```

  * How to use

You can test it first with a serial console: 


Exemple of commands for listening to chan 11, pan 0xF00D (61453) to 802.15.4. 

```
net chan 11
net pan 61453
net addr 00:00:00:00:00:00:00:01 
net pktdump
```