#ifndef TELEINFO_H
#define TELEINFO_H

#define UART_BUFSIZE        (128U)
#define TELEINFO_PRIO        (THREAD_PRIORITY_MAIN - 1)

#define TELEINFO_POWER_PORT GPIO_PIN(PORT_E, 4)


void *teleinfo_run(void *arg);
kernel_pid_t teleinfo_init(kernel_pid_t pid );

void teleinfo_enable_rx(void);
void teleinfo_disable_rx(void);



#endif