/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "kktypes.h"
#include "keykos.h"
#include "domain.h"
#include "bscr.h"
#include "ocrc.h"
 
   KEY comp    = 0;
   KEY sb2     = 1;
   KEY caller  = 2;
   KEY domkey  = 3;
   KEY sb      = 4;
   KEY meter   = 5;
   KEY domcre  = 6;
 
   KEY k0      = 7;
   KEY d1entry = 8;
   KEY dk0     = 9;
   KEY prod    = 10;
 
   int stacksiz=4096;
   char title[]= "BSCR    ";
 
   uint32 fork();
 
factory(oc,ord)
   UINT32 oc,ord;
{
    JUMPBUF;
/*  oc is the TYPE 0,1,2 */
 
/*  This domain becomes the initial producer domain
    and its clone becomes the initial consumer domain.
    Both domains self destruct after each is called the
    first time
*/
    UINT32 rc,limit;
    char type;
 
    if(oc > 2) return INVALIDOC_RC;
    type=oc;
    KC (domkey,Domain_MakeStart) KEYSTO(d1entry,dk0);
 
    if(!(rc=fork())) {  /* This is the initial consumer */
/* keep resume key out of CALLER so death does not use it */
       KC (d1entry,0) KEYSTO(,,,prod) RCTO(limit);  /* synchronize */
       for (;;) {  /* wait for initial use of consumer key */
          if(limit == KT) KC (prod,BSC_AKT)
                 CHARFROM(&type,1) KEYSTO(,,,prod) RCTO(limit);
          else if(limit > KT+1) KC (prod,INVALIDOC_RC) KEYSTO(,,,prod)
                 RCTO(rc);
          else break;
       }
       LDEXBL (d1entry,OK_RC) KEYSFROM(prod);
       FORKJUMP();
       return 0;  /* die once jump completes */
    }
    if(rc > 1) return 1;
 
/*  here begins the initial producer code */
 
    LDEXBL (dk0,OK_RC);         /* this is to syncronize the clone */
    LDENBL KEYSTO(,,,k0);
    RETJUMP();     /* wait for clone to call back first time */
 
/* now give both keys to requestor */
 
    KC (caller,0) KEYSFROM(k0) KEYSTO(,,,caller) RCTO(limit);
 
    for (;;) {   /* wait till first producer call */
      if(!limit) KC (caller,INVALIDOC_RC) KEYSTO(,,,caller) RCTO(limit);
      else if(limit == KT) KC (caller,BSP_AKT)
                 CHARFROM(&type,1) KEYSTO(,,,caller) RCTO(limit);
      else if(limit > KT+1) KC (caller,INVALIDOC_RC) KEYSTO(,,,caller)
                 RCTO(limit);
      else break;
    }
 
/*  now wait for initial consumer to call, then marry the users */
    LDEXBL(dk0,OK_RC);
    LDENBL KEYSTO(k0) OCTO(oc);
    RETJUMP();
 
/*  perform marriage */
    LDEXBL (k0,limit) KEYSFROM(,,,caller);
    FORKJUMP();
/* zap CALLER so that death does not use it */
    KC (domkey,Domain_GetKey+dk0) KEYSTO(caller);
    return 0;  /* die */
}
