/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************
*
*        FACTORY TO CREATE WAIT MULTIPLEXORS
*
*
*        COMPONENTS NODE CONTAINS
*         0 - SUPERNODE CREATOR
*         2 - RETURNER
*         3 - FRESH SEGMENT CREATOR
*         4 - DISCRIM
*         5 - CLOCK
*         6 - KernelP 
*
*        THERE WILL BE MANY DOMAINS INVOLVED
*
*        1 THE MBWAIT DOMAIN WHICH WILL MANAGE THE
*          BWAIT QUE AND HANDLE BWAIT REQUESTS
*          (THIS DOMAIN)
*        2 THE WAIT DOMAIN WHICH WILL DO THE ACTUAL
*          WAITING
*        3-N DOMAINS WICH WILL ACT AS A GUARD FOR THE GUARD NODE
*          NODES, ONE PER GUARD
*
*        KEYS TO THIS DOMAIN CAN HAVE THE FOLLOWING DATABYTES:
*        DATABYTE 0 IS THE MBWAIT KEY ENTRY
*        DATABYTE 1 IS THE ENTRY FOR THE WAIT DOMAIN WHEN THE
*             REAL BWAIT TIMER EXPIRES
*        DATABYTE 2 IS LIKE 0 BUT WEAKER; IT WON'T DO KT+4.
*             EACH REGISTERED WAITER
*
*        THERE IS A BLOCK CALLED WAITENT FOR EACH WAIT KEY.
*        SLOTS IN THE SNODE ARE ALLOCATED IN PARALLEL WITH WAITENTS.
*        IF THERE IS A DOMAIN WAITING ON A WAIT KEY, ITS WAITENT
*        IS IN A PRIORITY QUEUE.
*        THE PRIORITY QUEUE IS STRUCTURED AS A LEFTIST TREE
*        (SEE KNUTH, ART OF COMPUTER PROGRAMMING, V.3 PP 150-152
*        AND EXERCISE 5.2.3.35)
*
*        WHEN THE WAIT DOMAIN WAKES UP AT THE EXPIRATION OF THE CURRE
*        TIMER IT CALLS THIS DOMAIN WITH THE CLOCK VALUE AS A PARAMET
*        (THE CLOCK AT THE TIME THE DOMAIN WAKES UP).
*        THEN ANY WAITENTS IN THE PRIORITY QUEUE WHOSE TIMER VALUE
*        HAS EXPIRED IS REMOVED FROM THE QUEUE AND WOKEN UP.
*
*        ALL DOMAINS SHARE THE SAME MEMORY TREE, VIZ.:
*
*             0-FFFF    CODE
          10000- 10FFF   Kernel page
*        100000-10FFFF   STACK (all storage on stack)
*        200000-EFFFFF  TIMER QUE ENTRIES
********************************************************************/
#include "kktypes.h"
#include "keykos.h"
#include "domain.h"
#include "sb.h"
#include "node.h"
#include "snode.h"
#include "fs.h"
#include "dc.h"
#include "lli.h"
#include "kernelp.h"
 
#define FIRSTSLOT   0x00200000
#define MAXSLOT     0x00EFFFFF
#define MBWAIT2TY   0x425
/********************************************************
*          KEY SLOTS USED IN THREE DOMAINS
*************************************************************/
   KEY CNODE    = 0;   /* COMPONENTS NODE                   */
   KEY BWAIT    = 1;   /* CALLER SUPPLIED WAIT KEY          */
   KEY CALLER   = 2;
   KEY DOMKEY   = 3;   /* THIS DOMAIN KEY                   */
   KEY PSB      = 4;   /* A SPACE BANK (PROMPT)             */
   KEY M        = 5;   /* A METER                           */
   KEY DC       = 6;   /* CREATOR OF THIS DOMAIN            */
   KEY MBWAIT   = 7;   /* DB=0 IN MAIN, DB=1 IN WAITER      */
   KEY EXSNODE  = 8;   /* SNODE OF EXIT KEYS                */
   KEY RETURNER = 9;
   KEY SAVENODE = 10;  /* Place to save stuff               */
   KEY WAITDOM  = 11;  /* DOMAIN KEY FOR WAITER             */
   KEY K0       = 12;  /* don't move this  not 15           */
   KEY K1       = 13;  /* leave this + 2 slots scratch      */
   KEY K2       = 14;  /* as they are used by FORK          */
   KEY K3       = 15;  /* returned DOMAIN key from FORK1    */
   KEY K15      = 15;
