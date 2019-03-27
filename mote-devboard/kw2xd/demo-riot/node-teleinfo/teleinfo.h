#ifndef TELEINFO_H
#define TELEINFO_H

#define UART_BUFSIZE        (128U)
#define TELEINFO_PRIO        (THREAD_PRIORITY_MAIN - 1)

#define TELEINFO_POWER_PORT_0 GPIO_PIN(PORT_E, 4)
#define TELEINFO_POWER_PORT_1 GPIO_PIN(PORT_D, 1)



void *teleinfo_run(void *arg);
kernel_pid_t teleinfo_init(kernel_pid_t pid );

void teleinfo_enable_rx(uint8_t curtel);
void teleinfo_disable_rx(void);



#endif