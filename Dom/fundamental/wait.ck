/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************
 
         WAITF -  FACTORY TO CREATE WAIT KEYS
 
 
         COMPONENTS NODE CONTAINS
          0 - MBWAIT2
          2 - CLOCK
 
         THERE WILL BE 2 DOMAINS INVOLVED
 
         1 THE MAIN DOMAIN WHICH WILL MANAGE THE
           WAIT KEY CALLS.
         2 THE WAIT DOMAIN WHICH WILL DO THE ACTUAL
           WAITING
 
         KEYS TO THIS DOMAIN CAN HAVE THE FOLLOWING DATABYTES:
         DATABYTE 0 IS THE WAIT KEY ENTRY
         DATABYTE 1 IS THE WAIT DOMAIN ENTRY TO THIS DOMAIN
 
         ALL DOMAINS SHARE THE SAME MEMORY TREE:
 
              0-FFF    CODE
          10000-10FFF   WORK SPACE
*************************************************************/
#include <string.h>
#include "keykos.h"
#include "kktypes.h"
#include "sb.h"
#include "node.h"
#include "domain.h"
#include "dc.h"
#include "lli.h"
#include "wait.h"
#include "ocrc.h"
 
#define WAITTY    0x0025
 
   KEY CNODE      = 0;  /*           COMPONENTS NODE                */
   KEY GUARDNODE =  1;  /*           NODE TO GUARD US               */
   KEY CALLER    =  2;
   KEY DOMKEY    =  3;  /*           THIS DOMAIN KEY                */
   KEY PSB       =  4;  /*           A SPACE BANK (PROMPT)          */
   KEY M         =  5;  /*           A METER                        */
   KEY DC        =  6;  /*           CREATOR OF THIS DOMAIN         */
   KEY MBWAIT2   =  7;
   KEY WAITERKEY =  8;  /*  EXIT KEY TO THE GUY WAITING ON THIS KEY */
   KEY MAIN      =  9;  /*           ENTRY TO MAIN DOMAINEXIT       */
   KEY WAITDOMEXIT=10;  /*  EXIT KEY TO WAIT DOMAIN                 */
 
   KEY WAITDOM   = 12;  /*         Domain key to waitdom            */
   KEY K0        = 13;  /* must be scratch for FORK                 */
   KEY K1        = 14;  /* must be scratch for FORK                 */
   KEY K2        = 15;  /* must be scratch for FORK                 */
 
#define CNODEMBWAIT2    0
#define CNODECLOCK      2
 
  struct work {
     LLI todw;
     struct {
       UINT32 mbwaitid;
       LLI enttod;
     } waitid;
     UCHAR entwait;
#define ENTWAITM 0x80
#define ENTALLOC 0x01
     struct Wait_Value clocktime;
   };
 
   char title[]="WAIT    ";

   int stacksiz=4096;
 