/*************************************************************
*        COMPONENT NODE DEFINITIONS
**************************************************************/
#define CNODESNODEC     0  /*  SUPER NODE CREATOR            */
#define CNODERETURNER   2  /*  THE RETURNER                  */
#define CNODEFSC        3  /*  FRESH SEGMENT CREATOR         */
#define CNODEDISCRIM    4  /*  DISCRIM                       */
#define CNODECLOCK      5  /*  clock key                     */
#define CNODEKERNELP    6  /*  Kernel page                   */
 
#define SAVEWAITDOM     0  /*  DOMAINKEY FOR WAITER DOMAIN   */
#define SAVECALLER      1  /*  SAVE THE CALLER WHEN NEEDED   */
#define SAVEGUARDSNODE  3  /*  SNODE OF GUARD DOMAIN KEYS    */
#define SAVEK0          4  /*  callers K0                    */
/*************************************************************/
 
  struct waitent {
     LLI tod;
     struct waitent *left;   /* also flink for free chain */
     struct waitent *right;
     struct waitent *up;
     SINT16 dist;
     UCHAR  wait;
     UCHAR  unused;
     LLI  filler;
#define WAITM  0x80
#define ALLOC  0x01
  };
 
  struct work {
     struct {
        struct waitent *callent;
        LLI todw;
     } parms;
     struct waitent *freehd;
     struct waitent *freenext;
     struct waitent *treeroot;
     UCHAR zapflag;
     struct SNode_Slot sns;
     struct {
       UINT32 obcount;
       UINT32 topslot;
     } stats;
     LLI restarttod;  /* my idea of when restart was */
     LLI systimeoffset;
     struct KernelPage *KP;  /* the kernel page */
       /* w->KP->KP_RestartTOD is SYSTIME at IPL */
  };
 
  UINT32 doprocess();
  struct waitent *merge();
  
    int stacksiz=4096;
 
    char title[]="MBWAIT2C";
 
