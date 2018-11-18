/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "kktypes.h"
#include "keykos.h"
#include "node.h"
#include "domain.h"
#include "snode.h"
#include "setjmp.h"
#include <string.h>
#include "wombdefs.h"
#include "ocrc.h"

KEY comp     =0; 
KEY dk       =1; 
KEY caller   =2;
KEY dom      =3; 
KEY sb       =4; 
KEY meter    =5;
KEY dc       =6; 

#define COMPWAITF   1
#define COMPCLOCK   2

KEY hisdom   =7;
KEY me       =8;
KEY clock    =9;

KEY wait    =10;
// KEY waiter  =11;

KEY k2       =13;
KEY k1       =14;
KEY k0       =15;

      char title[]="PCSKeep ";
      int  stacksize=4096;


factory(oc,ord)
	UINT32 oc,ord;
{
      unsigned long long wakeuptime;
      int waiting,waitcnt,killing;
      unsigned long rc,atc;
      unsigned long long howlong,time;
      unsigned long long when;

      long procaddr,jmpbufaddr,parm2,seconds;

      union {
	struct Domain_SPARCRegistersAndControl drac;
        struct {
           long procaddr;
	   long jmpbufaddr;
           long parm2;
	   unsigned long seconds;
        } setup;
      } un;	

      JUMPBUF;

      KC(caller,KT+5) KEYSTO(hisdom,,,caller) RCTO(rc);

      KC (comp,Node_Fetch+COMPWAITF) KEYSTO(wait);
      KC (wait,0) KEYSFROM(sb,meter,sb) KEYSTO(wait);
      KC (comp,Node_Fetch+COMPCLOCK) KEYSTO(clock);

      KC (dom,Domain_MakeStart) KEYSTO(me);
      KC (hisdom,Domain_SwapKeeper) KEYSFROM(me);

      wakeuptime=0xFFFFFFFFFFFFFFFFull;
      when =     0xFFFFFFFFFFFFFFFFull;  /* forever */
      KC (wait,1) STRUCTFROM(when);

      if(!(rc=fork()))  {  /* waiter, hisdom is pcs domain key */
         for (;;) {
             KC(wait,0) RCTO(rc);
             if(rc) exit(); 
             KC(me,4) KEYSFROM(,,hisdom) RCTO(rc);  /* really keeper */
         }
      }
      if(rc > 1) {
          KC (wait,KT+4);
          exit(NOSPACE_RC);
      }
      waiting=0;
      killing=0;
//      KC (k0,64) KEYSTO(waiter);   // must never call him

      LDEXBL (caller,0) KEYSFROM(me);
      for(;;) {
	LDENBL OCTO(atc) KEYSTO(,,hisdom,caller) STRUCTTO(un);
	RETJUMP();

        if(4==atc) { /* timer */
                when=0xFFFFFFFFFFFFFFFFull;
                KC (wait,1) STRUCTFROM(when);  /* set to forever */
                LDEXBL (caller,0);
		if(!waiting) continue; /* waiter will wait forever */
                KC (clock,4) STRUCTTO(time);  /* get current time */
                if(time >= wakeuptime) {
                        waiting=0;
/* guess we wake the sucker up */
                        KC (hisdom,Domain_MakeBusy) KEYSTO(k0) RCTO(rc);
                        KC (hisdom,Domain_GetSPARCStuff) STRUCTTO(un.drac);
			un.drac.Control.PC=procaddr;
			un.drac.Control.NPC=un.drac.Control.PC+4;
			un.drac.Regs.o[0]=jmpbufaddr;
		        un.drac.Regs.o[1]=atc;
                        un.drac.Regs.o[2]=0;
			un.drac.Regs.o[3]=parm2;
			LDEXBL(hisdom,Domain_ResetSPARCStuff)
				KEYSFROM(,,,k0) STRUCTFROM(un.drac);
                        FORKJUMP();
                }
                else {
                      KC(wait,1) STRUCTFROM(wakeuptime);  /* reset, false alarm */
                }
		LDEXBL (caller,0);
		continue;  /* send back to wait on timer */
                           /* timer is probably set to forever */
        }
        if(0==atc) { /* setup */
		procaddr=un.setup.procaddr;
		jmpbufaddr=un.setup.jmpbufaddr;
		parm2=un.setup.parm2;
                seconds=un.setup.seconds;
                LDEXBL (caller,0);
		continue;
        }
        if(1==atc) { /* wants to wait */
		waiting=1;
                KC(clock,4) STRUCTTO(time);
                howlong=seconds;
                howlong=howlong*1000000;
                howlong=howlong*4096;
 
                wakeuptime=time+howlong;
                KC(wait,1) STRUCTFROM(wakeuptime);  /* set timer */

                LDEXBL (caller,0);
		continue;
        }
        if(KT+4==atc) { /* go bye bye */
                waiting=0;
                KC(wait,KT+4) RCTO(rc);
//                KC(waiter,KT+4) RCTO(rc);  /* will die on wait() failure */
                exit();
        }
        if(2==atc) { /* no longer waiting */
		waiting=0;
                wakeuptime=0xFFFFFFFFFFFFFFFFull;
                KC(wait,1) STRUCTFROM(wakeuptime);  /* cancel wakeup call */
                LDEXBL (caller,0);
		continue;
        }
/* some kind of fault */
		un.drac.Regs.o[0]=jmpbufaddr;
	        un.drac.Regs.o[1]=atc;
                un.drac.Regs.o[2]=un.drac.Control.TRAPEXT[0];
                if(atc == KT+7) {
               	    un.drac.Regs.o[2]=un.drac.Control.PC;
		}
		un.drac.Control.PC=procaddr;
		un.drac.Control.NPC=un.drac.Control.PC+4;
		un.drac.Regs.o[3]=parm2;
		LDEXBL(hisdom,Domain_ResetSPARCStuff)
				KEYSFROM(,,,caller) STRUCTFROM(un.drac);
      }
}

