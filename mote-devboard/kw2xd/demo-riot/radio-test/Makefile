# name of your application
APPLICATION = radio
BOARD = oflmote-dev


PORT = /dev/ttyUSB1
BAUD = 115200
TERMPROG=picocom
TERMFLAGS=$(PORT) -b $(BAUD) --imap lfcrlf --echo

RIOTBASE ?= /home/olivier/dev/RIOT.original

#RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
RIOTBOARD ?= $(CURDIR)/../boards

# Uncomment this to enable scheduler statistics for ps:
#USEMODULE += schedstatistics


# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1


USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
#USEMODULE += shell
#USEMODULE += od
USEMODULE += netstats_l2 
USEMODULE += periph_uart
USEMODULE += gnrc_mac 
#USEMODULE += random 
#USEMODULE += gnrc_netdev 

FEATURES_REQUIRED = periph_uart
FEATURES_REQUIRED += periph_rtt
FEATURES_REQUIRED += periph_adc


BOARD_PROVIDES_NETIF := oflmote-dev

FEATURES_OPTIONAL += config
FEATURES_OPTIONAL += periph_rtc


include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
ifneq (,$(filter cc110x,$(USEMODULE)))          # radio is cc110x sub-GHz
  DEFAULT_CHANNEL ?= 0
  CFLAGS += -DCC110X_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
else
  ifneq (,$(filter at86rf212b,$(USEMODULE)))    # radio is IEEE 802.15.4 sub-GHz
    DEFAULT_CHANNEL ?= 5
    CFLAGS += -DIEEE802154_DEFAULT_SUBGHZ_CHANNEL=$(DEFAULT_CHANNEL)
  else                                          # radio is IEEE 802.15.4 2.4 GHz
    DEFAULT_CHANNEL ?= 26
    CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
  endif
endif


CFLAGS += -DNODE_ID=0x98
CFLAGS += -DDEVELHELP -g
MOTES_BUILD_VERSION := $(shell git log | head -n 1 | cut -d\  -f2 )
BUILD_DATE := $(shell date  +%G%m%d_%H%M%S )
CFLAGS += -DMOTESVERSION=\"git.$(MOTES_BUILD_VERSION).$(BUILD_DATE)\"
