/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include <sys/lwp.h>
#include <ucontext.h>
#include <time.h>
#include <signal.h>
#include <sys/synch.h>

    char *ptr;
    lwp_sema_t sema;
    lwp_mutex_t mu = {{0,0,0},0,0};
 

main()
{
    ucontext_t uc;
    char *ustack;
    void routine(void *);
    lwpid_t lwpid;
    char buf[256];
    
    ustack=(char *)malloc(8192);

    _lwp_makecontext(&uc, routine, "Thread argument", 0, ustack, 8192);
    _lwp_create(&uc, LWP_DETACHED, &lwpid);
 
    _lwp_mutex_lock(&mu);
    printf("Main going to sleep\n");
    _lwp_mutex_unlock(&mu);
    sleep(3);
    _lwp_mutex_lock(&mu);
    printf("Main wakeup\n");
    _lwp_mutex_unlock(&mu);

}
void routine(void *arg)
{
     char *msg = (char *)arg;
     struct itimerval itv;

     _lwp_mutex_lock(&mu);
     printf("I am thread %d '%s'\n",_lwp_self(),msg);
     _lwp_mutex_unlock(&mu);

   ptr=(char *)getenv("FREEZEDRY");
   if(ptr) {
      if(!strcmp(ptr,"YES")) {
          _lwp_mutex_lock(&mu);
          printf("Thread %d attempting Freezedry\n",_lwp_self());
          _lwp_mutex_unlock(&mu);
          freezedry();
          _lwp_mutex_lock(&mu);
          printf("Freezedry return - thread %d\n",_lwp_self());
          _lwp_mutex_unlock(&mu);
      }
   }

    _lwp_exit();
}

asm("
        .type freezedry, #function
freezedry:
        ta 3
        set 0x0000ffff, %g1
        set 0, %o0
        ta  8
        retl
        nop
");

