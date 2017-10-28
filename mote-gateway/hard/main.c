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
#define MAX_ADDR_LEN            (8U)


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

static int data_tx(kernel_pid_t dev, char* data, char data_sz)
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


static int netif_set_flag(kernel_pid_t dev, netopt_t opt, netopt_enable_t set)
{
    if (gnrc_netapi_set(dev, opt, 0, &set, sizeof(netopt_enable_t)) < 0) {
        puts("error: unable to set option\r\n");
        return 1;
    }
 //   printf("success: %sset option\r\n", (set) ? "" : "un");
    return 0;
}

static int netif_set_i16(kernel_pid_t dev, netopt_t opt, char *i16_str)
{
    int16_t val = atoi(i16_str);

    if (gnrc_netapi_set(dev, opt, 0, (int16_t *)&val, sizeof(int16_t)) < 0) {
        printf("error: unable to set option with value str:'%s' decimal:%d", i16_str, val);
        puts("");
        return 1;
    }
    return 0;
}

static int netif_set_u16(kernel_pid_t dev, netopt_t opt, char *u16_str) 
{
    unsigned int res;
    bool hex = false;

    if ((res = strtoul(u16_str, NULL, 16)) == ULONG_MAX) {
        puts("error: unable to parse value.\n"
             "Must be a 16-bit unsigned integer (dec or hex)\n");
        return 1;
    }

    if (gnrc_netapi_set(dev, opt, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        printf("Error a1\n");
        return 1;
    }

    printf("success set option '%s'\n",u16_str);
    if (hex) {
        printf("0x%04x\n", res);
    }

    return 0;
}

static int netif_set_addr(kernel_pid_t dev, netopt_t opt, char *addr_str)
{
    uint8_t addr[MAX_ADDR_LEN];
    size_t addr_len = gnrc_netif_addr_from_str(addr, sizeof(addr), addr_str);

    if (addr_len == 0) {
        printf("error: unable to parse address.\n");
        return 1;
    }

    if (gnrc_netapi_set(dev, opt, 0, addr, addr_len) < 0) {
        printf("_netif_set_addr: error: unable to set option");
        return 1;
    }

    printf("success: set addr\n");

    return 0;
}















static int cmd_help(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Available commands:\r\nnet\t Network interface\r\nreboot\r\nReboot board\r\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Reboot not yet implemented\r\n");
    //pm_reboot(); 
    return 0;
}

// Network commands 
static int cmd_net(int argc, char **argv)
{
    (void) argc;
    (void) argv;
	kernel_pid_t ifs[GNRC_NETIF_NUMOF];
	size_t numof = gnrc_netif_get(ifs);

    if (argc==2 && strcmp(argv[1], "help")==0 ){
		printf("net info\t display network informations\r\nnet send\t send test BEEF frame\r\nnet addr\tSet 802.15.4 addr\r\nnet pan\tSet pan id\r\nnet chan XX\t Set radio chan XX (11<XX<26)\r\nnet mon\t Switch to promiscuous and dump reveived packets\r\n");
		return 0;
	}

    if (argc==2 && strcmp(argv[1], "info")==0 ){
        printf("Network info \r\n");
		// Initialize network 
		printf("Number of interfaces: %d\r\n", numof); 
		netstats_t *stats;
		int	res = gnrc_netapi_get(ifs[0], NETOPT_STATS, 0, &stats, sizeof(&stats));
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
		}
    }

    if (argc==2 && strcmp(argv[1], "send")==0 ){
		printf("Send B E E F\r\n");
		char raw_data[4] = {'B', 'E', 'E', 'F'};
		data_tx(ifs[0], raw_data, 4);
	}
    if (argc==3 && strcmp(argv[1], "chan")==0 ){
		printf("Set radio channel to '%s'\r\n", argv[2]);
		netif_set_i16(ifs[0], NETOPT_CHANNEL, argv[2]);
	}
    if (argc==3 && strcmp(argv[1], "pan")==0 ){
		printf("Set panId to '%s'\r\n", argv[2]);
		netif_set_i16(ifs[0], NETOPT_NID, argv[2]);
	}
    if (argc==3 && strcmp(argv[1], "addr")==0 ){
		printf("Set long addr to '%s'\r\n", argv[2]);
		netif_set_addr(ifs[0], NETOPT_ADDRESS_LONG, argv[2]);
	}
    if (argc==2 && strcmp(argv[1], "mon")==0 ){
		printf("Enable monitor .... RAW + PROMISC + CHANNEL 11\r\n");
		netif_set_u16(ifs[0], NETOPT_NID, "0xF00D");
		netif_set_addr(ifs[0], NETOPT_ADDRESS_LONG, "00:00:00:00:00:00:00:01");
		netif_set_flag(ifs[0], NETOPT_RAWMODE, NETOPT_ENABLE);
		netif_set_flag(ifs[0], NETOPT_PROMISCUOUSMODE, NETOPT_ENABLE);
		gnrc_pktdump_init();

	}

    return 0;
}

// Some tests
static int cmd_rand(int argc, char **argv)
{
	uint8_t data[4]; 
	memset(data,0,4);

	hwrng_read(data,4); 
    printf("Random values %02X %02X %02X %02X \r\n", data[0], data[1], data[2], data[3]);
	return 0; 
}


static const shell_command_t shell_commands[] = {   
    { "?", "cmd help", cmd_help },                                                  
    { "help", "cmd help", cmd_help },                                                  
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