factory(factoc,factord)
  UINT32 factoc,factord;
{
  JUMPBUF;
  static struct Domain_DataByte ddb1={1};
  static struct Domain_DataByte ddb2={2};
  static struct Node_DataByteValue ndb4={4};
  static struct Node_KeyValues lss5data = {3,14,
        {WindowM(0,0x00000100,2,0,0),
         WindowM(0,0x00000200,2,0,0),
         WindowM(0,0x00000300,2,0,0),
         WindowM(0,0x00000400,2,0,0),
         WindowM(0,0x00000500,2,0,0),
         WindowM(0,0x00000600,2,0,0),
         WindowM(0,0x00000700,2,0,0),
         WindowM(0,0x00000800,2,0,0),
         WindowM(0,0x00000900,2,0,0),
         WindowM(0,0x00000A00,2,0,0),
         WindowM(0,0x00000B00,2,0,0),
         WindowM(0,0x00000C00,2,0,0)}
  };
  UINT32 returncode;
  UINT32 oc,rc;
  SINT16 db;
  struct work w;
  w.KP=(struct KernelPage *)0x10000;
  w.restarttod.hi=-1;
  w.restarttod.low=-1;	
 
   KC (PSB,SB_CreateNode) KEYSTO(SAVENODE);
   KC (CNODE,Node_Fetch+CNODERETURNER) KEYSTO(RETURNER);
   KC (CNODE,Node_Fetch+CNODESNODEC) KEYSTO(EXSNODE);
   KC (EXSNODE,SNodeF_Create) KEYSFROM(PSB,M,PSB) KEYSTO(K1);
   KC (SAVENODE,Node_Swap+SAVEGUARDSNODE) KEYSFROM(K1);
   KC (EXSNODE,SNodeF_Create) KEYSFROM(PSB,M,PSB) KEYSTO(EXSNODE);
/**************************************************************
*        BUILD MEMORY TREE
**************************************************************/
   KC (DOMKEY,Domain_GetMemory)    KEYSTO(K2);    /* an LSS=5 node */
   KC (K2,Node_WriteData)          STRUCTFROM(lss5data);
   KC (CNODE,Node_Fetch+CNODEFSC)  KEYSTO(K0);
   KC (K0,FSF_Create) KEYSFROM(PSB,M,PSB) KEYSTO(K0);
   KC (K2,Node_Swap+2)             KEYSFROM(K0);
   KC (K2,Node_Fetch+0)            KEYSTO(K0);
   KC (PSB,SB_CreateNode)          KEYSTO(K1);
   KC (K1,Node_Swap+0)             KEYSFROM(K0); 
   KC (CNODE,Node_Fetch+CNODEKERNELP) KEYSTO(K0);
   KC (K1,Node_Swap+1)             KEYSFROM(K0);
   KC (K1,Node_MakeNodeKey) STRUCTFROM(ndb4) KEYSTO(K1);
   KC (K2,Node_Swap+0)   KEYSFROM(K1);
 
   returncode=0;   /* in case exit */
   w.stats.obcount=0;
   w.stats.topslot=0;
   w.freenext = (struct waitent *)FIRSTSLOT;
   w.freehd=0;
   setlow(&w);        /* initialize BWAIT key */
   KC (DOMKEY,Domain_MakeStart)    STRUCTFROM(ddb1) KEYSTO(MBWAIT);
/********************************************************************
*        BUILD WAITER DOMAIN
********************************************************************/
   if(!(rc=fork())) {   /* the WAITER domain, begins immediately */
      for (;;) {
        KC (BWAIT,0) RCTO(rc);       /* wait, initially long */
        KC (MBWAIT,rc) RCTO(rc);     /* inform master */
      }
   }
   if(rc > 1) {
       returncode=2;   /* exit with no space */
       goto getout;
   }
   KC (SAVENODE,Node_Swap+SAVEWAITDOM) KEYSFROM(K15);
 
   w.zapflag=0;
 
   KC (DOMKEY,Domain_MakeStart) KEYSTO(MBWAIT);
   LDEXBL (CALLER,0) KEYSFROM(MBWAIT,DOMKEY);  /* domkey for debug */
 
  for(;;) {           /* main loop */
     LDENBL OCTO(oc) STRUCTTO(w.parms) KEYSTO(K0,,,CALLER) DBTO(db);
     checks(&w);
     RETJUMP();
     checks(&w);
     KC (SAVENODE,Node_Swap+SAVEK0) KEYSFROM(K0);
     if (db == 1) {   /* this is the waiter calling */
       if(dowaiter(&w,oc))    break;    /* death */
       if(w.zapflag)          break;    /* death */
       LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER);
       continue;
     }
     if(w.zapflag) {
       LDEXBL (RETURNER,KT+1) KEYSFROM(,,,CALLER);
       continue;
     }
     if (!db) {        /* strong entry */
       if(oc==KT+4) {
          w.zapflag=1;
          KC (SAVENODE,Node_Swap+SAVECALLER) KEYSFROM(CALLER)
              KEYSTO(CALLER);
          w.parms.todw.hi=0;
          w.parms.todw.low=0;
          KC (BWAIT,1) STRUCTFROM(w.parms.todw) RCTO(rc); /*wakeup*/
          LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER);
          continue;
       }
     }
     if (oc==KT) {   /* request for alleged type */
       LDEXBL (RETURNER,MBWAIT2TY) KEYSFROM(,,,CALLER);
       continue;
     }
     if (oc==KT+6) { /* request for weaker key */
       KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ddb2) KEYSTO(K0);
       LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
       continue;
     }
     LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER);  /* default */
     switch (oc) {
       case 0:  break;                       /* wait */
       case 1:  settod(&w);continue;         /* set tod */
       case 2:  setinterval(&w);continue;    /* set interval */
       case 3:  settod(&w);break;            /* set tod and wait */
       case 4:  setinterval(&w);break;       /* set interval and wait */
       case 5:                               /* sense counts */
         LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER)
            STRUCTFROM(w.stats);
         continue;
       case 6:                               /* register */
         if(!(oc=doregister(&w)))
         LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER)
            CHARFROM(&w.parms.callent,4);
         else LDEXBL (RETURNER,oc) KEYSFROM(,,,CALLER);
         continue;
       case 7:  doderegister(&w);continue;   /* deregister */
