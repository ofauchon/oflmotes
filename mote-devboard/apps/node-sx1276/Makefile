# name of your application
APPLICATION = node-sx1276
BOARD ?= oflmote-dev

PORT ?= /dev/ttyUSB1
BAUD ?= 115200
RIOT_TERMINAL?=  picocom
TERMPROG     ?=  picocom
TERMFLAGS    ?=  $(PORT) -b $(BAUD) --imap lfcrlf --echo

RIOTBASE ?= ../../../../RIOT.benemorius

# LORA start Configuration
DEVEUI ?= AA13DEDD01010101
APPEUI ?= 0000000001010101
APPKEY ?= B896B9034CB472E7856F05B512974F8F

LORA_REGION = EU868
USEPKG += semtech-loramac
USEMODULE += sx1276
USEMODULE += auto_init_loramac
USEMODULE += semtech_loramac_rx
USEMODULE += fmt
CFLAGS += -DDEVEUI=\"$(DEVEUI)\" -DAPPEUI=\"$(APPEUI)\" -DAPPKEY=\"$(APPKEY)\"
# LORA End configuration


# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

BOARD_PROVIDES_NETIF := oflmote-dev

include $(RIOTBASE)/Makefile.include

# VERSIONS
CFLAGS += -DNODE_ID=0x0F
CFLAGS += -DDEVELHELP -g
RIOT_BUILD_VERSION := $(cd $(RIOTBASE); shell git log | head -n 1 | cut -d\  -f2 )
MOTES_BUILD_VERSION := $(shell git log | head -n 1 | cut -d\  -f2 )
BUILD_DATE := $(shell date  +%G%m%d_%H%M%S )
CFLAGS += -DMOTESVERSION=\"git.$(MOTES_VERSION)\" -DRIOTVERSION=\"git.$(RIOT_VERSION)\" -DBUILDDATE=\"$(BUILD_DATE)\"

OPENOCD_ADAPTER_INIT ?= -c 'source ../openocd/ftdi.cfg'  -c 'transport select swd'

picocom:   
	picocom /dev/ttyUSB1 --baud 115200  --imap crcrlf,lfcrlf  --logfile picocom.log

picocom9600:   
	picocom /dev/ttyUSB1 --baud 115200  --imap crcrlf,lfcrlf  --logfile picocom.log

