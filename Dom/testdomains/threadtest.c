/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define _REENTRANT
#include <thread.h>
#define NUM_THREADS 5
#define SLEEP_TIME 10

void *sleeping(void *);
thread_t tid[NUM_THREADS];

main(argc,argv)
  int argc;
  char **argv;
{
  int i;
 
  for(i=0;i<NUM_THREADS;i++) {
     thr_create(NULL,0,sleeping,(void *)SLEEP_TIME,NULL, &tid[i]);
  }
  printf("main() beginning wait\n");
  while (thr_join(NULL,NULL,NULL) == 0) ;
  printf("main() reporting all %d threads have terminated\n",i);
}

void *
sleeping(arg)
    void *arg;
{
    int i;
    int sleep_time = (int) arg;

    printf("thread %d computing \n",thr_self());
    for(i=1;i<1000000;i++);
    printf("thread %d sleeping %d seconds ... \n", thr_self(), sleep_time);
    sleep(sleep_time);

   printf("\nthread %d awakening\n", thr_self());
}