/*     case 8:                                  collect trash */
       default:
          LDEXBL (RETURNER,KT+2) KEYSFROM(,,,CALLER);
          continue;
     }   /* end switch on OC, fall thru to WAIT */
/*****************************************************************
   Wait Place
*****************************************************************/
     if(w.parms.callent->wait & WAITM) {
        LDEXBL (RETURNER,1) KEYSFROM(,,,CALLER);
        continue;
     }
     w.sns.Slot=getslot(&w,w.parms.callent);
     KC (EXSNODE,SNode_Swap) STRUCTFROM(w.sns) KEYSFROM(CALLER)
        KEYSTO(CALLER);
     w.parms.callent->wait |= WAITM;
     enque(&w,w.parms.callent);
     setlow(&w);
     LDEXBL (RETURNER,0) KEYSFROM(,,,CALLER);
  }  /* end of for(;;) loop */
getout: 
/*  exit forever loop means death */
 
  zapall(&w);    /* kill all guards, start all waiters */
  zapwaiter(&w); /* kill waiter */
 
  KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
 
  KC (K0,Node_Fetch+2) KEYSTO(K0);  /* FSC */
  KC (K0,KT+4) RCTO(rc);
 
  KC (SAVENODE,Node_Fetch+SAVEGUARDSNODE) KEYSTO(K1);
  KC (K1,KT+4) RCTO(rc);
  KC (EXSNODE,KT+4) RCTO(rc);
  KC (PSB,SB_DestroyNode) KEYSFROM(SAVENODE) RCTO(rc);
     /* return to prologue for death */
  exit(returncode);
}  /* end main */
/*******************   BEGIN SUBROUTINES ***********************/
 
/***************************************************************
  SETINTERVAL   sets timer for entry based on interval
****************************************************************/
setinterval(w)
  struct work *w;
{
    JUMPBUF;
    UINT32 rc;
    LLI tod;
 
    KC (CNODE,Node_Fetch+CNODECLOCK) KEYSTO(K0);
    KC (K0,4) STRUCTTO(tod) RCTO(rc);
 
    llilsl(&w->parms.todw,12);    /* convert interval to ticks */
    lliadd(&w->parms.todw,&tod);
    settod(w);
    return 0;
}
 
/***************************************************************
  SETTOD        sets timer for entry based on absolute TOD
****************************************************************/
settod(w)
  struct work *w;
{
    UINT32 rc;
 
    if(w->parms.callent->wait & WAITM) {
       deque(w,w->parms.callent);
       w->parms.callent->tod=w->parms.todw;
       enque(w,w->parms.callent);
       setlow(w);
    }
    else {
       w->parms.callent->tod=w->parms.todw;
    }
    return 0;
}
/*******************************************************************
*  DOREGISTER  - gets a guard domain and prepares it for waiting
*******************************************************************/
doregister(w)
   struct work *w;
{
   JUMPBUF;
   UINT32 oc,rc;
 
