#include "pt.h"
#include <stdio.h> 
#include "lptimer.h"

static int is_thread_1, is_thread_2, is_thread_3;
static struct pt pt1, pt2, pt3;

static int thread_1(struct pt *pt)
{

  PT_BEGIN(pt);

  while(1) 
  {
    PT_WAIT_UNTIL(pt, is_thread_2 != 0);
    printf("Thread 1 running\n\r");

    is_thread_2 = 0;
    is_thread_3 = 1;
    
    lptimer_sleep(1000);
  }

  PT_END(pt);
}

static int thread_2(struct pt *pt)
{
  PT_BEGIN(pt);

  while(1) 
  {
    is_thread_1 = 2;
    PT_WAIT_UNTIL(pt, is_thread_3 != 0);
    printf("Thread 2 running\n\r");

    is_thread_3 = 0;

    lptimer_sleep(1000);
  }
  PT_END(pt);
}

static int thread_3(struct pt *pt)
{

  PT_BEGIN(pt);

  while(1) 
  {
    PT_WAIT_UNTIL(pt, is_thread_1 != 0);
    printf("Thread 3 running\n\r");
    is_thread_1 = 0;
    is_thread_2 = 1;

    lptimer_sleep(1000);
  }

  PT_END(pt);
}

int main(void)
{

  PT_INIT(&pt1);
  PT_INIT(&pt2);
  PT_INIT(&pt3);
  
  while(1) {
    thread_1(&pt1);
    thread_2(&pt2);
    thread_3(&pt3);
  }
}
