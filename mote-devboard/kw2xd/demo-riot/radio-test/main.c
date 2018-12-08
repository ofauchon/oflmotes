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
 * @brief       802.15.4 Teleinfo Mote Sensor
 *
 * @author      Olivier Fauchon <ofauchon2204@gmail.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "periph_conf.h"

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

#define SWO 0

#if SWO
#include "swo.h"
char buflog[255];
  #define zprintf(args...) snprintf(buflog,sizeof(buflog),args); SWO_PrintString(buflog);
#else
#  define zprintf(args...) printf(args);
#endif

#define CYCLE_PAUSE_SEC 10
#define READ_DELAY 10
#define UART_BUFSIZE        (128U)

#ifndef NODE_ID
#warn 'NODE_ID is undefined, defaulting to 0xFA'
#define NODE_ID 0xFA
#endif



/* Threads Stuff */
#define BLINKER_PRIO        (THREAD_PRIORITY_MAIN - 2)
static kernel_pid_t blinker_pid;
static char blinker_stack[THREAD_STACKSIZE_MAIN];
static kernel_pid_t main_pid;
static kernel_pid_t ifpid = 0;


/* Sensor variables */
static bmx280_t bmx_dev;
static int16_t temperature;
static uint32_t pressure;
static uint16_t humidity;

static uint8_t loop_cntr; 

/*
 * 
 */
static void sensor_measure(void){
          /* Get temperature in centi degrees Celsius */
        temperature = bmx280_read_temperature(&bmx_dev);
        bool negative = (temperature < 0);
        if (negative) {
            temperature = -temperature;
        }

        /* Get pressure in Pa */
        pressure = bmx280_read_pressure(&bmx_dev);

        /* Get pressure in %rH */
        humidity = bme280_read_humidity(&bmx_dev);

}


/*
 * Send 802.16.4 frame 
 */
static int data_tx (char *data, char data_sz)
{
  zprintf("data_tx: %s\r\n", data);

  uint8_t src[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NODE_ID };
  uint8_t dst[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

  gnrc_pktsnip_t *pkt, *hdr;

  // put packet together 
  pkt = gnrc_pktbuf_add (NULL, data, data_sz, GNRC_NETTYPE_UNDEF);
  if (pkt == NULL)
    {
      puts ("error: packet buffer full");
      return 1;
    }

  hdr = gnrc_netif_hdr_build (src, sizeof (src), dst, sizeof (dst));
  if (hdr == NULL)
    {
      puts ("error: packet buffer full");
      gnrc_pktbuf_release (pkt);
      return 1;
    }
  LL_PREPEND (pkt, hdr);
  // and send it 
  if (gnrc_netapi_send (ifpid, pkt) < 1)
    {
      puts ("error: unable to send");
      gnrc_pktbuf_release (pkt);
      return 1;
    }
  return 1;
}


/*
 * Thread for led blinking 
 */
static void* blinker_thread (void *arg)
{
  (void) arg;
  msg_t msg;
  msg_t msg_queue[8];
  msg_init_queue (msg_queue, 8);
  while (1)
    {
      // 
      msg_receive (&msg);
      LED0_ON;
      xtimer_sleep(1);
      LED0_OFF;
      xtimer_sleep(1);
    }
  return NULL;
}



/*
 * Wrapper to gnrc_netapi_set RIOT-OS 
 */
static int netapi_set (kernel_pid_t pid, netopt_t opt, uint16_t context, void *data,
	    size_t data_len)
{
  int ret;
  ret = gnrc_netapi_set (pid, opt, context, data, data_len);
  if (ret < 0)
    {
      zprintf ("!: netapi_set: Can't set option\r\n");
    }
  return ret;
}



/*
 * HW Initialisation
 */ 
static void prepare_all (void)
{
  zprintf ("Start HW init\r\n");

  gpio_init(GPIO_PIN(PORT_E, 2), GPIO_OUT);
  gpio_init(GPIO_PIN(PORT_E, 3), GPIO_OUT);
  GPIOE->PCOR = 1 << 3 ;  // PE3 is GND
  GPIOE->PSOR = 1 << 2 ;  // PE2 is 3.3


  uint8_t out[GNRC_NETIF_L2ADDR_MAXLEN];

  /* initialize Network */
  gnrc_netif_t *iff = NULL;
  iff = gnrc_netif_iter (iff);
  if (iff == NULL)
    {
      zprintf ("ERROR : NO INTERFACE \r\n");
      return;
    }
  ifpid = iff->pid;

  zprintf ("Switch to chan 11\r\n");
  int16_t val = 11;
  netapi_set (iff->pid, NETOPT_CHANNEL, 0, (int16_t *) & val,
	      sizeof (int16_t));

  // Set PAN to 0xF00D
  zprintf ("Set pan to 0xF00D\r\n");
  val = 0xF00D;
  netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));

  // Set address
  zprintf ("Set addr to 00:...:99\r\n");
  char src[24];
  sprintf(src, "00:00:00:00:00:00:00:%02x", NODE_ID);

  size_t addr_len = gnrc_netif_addr_from_str (src, out);
  if (addr_len == 0)
    {
      zprintf ("!!! Unable to parse address !!!\r\n");
    }
  else
    {
      netapi_set (iff->pid, NETOPT_ADDRESS_LONG, 0, out, sizeof (out));
    }

  zprintf ("Radio Init Done\r\n");




  // Sensor Init
  int result = bmx280_init(&bmx_dev, &bmx280_params[0]);
  if (result == -1) {
      zprintf("[Error] The given i2c is not enabled\r\n");
  }

  if (result == -2) {
      zprintf("[Error] The sensor did not answer correctly at address 0x%02X\r\n", bmx280_params[0].i2c_addr);
  }

  zprintf ("I2C Init Done\r\n");


}

