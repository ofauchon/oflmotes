/*
 * Copyright (C) 2008, 2009, 2010  Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2013 Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Application that make led blink with a thread
 *
 * @author      Olivier Fauchon <ofauchon2204@gmail.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>


#include "periph/hwrng.h"                           


#include "thread.h"
#include "timex.h"
#include "xtimer.h"

#include "shell_commands.h"
#include "shell.h"

#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/netopt.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif/hdr.h"

#include "./pktdump.h"

#define PAUSE 2
#define MAX_ADDR_LEN            (8U)

char blink_thread_stack[THREAD_STACKSIZE_MAIN];

void *blink_thread(void *arg)
{
    (void) arg;
    while (1) {
		//printf("Tick\r\n");
		LED0_TOGGLE;
		LED1_TOGGLE;
        xtimer_sleep(PAUSE);
    }
    return NULL;
}

static int _send(kernel_pid_t dev, char* data, char data_sz)
{
    gnrc_pktsnip_t *pkt, *hdr;
    gnrc_netif_hdr_t *nethdr;
    uint8_t flags = 0x00;
    uint8_t addr[MAX_ADDR_LEN];
    size_t addr_len = MAX_ADDR_LEN;


    flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST; 

    // put packet together 
    pkt = gnrc_pktbuf_add(NULL, data, data_sz , GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        puts("error: packet buffer full");
        return 1;
    }
    hdr = gnrc_netif_hdr_build(NULL, 0, addr, addr_len);
    if (hdr == NULL) {
        puts("error: packet buffer full");
        gnrc_pktbuf_release(pkt);
        return 1;
    }
    LL_PREPEND(pkt, hdr);
    nethdr = (gnrc_netif_hdr_t *)hdr->data;
    nethdr->flags = flags;
    // and send it 
    if (gnrc_netapi_send(dev, pkt) < 1) {
        puts("error: unable to send");
        gnrc_pktbuf_release(pkt);
        return 1;
    }
return 1; 
}


static int _netif_set_flag(kernel_pid_t dev, netopt_t opt, netopt_enable_t set)
{
    if (gnrc_netapi_set(dev, opt, 0, &set, sizeof(netopt_enable_t)) < 0) {
        puts("error: unable to set option\r\n");
        return 1;
    }
    printf("success: %sset option\r\n", (set) ? "" : "un");
    return 0;
}

static int _netif_set_i16(kernel_pid_t dev, netopt_t opt, char *i16_str)
{
    int16_t val = atoi(i16_str);

    if (gnrc_netapi_set(dev, opt, 0, (int16_t *)&val, sizeof(int16_t)) < 0) {
        printf("error: unable to set option with value str:'%s' decimal:%d", i16_str, val);
        puts("");
        return 1;
    }
    return 0;
}



static int print_help(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("This is help\r\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Rebooting \r\n");
    return 0;
}

static int cmd_net(int argc, char **argv)
{
    (void) argc;
    (void) argv;
	kernel_pid_t ifs[GNRC_NETIF_NUMOF];
	size_t numof = gnrc_netif_get(ifs);

    if (argc==2 && strcmp(argv[1], "help")==0 ){
		printf("net info\t display network informations\r\nnet send\t send test BEEF frame\r\nnet chan XX\tSet radio chan XX (11<XX<26)\r\n");
		return 0;
	}
    if (argc==2 && strcmp(argv[1], "info")==0 ){
        printf("Network info \r\n");
		// Initialize network 
		printf("Number of interfaces: %d\r\n", numof); 

    }
    if (argc==2 && strcmp(argv[1], "send")==0 ){
		printf("Send B E E F\r\n");
		char raw_data[4] = {'B', 'E', 'E', 'F'};
		_send(ifs[0], raw_data, 4);
	}
    if (argc==3 && strcmp(argv[1], "chan")==0 ){
		printf("Set radio channel to '%s'\r\n", argv[2]);
		_netif_set_i16(ifs[0], NETOPT_CHANNEL, argv[2]);
	}
    if (argc==2 && strcmp(argv[1], "monitor")==0 ){
		printf("Enable monitor\r\n");
		//kernel_pid_t dump_pid=0;
		_netif_set_flag(ifs[0], NETOPT_RAWMODE, NETOPT_ENABLE);
		_netif_set_flag(ifs[0], NETOPT_PROMISCUOUSMODE, NETOPT_ENABLE);
		gnrc_pktdump_init();

	}

    return 0;
}

static int cmd_rand(int argc, char **argv)
{
	uint8_t data[4]; 
	memset(data,0,4);

	hwrng_read(data,4); 
    printf("Random values %02X %02X %02X %02X \r\n", data[0], data[1], data[2], data[3]);
	return 0; 
}


static const shell_command_t shell_commands[] = {   
    { "help", "print help", print_help },                                                  
    { "net", "network commands",cmd_net },   
    { "rand", "get random",cmd_rand },   
    { "reboot", "reboot system",cmd_reboot },   
    { NULL, NULL, NULL }                            
}; 


int main(void)
{

    // INIT
    LED0_ON;
    LED1_ON;
    puts("\r\n*** OFlabs 802.15.4 ***\r\n");

    // Blink thread 
    thread_create(blink_thread_stack, sizeof(blink_thread_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  blink_thread, NULL, "blink_thread");


    /* define buffer to be used by the shell */
    puts("Starting shell\r\n");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
}
