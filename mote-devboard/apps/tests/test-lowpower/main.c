/*
 * Copyright (C) 2014 Freie Universität Berlin
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
 * @brief       Hello World application
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "board.h"
#include "periph_conf.h"
#include "periph/gpio.h"
#include "xtimer.h"


#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/netopt.h"

        
        

/*
static kernel_pid_t ifpid = 0;
static netopt_state_t state;
*/

int main(void)
{

    puts("Hello World!");
    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);

#ifdef MODULE_PM_LAYERED
    printf("MODULE_PM_LAYERED enabled\n");
#endif
#ifdef MODULE_PERIPH_LLWU
    printf("MODULE_PERIPH_LLWU enabled \n");
#endif

#ifdef KINETIS_HAVE_LPUART
    printf("KINETIS_HAVE_LPUART\n");
#endif

#ifdef KINETIS_HAVE_UART
    printf("KINETIS_HAVE_UART\n");
#endif
#ifdef UART0
    printf("UART0 defined\n");
#endif
#ifdef LPUART0
    printf("LPUART0 defined\n");
#endif

    LED0_ON;
    LED1_OFF;
  /* initialize Network */



/*
    uint8_t t= 5; 
    LED0_ON;
    LED1_OFF;
    while (t-->0) {
        LED0_TOGGLE;
        LED1_TOGGLE;
        xtimer_sleep(1);
    }
    LED0_OFF;
    LED1_OFF;



    gnrc_netif_t *iff = NULL;
    iff = gnrc_netif_iter (iff);
    ifpid = iff->pid;

    state = NETOPT_STATE_OFF;
    gnrc_netapi_set (ifpid, NETOPT_STATE, 0, &state , sizeof (state));
*/
    while(1){
        LED0_TOGGLE;
        LED1_TOGGLE;
        xtimer_sleep(1); 
    }

    return 0;
}
