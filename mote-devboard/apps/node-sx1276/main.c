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

#include "ringbuffer.h"
#include "strings.h"

#include "periph/uart.h"
#include "periph/gpio.h"
#include "periph/hwrng.h"
#include "periph/adc.h"

#include "fmt.h"

#include "net/loramac.h"
#include "semtech_loramac.h"

char buflog[255];

#define CYCLE_PAUSE_SEC 60
#define TX_BUFSIZE (128U)

static uint8_t cnf_enable_pm_radiosleep=0;

static uint8_t deveui[LORAMAC_DEVEUI_LEN];
static uint8_t appeui[LORAMAC_APPEUI_LEN];
static uint8_t appkey[LORAMAC_APPKEY_LEN];

semtech_loramac_t loramac;


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

int main (void)
{


 pm_block(KINETIS_PM_STOP);

  uint16_t loop_cntr=0; 
  LED0_OFF; 
  LED1_OFF; 

  uint8_t cnt; 
  for (cnt=0; cnt<4; cnt++){
    LED0_TOGGLE;
    xtimer_usleep(200 * 1000); 
  }

  gpio_clear(GPIO_PIN(PORT_C, 4));

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


  //net_config();
  //sensors_config();
  int i; 
  for (i=0;i<3;i++){
  gpio_set(GPIO_PIN(PORT_C, 4));
  puts("Wait 1sec");
    LED1_ON;
    xtimer_sleep(1); 
  gpio_clear(GPIO_PIN(PORT_C, 4));
    LED1_OFF;
}


    puts("Initializing Lora");
    /* Use a fast datarate, e.g. BW125/SF7 in EU868 */
    semtech_loramac_set_dr(&loramac, LORAMAC_DR_5);

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

