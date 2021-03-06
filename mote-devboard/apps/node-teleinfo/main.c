/*
 * Copyright (C) 2018, Olivier Fauchon <ofauchon2204@gmail.com>

 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     nogroup
 * @{
 *
 * @file
 * @brief       802.15.4 BMX280 Temperature,pressure,humidity Sensor
 *
 * @author      Olivier Fauchon <ofauchon2204@gmail.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "periph_conf.h"

#include "pm_layered.h"

#include "thread.h"
#include "timex.h"
#include "xtimer.h"

#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/netopt.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/netif/hdr.h"
#include "net/netstats.h"

#include "ringbuffer.h"
#include "strings.h"

#include "periph/uart.h"
#include "periph/gpio.h"
#include "periph/hwrng.h"
#include "periph/adc.h"

#include "bmx280_params.h"
#include "bmx280.h"

#include "kw2xrf.h"

#include "teleinfo.h"

char buflog[255];

#define TELEINFO_LISTEN_SEC 5 
#define CYCLE_PAUSE_SEC 60
#define TX_BUFSIZE (128U)

#ifndef NODE_ID
#warn 'NODE_ID is undefined, defaulting to 0xFA'
#define NODE_ID 0xFA
#endif

static kernel_pid_t ifpid = 0;
static kernel_pid_t teleinfo_pid; 

static char tx_msg[TX_BUFSIZE];


/*
 * Send 802.16.4 frame 
 */
static int data_tx (char *data, char data_sz)
{
  //uint8_t flags = 0 | GNRC_NETIF_HDR_FLAGS_BROADCAST;
  uint8_t flags = 0x00;

  printf("data_tx: [%s]\n", data);
  if (data_sz>45){
    printf("data_tx : WARN ! Payload size > 45, TX maix FAIL !\n");
  }

  uint8_t src[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NODE_ID };
  uint8_t dst[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

  gnrc_pktsnip_t *pkt, *hdr;
  gnrc_netif_hdr_t *nethdr;

  // put packet together 
  pkt = gnrc_pktbuf_add (NULL, data, data_sz, GNRC_NETTYPE_UNDEF);
  if (pkt == NULL)
    {
      printf("error: packet buffer full\r\n");
      return 1;
    }

  hdr = gnrc_netif_hdr_build (src, sizeof (src), dst, sizeof (dst));
  if (hdr == NULL)
    {
      printf("error: packet buffer full\r\n");
      gnrc_pktbuf_release (pkt);
      return 1;
    }
  LL_PREPEND (pkt, hdr);
  nethdr = (gnrc_netif_hdr_t *)hdr->data;
  nethdr->flags = flags;

  // and send it 
  if (gnrc_netapi_send (ifpid, pkt) < 1)
    {
      printf("error: unable to send\r\n");
      gnrc_pktbuf_release (pkt);
      return 1;
    }
  return 1;
}


/*
 * Configure 802.15.4 networking
 */ 
static void net_config (void)
{
  printf ("main: net_config: Start\n");

  uint8_t out[GNRC_NETIF_L2ADDR_MAXLEN];
  int res;

  /* initialize Network */
  gnrc_netif_t *iff = NULL;
  iff = gnrc_netif_iter (iff);
  if (iff == NULL)
    {
      printf ("main: net_config: !!! ERROR : NO INTERFACE\n");
      return;
    }
  ifpid = iff->pid;

  printf ("main: net_config: Switch to chan 11\n");
  int16_t val = 11;
  res=gnrc_netapi_set (iff->pid, NETOPT_CHANNEL, 0, (int16_t *) & val, sizeof (int16_t));
  if (res<0){
    printf ("main: net_config: Can't switch to chan (err:%d)\n", res);
  }

  // Set PAN to 0xF00D
  printf ("main: net_config: Set pan to 0xF00D\n");
  val = 0xF00D;
  res=gnrc_netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));
  if (res<0){
    printf ("main: net_config: Can't set PAN ID(err:%d)\n", res);
  }

  // Set address
  printf ("main: net_config: Set addr to 00:...:99\n");
  char src[24];
  sprintf(src, "00:00:00:00:00:00:00:%02x", NODE_ID);

  size_t addr_len = gnrc_netif_addr_from_str (src, out);
  if (addr_len == 0)
    {
      printf ("main: net_config: Unable to parse address !!!\n");
    }
  else
    {
      gnrc_netapi_set (iff->pid, NETOPT_ADDRESS_LONG, 0, out, sizeof (out));
      res=gnrc_netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));
      if (res<0){
        printf ("main: net_config: Can't set Network address(err:%d)\n", res);
      }
    }

  // Set Max TX Power
  printf ("main: net_config: Set tx_power to max\n");
  val = KW2XDRF_OUTPUT_POWER_MAX;
  res=gnrc_netapi_set (iff->pid, NETOPT_TX_POWER, 0, (int16_t *) & val, sizeof (int16_t));
  if (res<0){
    printf ("main: net_config: Can't set PAN ID(err:%d)\n", res);
  }
}


