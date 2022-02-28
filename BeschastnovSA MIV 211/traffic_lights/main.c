#include "stdint.h"
#include <board.h>
#include <periph/gpio.h>
#include "periph_conf.h"
#include "xtimer.h"

#define RED_FREAQ       (500LU * US_PER_MS) 
#define YELLOW_FREAQ    (50LU * US_PER_MS) 
#define NUM_OF_BUTT     2
#define RED_TIME        10000
#define YELLOW_TIME     1000
#define GREEN_TIME      10000

xtimer_ticks32_t t1, t3;
static volatile uint32_t red_time    = RED_TIME,
                         yellow_time = YELLOW_TIME,
                         green_time  = GREEN_TIME; 
xtimer_ticks32_t wakeup_btn,
                 wakeup_change_time;

xtimer_t btn_timer,
         change_time_timer;

static uint32_t time_left(void)
{
    return ((red_time + yellow_time) - (xtimer_usec_from_ticks(xtimer_diff(t3, t1)))/1e3)/1e3;   
}

void callback_btn(void *arg)
{
    (void) arg;
    gpio_irq_enable(UNWD_GPIO_6);
}
void callback_change_time(void *arg)
{
    (void) arg;
    gpio_irq_enable(UNWD_GPIO_7);
}


static void btn_handler(void * arg)
{
    (void) arg;
    gpio_irq_disable(UNWD_GPIO_6);

    printf("%"PRIu32" seconds left for green color\n\r", time_left());

    xtimer_set(&btn_timer, 500*1000);
}

static void change_time(void * arg)
{
    (void) arg;
    gpio_irq_disable(UNWD_GPIO_7);
    
    if(time_left() > 5)
    {   
        t1 = xtimer_now();
        red_time    = 0;
        yellow_time = 500;
        printf("Wait 5 seconds and go\n\r");
    }
    else if (time_left() > 0 && time_left() <= 5)
    {
        printf("Less then 5 seconds left, please wait\n\r");
    } else {
        printf("You can go\n\r");
    }

    xtimer_set(&change_time_timer, 500*1000);

}

static void light_shinig(int freq, uint32_t left)
{
    uint32_t diff = 0;
    xtimer_ticks32_t t2 = xtimer_now();

    while(diff <= left)
    {
        gpio_set(LED0_PIN);
        xtimer_usleep(freq);
        if(freq) gpio_clear(LED0_PIN);
        xtimer_usleep(freq);
        t3 = xtimer_now();

        switch(freq)
        {
            case RED_FREAQ:
                left = red_time;
            break;
            case YELLOW_FREAQ:
                left = yellow_time; 
            break;
            case 0:
                left = green_time; 
            break;
        }

        diff = (xtimer_usec_from_ticks(xtimer_diff(t3, t2)))/1000;
    }
}


static void set_interrupts(void)
{
    gpio_t arr_interrupts[NUM_OF_BUTT] = {UNWD_GPIO_6, UNWD_GPIO_7};

    for(int i = 0; i < NUM_OF_BUTT; i++)
        gpio_init_int(arr_interrupts[i],
                    GPIO_IN_PU,
                    GPIO_FALLING,
                    (!i) ? btn_handler : change_time,
                    NULL); 
}

int main(void)
{
    btn_timer.callback = callback_btn;
    change_time_timer.callback = callback_change_time;
    gpio_init(LED0_PIN, GPIO_OUT);
    set_interrupts();

    while(true) 
    {
        t1 = xtimer_now();
        red_time    = RED_TIME;
        yellow_time = YELLOW_TIME;

        light_shinig(RED_FREAQ, red_time);
        light_shinig(YELLOW_FREAQ, yellow_time);
        light_shinig(0, green_time);

    }

    return 0;
}