factory(factoc,factord)
   UINT32 factoc,factord;
{
 JUMPBUF;
 static  struct Domain_DataByte ddb = {1};
 static  LLI maxtod = {0xFFFFFFFF,0xFFFFFFFF};
 struct work w;
 
   UINT32 rc,oc;
   SINT16 db;
 
 w.waitid.enttod=maxtod;    /* initialize to max time */
 w.entwait=0;               /* and not waiting */
 
 KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(MAIN);
 KC (CNODE,Node_Fetch+CNODEMBWAIT2) KEYSTO(MBWAIT2);
/*******************************************************************
*        REGISTER WITH MBWAIT2
********************************************************************/
 KC (PSB,SB_CreateNode) KEYSTO(GUARDNODE);
 KC (MBWAIT2,6) KEYSFROM(GUARDNODE) STRUCTTO(w.waitid,4); /* only 4 */
/****************************************************************
*    Generate Helper Domain
*****************************************************************/
 if (!(rc=fork())) { /* the helper, FORK1 leaves domain key in C15 */
     KC(DOMKEY,Domain_SwapKeeper);
     for(;;) {
        KC (MAIN,rc)   STRUCTTO(w.waitid) RCTO(rc);
        KC (MBWAIT2,3) STRUCTFROM(w.waitid) RCTO(rc);
     }
 }
 if(rc > 1) {
     KC (MBWAIT2,7) STRUCTFROM(w.waitid,4) RCTO(rc);  /* de-register */
     KC (PSB,SB_DestroyNode) KEYSFROM(GUARDNODE) RCTO(rc);
     exit(NOSPACE_RC);
 }
   
 KC (DOMKEY,Domain_GetKey+15) KEYSTO(WAITDOM,,K0);
 LDENBL OCTO(oc) KEYSTO(,,,WAITDOMEXIT);
 LDEXBL (K0,0);
 RETJUMP();            /* wait for helper to call back */
 KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
 LDEXBL (CALLER,0) KEYSFROM(K0);
 
 for (;;) {                 /* loop here forever */
   LDENBL OCTO(oc) STRUCTTO(w.todw) KEYSTO(,,,CALLER) DBTO(db);
   RETJUMP();
 
   if (!db)   {   /* user entry */
/*******************************************************************
*     WAIT KEY  Processing
********************************************************************/
      if(oc==KT+4) break;     /* done */
      if(oc==KT) {
        LDEXBL (CALLER,WAITTY);
        continue;
      }
      LDEXBL (CALLER,0);                  /* in case no error */
      switch(oc) {                        /* select ordercode */
        case Wait_Wait:           break;  /* go directly to wait */
        case Wait_SetTOD:         settod(&w);continue; /* set,ret */
        case Wait_SetTODAndWait:  settod(&w);break;    /* set,wait*/
        case Wait_SetInterval:    setinterval(&w);continue;
        case Wait_SetIntervalAndWait: setinterval(&w);break;
        case Wait_ShowTime:
               oc=(UINT32)showtime(&w);
               if(oc) LDEXBL (CALLER,oc);
               else LDEXBL (CALLER,0) STRUCTFROM(w.clocktime,31);
               continue;
        case Wait_ShowTOD:
               KC (CNODE,Node_Fetch+CNODECLOCK) KEYSTO(K0);
               KC (K0,4) STRUCTTO(w.todw);
               LDEXBL (CALLER,0) STRUCTFROM(w.todw);
               continue;
        default:                  {LDEXBL (CALLER,KT+2);continue;}
      }
/*******************************************************************
*        WAIT PLACE
********************************************************************/
     if(w.entwait & ENTWAITM){
       LDEXBL (CALLER,1);     /* RC=1 */
       continue;
     }
     KC (DOMKEY,Domain_SwapKey+WAITERKEY) KEYSFROM(CALLER)
          KEYSTO(,,CALLER);  /* get a DK0 */
     w.entwait |= ENTWAITM;
     LDEXBL (WAITDOMEXIT,0) STRUCTFROM(w.waitid);
     FORKJUMP();
     LDEXBL (CALLER,0);     /* start waiter domain */
     continue;              /* become available */
   }           /* end of user entry */
 
   else  {    /* waiter entry */
/******************************************************************
* THE WAITER DOMAIN CALLED.
* oc HAS THE RETURN CODE FROM THE MBWAIT(0) CALL.
*
*    THIS IS THE ENTRY FROM THE WAITER SIGNIFYING THAT A TIMER HAS
*    GONE OFF.
******************************************************************/
     if(!(w.entwait & ENTWAITM)) crash("Not waiting");
     w.entwait &= ~ENTWAITM;
     KC (DOMKEY,Domain_SwapKey+WAITDOMEXIT) KEYSFROM(CALLER)
        KEYSTO(,CALLER);   /* put helper exit away */
     LDEXBL (WAITERKEY,oc);  /* if dying, so indicate */
     FORKJUMP();           /* run the waiter */
     if(oc) break;         /* opps, destroy self */
     LDEXBL (CALLER,0);      /* return to oblivion */
     continue;
   }          /* end waiter */
 }
/******************************************************************
*  Fell out of loop.  Destroy self and helper
******************************************************************/
  KC (PSB,SB_DestroyNode) KEYSFROM(GUARDNODE) RCTO(rc);
  KC (WAITDOM,Domain_MakeBusy) RCTO(rc);           /* stop helper */
  KC (MBWAIT2,7) STRUCTFROM(w.waitid,4) RCTO(rc);  /* de-register */
  KC (WAITDOM,Domain_GetMemory) KEYSTO(K0);        /* return memory */
  KC (K0,Node_Fetch+1) KEYSTO(K1);
  KC (PSB,SB_DestroyPage) KEYSFROM(K1);
  KC (PSB,SB_DestroyNode) KEYSFROM(K0);
  KC (DC,DC_DestroyDomain) KEYSFROM(WAITDOM,PSB);  /* zap domain */
  LDEXBL (WAITERKEY,KT+1);   /* wait any waiter */
  FORKJUMP();
  exit(0);
}     /* end of main is destroy self */
 
/******************** subroutines **************************/
setinterval(w)
  struct work *w;
{
    JUMPBUF;
    UINT32 rc;
    LLI tod;
 
    KC (CNODE,Node_Fetch+CNODECLOCK) KEYSTO(K1);
    KC (K1,4) STRUCTTO(tod);
 
    llilsl(&w->todw,12);    /* microsecond interval to ticks */
    lliadd(&w->todw,&tod);
    settod(w);
    return 0;
}
 
settod(w)
  struct work *w;
{
    JUMPBUF;
    UINT32 rc;
 
    w->waitid.enttod=w->todw;
    if(w->entwait & ENTWAITM)
        KC (MBWAIT2,1) STRUCTFROM(w->waitid) RCTO(rc);
    return 0;
}
 
showtime(w)
  struct work *w;
{
    JUMPBUF;
   UINT32 rc;
 
/* if(!(w->entwait & ENTWAITM)) return 2;  */
   KC (CNODE,Node_Fetch+CNODECLOCK) KEYSTO(K1);
   KC (K1,5) STRUCTFROM(w->waitid.enttod)
             STRUCTTO(w->clocktime) RCTO(rc);
   return 0;
}