   KC (SAVENODE,Node_Fetch+SAVEK0) KEYSTO(K0);
   KC (CNODE,Node_Fetch+CNODEDISCRIM) KEYSTO(K3);
   KC (K3,0) KEYSFROM(K0) RCTO(rc);  /* test WAIT supplied key */
   if (rc != 3) return 2;
   getent(w);   /* puts pointer in w->parms.callent */
 
   if(!fork1()) {  /* the guard domain */
      KC (MBWAIT,7) CHARFROM(&w->parms.callent,4);
   }  /* never returns */
 
   w->sns.Slot=getslot(w,w->parms.callent);  /* gets slot for waitent */
   KC (SAVENODE,Node_Fetch+SAVEGUARDSNODE) KEYSTO(K1);
   KC (K1,SNode_Swap) STRUCTFROM(w->sns) KEYSFROM(K15);
   KC (K15,Domain_MakeReturnKey) KEYSTO(K1) RCTO(rc);
   KC (SAVENODE,SAVEK0) KEYSTO(K0);
   KC (K0,Node_Swap+0) KEYSFROM(K1);  /* put resume key in node */
   return 0;
}
/*******************************************************************
*  DODEREGISTER  - not much more than ZAPGUARD
*******************************************************************/
doderegister(w)
   struct work *w;
{
   zapguard(w,w->parms.callent);
   return 0;
}
/*******************************************************************
*  ZAPGUARD  - zaps the guard domain for a registered entry
*******************************************************************/
zapguard(w,we)         /* zap guard for we */
   struct work *w;
   struct waitent *we;
{
   JUMPBUF;
   UINT32 oc,rc;
 
   if(we->wait & WAITM) {
      we->wait &= ~WAITM;
      deque(w,we);
      w->sns.Slot=getslot(w,we);
      KC (EXSNODE,SNode_Fetch) STRUCTFROM(w->sns) KEYSTO(K1);
      LDEXBL (RETURNER,KT+1) KEYSFROM(,,,K1);
      FORKJUMP();
   }
   w->sns.Slot=getslot(w,we);
   freeque(w,we);
   KC (SAVENODE,Node_Fetch+SAVEGUARDSNODE) KEYSTO(K0);
   KC (K0,SNode_Fetch) STRUCTFROM(w->sns) KEYSTO(K0);
   zapdom(w);  /* kill domain in k0 */
   return 0;
}
/*******************************************************************
*  ZAPWAITER   - zaps the domain waiting on the BWAIT key
*
*   This code assumes the structure in CFSTART
*******************************************************************/
zapwaiter(w)          /* kill off waiter domain  */
   struct work *w;
{
   JUMPBUF;
   UINT32 oc,rc;
 
  KC (SAVENODE,Node_Fetch+SAVEWAITDOM) KEYSTO(K0);
  zapdom(w);
  return 0;
}
/*******************************************************************
*  ZAPDOM  - zaps the domain with key in K0
*
*   This code assumes the structure in CFSTART
*******************************************************************/
zapdom(w)                /* domain key in k0 */
   struct work *w;
{
   JUMPBUF;
   UINT32 oc,rc;
 
