#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ringbuffer.h"
#include "msg.h"
#include "board.h"
#include "periph/uart.h"
#include "periph/gpio.h"

#include "teleinfo.h"


#define ENABLE_DEBUG  (0)
#include "debug.h"


char buffer[UART_BUFSIZE];
char msg_buf[UART_BUFSIZE];


typedef struct
{
  int16_t papp;
  int16_t iinst;
  int32_t base;
} results_t;
results_t results[UART_NUMOF];

typedef struct {
    char rx_mem[UART_BUFSIZE];
    ringbuffer_t rx_buf;
} uart_ctx_t;

uart_ctx_t ctx[UART_NUMOF];

kernel_pid_t teleinfo_pid;
char teleinfo_stack[THREAD_STACKSIZE_MAIN];

static kernel_pid_t main_pid; 

static uint8_t rxen;

/* 
 * UART RX Callback function
 *
 * Discart 8th bit as PitInfo Teleinformation interface is 7N1
 * Append new char to ringbuffer
 * If new char is newline, send a message 
 * If not \n, append char to ringbuffer
 */
void teleinfo_rx_cb (void *arg, uint8_t data)
{
    if (rxen){
        uart_t dev = (uart_t) arg;
        data &= 0x7F;
        DEBUG("rxcb: %02x \n", data);
        if (data == '\n')
        {
            DEBUG("rxcb: '\\n' detected\n");
            msg_t msg;
            msg.content.value = (uint32_t) dev;
            msg_send (&msg, teleinfo_pid);
        }
        else if (data >= ' ')
        {
            ringbuffer_add_one (&(ctx[dev].rx_buf), data);
        }
    }
}

/*
 *  Enable teleinfo receive
 */
void teleinfo_enable_rx(void)
{
    rxen=1; 
    gpio_set(TELEINFO_POWER_PORT);
    thread_wakeup(teleinfo_pid);
}
/*
 *  Disable teleinfo receive
 */
void teleinfo_disable_rx(void)
{
    rxen=0;
    gpio_clear(TELEINFO_POWER_PORT);

    //thread_sleep();
}


/*
 * Teleinfo main loop
 * 
 * - Wait from teleinfo frames (see rx_cb for serial reception)
 * - When all BASE, PAPP, IINST are received, new message is sent to main thread
 */
void *teleinfo_run(void *arg)
{
    printf("teleinfo_run() Thread Starting\n");
    (void)arg;
    msg_t msg;
    msg_t msg_queue[8];
    msg_init_queue(msg_queue, 8);

    while (1) {
        DEBUG("teleinfo_run: Wait for message\n");
        msg_receive(&msg);
        uart_t dev = (uart_t)msg.content.value;
        bzero (buffer, sizeof (buffer));
        ringbuffer_get (&(ctx[dev].rx_buf), buffer, UART_BUFSIZE);

        printf("teleinfo_run: UART_DEV(%i) RX: [%s]\n", dev, buffer);

        //'BASE 014038982 .'
        if (strncmp (buffer, "BASE", 4) == 0 && strlen (buffer) >= 14)
        {
            buffer[14] = 0;
            results[dev].base = atoi (buffer + 5);
        }
        //'PAPP 02340 *'
        else if (strncmp (buffer, "PAPP", 4) == 0 && strlen (buffer) >= 10)
        {
            buffer[10] = 0;
            results[dev].papp = atoi (buffer + 5);
        }
        //'IINST 010 X'
        else if (strncmp (buffer, "IINST", 5) == 0 && strlen (buffer) >= 9)
        {
            buffer[9] = 0;
            results[dev].iinst = atoi (buffer + 6);
        }

        // Check if we have complete sequence (PAPP + BASE + IINST) on every UART
        uint8_t  k=0; 
        for (k=0; k<UART_NUMOF; k++){        
                DEBUG ( "DBG => [PAPP%d:%i;BASE%d:%li;IINST%d:%i]\n",
                k, results[k].papp,
                k, results[k].base,
                k, results[k].iinst);

            if (results[k].papp > 0 && results[k].iinst > 0 && results[k].base > 0)
            {
                /* Append new metrics */
                sprintf (msg_buf + strlen(msg_buf), "PAPP%d:%i;BASE%d:%li;IINST%d:%i;",
                k, results[k].papp,
                k, results[k].base,
                k, results[k].iinst);


            results[k].papp = results[k].base = results[k].iinst = 0; 
            }
        }

        /* Great! We have something to send to main thread */
        if (msg_buf[0] >0){
            printf ("teleinfo_run: message '%s' will be sent to main \n", msg_buf);
                
            msg_t msg;
            msg.content.ptr = (void*) msg_buf;
            msg_send (&msg, main_pid);


            printf("teleinfo_run: stop RX and sleep\n");
            rxen=0;
            thread_sleep();

            bzero (msg_buf, sizeof (msg_buf));
            rxen=1;
            printf("teleinfo_run: wake up\n");
 
        
        }

    }
    /* this should never be reached */
    return NULL;
}


/*
 * Teleinfo module initialisation
 * 
 * - serial ringbuffer
 * - serial port configuration
 * - thread creation
 */
kernel_pid_t teleinfo_init(kernel_pid_t pid ){

    printf("teleinfo_init() Start\n");

    // We use PTE3 to power Teleinfo
    gpio_init(TELEINFO_POWER_PORT, GPIO_OUT);
    gpio_clear(TELEINFO_POWER_PORT);


    main_pid=pid; 
    rxen=0;
    bzero (buffer, sizeof (buffer));
    bzero (msg_buf, sizeof (msg_buf));

    /* initialize ringbuffers and results*/
    for (unsigned i = 0; i < UART_NUMOF; i++) {
        ringbuffer_init(&(ctx[i].rx_buf), ctx[i].rx_mem, UART_BUFSIZE);
        results[i].papp = results[i].base = results[i].iinst=0; 
    }

    /* initialize UART */
    int res;
    int dev=1;
    //uint32_t baud=115200; 
    uint32_t baud=1200; 
    res = uart_init(UART_DEV(1), baud, teleinfo_rx_cb, (void *)dev);
    if (res == UART_NOBAUD) {
        printf("teleinfo_init: Given baudrate (%u) not possible\n", (unsigned int)baud);
        return 0;
    }
    else if (res != UART_OK) {
        puts("teleinfo_init: Unable to initialize UART device\n");
        return 0;
    }
    DEBUG("teleinfo_init: Successfully initialized UART_DEV(%i)\n", dev);


    /* start the printer thread */
    teleinfo_pid = thread_create(teleinfo_stack, sizeof(teleinfo_stack),
                                TELEINFO_PRIO, THREAD_CREATE_WOUT_YIELD, teleinfo_run, NULL, "teleinfo");

return teleinfo_pid; 

 
}
