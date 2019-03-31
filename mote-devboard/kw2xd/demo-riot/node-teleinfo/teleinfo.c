#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ringbuffer.h"
#include "msg.h"
#include "board.h"
#include "periph/uart.h"
#include "periph/gpio.h"

#include "teleinfo.h"


#define ENABLE_DEBUG  (1)
#include "debug.h"


char buffer[UART_BUFSIZE];
char msg_buf[UART_BUFSIZE];

#define  BASE_DONE (1 >>0 ) 
#define  PAPP_DONE (1 >>1 ) 
#define  IINST_DONE (1 >>2 ) 

typedef struct
{
  int16_t papp;
  int16_t iinst;
  int32_t base;
  uint8_t state;
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

static uint8_t rx_teleinfo;

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
    if (rx_teleinfo){
        uart_t dev = (uart_t) arg;
        data &= 0x7F;
        //DEBUG("rxcb: %02x \n", data);
        if (data == '\n')
        {
            //DEBUG("rxcb: '\\n' detected\n");
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
void teleinfo_enable_rx(uint8_t curtel)
{
    rx_teleinfo=curtel; 
    if (curtel==1) {
        gpio_clear(TELEINFO_POWER_PORT_2);
        gpio_set(TELEINFO_POWER_PORT_1);
    }
    else if (curtel==2) {
        gpio_clear(TELEINFO_POWER_PORT_1);
        gpio_set(TELEINFO_POWER_PORT_2);
    }
    thread_wakeup(teleinfo_pid);
}
/*
 *  Disable teleinfo receive
 */
void teleinfo_disable_rx(void)
{
    rx_teleinfo=0;
    gpio_clear(TELEINFO_POWER_PORT_1);
    gpio_clear(TELEINFO_POWER_PORT_2);
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
        DEBUG("teleinfo: Wait for teleinfo frame\n");
        msg_receive(&msg);
        if (rx_teleinfo==1) {LED0_TOGGLE;}
        if (rx_teleinfo==2) {LED1_TOGGLE;}
        uart_t dev = (uart_t)msg.content.value;
        bzero (buffer, sizeof (buffer));
        ringbuffer_get (&(ctx[dev].rx_buf), buffer, UART_BUFSIZE);

        DEBUG("teleinfo: UART%d RX: [%s]\n", dev, buffer);

        //'BASE 014038982 .'
        if (strncmp (buffer, "BASE", 4) == 0 && strlen (buffer) >= 14)
        {
            buffer[14] = 0;
            results[dev].base = atoi (buffer + 5);
            results[dev].state |= BASE_DONE ; 
        }
        //'PAPP 02340 *'
        else if (strncmp (buffer, "PAPP", 4) == 0 && strlen (buffer) >= 10)
        {
            buffer[10] = 0;
            results[dev].papp = atoi (buffer + 5);
            results[dev].state |= PAPP_DONE ;
        }
        //'IINST 010 X'
        else if (strncmp (buffer, "IINST", 5) == 0 && strlen (buffer) >= 9)
        {
            buffer[9] = 0;
            results[dev].iinst = atoi (buffer + 6);
            results[dev].state |= IINST_DONE ;
        }

        // Check if we have complete sequence (PAPP + BASE + IINST) on every UART
        uint8_t  k=0; 
        for (k=0; k<UART_NUMOF; k++){        
                DEBUG("teleinfo: UART%d :[PAPP:%i;BASE:%li;IINST:%i]\n",
                k,
                results[k].papp,
                results[k].base,
                results[k].iinst);

            if (results[k].state == (BASE_DONE | PAPP_DONE | IINST_DONE) )
            {
                /* Append new metrics */
                sprintf (msg_buf + strlen(msg_buf), "PAPP%d:%i;BASE%d:%li;IINST%d:%i;",
                rx_teleinfo, results[k].papp,
                rx_teleinfo, results[k].base,
                rx_teleinfo, results[k].iinst);

                DEBUG("teleinfo: append metrics [%s]\n", msg_buf);
                results[k].state = 0; 
            }
        }

        /* Great! We have something to send to main thread */
        if (strlen(msg_buf) >0){
            DEBUG("teleinfo: '%s' will be sent to main \n", msg_buf);
                
            msg_t msg;
            msg.content.ptr = (void*) msg_buf;
            msg_send (&msg, main_pid);


            DEBUG("teleinfo: stop RX and sleep\n");
            if (rx_teleinfo==1) {LED0_OFF;}
            if (rx_teleinfo==2) {LED1_OFF;}
            rx_teleinfo=0;
            thread_sleep();

            bzero (msg_buf, sizeof (msg_buf));
            printf("teleinfo: wake up\n");
 
        
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

    printf("teleinfo: init\n");

    // We use GPIOs  to power Teleinfo
    gpio_init(TELEINFO_POWER_PORT_1, GPIO_OUT);
    gpio_clear(TELEINFO_POWER_PORT_1);

    gpio_init(TELEINFO_POWER_PORT_2, GPIO_OUT);
    gpio_clear(TELEINFO_POWER_PORT_2);

    main_pid=pid; 
    rx_teleinfo=0;
    bzero (buffer, sizeof (buffer));
    bzero (msg_buf, sizeof (msg_buf));

    /* initialize ringbuffers and results*/
    for (unsigned i = 0; i < UART_NUMOF; i++) {
        ringbuffer_init(&(ctx[i].rx_buf), ctx[i].rx_mem, UART_BUFSIZE);
        results[i].state=0; 
    }

    /* initialize UART */
    int res;
    int uart_dev=1;
    //uint32_t baud=115200; 
    uint32_t baud=1200; 
    res = uart_init(UART_DEV(1), baud, teleinfo_rx_cb, (void *)uart_dev);
    if (res == UART_NOBAUD) {
        printf("teleinfo: Given baudrate (%u) not possible\n", (unsigned int)baud);
        return 0;
    }
    else if (res != UART_OK) {
        printf("teleinfo: Unable to initialize UART device\n");
        return 0;
    }
    printf("teleinfo: Successfully initialized UART_DEV(%i)\n", uart_dev);


    /* start the printer thread */
    teleinfo_pid = thread_create(teleinfo_stack, sizeof(teleinfo_stack),
                                TELEINFO_PRIO, THREAD_CREATE_WOUT_YIELD, teleinfo_run, NULL, "teleinfo");

return teleinfo_pid; 

}
