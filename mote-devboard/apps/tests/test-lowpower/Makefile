# name of your application
APPLICATION = test-lowpower

# If no BOARD is found in the environment, use this default:
BOARD = oflmote-dev

# This has to be the absolute path to the RIOT base directory:
RIOTBASE= /home/olivier/dev/RIOT.benemorius
RIOTBOARD=../../../boards



USEMODULE += xtimer
USEMODULE += periph_uart


#USEMODULE += gnrc_netdev_default
#USEMODULE += auto_init_gnrc_netif




# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1


OPENOCD_ADAPTER_INIT ?= -c 'source ../../openocd/ftdi.cfg'  -c 'transport select swd'
#OPENOCD_EXTRA_INIT ?= "-d 3"

include $(RIOTBASE)/Makefile.include

picocom:   
	picocom /dev/ttyUSB0 --baud 115200  --imap crcrlf,lfcrlf

picocom9600:   
	picocom /dev/ttyUSB0 --baud 9600  --imap crcrlf,lfcrlf