/*
 * Use bandgap reference and ADC to measure board's voltage
 */
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


int main (void)
{
    LED0_ON; 

#if SWO
    SWO_Init(0x01,  48000000);
#endif

  // INIT
  zprintf("\r\n*** OFlabs 802.15.4 OFLMote Sensor - Demo ***\r\n");
  zprintf("*** OFlMotes Release %s\r\n", MOTESVERSION);
  prepare_all();

  loop_cntr=0; 

  /* start the blinker thread */
  blinker_pid = thread_create (blinker_stack, sizeof (blinker_stack),
			       BLINKER_PRIO, 0,
			       blinker_thread, NULL, "blink_thread");

  main_pid=thread_getpid();

  // Blink 3 times at start
  int j;
  for (j=0;j<3; j++){
  msg_t msg;
  msg.content.value = 1;
  msg_send (&msg, blinker_pid);
  }

 // uart_poweroff(UART_DEV(0));

  char buffer[UART_BUFSIZE]; // Todo: move me 
  while (1)
    {
        uart_poweron(UART_DEV(0));
        zprintf("Resume for Hibernate\r\n");

        // Blink
        msg_t msg;
        msg.content.value = 1;
        msg_send (&msg, blinker_pid);

        //uart_poweron(UART_DEV(0));
        // Enable Radio for TX
        netopt_state_t state;
        state = NETOPT_STATE_TX;
        netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));

        // Measure sensor and transmit
        sensor_measure();
        sprintf(buffer, "DEVICE:DEV;VER=01;BATLEV:%d;TEMP:%c%02d.%02d;PRES:%04lu;HUMI:%02d.%02d", 
          getBat(),
          (temperature<0) ? '-' : '+',
          temperature/100,
          temperature % 100, 
          pressure/100,
          humidity/100, 
          humidity%100);
        data_tx (buffer, strlen (buffer));

        // Powersave (Disable Radio)
       // state = NETOPT_STATE_OFF;
       // netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
        zprintf("Hibernate\r\n");
        uart_poweroff(UART_DEV(0));

        xtimer_sleep(CYCLE_PAUSE_SEC);
        loop_cntr++; 

    }
}

