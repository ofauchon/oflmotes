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


#include "./pktdump.h"

#define PAUSE 2


/* This was my first 'hello world' thread (led blinking) */
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

/*
 * Send 802.16.4 frame 
 */
/*
static int data_tx(kernel_pid_t dev, char* data, char data_sz)
{
    gnrc_pktsnip_t *pkt, *hdr;
    gnrc_netif_hdr_t *nethdr;
    uint8_t flags = 0x00;
    uint8_t addr[IEEE802154_LONG_ADDRESS_LEN];
    size_t addr_len = IEEE802154_LONG_ADDRESS_LEN;

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
*/

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




/* 
 *  Command : help
 */ 
static int cmd_help(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Available commands:\r\nnet\t Network interface\r\nreboot\tReboot board\r\nabout\tAbout information\r\n");
    return 0;
}

/* 
 *  Command : reboot
 */ 
static int cmd_reboot(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Reboot not yet implemented\r\n");
    //pm_reboot(); 
    return 0;
}

/* 
 *  Command : net
 */ 
static int cmd_net(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    size_t numof = gnrc_netif_numof();

    gnrc_netif_t* iff=NULL;
    iff=gnrc_netif_iter(iff); 
    if (iff== NULL) {
	printf("ERROR : NO INTERFACE \r\n");
	return 0; 
	}

    if (argc==2 && strcmp(argv[1], "info")==0 ){
        printf("Network info \r\n");
		// Initialize network 
		printf("Number of interfaces: %d\r\n", numof); 


		netstats_t *stats;
		int	res = gnrc_netapi_get(iff->pid, NETOPT_STATS, 0, &stats, sizeof(&stats));
		if (res>=0){
	        printf("           Statistics \r\n"
               "            RX packets %u  bytes %u\r\n"
               "            TX packets %u (Multicast: %u)  bytes %u\r\n"
               "            TX succeeded %u errors %u\r\n",
               (unsigned) stats->rx_count,
               (unsigned) stats->rx_bytes,
               (unsigned) (stats->tx_unicast_count + stats->tx_mcast_count),
               (unsigned) stats->tx_mcast_count,
               (unsigned) stats->tx_bytes,
               (unsigned) stats->tx_success,
	        (unsigned) stats->tx_failed);
		return 0; 
    		}
	}
    else if (argc==3 && strcmp(argv[1], "chan")==0 ){
		printf("Set radio channel to '%s'\r\n", argv[2]);
    	int16_t val = atoi(argv[2]);
    	gnrc_netapi_set( iff->pid, NETOPT_CHANNEL, 0, (int16_t *)&val, sizeof(int16_t));
		return 0; 
	}
    else if (argc==3 && strcmp(argv[1], "pan")==0 ){
		printf("Set pan id to '%s'\r\n", argv[2]);
    	int16_t val = atoi(argv[2]);
		printf(" val is %d\r\n ", val);
    	netapi_set(iff->pid, NETOPT_NID, 0, (int16_t *)&val, sizeof(int16_t));
		return 0; 
	}
    else if (argc==3 && strcmp(argv[1], "addr")==0 ){
		printf("Set long addr to '%s'\r\n", argv[2]);
    	uint8_t addr[IEEE802154_LONG_ADDRESS_LEN];
    	size_t addr_len = gnrc_netif_addr_from_str(argv[2], addr);
    	if (addr_len == 0) {
        	printf("error: unable to parse address.\n");
        	return 1;
    	}
    	netapi_set(iff->pid, NETOPT_ADDRESS_LONG, 0, addr, addr_len);
		return 0;
	}
    else if (argc==2 && strcmp(argv[1], "promisc")==0 ){
		netopt_enable_t fl=NETOPT_ENABLE;
		netapi_set(iff->pid, NETOPT_PROMISCUOUSMODE, 0, &fl, sizeof(fl));
		netapi_set(iff->pid, NETOPT_RAWMODE, 0, &fl, sizeof(fl));
		return 0; 
	}
    else if (argc==2 && strcmp(argv[1], "pktdump")==0 ){
		gnrc_pktdump_init();
		return 0; 
	}
	else {
		printf(		"net info									Show interface details\n"
					"net send									Send sample 'BEEF' frame\n"
					"net addr XX:XX:XX:XX:XX:XX:XX:XX			Set 802.15.4 addr\n"
					"net pan 0xXXXX								Set pan id to 0xXXXX\n"
					"net chan XX								Set radio chan XX (11<XX<26)\n"
					"net promisc								Enable Promisciuous and raw options\n"
					"net pktdump 								Dump packets\n");
		return 0;
	}

    return 0;
}

// Some tests
// 
// 
/*
static int cmd_rand(int argc, char **argv)
{
    (void) argc;
    (void) argv;
	uint8_t data[4]; 
	memset(data,0,4);

	hwrng_read(data,4); 
	printf("Random values %02X %02X %02X %02X \r\n", data[0], data[1], data[2], data[3]);
	return 0; 
}
*/
static int cmd_about(int argc, char **argv)
{
	(void) argc;
	(void) argv; 
	printf("OFLmotes gateway - Olivier Fauchon\r\n");
	printf("Build %s \r\n", BUILDVERSION);
	return 0;
}


static const shell_command_t shell_commands[] = {   
    { "?", "cmd help", cmd_help },                                                  
    { "help", "cmd help", cmd_help },                                                  
    { "net", "network commands", cmd_net },   
//    { "rand", "get random", cmd_rand },   
    { "reboot", "reboot system",cmd_reboot },   
    { "about", "about informations",cmd_about },   
    { NULL, NULL, NULL }                            
}; 


int main(void)
{

    // INIT
    LED0_ON;
    LED1_ON;
    puts("\r\n*** OFlabs 802.15.4 Gateway ***\r\n");

    // Blink thread 
    thread_create(blink_thread_stack, sizeof(blink_thread_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  blink_thread, NULL, "blink_thread");


    /* define buffer to be used by the shell */
    puts("Starting shell\r\n");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
}
