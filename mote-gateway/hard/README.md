oflmote-gateway

This is the firmware for the OFLmote Gateway. 

  * Build and flash

Change RIOTBASE in Makefile to point to git clone  https://github.com/ofauchon/RIOT  (phywave-ftdi branch)

```
make clean
make BOARD=phywave-ftdi
make flash
```

  * How to use

You can test it first with a serial console: 

If you have 'picocom' installed, just run : 
$ make term 


Exemple of commands for listening to chan 11, pan 0xF00D (61453) to 802.15.4. 

```
net chan 11
net pan 61453
net addr 00:00:00:00:00:00:00:01 
net pktdump
```
