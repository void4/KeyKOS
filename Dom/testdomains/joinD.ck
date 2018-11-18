/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "domain.h"
#include "keykos.h"
#define maxstring 20
#define kt 0x80000000

KEY   CALLER     = 2;
KEY   DOMKEY   = 3; // From factory
KEY   me = 9; // Start key to me
KEY   DKZ = 10; // DK(0) to return to, to become available.
KEY   COMP = 0;
KEY   RETR     =1;
KEY   k1       = 4;
KEY   k2       = 5;
KEY   k3       = 6;
KEY   K1       = 11;
KEY   K2       = 12;
KEY   K3       = 13;
KEY   K4       = 14;
KEY   K5       = 15;
char title[]="JOIND   ";

SINT32 factory(unsigned int sz)
{
   JUMPBUF;
   KC (DOMKEY,Domain_MakeStart) KEYSTO(me, DKZ);
   KC (COMP, 0) KEYSTO(RETR);
   if(sz > 4096) sz = 4096;
   while(1){  // Once per invocation of DJ start key
     uint32 OC=0, mc, len, rl, save;
     char strng[sz];
     LDEXBL (CALLER,0) KEYSFROM(me);
     while(1){ // Once per invocation of DJ return key
       again: LDENBL OCTO(mc) CHARTO(strng, sz, len) KEYSTO(k1,k2,k3,CALLER); 
       CALLJUMP();
       switch(mc){
         case 1: LDEXBL (CALLER, OC) KEYSFROM(K1,K2,K3) CHARFROM(strng, rl);
            break;
         case 2: LDEXBL (CALLER, OC) KEYSFROM(K4,K5) CHARFROM(strng, rl);
            break;
         case 3: KC (RETR, 0) KEYSFROM(k1,k2,k3) KEYSTO(K1,K2,K3);
            LDEXBL (CALLER, 0);
            break;
         case 4: KC (RETR, 0) KEYSFROM(k1,k2) KEYSTO(K4,K5);
            LDEXBL (CALLER, 0);
            break;
         case 5: rl = len;
            save = *(uint32*)strng;
            LDEXBL (CALLER, 0);
            break;
         case 7: memcpy((void*)&OC, strng, 4);
            if(len != 4) {KFORK (CALLER, kt+2); return;}
            OC = *(uint32*)strng;
            *(uint32*)strng = save;
            LDEXBL (CALLER, 0);
            goto again;

         default:
            if (mc == 16) {
              LDEXBL (K5, OC) KEYSFROM(K1,K2,K3,K4) CHARFROM(strng, rl);
              LDENBL KEYSTO(K1,K2,K3,K4) CHARTO(strng, sz, rl) RCTO(OC);
              /* KC (K5, OC) KEYSFROM(K1,K2,K3) KEYSTO(K1,K2,K3,K4)
                CHARTO(strng, sz, rl) RCTO(OC); */
              RETJUMP();
              save = *(uint32*)strng;
              LDEXBL (CALLER, OC) CHARFROM(strng, rl);
              goto again;
            }
            else if (mc == kt+4) return;
            else LDEXBL (CALLER, kt+2);
       }
       if (len) {KFORK (CALLER, kt+2); return;}
     }
   }
 }
