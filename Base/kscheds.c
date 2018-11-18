/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "domamdh.h"
#include "kschedh.h"
#include "kerinith.h"

extern struct DIB idledib;
 
void kscheds(void)
{
   waitstateprocess = nowaitstateprocess;
   {  /* Initialize idledib. */
      idledib.cpucache = 0;   /* set idledib's cpucache to zero */
      idledib.readiness = BUSY + ZEROCACHE;
      init_idledib_md(&idledib);
   }
}
