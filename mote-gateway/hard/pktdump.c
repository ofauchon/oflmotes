/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_pktdump
 * @{
 *
 * @file
 * @brief       Generic module to dump packages received via netapi to STDOUT
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <inttypes.h>
#include <stdio.h>

#include <errno.h>
#include "byteorder.h"
#include "thread.h"
#include "msg.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc.h"
#include "net/icmpv6.h"
#include "net/ipv6/addr.h"
#include "net/ipv6/hdr.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/sixlowpan.h"
#include "od.h"

#include "myutils.h"

/**
 * @brief   PID of the pktdump thread
 */
kernel_pid_t gnrc_pktdump_pid = KERNEL_PID_UNDEF;

/**
 * @brief   Stack for the pktdump thread
 */
static char _stack[GNRC_PKTDUMP_STACKSIZE];

#define PERLINE 20
static void
_dump (gnrc_pktsnip_t * pkt)
{
  int snips = 0;
  int size = 0;
  gnrc_pktsnip_t *snip = pkt;

  printf ("<PAYLOAD=");
  while (snip != NULL)
    {
      if (snip->type == GNRC_NETTYPE_UNDEF)
	{
	  // Dump
	  uint8_t p;
	  for (p = 0; p < snip->size; p++)
	    printf ("%02X", ((char *) snip->data)[p]);
	  printf (";");
	}
      if (snip->type == GNRC_NETTYPE_NETIF)
	{
	  gnrc_netif_hdr_t *hdr = snip->data;
	  char addr_str[8 * 3];
	  gnrc_netif_addr_to_str (gnrc_netif_hdr_get_src_addr (hdr), 8,
				  addr_str);
	  printf ("SMAC=%s;LQI=%u;RSSI=%d", addr_str, hdr->lqi, hdr->rssi);

	}

      ++snips;
      size += snip->size;
      snip = snip->next;
    }
  printf (">\n");

  //printf("~~ PKT    - %2i snips, total size: %3i byte\n", snips, size);
  gnrc_pktbuf_release (pkt);
}

static void *
_eventloop (void *arg)
{
  printf ("pktdump : start event loop\r\n");
  (void) arg;
  msg_t msg, reply;
  msg_t msg_queue[GNRC_PKTDUMP_MSG_QUEUE_SIZE];

  /* setup the message queue */
  msg_init_queue (msg_queue, GNRC_PKTDUMP_MSG_QUEUE_SIZE);

  reply.content.value = (uint32_t) (-ENOTSUP);
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;

  /* register thread with currently active pid for messages of type GNRC_NETTYPE_IPV6 */
  gnrc_netreg_entry_t me =
    GNRC_NETREG_ENTRY_INIT_PID (GNRC_NETREG_DEMUX_CTX_ALL, sched_active_pid);
  gnrc_netreg_register (GNRC_NETTYPE_UNDEF, &me);



  while (1)
    {
      msg_receive (&msg);

      switch (msg.type)
	{
	case GNRC_NETAPI_MSG_TYPE_RCV:
	  puts("DEBUG: data received:");
	  _dump (msg.content.ptr);
	  break;
	case GNRC_NETAPI_MSG_TYPE_SND:
	  // puts("PKTDUMP: data to send:");
	  _dump (msg.content.ptr);
	  break;
	case GNRC_NETAPI_MSG_TYPE_GET:	// Always respond to GET/SET messages with ACK ! 
	case GNRC_NETAPI_MSG_TYPE_SET:
	  msg_reply (&msg, &reply);
	  break;
	default:
	  puts ("PKTDUMP: received something unexpected");
	  break;
	}
    }

  /* never reached */
  return NULL;
}

kernel_pid_t
gnrc_pktdump_init (void)
{
  if (gnrc_pktdump_pid == KERNEL_PID_UNDEF)
    {
      gnrc_pktdump_pid =
	thread_create (_stack, sizeof (_stack), GNRC_PKTDUMP_PRIO,
		       THREAD_CREATE_STACKTEST, _eventloop, NULL, "pktdump");
    }
  return gnrc_pktdump_pid;
}
