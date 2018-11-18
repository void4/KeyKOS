/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "devkeyh.h"
#include "primcomh.h"
#include "consmdh.h"
#include "kernkeyh.h" 
 
 
void jdevice(key)       /* Handle device key calls  */
/* Input - */
struct Key *key;            /* The key being invoked */
/*
   cpudibp - has the jumper's DIB
   cpuordercode - has order code.
   The invoked key type is device
*/
{
   switch (key->nontypedata.devk.slot) {
    case DEVKMASTERCPU:
      jconsole(key);
      return;
 
    default:
      simplest(KT+2);
      return;
   } /* End switch on order code */
}
