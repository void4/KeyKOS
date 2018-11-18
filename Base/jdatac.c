/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "primcomh.h"
#include "kernkeyh.h"
#include "memutil.h"


void jdata(key)    /* Handle jumps to data keys */
struct Key *key;
{
   if (cpuordercode == 1) {
      memzero(cpuargpage,5);     /* Clear start of returned string */
      Memcpy(cpuargpage+5,key->nontypedata.dk11.databody11,11);
      cpuarglength = 16;
   }
   else {
      Memcpy(cpuargpage,key->nontypedata.dk6.databody6,6);
      cpuarglength = 6;
   }
   cpuargaddr = cpuargpage;
   cpuexitblock.argtype = arg_regs;
   cpuordercode = KT+1;
   jsimple(0); /* no keys */
} /* end jdata */
 
 
void jhook(key)    /* Handle jumps to hook keys */
struct Key *key;
{
   jdata(hook_look(key));
}