/*
 * Use bandgap reference and ADC to measure board's voltage
 i2c operation dislikes LPM*/
int getBat(void){
  // Enable bandgap reference for battery
  PMC->REGSC |= 1;
  // Init ADC line 6
  adc_init(ADC_LINE(6));
  int sample = adc_sample(ADC_LINE(6), ADC_RES_10BIT);
  int mv = (int)(1024*1000/sample);
  PMC->REGSC &= (uint8_t) ~1;
  return mv;
}



void radio_on(void){
    printf("main: radio_on: Switch radio ON\n");
    netopt_state_t state;
    gnrc_netapi_get(ifpid, NETOPT_STATE, 0, &state, sizeof(state));

    if (state == NETOPT_STATE_OFF){
        KW2XDRF_GPIO->PSOR = (1 << KW2XDRF_RST_PIN);

        /* wait for modem IRQ_B interrupt request */
        while (KW2XDRF_GPIO->PDIR & (1 << KW2XDRF_IRQ_PIN)) {};

        // We resetted the modem with RST_PIN, so we use NETOPT RESET STATE to run kw2xrf initialisation again
        state = NETOPT_STATE_RESET;
        gnrc_netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
        // Now modem is resetted, we switch to IDLE STATE 
        state = NETOPT_STATE_IDLE;
        gnrc_netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
        // DISABLE ACKs 
        state = NETOPT_DISABLE;
        gnrc_netapi_set (ifpid, NETOPT_AUTOACK, 0, &state , sizeof (state));
        gnrc_netapi_set (ifpid, NETOPT_ACK_REQ, 0, &state , sizeof (state));

        // Set radio channel, power, pan ... etc 
        net_config();
    } else {
        printf ("main: radio_on: !!! radio was NOT in state NETOPT_STATE_OFF (state =%d)\n",state);
    }
}

void radio_off(void){
    netopt_state_t state;
    gnrc_netapi_get(ifpid, NETOPT_STATE, 0, &state, sizeof(state));

    if (state != NETOPT_STATE_OFF){
        state = NETOPT_STATE_OFF;
        gnrc_netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
    } else {
        printf ("radio_off: !!! radio was already off (NETOPT_STATE_OFF)\n");
    }
   KW2XDRF_GPIO->PCOR = (1 << KW2XDRF_RST_PIN);
}


int main (void)
{

  uint16_t loop_cntr=0; 

  /* Startup blink led */
  LED0_OFF; 
  LED1_OFF; 
  uint8_t cnt; 
  for (cnt=0; cnt<4; cnt++){
    LED0_TOGGLE;
    xtimer_usleep(100 * 1000); 
  }

  printf("\n*** OFlabs 802.15.4 OFLMote Sensor Teleinfo aa\n");
  printf("Riot Version: %s\n", RIOTVERSION);
  printf("Mote Version: %s\n", MOTESVERSION);
  printf("Build   Date: %s\n", BUILDDATE);

  net_config();
  teleinfo_pid = teleinfo_init(thread_getpid()); 

  // Thread message queue 
  msg_t msg_queue[8];
  msg_init_queue(msg_queue, 8);
  msg_t m;

  while (1)
    {
        printf("main : Start cycle\n");
        bzero(tx_msg, sizeof(tx_msg) );

        PM_BLOCK(KINETIS_PM_STOP); // Disable LowPower

        for (uint8_t c=1; c<3; c++){
          printf("main : Listening Teleinfo #%d\n", c);
          teleinfo_enable_rx(c);
          if (c==1)  {LED0_ON;}
          if (c==2)  {LED1_ON;}
          xtimer_sleep(TELEINFO_LISTEN_SEC);
          if (c==1)  {LED0_OFF;}
          if (c==2)  {LED1_OFF;}
          teleinfo_disable_rx();
          printf("main : Stop listening Teleinfo #%d\n", c);
        }


        // Send metrics
        radio_on();

        // Common metrics
        sprintf(tx_msg, "FWRV:01;BATL:%d;LOOP:%d", getBat(), loop_cntr );
        data_tx(tx_msg, strlen (tx_msg));
        xtimer_sleep(1);

        // Check if we have new messages.
        while (msg_try_receive(&m) == 1 ){
          if (m.sender_pid ==  teleinfo_pid) {
            printf("main: new msg from teleinfo %s\n", tx_msg);
            //printf("Message received in main from pid: %d: [%s]\n",  m.sender_pid, (char*) m.content.ptr);
            data_tx(m.content.ptr, strlen(m.content.ptr));
            xtimer_sleep(1);
            free(m.content.ptr);
          } 
        }



        printf("main : Hibernate\n");
        radio_off();
        /* Press any button to lock MCU in Run mode
           ... so we can connect with debugger */
        /*
        if (!gpio_read(BTN0_PIN) ||  !gpio_read(BTN1_PIN) ) {
            LED0_ON;
            LED1_ON;
            while(1){}
        }*/

        PM_UNBLOCK(KINETIS_PM_STOP); // Re-enable LPM 
        xtimer_sleep(CYCLE_PAUSE_SEC);

        loop_cntr++; 

    }
}

