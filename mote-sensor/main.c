/*
 * Copyright (C) 2017, Olivier Fauchon <oflmotes@oflabs.com>

 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     nogroup
 * @{
 *
 * @file
 * @brief       Acts as a 802.15.4 to serial gateway
 *
 * @author      Olivier Fauchon <ofmotes@oflabs.com>
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
#include "net/gnrc/netreg.h"
#include "net/gnrc/netif/hdr.h"
#include "net/netstats.h" 

#include "periph/uart.h"

#define PAUSE 5

// Node ID 
#ifndef NODE_ID
#warning '** Using default NODE_NO=0x99'
#define NODE_ID 0x99
#endif



static kernel_pid_t ifpid=0;

//static int data_tx(kernel_pid_t dev, char* data, char data_sz);

/* Blink led thread */
char blink_thread_stack[THREAD_STACKSIZE_MAIN];

void *blink_thread(void *arg)
{
	(void) arg;
	while (1) {
		LED0_TOGGLE;
		LED1_TOGGLE;
		xtimer_sleep(PAUSE);
    }
    return NULL;
}


static void rx_cb(void *arg, uint8_t data)
{
	(void) arg;
	// 8th bit is in fact parity bit... drop it 
	data &=0x7F; 
	printf("%c", data); 
}




/*
 * Send 802.16.4 frame 
 */
static int data_tx(kernel_pid_t dev, char* data, char data_sz)
{

    uint8_t src[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NODE_ID };
    uint8_t dst[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    gnrc_pktsnip_t *pkt, *hdr;

    // put packet together 
    pkt = gnrc_pktbuf_add(NULL, data, data_sz , GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        puts("error: packet buffer full");
        return 1;
    }

    hdr = gnrc_netif_hdr_build(src, sizeof(src), dst, sizeof(dst) );
    if (hdr == NULL) {
        puts("error: packet buffer full");
        gnrc_pktbuf_release(pkt);
        return 1;
    }
    LL_PREPEND(pkt, hdr);
    // and send it 
    if (gnrc_netapi_send(dev, pkt) < 1) {
        puts("error: unable to send");
        gnrc_pktbuf_release(pkt);
        return 1;
    }
return 1; 
}

/*
 * Wrapper to gnrc_netapi_set RIOT-OS 
 * 
 */
static int netapi_set (kernel_pid_t pid, netopt_t opt, uint16_t context, void *data, size_t data_len){
	int ret; 
    ret=gnrc_netapi_set(pid, opt, context, data, data_len);
    if (ret<0) {
		printf("!: netapi_set: Can't set option\n"); 
	} else {
		printf("#: netapi_set: Success.\n"); 
	}
	return ret;
}


static void prepare_all(void)
{
	 uint8_t out[GNRC_NETIF_L2ADDR_MAXLEN];

	printf("Start HW init\r\n");

	/* initialize UART */
	int res; 
	int dev=1; 
	res = uart_init(UART_DEV(1), 1200 , rx_cb, (void *)dev);
	if (res != UART_OK) {
		puts("Error: Unable to initialize UART device\n");
		return;
	}
	printf("Successfully initialized UART_DEV(%i)\n", dev);

	
	/* initialize Network */
	gnrc_netif_t* iff=NULL;
	iff=gnrc_netif_iter(iff); 
	if (iff== NULL) {
		printf("ERROR : NO INTERFACE \r\n");
		return; 
	}
	ifpid=iff->pid;

	printf("Switch to chan 11\r\n"); 
    	int16_t val = 11;
    	netapi_set( iff->pid, NETOPT_CHANNEL, 0, (int16_t *)&val, sizeof(int16_t));

	// Set PAN to 0xF00D
	printf("Set pan to 0xF00D\r\n"); 
	val=0xF00D;
    	netapi_set(iff->pid, NETOPT_NID, 0, (int16_t *)&val, sizeof(int16_t));

	// Set address
	printf("Set addr to 00:...:99\r\n"); 
        char src[24];
	sprintf(src,"00:00:00:00:00:00:00:%02x", NODE_ID);

	size_t addr_len = gnrc_netif_addr_from_str(src, out);
	if (addr_len == 0) {
		printf("!!! Unable to parse address !!!\r\n");
	} else 
	{
		printf("!!! addr len %d !!!\r\n", addr_len);
    		netapi_set(iff->pid, NETOPT_ADDRESS_LONG, 0, out, sizeof(out) );
	}

	printf("Done HW init\r\n");
}


int main(void)
{

    // INIT
    LED0_ON;
    LED1_ON;
    puts("\r\n*** OFlabs 802.15.4 Teleinfo mote ***\r\n");
    prepare_all();
    data_tx(ifpid, "Hello", 5);
 
    printf("Start thread\r\n"); 
    // Blink thread 
    thread_create(blink_thread_stack, sizeof(blink_thread_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  blink_thread, NULL, "blink_thread");


    char buf[128];
    uint16_t loopcount=0; 
    while (1){

		if (ifpid) {
			printf("TX Data to pid %d\r\n", ifpid);
			sprintf(buf, "FWVER:9999;CAPA:0001;BATLEV:3210;AWAKE_SEC:0007;MAIN_LOOP:%d", loopcount);
    data_tx(ifpid, "Hello", 5);
	//		data_tx(ifpid, buf,strlen(buf));
		}
		xtimer_sleep(PAUSE);
    }
}
