# name of your application
APPLICATION = node-sx1276
BOARD ?= oflmote-dev

PORT = /dev/ttyUSB1
BAUD = 115200
TERMPROG=picocom
TERMFLAGS=$(PORT) -b $(BAUD) --imap lfcrlf --echo

RIOTBASE ?= ../../../../RIOT.benemorius
#RIOTBASE ?= /home/olivier/dev/RIOT

#RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
RIOTBOARD ?= $(CURDIR)/../../boards



# LORA START

DEVEUI ?= AA13DEDD01010101
APPEUI ?= 0000000001010101
APPKEY ?= B896B9034CB472E7856F05B512974F8F

# Default radio driver is Semtech SX1276 (used by the B-L072Z-LRWAN1 board)
DRIVER ?= sx1276

# Default region is Europe and default band is 868MHz
LORA_REGION = EU868

# Include the Semtech-loramac package
USEPKG += semtech-loramac

USEMODULE += $(DRIVER)
USEMODULE += fmt
FEATURES_REQUIRED += periph_rtc

CFLAGS += -DDEVEUI=\"$(DEVEUI)\" -DAPPEUI=\"$(APPEUI)\" -DAPPKEY=\"$(APPKEY)\"

# LORA END


# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += xtimer

FEATURES_REQUIRED += periph_adc
FEATURES_REQUIRED += periph_spi

BOARD_PROVIDES_NETIF := oflmote-dev

FEATURES_OPTIONAL += config
FEATURES_OPTIONAL += periph_rtc


CFLAGS += -DNODE_ID=0x0F
CFLAGS += -DDEVELHELP -g
RIOT_BUILD_VERSION := $(cd $(RIOTBASE); shell git log | head -n 1 | cut -d\  -f2 )
MOTES_BUILD_VERSION := $(shell git log | head -n 1 | cut -d\  -f2 )
BUILD_DATE := $(shell date  +%G%m%d_%H%M%S )
CFLAGS += -DMOTESVERSION=\"git.$(MOTES_VERSION)\" -DRIOTVERSION=\"git.$(RIOT_VERSION)\" -DBUILDDATE=\"$(BUILD_DATE)\"

OPENOCD_ADAPTER_INIT ?= -c 'source ../openocd/ftdi.cfg'  -c 'transport select swd'



include $(RIOTBASE)/Makefile.include

swo:   
	openocd  -f 'interface/jlink.cfg' -c 'transport select swd' -f 'target/kx.cfg' -c 'tpiu config internal /tmp/traceswo.log uart off 48000000 100000'
trace:
	 ./swo-tracer -t  /tmp/traceswo.log


picocom:   
	picocom /dev/ttyUSB1 --baud 115200  --imap crcrlf,lfcrlf  --logfile picocom.log