   KC (K0,Domain_MakeBusy) RCTO(rc);  /* stop domain */
   KC (K0,Domain_GetMemory) KEYSTO(K1);
   KC (K1,Node_Fetch+1) KEYSTO(K2);
   KC (PSB,SB_DestroyPage) KEYSFROM(K2);
   KC (PSB,SB_DestroyNode) KEYSFROM(K1);
   KC (DC,DC_DestroyDomain) KEYSFROM(K0,PSB);
   return 0;
}
/***************** THE WAITER DOMAIN CALLED. *************************
* R1 HAS THE RETURN CODE FROM THE BWAIT(0) CALL.
*
*        THIS IS THE ENTRY FROM THE WAITER SIGNIFYING THAT A TIMER HAS
*        GONE OFF. THE ENTRY ON THE TOP OF THE HEAP IS CHECKED TO SEE
*        IF IT SHOULD BE DISPATCHED.  IF SO IT IS REMOVED FROM THE HEAP
*        THE EXIT KEY IS FORKED TO AND THE PROCESS REPEATED
*********************************************************************/
dowaiter(w,bwaitrc)
  struct work *w;
  UINT32 bwaitrc;
{
   JUMPBUF;
   LLI todw;
   struct waitent *we;
   UINT32 rc;
 
   KC (CNODE,Node_Fetch+CNODECLOCK) KEYSTO(K0);
   KC (K0,4) STRUCTTO(todw) RCTO(rc);
/*******************************************************************
*        WAKE UP ALL TIMERS BEFORE (OR EQUAL TO) THE VALUE IN TODW.
*******************************************************************/
   while (we=w->treeroot) {  /* correct use of "=" */
     if(llicmp(&todw,&we->tod)>= 0) {   /* todw >= enttod */
       if(!(we->wait & WAITM)) crash("on queue, not waiting");
       we->wait &= ~WAITM;
       deque(w,we);
       w->sns.Slot=getslot(w,we);
       KC (EXSNODE,SNode_Fetch) STRUCTFROM(w->sns) KEYSTO(K0) RCTO(rc);
       LDEXBL (RETURNER,0) KEYSFROM(,,,K0);
       FORKJUMP();
     }
     else break;   /* no more that quailfy */
   }
#ifdef xxx
   if(bwaitrc) return 1;       /* signal death wanted */
#endif
   setlow(w);                  /* reset bwait key */
   return 0;
}
/*******************************************************************
*   ZAPALL  -  Zaps all the guard domains for registered entries
*******************************************************************/
zapall(w)
  struct work *w;
{
   struct waitent *we;
 
   we=(struct waitent *)FIRSTSLOT;
   while((UINT32)we < (UINT32)w->freenext) {
     if(we->wait & ALLOC) zapguard(w,we);
     we++;
   }
   return 0;
}
getent(w)
  struct work *w;
{
  static struct waitent zwe= {{0,0},0,0,0,0,0};
  struct waitent *we;
 
  if(we=w->freehd) {  /* correct use of "=" */
     w->freehd=we->left;     /* free entries linked on left */
  }
  else {              /* must allocate new entry */
    we=w->freenext;
    w->freenext++;
    w->stats.topslot++;
  }
  w->stats.obcount++;
  *we=zwe;
  we->wait |= ALLOC;
  w->parms.callent=we;
  return 0;
}
freeque(w,we)
  struct work *w;
  struct waitent *we;
{
  if(we->wait & WAITM) crash("freeing waiting entry");
  if(!(we->wait & ALLOC)) crash("freeing unallocated entry");
  we->wait=0;
  we->left=w->freehd;
  w->freehd=we;
  w->stats.obcount--;
  return 0;
}
getslot(w,we)
  struct work *w;
  struct waitent *we;
{
  return we-(struct waitent *)FIRSTSLOT;
}
/*
*        SUBROUTINES TO HANDLE THE PRIORITY QUEUE.
*
*        ENQUE
*
*        PUT ENTRY INTO HEAP
*/
enque(w,we)
  struct work *w;
  struct waitent *we;
{
  struct waitent *p,*q;
 
