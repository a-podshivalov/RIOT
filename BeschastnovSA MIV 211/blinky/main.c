#include "stdint.h"
#include <board.h>
#include <periph/gpio.h>
#include "periph_conf.h"
#include "xtimer.h"

static volatile int frequancy = 200*1e3;
static char stack_size[THREAD_STACKSIZE_DEFAULT];

xtimer_t btn_timer;
xtimer_ticks32_t t1, t2;
static kernel_pid_t rcv_pid;
bool check = false;

void * blinky_thread(void * arg)
{
    msg_t msg_req;
    msg_req.content.value = 0;

    (void) arg;
    
    while(true)
    {
        msg_receive(&msg_req);
        if(msg_req.content.value)
        {
            msg_req.content.value = 0;
            frequancy = (frequancy == 200*1e3) ? 50*1e3 : 200*1e3;
        } 
    }
}

void callback_btn(void * arg)
{
    (void) arg;

    gpio_irq_enable(UNWD_GPIO_6);
}

void btn_handler(void * arg)
{   
    gpio_irq_disable(UNWD_GPIO_6);
    (void) arg;

    t1 = xtimer_now();
    check = true;

    xtimer_set(&btn_timer, 500*1e3);
}

int main(void)
{
    msg_t msg;
    uint32_t diff;

    btn_timer.callback = callback_btn;        
    msg.content.value = 1;

    //gpio_init(UNWD_GPIO_7, GPIO_IN_PU);
    gpio_init(LED0_PIN, GPIO_OUT);

    gpio_init_int(UNWD_GPIO_6,
                  GPIO_IN_PU,
                  GPIO_FALLING,
                  btn_handler,
                  NULL); 

    rcv_pid = thread_create(stack_size,
                  sizeof(stack_size),
                  THREAD_PRIORITY_MAIN - 1,
                  THREAD_CREATE_STACKTEST,
                  blinky_thread,
                  NULL,
                  "blinky_thread");

    while(true) 
    {
        xtimer_usleep(frequancy);
        gpio_set(LED0_PIN);
        xtimer_usleep(frequancy);
        gpio_clear(LED0_PIN);

        if(!gpio_read(UNWD_GPIO_6) && check) 
        {
            t2 = xtimer_now();
            diff = xtimer_usec_from_ticks(xtimer_diff(t2, t1))/1e6;
            printf("%"PRIu32" seconds left for green color\n\r", diff);

            if (diff >= 3)
            {
                check = false;
                msg_send(&msg, rcv_pid);
            } 
 
        }
    }

    return 0;
}