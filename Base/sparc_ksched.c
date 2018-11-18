/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* sparc_ksched.c - kschmd for the Sparc */
/* The header for this file is kschmdh.h */

#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "domamdh.h"
#include "prepkeyh.h"
#include "sparc_mem.h"
#include "kschedh.h"
#include "kschmdh.h"
#include "meterh.h"

void md_startdom(void)
{
}

void md_putawaydomain(void)
{
}

void slowstart()
/* Handle bits on in cpudibp->readiness */
{
   uchar * ready = &cpudibp->readiness;
   /* There is something to do just before we run this domain */
   if (*ready & (LOWPRIORITY | AGEING | STALECACHE)) {
     if (*ready & (AGEING | STALECACHE)) *ready &= ~(AGEING | STALECACHE);
     else {
       NODE *rn = cpudibp->rootnode;
       *ready &= ~LOWPRIORITY;
       if (cpudibp != &idledib) {
         putawaydomain();
         rundomifok(rn);         /* Re-queue domain in priority order */
       }
       select_domain_to_run();
     }
   }
   else {
     if(*ready & (ZEROCACHE | TRAPPED)) {
       if (*ready & ZEROCACHE){
         refill_cpucache();
         return;
#if 0
         {   /* THIS CODE TURNED OFF FOR LACK OF UNDERSTANDING */
            if(cpudibp && *ready & ZEROCACHE) {
              if(!diskless_kernel) Panic(); // See if cpudibp is already zero!
                                            // if so remove following call to putawaydomain!!
                                            // otherwise just remove this Panic!!
           
              putawaydomain();
            } // incorrigible
         }
#endif
/* THIS CODE ADDED to stop the kernel form continuously trying to run this domain with its broken meter */
         if(*ready & TRAPPED) dispatch_trapped_domain();   /* got trapped refilling cache */

       }
       else dispatch_trapped_domain();}
     else if (*ready & HOOKED) {
       if (cpuactor->domhookkey.type==pihk)
/* The above pihk clause is tentative and may not be needed....*/
         zaphook(cpuactor);
      else *ready &=~HOOKED;
     }
     else Panic();
   }
}
