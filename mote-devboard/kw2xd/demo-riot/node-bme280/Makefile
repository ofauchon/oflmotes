# name of your application
APPLICATION = node-bme280
BOARD ?= oflmote-dev

PORT = /dev/ttyUSB1
BAUD = 115200
TERMPROG=picocom
TERMFLAGS=$(PORT) -b $(BAUD) --imap lfcrlf --echo

RIOTBASE ?= /home/olivier/dev/RIOT.gebart
#RIOTBASE ?= /home/olivier/dev/RIOT.original

#RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
RIOTBOARD ?= $(CURDIR)/../boards

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
#USEMODULE += netstats_l2 
# UART removed as KW2x don't have LPUART and regular UART Block Low Power Mode
#USEMODULE += periph_uart
#USEMODULE += gnrc_mac 
#USEMODULE += periph_llwu
USEMODULE += xtimer

# UART removed as KW2x don't have LPUART and regular UART Block Low Power Mode
#FEATURES_REQUIRED = periph_uart
#FEATURES_REQUIRED += periph_rtt
FEATURES_REQUIRED += periph_adc

BOARD_PROVIDES_NETIF := oflmote-dev

FEATURES_OPTIONAL += config
FEATURES_OPTIONAL += periph_rtc

FEATURES_REQUIRED += periph_i2c
USEMODULE +=  bme280
CFLAGS += -DBMX280_PARAM_I2C_DEV=I2C_DEV\(0\)
CFLAGS += -DBMX280_PARAM_I2C_ADDR=0x76


CFLAGS += -DNODE_ID=0x98
CFLAGS += -DDEVELHELP -g
RIOT_BUILD_VERSION := $(cd $(RIOTBASE); shell git log | head -n 1 | cut -d\  -f2 )
MOTES_BUILD_VERSION := $(shell git log | head -n 1 | cut -d\  -f2 )
BUILD_DATE := $(shell date  +%G%m%d_%H%M%S )
CFLAGS += -DMOTESVERSION=\"git.$(MOTES_VERSION)\" -DRIOTVERSION=\"git.$(RIOT_VERSION)\" -DBUILDDATE=\"$(BUILD_DATE)\"

OPENOCD_ADAPTER_INIT ?= -c 'source [find interface/jlink.cfg]' -c 'transport select swd'
#OPENOCD_ADAPTER_INIT ?= -c 'source [find interface/jlink.cfg]' 


include $(RIOTBASE)/Makefile.include

swo:   
	openocd  -f 'interface/jlink.cfg' -c 'transport select swd' -f 'target/kx.cfg' -c 'tpiu config internal /tmp/traceswo.log uart off 48000000 100000'
trace:
	 ./swo-tracer -t  /tmp/traceswo.log


picocom:   
	picocom /dev/ttyUSB0 --baud 115200  --imap crcrlf,lfcrlf --logfile picocom.log

