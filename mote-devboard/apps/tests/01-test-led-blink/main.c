/*
 * Copyright (C) 2016-2018 Bas Stottelaar <basstottelaar@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Power management peripheral test.
 *
 * @author      Bas Stottelaar <basstottelaar@gmail.com>
 * @author      Vincent Dupont <vincent@otakeys.com>
 *
 * @}
 */


#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include "xtimer.h"


/*
#ifdef MODULE_PM_LAYERED
#warning MODULE_PM_LAYERED defined !!
#endif 
*/

int main(void)
{

    printf("This is a simple LED TEST\n");
    while(1){
        printf("K\n");
        LED0_TOGGLE;
        LED1_TOGGLE;
        xtimer_sleep(1);
    }



    return 0;
}