  we->left=0;
  we->right=0;
  we->dist=1;
  p=w->treeroot;
  q=we;
  w->treeroot=merge(w,p,q);
  checks(w);
  return 0;
}
checks(w)
  struct work *w;
{
  if(w->treeroot) {
    if(w->treeroot->up) crash("root has ancestor");
    if(!(w->treeroot->wait & WAITM)) crash("root not waiting");
  }
  return 0;
}
/*
*        DEQUE
*
* SOME DEFINITIONS TO AID IN THE PROOF OF DEQUE
*
* UWF(P,Q) MEANS:
*        P = NIL OR
*        (LET T BE LEFT OR RIGHT SUCH THAT T(P) = Q.
*         LET ^T BE LEFT OR RIGHT SUCH THAT T ^= ^T.
*         ^T(P) IS WELL FORMED
*         AND IF ^T(P) ^= NIL THEN KEY(P) >= KEY(^T(P))
*                                  AND UP(^T(P)) = P FI
*         AND DIST(P) = 1 + MIN(DIST(LEFT(P)), DIST(RIGHT(P)))
*         AND UWF(UP(P), P) ).
*
* THEOREM:  IF P IS WELL FORMED AND P ^= NIL AND UWF(UP(P), P)
*        THEN UWF(P, LEFT(P)) AND UWF(P, RIGHT(P)).
*
* AT EQUILIBRIUM WE ALWAYS HAVE:
*        TREEROOT IS WELL FORMED AND
*        (ROOT = NIL OR UP(ROOT) = NIL).
* AT ENTRY TO DEQUE WE ASSUME R10 IS IN THE TREE, I.E. THERE EXIST
*        Ti (RIGHT OR LEFT) SUCH THAT
*        Tn(Tn-1(...T1(ROOT)...)) = R10 AND R10 ^= NIL.
*        n MAY BE ZERO.
* IT FOLLOWS BY INDUCTION ON n THAT
*        R10 IS WELL FORMED AND UWF(UP(R10), R10).
*/
deque(w,we)
  struct work *w;
  struct waitent *we;
{
  struct waitent *p,*q,*r;
  SINT16 dist0,dist4;
 
  p=we->left;
  q=we->right;
  p=merge(w,p,q);   /* we is now out of the tree */
  q=we->up;         /* ancestor */
/* ASSERT UWF(Q, R10) AND P IS WELL FORMED AND     */
/* (P = NIL OR UP(P) = NIL).                       */
  if(!q)  {  /* removed the root */
    w->treeroot=p;
  }
  else {
    if(we == q->left) q->left=p;
    else q->right=p;
    if(p) p->up=q;
/* ASSERT UWF(UP(Q), Q) AND A FEW OTHER THINGS. */
/* ADJUST THE DIST FIELDS IN THE ANCESTORS.     */
    for(;;) {
      p=q->left;
      if(p) dist0=p->dist;
      else  dist0=0;
      r=q->right;
      if(r) dist4=r->dist;
      else dist4=0;
      if(dist0 < dist4) {
         q->left=r;
         q->right=p;
         dist4=dist0;
      }
      dist4++;
      if(dist4==q->dist) break;
      q->dist=dist4;
      q=q->up;
      if(!q) break;
    }
  }
  checks(w);
}
/*
*        MERGE TWO DISJOINT LEFTIST TREES
*
* R4 HAS RETURN ADDRESS.
* THIS PROGRAM BEARS A SUPERFICIAL RESEMBLANCE TO THE ANSWER TO
*   EXERCISE 5.2.3.32 IN KNUTH.
* NOTE: THERE ARE TWO BUGS IN KNUTH'S ALGORITHM (IN THE FIRST EDITION)
*
* SOME DEFINITIONS TO AID IN THE PROOF OF CORRECTNESS OF THE PROGRAM.
*
* A LEFTIST TREE P IS WELL FORMED IFF
*        P = NIL OR
*        (LEFT(P) IS WELL FORMED
*        AND RIGHT(P) IS WELL FORMED
*        AND IF LEFT(P) ^= NIL THEN KEY(P) >= KEY(LEFT(P))
*                                   AND UP(LEFT(P)) = P FI
*        AND IF RIGHT(P) ^= NIL THEN KEY(P) >= KEY(RIGHT(P))
*                                   AND UP(RIGHT(P)) = P FI
*        AND DIST(P) = 1 + DIST(RIGHT(P))
*        AND DIST(LEFT(P)) >= DIST(RIGHT(P))  ).
*
* KEY(P) IS MINUS ENTTOD(P).  (WE WANT THE SMALLEST FIRST.)
*
* LET UP**K(R) DENOTE UP(UP(... K TIMES ... UP(R) ... )).
* OF COURSE UP**0(R) = R.
* DIST(NIL) = 0.   (FOLLOWING KNUTH)
*
* LET I1 BE THE ASSERTION
*        UP**K(R) = NIL
*        AND (FOR ALL I SUCH THAT 0 <= I < K,
*             LET Z = UP**I(R);
*             LEFT(Z) IS WELL FORMED
*             AND IF UP(Z) ^= NIL THEN KEY(UP(Z)) >= KEY(Z) FI
*             AND IF LEFT(Z) ^= NIL THEN
*                    KEY(Z) >= KEY(LEFT(Z))
*                    AND UP(LEFT(Z)) = Z FI )
*        AND P IS WELL FORMED
*        AND IF K > 0 AND P ^= NIL THEN KEY(R) >= KEY(P) FI.
*
* LET I2 BE THE ASSERTION
*        I1
*        AND Q IS WELL FORMED
*        AND IF K > 0 AND Q ^= NIL THEN KEY(R) >= KEY(Q) FI.
*/
struct waitent *merge(w,p,q)
  struct work *w;
  struct waitent *p,*q;
{
  struct waitent *r,*t;
  SINT16 dist0,dist4;
 
