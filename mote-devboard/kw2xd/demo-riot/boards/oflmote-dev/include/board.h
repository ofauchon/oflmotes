/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2015 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     boards_pba-d-01-kw2x
 * @{
 *
 * @file
 * @brief       Board specific definitions for the phyWAVE evaluation board
 *
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @author      Sebastian Meiling <s@mlng.net>
 */

#ifndef BOARD_H
#define BOARD_H

#include "cpu.h"
#include "periph_conf.h"

#ifdef __cplusplus
extern "C"
{
#endif



/**
 * @name    xtimer configuration
 * @{
 */
/* LPTMR xtimer configuration */
#define XTIMER_DEV                  (TIMER_LPTMR_DEV(0))
#define XTIMER_CHAN                 (0)
/* LPTMR is 16 bits wide and runs at 32768 Hz (clocked by the RTC) */
#define XTIMER_WIDTH                (16)
#define XTIMER_BACKOFF              (5)
#define XTIMER_ISR_BACKOFF          (5)
#define XTIMER_OVERHEAD             (4)
#define XTIMER_HZ                   (32768ul)


/**
 * @name    LED pin definitions and handlers
 * @{
 */
#define LED0_PIN            GPIO_PIN(PORT_D, 5)
#define LED1_PIN            GPIO_PIN(PORT_D, 4)

#define LED0_MASK           (1 << 5)
#define LED1_MASK           (1 << 4)

#define LED0_ON            (GPIOD->PSOR = LED0_MASK)
#define LED0_OFF           (GPIOD->PCOR = LED0_MASK)
#define LED0_TOGGLE        (GPIOD->PTOR = LED0_MASK)

#define LED1_ON            (GPIOD->PSOR = LED1_MASK)
#define LED1_OFF           (GPIOD->PCOR = LED1_MASK)
#define LED1_TOGGLE        (GPIOD->PTOR = LED1_MASK)

/** @} */

/**
 * @name Macro for button S1/S2.
 * @{
 */
#define BTN0_PORT           PORTE
#define BTN0_PIN            GPIO_PIN(PORT_E, 2)
#define BTN0_MODE           GPIO_IN_PU
/** @} */

/**
 * @name Macro for capacitive sensor button.
 * @{
 */
#define BTN1_PORT           PORTE
#define BTN1_PIN            GPIO_PIN(PORT_E, 3)
#define BTN1_MODE           GPIO_IN_PU
/** @} */

/**
 * @name KW2XRF configuration
 *
 * {spi bus, cs pin, int pin, spi speed,}
 * @{
 */
#define KW2XRF_PARAM_SPI           SPI_DEV(1)
#define KW2XRF_PARAM_SPI_CLK       (SPI_CLK_10MHZ)
#define KW2XRF_PARAM_CS            GPIO_PIN(KW2XDRF_PORT, KW2XDRF_PCS0_PIN)
#define KW2XRF_PARAM_INT           GPIO_PIN(KW2XDRF_PORT, KW2XDRF_IRQ_PIN)
#define KW2XRF_SHARED_SPI          (0)
/** @}*/

/**
 * @brief Initialize board specific hardware, including clock, LEDs and std-IO
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
/** @} */
