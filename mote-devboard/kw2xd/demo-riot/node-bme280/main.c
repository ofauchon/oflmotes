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


char buflog[255];

#define CYCLE_PAUSE_SEC 60
#define TX_BUFSIZE (128U)

#ifndef NODE_ID
#warn 'NODE_ID is undefined, defaulting to 0xFA'
#define NODE_ID 0xFA
#endif

static kernel_pid_t ifpid = 0;

/* Sensor variables */
static bmx280_t bmx_dev;
static int16_t temperature;
static uint32_t pressure;
static uint16_t humidity;

static char tx_msg[TX_BUFSIZE];

static uint8_t cnf_enable_pm_radiosleep=1;
static uint8_t cnf_enable_pm_mcusleep=1;


/*
 * Run measurements
 */
static void sensor_measure(void){
    printf("sensor_measure: Starting measures\n");

    /* Get temperature in centi degrees Celsius */
    temperature = bmx280_read_temperature(&bmx_dev);
    bool negative = (temperature < 0);
    if (negative) {
        temperature = -temperature;
    }

    if (cnf_enable_pm_mcusleep) {
        //i2c not working in Low Power Modes
        pm_block(KINETIS_PM_STOP);
    }

    /* Get pressure in Pa */
    pressure = bmx280_read_pressure(&bmx_dev);

    /* Get pressure in %rH */
    humidity = bme280_read_humidity(&bmx_dev);

    if (cnf_enable_pm_mcusleep) {
        // Restore LPM
        pm_unblock(KINETIS_PM_STOP);
    }

    printf("sensor_measure : Sensor measure Done\n");
}


/*
 * Send 802.16.4 frame 
 */
static int data_tx (char *data, char data_sz)
{
  printf("data_tx: %s\r\n", data);

  uint8_t src[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NODE_ID };
  uint8_t dst[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

  gnrc_pktsnip_t *pkt, *hdr;

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
 * HW Initialisation
 */ 
static void hw_init (void)
{
  printf ("hw_init: Start\n");

  uint8_t out[GNRC_NETIF_L2ADDR_MAXLEN];
  int res;

  /* initialize Network */
  gnrc_netif_t *iff = NULL;
  iff = gnrc_netif_iter (iff);
  if (iff == NULL)
    {
      printf ("hw_init: ERROR : NO INTERFACE\n");
      return;
    }
  ifpid = iff->pid;

  printf ("hw_init: Switch to chan 11\n");
  int16_t val = 11;
  res=gnrc_netapi_set (iff->pid, NETOPT_CHANNEL, 0, (int16_t *) & val, sizeof (int16_t));
  if (res<0){
    printf ("hw_init: Can't switch to chan (err:%d)\n", res);
  }

  // Set PAN to 0xF00D
  printf ("hw_init: Set pan to 0xF00D\n");
  val = 0xF00D;
  res=gnrc_netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));
  if (res<0){
    printf ("hw_init: Can't set PAN ID(err:%d)\n", res);
  }

  // Set address
  printf ("hw_init: Set addr to 00:...:99\n");
  char src[24];
  sprintf(src, "00:00:00:00:00:00:00:%02x", NODE_ID);

  size_t addr_len = gnrc_netif_addr_from_str (src, out);
  if (addr_len == 0)
    {
      printf ("hw_init: Unable to parse address !!!\n");
    }
  else
    {
      gnrc_netapi_set (iff->pid, NETOPT_ADDRESS_LONG, 0, out, sizeof (out));
      res=gnrc_netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));
      if (res<0){
        printf ("hw_init: Can't set Network address(err:%d)\n", res);
      }
    }

  if (cnf_enable_pm_mcusleep) {
     //i2c not working in LPM
      pm_block(KINETIS_PM_STOP);
  }

  // Sensor Init
  // Right now, I'm not sure i2c transactions are working under LLS power mode
  int result = bmx280_init(&bmx_dev, &bmx280_params[0]);
  if (result == -1) {
      printf("hw_init: ERROR: The given i2c is not enabled\r\n");
  }

  if (result == -2) {
      printf("hw_init: ERROR:  The sensor did not answer correctly at address 0x%02X\r\n", bmx280_params[0].i2c_addr);
  }

  if (cnf_enable_pm_mcusleep) {
     //Restore LPM
      pm_unblock(KINETIS_PM_STOP);
  }

  printf ("hw_init: I2C Init Done\n");
  printf ("hw_init: All Done\n");

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
    netopt_state_t state;
    gnrc_netapi_get(ifpid, NETOPT_STATE, 0, &state, sizeof(state));

    if (state == NETOPT_STATE_OFF){
        state = NETOPT_STATE_IDLE;
        gnrc_netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
    } else {
        printf ("radio_on: !!! radio was NOT OFF (state =%d)\n",state);
    }
 //   KW2XDRF_GPIO->PSOR = (1 << KW2XDRF_RST_PIN);
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
  //  KW2XDRF_GPIO->PCOR = (1 << KW2XDRF_RST_PIN);
}


int main (void)
{

  uint16_t loop_cntr=0; 
  LED0_OFF; 
  LED1_OFF; 

  printf("\n*** OFlabs 802.15.4 OFLMote Sensor\n");
  printf("Riot Version: %s\n", RIOTVERSION);
  printf("Mote Version: %s\n", MOTESVERSION);
  printf("Build   Date: %s\n", BUILDDATE);
  printf("Powersave mcu sleep: %d\n", cnf_enable_pm_mcusleep);
  printf("Powersave radio sleep: %d\n", cnf_enable_pm_radiosleep);

  // This will definitly disable LPM as pm_layered lock counter 
  // for state (KINETIS_PM_STOP) will never reach zero.
  if (cnf_enable_pm_mcusleep) {
    pm_block(KINETIS_PM_STOP);
  }

  hw_init();

  while (1)
    {
        printf("main : Start cycle\n");

        // 
        uint8_t cnt; 
        for (cnt=0; cnt<4; cnt++){
            xtimer_usleep(200 * 1000); 
            LED0_TOGGLE;
        }

        sensor_measure();

        if (cnf_enable_pm_radiosleep) {
            radio_on();
        }

        sprintf(tx_msg, "DEVICE:DEV;VER:01;BATLEV:%d;TEMP:%c%02d.%02d;PRES:%04lu;HUMI:%02d.%02d", 
          getBat(),
          (temperature<0) ? '-' : '+',
          temperature/100,
          temperature % 100, 
          pressure/100,
          humidity/100, 
          humidity%100);

        data_tx (tx_msg, strlen (tx_msg));

        printf("main : Radio OFF and Hibernate\n");

        if (cnf_enable_pm_radiosleep) {
            radio_off();
        }

        xtimer_sleep(CYCLE_PAUSE_SEC);

        loop_cntr++; 

    }
}

