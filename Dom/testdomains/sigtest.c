/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>

   void handler(int, siginfo_t *, ucontext_t *);

struct sigaction sa = {SA_SIGINFO,handler,0,0,0};

   int *ptr=-1;
   char message[64]="Fixed pointer";

main() {
    struct sigaction oldsa;
    
    printf("Size of sig_info %X, Size of ucontext %X\n",
         sizeof(siginfo_t),sizeof(struct ucontext));
    sigaction(SIGBUS,&sa,&oldsa);

    if(*ptr != 0) {
        printf("'%s' is the message\n",ptr);
    }

    printf("Going to sleep for 3 seconds\n");
    sleep(3);
    printf("Wakeup\n");
}
void handler(int sig, siginfo_t *si, ucontext_t *uap) {
 
    printf("Signal caught si->code %d proc %X\n",si->si_code,si->__data.__fault.__addr);
    printf("PC = %X, trapno = %d\n",si->__data.__fault.__pc,si->__data.__fault.__trapno);

    ptr=message;
    uap->uc_mcontext.gregs[REG_O0]=message;
}
     
