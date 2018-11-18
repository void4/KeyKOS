/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "kktypes.h"
#include "keykos.h"
#include "domain.h"
#include "forkf.h"
#include "ocrc.h"
 
   KEY comp    = 0;
   KEY sb2     = 1;
   KEY caller  = 2;
   KEY domkey  = 3;
   KEY sb      = 4;
   KEY meter   = 5;
   KEY domcre  = 6;
 
   KEY k0      = 7;
   KEY k1      = 8;
   KEY k2      = 9;
   KEY k3      = 10;
   KEY dk0     = 11;
   KEY userkey  = 12;
 
   int stacksiz=8192;
   char title[]= "FORKF   ";
 
factory(oc,ord)
   UINT32 oc,ord;
{
   JUMPBUF;

   char usecall,reuse,fast;
   static struct Domain_DataByte db1={1};
   char parm[4096];
   int actlen;
   short db;
   UINT32 rc;
 
   fast=   (oc >> 12) & 0x0F;
   usecall= (oc >> 8) & 0x0F;
   reuse=   (oc >> 4) & 0x0F;
/* C version always accepts 4096 byte string */
 
   KC (domkey,Domain_MakeStart) KEYSTO(k0);
   if(reuse) KC (domkey,Domain_MakeStart) STRUCTFROM(db1)
                 KEYSTO(k1);
   LDEXBL (caller,0) KEYSFROM(k0,k1);
   for (;;) {
     LDENBL OCTO(oc) KEYSTO(k0,k1,k2,caller) CHARTO(parm,4096,actlen)
       DBTO(db);
     RETJUMP();
 /* call to action */
     if(db)  {  /* control entry */
        if(oc == KT) {
           LDEXBL (caller,ForkControl_AKT);
           continue;
        }
        if(!oc) {
           KC (domkey,Domain_SwapKey+userkey) KEYSFROM(k0);
           LDEXBL (caller,0);
           continue;
        }
        return 0;  /* bye */
     }
 /* doit entry */
     if(!fast) {     /* use the key in userkey */
        if(usecall) { /* call */
           KC (userkey,oc) KEYSFROM(k0,k1,k2) CHARFROM(parm,actlen)
              KEYSTO(k0,k1,k2,k3) CHARTO(parm,4096,actlen) RCTO(rc);
           LDEXBL (caller,rc) KEYSFROM(k0,k1,k2,k3)
                 CHARFROM(parm,actlen);
           if(reuse) continue;
           FORKJUMP();
           KC (domkey,Domain_GetKey+dk0) KEYSTO(caller);
           return 0;  /* done */
        }
        else {        /* fork */
           LDEXBL (userkey,oc) KEYSFROM(k0,k1,k2,caller)
              CHARFROM(parm,actlen);
           FORKJUMP();
           if(reuse) {LDEXBL (dk0,0); continue;}
           KC (domkey,Domain_GetKey+dk0) KEYSTO(caller);
           return 0;
        }
     }
     else {  /* key from K2 */
        if(usecall) { /* call */
           KC (k2,oc) KEYSFROM(k0,k1) CHARFROM(parm,actlen)
              KEYSTO(k0,k1,k2,k3) CHARTO(parm,4096,actlen) RCTO(rc);
           LDEXBL (caller,rc) KEYSFROM(k0,k1,k2,k3)
                 CHARFROM(parm,actlen);
           if(reuse) continue;
           FORKJUMP();
           KC (domkey,Domain_GetKey+dk0) KEYSTO(caller);
           return 0;  /* done */
        }
        else {        /* fork */
           LDEXBL (k2,oc) KEYSFROM(k0,k1,,caller)
              CHARFROM(parm,actlen);
           FORKJUMP();
           if(reuse) {LDEXBL (dk0,0); continue;}
           KC (domkey,Domain_GetKey+dk0) KEYSTO(caller);
           return 0;
        }
     }
  }  /* for ever */
}
