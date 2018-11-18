/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

//#include "kktypes.h"
#include "domain.h"
//#include "snode.h"
#include "keykos.h"
#define maxstring 20
/*This is a mere message buffer.
The first buffer invocation accepts a key to the recipient and returns.
The recipient is thereafter constant.
All subsequent invocations merely forward the entire message,
 (all four keys) to the recipient.
The buffer becomes available immediately after this delivery.
There is no command to delete this object.
This could be unified with the join object under the name
"Swiss Army Knife".
A start key to the buffer with another data byte could
provide means to delete the object.
*/

KEY   CALLER     = 2;
KEY   DOMKEY   = 3; // From factory
KEY   Recipient = 9; // Key to call.
KEY   me = 10; // Start key to me
KEY   DKZ = 11; // DK(0) to return to, to become available.
KEY   K0       = 12;
KEY   K1       = 13;
KEY   K2       = 14;
KEY   K3       = 15;
char title[]="JOIN    ";

SINT32 factory(unsigned int sz)
{
   JUMPBUF;
   KC (DOMKEY,Domain_MakeStart) KEYSTO(me, DKZ);
   if(sz > 4096) sz = 4096;
   if(!sz) sz = 20; // as promised
   LDENBL KEYSTO(Recipient,,,CALLER);
   LDEXBL (CALLER, 0) KEYSFROM(me); RETJUMP();
     {int oc, mc, len;
       char strng[sz];
       LDENBL OCTO(mc) CHARTO(strng, sz, len) KEYSTO(K0,K1,K2,K3);
       while(1){
         LDEXBL (CALLER, 0); RETJUMP();
         LDEXBL (Recipient, mc) KEYSFROM(K0,K1,K2,K3)
             CHARFROM(strng, len < sz ? len : sz);
         FORKJUMP();
     }
   }
}
