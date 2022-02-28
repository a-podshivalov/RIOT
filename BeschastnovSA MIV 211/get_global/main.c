#include <board.h>
#include <periph/gpio.h>
#include "periph_conf.h"

int gi;
const int gci;
static int gsi;
static const int gsci;
static char stack_size[THREAD_STACKSIZE_DEFAULT];

void * new_thread(void * arg)
{
    (void) arg;

    int ti;
    const int tci;
    static int tsi;
    static const int tsci;
    
    printf("Thread int is at %p\n\r", &ti);
    printf("Thread const int is at %p\n\r", &tci);
    printf("Thread static int is at %p\n\r", &tsi);
    printf("Thread static const int is at %p\n\r", &tsci);
    printf("\n\n\n");

    while(true)
    {
        //do nothing...
    }
}


void btn_handler(void * arg)
{
    (void) arg;

    int hi;
    const int hci;
    static int hsi;
    static const int hsci;

    printf("Handler int is at %p\n\r", &hi);
    printf("Handler const int is at %p\n\r", &hci);
    printf("Handler static int is at %p\n\r", &hsi);
    printf("Handler static const int is at %p\n\r", &hsci);
    printf("\n\n\n");
}


void print_addresses(void)
{
    int fi;
    const int fci;
    static int fsi;
    static const int fsci;
    
    printf("Function int is at %p\n\r", &fi);
    printf("Function const int is at %p\n\r", &fci);
    printf("Function static int is at %p\n\r", &fsi);
    printf("Function static const int is at %p\n\r", &fsci);
    printf("\n\n\n");
}

int main(void) 
{
    int li;
    const int lci;
    static int lsi;
    static const int lsci; 

    printf("Size of the stack %p\n\r", &stack_size);
    printf("THREAD_STACKSIZE_DEFAULT of size %d\n\r", THREAD_STACKSIZE_DEFAULT);
    printf("THREAD_STACKSIZE_MAIN of size %d\n\r", THREAD_STACKSIZE_MAIN);
    printf("\n\n\n");

    printf("Global int is at %p\n\r", &gi);
    printf("Global const int is at %p\n\r", &gci);
    printf("Global static int is at %p\n\r", &gsi);
    printf("Global static const int is at %p\n\r", &gsci);
    printf("\n\n\n");

    printf("Local int is at %p\n\r", &li);
    printf("Local const int is at %p\n\r", &lci);
    printf("Local static int is at %p\n\r", &lsi);
    printf("Local static const int is at %p\n\r", &lsci);
    printf("\n\n\n");

    gpio_init_int(UNWD_GPIO_4,
                  GPIO_IN_PU,
                  GPIO_FALLING, 
                  btn_handler, 
                  NULL);

    print_addresses();

    thread_create(stack_size,
                  sizeof(stack_size),
                  THREAD_PRIORITY_MAIN - 1,
                  THREAD_CREATE_STACKTEST,
                  new_thread,
                  NULL,
                  "first_thread");
}