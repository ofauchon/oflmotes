/*
 * Copyright (C) 2017, Olivier Fauchon <ofauchon2204@gmail.com>

 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     nogroup
 * @{
 *
 * @file
 * @brief       802.15.4 Sensor
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
#include "net/gnrc/netreg.h"
#include "net/gnrc/netif/hdr.h"
#include "net/netstats.h"

#include "ringbuffer.h"
#include "strings.h"

#include "periph/uart.h"

#define PAUSE 60
#define UART_BUFSIZE        (128U)

// Node ID 
#ifndef NODE_ID
#warning '** Using default NODE_NO=0x99'
#define NODE_ID 0x99
#endif

typedef struct
{
  char rx_mem[UART_BUFSIZE];
  ringbuffer_t rx_buf;
} uart_ctx_t;
static uart_ctx_t ctx[UART_NUMOF];

typedef struct
{
  uint16_t papp;
  uint16_t iinst;
  uint32_t base;
} results_t;
static results_t results;

static kernel_pid_t ifpid = 0;

/* Blink led thread */
#define BLINKER_PRIO        (THREAD_PRIORITY_MAIN - 1)
static kernel_pid_t blinker_pid;
static char blinker_stack[THREAD_STACKSIZE_MAIN];

/* Processor thread */
#define PROCESSOR_PRIO        (THREAD_PRIORITY_MAIN - 1)
static kernel_pid_t processor_pid;
static char processor_stack[THREAD_STACKSIZE_MAIN];

/*
 * Send 802.16.4 frame 
 */
static int
data_tx (char *data, char data_sz)
{

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


static void *
blinker_thread (void *arg)
{
  (void) arg;
  while (1)
    {
      LED0_TOGGLE;
      LED1_TOGGLE;
      xtimer_sleep (1);
    }
  return NULL;
}


static void *
processor_thread (void *arg)
{
  (void) arg;
  msg_t msg;
  msg_t msg_queue[8];
  msg_init_queue (msg_queue, 8);
  char buffer[UART_BUFSIZE];

  while (1)
    {
      msg_receive (&msg);
      uart_t dev = (uart_t) msg.content.value;
      bzero (buffer, sizeof (buffer));
      ringbuffer_get (&(ctx[dev].rx_buf), buffer, UART_BUFSIZE);

      //'BASE 014038982 .'
      if (strncmp (buffer, "BASE", 4) == 0 && strlen (buffer) >= 14)
	{
	  buffer[14] = 0;
	  results.base = atoi (buffer + 5);
	}
      //'PAPP 02340 *'
      if (strncmp (buffer, "PAPP", 4) == 0 && strlen (buffer) >= 10)
	{
	  //      printf("PAPP: %s", buffer);
	  buffer[10] = 0;
	  results.papp = atoi (buffer + 5);
	}
      //'IINST 010 X'
      if (strncmp (buffer, "IINST", 5) == 0 && strlen (buffer) >= 9)
	{
	  buffer[9] = 0;
	  results.iinst = atoi (buffer + 6);
	}

      if (results.papp > 0 && results.iinst > 0 && results.base > 0)
	{
	  bzero (buffer, sizeof (buffer));
	  sprintf (buffer, "PAPP:%i;BASE:%li;IINST:%i", results.papp,
		   results.base, results.iinst);
	  printf ("=> %s \r\n", buffer);
	  data_tx (buffer, strlen (buffer));

	  results.papp = 0;
	  results.base = 0;
	  results.iinst = 0;

	  // Stop serial port
	  uart_poweroff (UART_DEV (1));

	}
    }

  /* this should never be reached */
  return NULL;
}


// RX Callback
static void
rx_cb (void *arg, uint8_t data)
{
  uart_t dev = (uart_t) arg;
  data &= 0x7F;

  if (data == '\n')
    {
      msg_t msg;
      msg.content.value = (uint32_t) dev;
      msg_send (&msg, processor_pid);
    }
  else if (data >= ' ')
    {
      ringbuffer_add_one (&(ctx[dev].rx_buf), data);
    }
}


/*
 * Wrapper to gnrc_netapi_set RIOT-OS 
 * 
 */
static int
netapi_set (kernel_pid_t pid, netopt_t opt, uint16_t context, void *data,
	    size_t data_len)
{
  int ret;
  ret = gnrc_netapi_set (pid, opt, context, data, data_len);
  if (ret < 0)
    {
      printf ("!: netapi_set: Can't set option\n");
    }
  else
    {
      printf ("#: netapi_set: Success.\n");
    }
  return ret;
}


static void
prepare_all (void)
{
  uint8_t out[GNRC_NETIF_L2ADDR_MAXLEN];

  printf ("Start HW init\r\n");

  /* initialize UART */
  /* initialize ringbuffers */
  for (unsigned i = 0; i < UART_NUMOF; i++)
    {
      ringbuffer_init (&(ctx[i].rx_buf), ctx[i].rx_mem, UART_BUFSIZE);
    }

  int res;
  int dev = 1;
  res = uart_init (UART_DEV (1), 1200, rx_cb, (void *) dev);
  if (res != UART_OK)
    {
      puts ("Error: Unable to initialize UART device\n");
      return;
    }
  printf ("Successfully initialized UART_DEV(%i)\n", dev);


  /* initialize Network */
  gnrc_netif_t *iff = NULL;
  iff = gnrc_netif_iter (iff);
  if (iff == NULL)
    {
      printf ("ERROR : NO INTERFACE \r\n");
      return;
    }
  ifpid = iff->pid;

  printf ("Switch to chan 11\r\n");
  int16_t val = 11;
  netapi_set (iff->pid, NETOPT_CHANNEL, 0, (int16_t *) & val,
	      sizeof (int16_t));

  // Set PAN to 0xF00D
  printf ("Set pan to 0xF00D\r\n");
  val = 0xF00D;
  netapi_set (iff->pid, NETOPT_NID, 0, (int16_t *) & val, sizeof (int16_t));

  // Set address
  printf ("Set addr to 00:...:99\r\n");
  char src[24];
  sprintf (src, "00:00:00:00:00:00:00:%02x", NODE_ID);

  size_t addr_len = gnrc_netif_addr_from_str (src, out);
  if (addr_len == 0)
    {
      printf ("!!! Unable to parse address !!!\r\n");
    }
  else
    {
      printf ("!!! addr len %d !!!\r\n", addr_len);
      netapi_set (iff->pid, NETOPT_ADDRESS_LONG, 0, out, sizeof (out));
    }

  printf ("Done HW init\r\n");
}


int
main (void)
{

  // INIT
  printf("*** OFlabs 802.15.4 OFLMote Sensor - Teleinfo ***\r\n");
  printf("*** Release %s\r\n", DBUILDVERSION)
  prepare_all ();

  results.base = 0;
  results.iinst = 0;
  results.papp = 0;

  /* start the blinker thread */
  blinker_pid = thread_create (blinker_stack, sizeof (blinker_stack),
			       BLINKER_PRIO, 0,
			       blinker_thread, NULL, "blink_thread");

  /* start the processor thread */
  processor_pid = thread_create (processor_stack, sizeof (processor_stack),
				 PROCESSOR_PRIO, 0,
				 processor_thread, NULL, "processor_thread");

  while (1)
    {
      printf ("Wake up, start uart\r\n");
      uart_poweron (UART_DEV (1));
      xtimer_sleep (3);

      printf ("stop uart, adn sleep \r\n");
      xtimer_sleep (PAUSE);

    }
}
