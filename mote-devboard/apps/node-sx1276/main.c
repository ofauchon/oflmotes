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
#include "semtech_loramac.h"
#include "fmt.h"

extern semtech_loramac_t loramac;



char buflog[255];

#define CYCLE_PAUSE_SEC 60
#define TX_BUFSIZE (128U)

static uint8_t cnf_enable_pm_radiosleep=0;

static uint8_t deveui[LORAMAC_DEVEUI_LEN];
static uint8_t appeui[LORAMAC_APPEUI_LEN];
static uint8_t appkey[LORAMAC_APPKEY_LEN];



int main (void)
{


 //pm_block(KINETIS_PM_STOP);

  uint16_t loop_cntr=0; 
  LED0_ON; 
  LED1_ON; 

  printf("\n*** OFlabs 802.15.4 OFLMote LoRa\n");
  printf("Riot Version: %s\n", RIOTVERSION);
  printf("Mote Version: %s\n", MOTESVERSION);
  printf("Build   Date: %s\n", BUILDDATE);
  printf("Powersave radio sleep: %d\n", cnf_enable_pm_radiosleep);

  /* Convert identifiers and application key */
  fmt_hex_bytes(deveui, DEVEUI);
  fmt_hex_bytes(appeui, APPEUI);
  fmt_hex_bytes(appkey, APPKEY);

  /* Initialize the loramac stack */
  semtech_loramac_init(&loramac);
  semtech_loramac_set_deveui(&loramac, deveui);
  semtech_loramac_set_appeui(&loramac, appeui);
  semtech_loramac_set_appkey(&loramac, appkey);
  semtech_loramac_set_public_network(&loramac, 0);




  //net_config();
  //sensors_config();
  puts("Wait 5sec");
  xtimer_sleep(5); 
  puts("Resume from pause");

    puts("Initializing Lora");
    /* Use a fast datarate, e.g. BW125/SF7 in EU868 */
 //   semtech_loramac_set_dr(&loramac, LORAMAC_DR_2);

    /* Start the Over-The-Air Activation (OTAA) procedure to retrieve the
     * generated device address and to get the network and application session
     * keys.
     */
    puts("Starting join procedure");
    if (semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA) != SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
        puts("Join procedure failed");
        return 1;
    }
    puts("Join procedure succeeded");



  while (1)
    {
        printf("\n\nmain : Start cycle\n");

        // Quick blink and block cpu if button is press
        // So we can debug or reflash 
        LED1_ON;
        xtimer_usleep(500 * 1000); 
        LED1_OFF;
        if (!gpio_read(BTN0_PIN) ||  !gpio_read(BTN1_PIN) ) {
            LED0_ON;
            LED1_ON;
            while(1){}
        }

        printf("main : Radio OFF and Hibernate\n");

        xtimer_sleep(CYCLE_PAUSE_SEC);

        loop_cntr++; 

    }
}

