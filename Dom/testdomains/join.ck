/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

//#include "kktypes.h"
#include "domain.h"
//#include "snode.h"
#include "keykos.h"
#define maxstring 20

KEY   CALLER     = 2;
KEY   DOMKEY   = 3; // From factory
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
   while(1){  // Once per message
     int oc, mc, len;
     char strng[sz];
     KC (CALLER,0) KEYSFROM(me)
          OCTO(mc) CHARTO(strng, sz, len) KEYSTO(K0,K1,K2,K3);
     LDEXBL (DKZ, 0);
     while(1){LDENBL OCTO(oc) KEYSTO(,,,CALLER);
       RETJUMP();
       if(oc == 1) LDEXBL (CALLER, mc) KEYSFROM(K0,K1,K2,K3)
               CHARFROM(strng, len < sz ? len : sz);
       else if(oc == 0x80000000) LDEXBL (CALLER, 0x31);
       else if(oc == 0x80000004) return;
       else if(oc == 0) break;
       else LDEXBL (CALLER, 0x80000002);
     }
   }
}