  r=0;
  while(q) {                                 /* q != 0 */
      if(!p) {t=q;q=p;p=t;continue;}         /* m2swap */
      if(llicmp(&p->tod,&q->tod)>0) {
         t=q;q=p;p=t;                        /* m2swap */
         continue;
      }
      else {
         p->up=r;
         r=p;
         p=p->right;                         /* (k := k+1) */
      }
  }                                          /* m2 loop */
  if(!p) return p;                     /* q and p = 0 */
  p->up=r;
  while(r) {
       q=r->left;
       if(!q) {r->left=p;p=q;}
       else {
         if(q->dist<p->dist) {r->left=p;p=q;}
       }
/*ASSERT I1 AND K > 0 AND IF P ^= NIL THEN UP(P) = R FI  */
/*        AND DIST(LEFT(R)) >= DIST(P).                  */
       r->right=p;
       dist0=1;
       if(p) dist0+=p->dist;
       r->dist=dist0;
       p=r;
       r=r->up;                         /* k := k-1) */
   }                                    /* m3 loop */
   return p;
/* ASSERT P IS WELL FORMED AND UP(P) = NIL. */
}
/******************************************************************
*        SETLOW
*
*        CALL BWAIT KEY WITH TOD OF LOWEST NON-ZERO VALUE
*        RETURNS CONDITION CODE NZ IF BWAIT KEY IS GONE.
*****************************************************************/
setlow(w)
  struct work *w;
{
   JUMPBUF;
   UINT32 rc;
 static LLI longtime = {0xFFFFFFFF,0xFFFFFFFF};
  LLI tod;
 
   if(w->treeroot) {
#ifdef xx
      if(llicmp(&(w->KP->KP_RestartTOD),&(w->restarttod))) { /* restarted. get offset */
#endif
         KC(CNODE,CNODECLOCK) KEYSTO(K0);
         KC(K0,9) STRUCTTO(w->systimeoffset);  /* get offset from clock  */
         w->restarttod=w->KP->KP_RestartTOD;
#ifdef xx
      }
#endif
      tod=w->treeroot->tod; 
      llisub(&tod,&(w->systimeoffset));
      KC (BWAIT,1) STRUCTFROM(tod) RCTO(rc);
   }
   else
      KC (BWAIT,1) STRUCTFROM(longtime) RCTO(rc);
}

