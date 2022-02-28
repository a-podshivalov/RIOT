#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "thread.h"
#include "mutex.h"
#include "xtimer.h"

mutex_t res_mtx;

char stack_high[THREAD_STACKSIZE_MAIN];
char stack_mid[THREAD_STACKSIZE_MAIN];
char stack_low[THREAD_STACKSIZE_MAIN];

void * t_low_handler(void * arg)
{
    (void) arg;

    while(true)
    {
        printf("low - allocating resource\n\r");
        mutex_lock(&res_mtx);
        printf("low - got resource\n\r");
        xtimer_sleep(1);

        printf("low - freeing resource\n\r");
        mutex_unlock(&res_mtx);
        printf("low - freed resource\n\r");
        xtimer_sleep(1);
    }
    return NULL;
}

void * t_mid_handler(void * arg)
{
    (void) arg;

    xtimer_sleep(3);

    printf("middle - do something\n\r");
    while(true)
    {
        thread_yield_higher();
    }
}

void * t_high_handler(void * arg)
{
    (void) arg;

    xtimer_usleep(500);
    while(true)
    {
        printf("high - allocating resource\n\r");
        mutex_lock(&res_mtx);
        printf("high - got resource\n\r");
        xtimer_sleep(1);

        printf("high - freeing resource\n\r");
        mutex_unlock(&res_mtx);
        printf("high - freed resource\n\r");
    }
    return NULL;
}


int main(void)
{
    xtimer_init();
    mutex_init(&res_mtx);

    thread_create(stack_low, sizeof(stack_low),
        THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_STACKTEST,
        t_low_handler, NULL,
        "low_pri");

    thread_create(stack_mid, sizeof(stack_mid),
        THREAD_PRIORITY_MAIN - 2,
        THREAD_CREATE_STACKTEST,
        t_mid_handler, NULL,
        "middle_pr");

    thread_create(stack_high, sizeof(stack_high),
        THREAD_PRIORITY_MAIN - 3,
        THREAD_CREATE_STACKTEST,
        t_high_handler, NULL,
        "high_pr");

    return 0;
}