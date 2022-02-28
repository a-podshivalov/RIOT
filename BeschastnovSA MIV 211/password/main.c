#include "stdint.h"
#include <board.h>
#include <periph/gpio.h>
#include "periph_conf.h"
#include "xtimer.h"

#define NUM_OF_BTN      4
#define NUMBER_OF_PASS  4
#define PASSWORD        "1234"

static volatile char counter[NUMBER_OF_PASS];
static volatile int count = 0;
static const char * password = PASSWORD;

xtimer_t btn_timer;

void callback_btn(void *arg)
{
    (void) arg;
    gpio_t arr_4btn[NUM_OF_BTN] = {UNWD_GPIO_4, UNWD_GPIO_5, UNWD_GPIO_6, UNWD_GPIO_7};

    for(int i = 0; i < NUMBER_OF_PASS; i++)
        gpio_irq_enable(arr_4btn[i]);
}

static bool check_pass(void)
{
    count = 0;
    for(int i = 0; i < NUMBER_OF_PASS; i++)
        if((int)counter[i] != (int)password[i]) return false;

    return true;
}

static void btn_handler(void *arg)
{
    gpio_irq_disable(*(gpio_t *) arg);
    char letter = '0';

    switch(*(gpio_t *) arg)
    {
     case UNWD_GPIO_4:
        letter = '1';
        break;
     case UNWD_GPIO_5:
        letter = '2';
        break;
     case UNWD_GPIO_6:
        letter = '3';
        break;
     case UNWD_GPIO_7:
        letter = '4';
        break;
    }

    counter[count++] = letter;
    printf("%c\n\r", letter);
    gpio_clear(LED0_PIN);

    xtimer_set(&btn_timer, 500*1000);
    
}

static void btn_int_init(void)
{
    gpio_t arr_4btn[NUM_OF_BTN] = {UNWD_GPIO_4, UNWD_GPIO_5, UNWD_GPIO_6, UNWD_GPIO_7};

    printf("%d\n\r", (int) arr_4btn[0]);

    for(int i = 0; i < NUM_OF_BTN; i++)
        gpio_init_int(arr_4btn[i],
                      GPIO_IN_PU,
                      GPIO_FALLING,
                      btn_handler,
                      &arr_4btn[i]); 
}

int main(void)
{

    btn_timer.callback = callback_btn;

    gpio_init(LED0_PIN, GPIO_OUT);
    btn_int_init();

    while(true) 
    {

        if(count == NUMBER_OF_PASS)
        {
            printf("\n\r");
            if(check_pass()) 
            {
                printf("PASSWORD is correct\n\r");
                gpio_set(LED0_PIN);
            } else {
                printf("PASSWORD is not correct\n\r");
            }
        }
    }

    return 0;
}