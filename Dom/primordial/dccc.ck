/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports DCC and DC
 
  An attempt has been made to make the code compatible with
  a factory product AND a WOMB object.  The define symbol WOMB
  will generate the WOMB version if defined.  Else the factory
  version will be generated for testing.
 
*************************************************************/

#define WOMB 1

#include "keykos.h"
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "discrim.h"
#include "dcc.h"
#include "domtool.h"
#include "wombdefs.h"
 
 
   KEY COMP        = 0;    /* a node in wombboot mode */
   KEY RETURNER    = 1;
   KEY CALLER      = 2;    /* must be here for FORK */
   KEY DOMKEY      = 3;    /*          .            */
                           /* this is BRANDER of DC */
   KEY PSB         = 4;    /*          .            */
                           /* this is a parameter of caller */
   KEY METER       = 5;    /*          .            */
                           /* this is my meter */
   KEY DC          = 6;    /* must be here for FORK */
                           /* this is DCCDC of old code */
   KEY DOMTOOL     = 7;    /* Domain TOOL */
   KEY SBT         = 8;    /* only used by DCC */
   KEY MYSB        = 9;    /* my saved spacebank  */
 
   KEY ROOT        = 10;
   KEY KEYS        = 11;
   KEY REGS        = 12;
 
   KEY K2          = 13;   /* used by FORK and makedom */
   KEY K1          = 14;
   KEY K0          = 15;
 
#define COMPRETR       0
#define COMPDOMTOOL    1
#define COMPSBTDCBRAND 2  /* domain key of SBT DC */
#define COMPSBT        3  /* SBT */
#define COMPWOMBFACIL  4  /* WOMBFACIL for DOMKEEP */
 
    char title[]="DCC     ";
#ifdef WOMB
    long  bootwomb=1;
#endif
 
    void crash();
    UINT32 fork(),dodc(),zapdom(),testbank(),makedom(),doid();
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;   /* this is true for either version */
{
   JUMPBUF;
   UINT32 oc,rc;
 
   KC (COMP,COMPRETR) KEYSTO(RETURNER);
   KC (COMP,COMPDOMTOOL) KEYSTO(DOMTOOL);
   KC (COMP,COMPSBT) KEYSTO(SBT);
   if (factoc)  {     /* ordercode 1 means DC */
      dodc();          /* hard coded DCs don't return */
      crash("Basic DC returned");
   }
   KC (DOMKEY,64) KEYSTO(K0);
   LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
   for (;;) {  /* DCC loop */
     LDENBL OCTO(oc) KEYSTO(PSB,METER,,CALLER);
     RETJUMP();
 
     if(oc == KT) {
         LDEXBL (RETURNER,DCC_AKT) KEYSFROM(,,,CALLER);
         continue;
     }
 
     if(oc == 1) {   /* Identify */
       KC (DC,DC_IdentifyStart) KEYSFROM(PSB) RCTO(rc);
       LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
       continue;
     }
 
     if(oc == 0) {   /* Create a DC */
       KC (SBT,0) KEYSFROM(PSB) RCTO(rc);
       if(rc & 0x80) {
         LDEXBL (RETURNER,1) KEYSFROM(,,,CALLER);
         continue;
       }
       KC (COMP,COMPWOMBFACIL) KEYSTO(REGS);
       KC (REGS,WOMB_FACIL_RES) KEYSTO(REGS);
       KC (REGS,WOMB_FACIL_RES_DOMKEEP) KEYSTO(REGS);
       KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(REGS) KEYSTO(REGS);
       if(!(rc=fork())) {  /* new DC */
         rc=dodc();        /* DC, if return, dissolve */
         return rc;
       }
       KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(REGS) KEYSTO(REGS);
       rc=rc-1;            /* should be zero */
       if(!rc) KC (DOMKEY,Domain_SwapKey+CALLER); /* DC worked */
       else rc=KT+1;       /* play dead */
       LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
       continue;   /* if DCC return to DK0 */
     }
 
 /* bad OC */
     LDEXBL (RETURNER,KT+2) KEYSFROM(,,,CALLER);
     continue;
  }
}
/*************************************************************
   DC code
*************************************************************/
UINT32 dodc()
{
   JUMPBUF;
   UINT32 oc,rc,urc,parm;
   SINT16 db;
static struct Domain_DataByte ddb={1};
 
   KC (DOMKEY,Domain_GetKey+PSB) KEYSTO(MYSB);
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
   for (;;) {  /* DC loop */
     parm=0;
     LDENBL OCTO(oc) KEYSTO(K0,PSB,,CALLER) DBTO(db)
       STRUCTTO(parm);
     RETJUMP();
     if(oc==KT) {
       LDEXBL (RETURNER,DC_AKT) KEYSFROM(,,,CALLER);
       continue;
     }
 
     if(oc==KT+4) {  /* destroy self */
       if(db) {rc=KT+2;break;}
       KC (DOMKEY,Domain_GetKey+MYSB) KEYSTO(PSB);
       return 0;  /* return causes death */
     }
 
     if(oc >= 0x40000000) {  /* obsolete destroy caller */
       oc=oc-0x40000000;
       KC (DOMTOOL,DomTool_IdentifyResume) KEYSFROM(CALLER,DOMKEY)
          KEYSTO(ROOT) RCTO(rc);
       if(rc >= KT) break; /* not mine */
       rc=zapdom();    /* zaps ROOT using PSB */
       LDEXBL (RETURNER,oc) KEYSFROM(,,,K0);
       continue;
     }
 
     switch(oc) {           /* what to do, oh what to do? */
       case 0:  /* old create */
          if(rc=testbank()) break;
          if(rc=makedom()) break;
          LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
          continue;
       case DC_DestroyDomain:
          KC (DOMTOOL,DomTool_IdentifyDomain) KEYSFROM(K0,DOMKEY)
              KEYSTO(ROOT) RCTO(rc);
          if(rc >= KT) {rc=3;break;}
          rc=zapdom();  /* zaps ROOT using PSB */
          break;
       case DC_IdentifyStart:
          KC (DOMTOOL,DomTool_IdentifyStart) KEYSFROM(K0,DOMKEY)
            KEYSTO(ROOT) RCTO(urc);
          if(urc >= KT) {rc=0xFFFFFFFF;break;}
          KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(ROOT)
            KEYSTO(K0) RCTO(rc);
          LDEXBL (RETURNER,urc) KEYSFROM(K0,,,CALLER);
          continue;
       case DC_IdentifyResume:
          KC (DOMTOOL,DomTool_IdentifyResume) KEYSFROM(K0,DOMKEY)
            KEYSTO(ROOT) RCTO(urc);
          if(urc >= KT) {rc=0xFFFFFFFF;break;}
          KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(ROOT)
            KEYSTO(K0) RCTO(rc);
          LDEXBL (RETURNER,urc) KEYSFROM(K0,,,CALLER);
          continue;
       case 4:  /* old destroy me */
          if(db) {rc=KT+2;break;}
          KC (DOMKEY,Domain_GetKey+MYSB) KEYSTO(PSB);
          return 0; /* return causes death */
       case DC_IdentifySegment:
          rc=doid(DomTool_IdentifySegment,DomTool_IdentifyStart);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_IdentifySegmentWithResumeKeyKeeper:
          rc=doid(DomTool_IdentifySegmentWithResumeKeyKeeper,
                DomTool_IdentifyResume);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_IdentifySegmentWithDomainKeyKeeper:
          rc=doid(DomTool_IdentifySegmentWithDomainKeyKeeper,
                DomTool_IdentifyDomain);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_DestroyMe:
         KC (DOMTOOL,DomTool_IdentifyResume) KEYSFROM(CALLER,DOMKEY)
            KEYSTO(ROOT) RCTO(rc);
         if(rc >= KT) {rc=3;break;} /* not mine */
         rc=zapdom();
         LDEXBL (RETURNER,parm) KEYSFROM(,,,K0);
         continue;
       case DC_IdentifyMeter:
          rc=doid(DomTool_IdentifyMeter,DomTool_IdentifyStart);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_IdentifyMeterWithResumeKeyKeeper:
          rc=doid(DomTool_IdentifyMeterWithResumeKeyKeeper,
                DomTool_IdentifyResume);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_IdentifyMeterWithDomainKeyKeeper:
          rc=doid(DomTool_IdentifyMeterWithDomainKeyKeeper,
                DomTool_IdentifyDomain);
          LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
          continue;
       case DC_CreateDomain:
          KC (DOMKEY,Domain_GetKey+K0) KEYSTO(PSB);
          if(rc=testbank()) break;
          if(rc=makedom()) break;
          LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
          continue;
       case DC_Weaken:
          KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ddb)
            KEYSTO(K0) RCTO(rc);
          LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
          continue;
       case DC_SeverDomain:
          KC (DOMTOOL,DomTool_IdentifyStart) KEYSFROM(K0,DOMKEY)
              KEYSTO(ROOT) RCTO(rc);
          if(-1 == rc) {
             KC (DOMTOOL,DomTool_IdentifyResume) KEYSFROM(K0,DOMKEY)
                KEYSTO(ROOT) RCTO(rc);
             if (-1 == rc) {rc=3;break;}
          }
          KC (PSB,SB_SeverNode) KEYSFROM(ROOT) KEYSTO(ROOT) RCTO(rc);
          if(rc) break; 
          KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(ROOT) KEYSTO(K0)
             RCTO(rc);
          if(rc) break;
          LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
          continue;
       case DC_SeverMe:
          KC (DOMTOOL,DomTool_IdentifyResume) KEYSFROM(CALLER,DOMKEY)
            KEYSTO(ROOT) RCTO(rc);
          if(rc >= KT) {rc=3;break;} /* not mine */
          KC (PSB,SB_SeverNode) KEYSFROM(ROOT) KEYSTO(ROOT) RCTO(rc);
          if(rc) break; 
          KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(ROOT) KEYSTO(K0)
              RCTO(rc);  /* no CALLER key anymore.. bye bye */
          if(rc) break;
          KC (K0,Domain_MakeBusy) KEYSTO(CALLER) RCTO(rc);
          if(rc) break;  /* no CALLER key anymore.. bye bye */
          LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
          continue; 
       default:
         rc=KT+2;
         break;
     } /* DC oc SWITCH */
     LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
   } /* DC loop */
}
/***************************************************************
  ZAPDOM    zaps domain in ROOT using PSB
            calls testbank() which uses K2
***************************************************************/
UINT32 zapdom()
{
   JUMPBUF;
   UINT32 rc;
 
    if(testbank()) return 1;  /* untrusted bank */     KC (ROOT,Node_Fetch+9) 
    KEYSTO(KEYS) RCTO(rc);   /* floating point node */
    KC (PSB,SB_DestroyNode) KEYSFROM(KEYS) RCTO(rc);
    KC (ROOT,Node_Fetch+14) KEYSTO(KEYS) RCTO(rc);
    if(rc) return 4;
    KC (ROOT,Node_Fetch+15) KEYSTO(REGS) RCTO(rc);
    if(rc) return 4;
    KC (REGS,Node_Fetch+0) KEYSTO(K2) RCTO(rc);
    if(rc) return 4;
    KC (PSB,SB_DestroyThreeNodes) KEYSFROM(ROOT,KEYS,REGS) RCTO(rc);
    if(rc) return 2;
    KC (K2,Node_Fetch+0) KEYSTO(KEYS) RCTO(rc);
    KC (KEYS,Node_Fetch+0) KEYSTO(REGS) RCTO(rc);
    KC (PSB,SB_DestroyThreeNodes) KEYSFROM(K2,KEYS,REGS) RCTO(rc);
    return 0;
}
/*******************************************************************
*   TESTBANK()  tests bank in PSB returns 1 if bank no good        *
*               uses K2                                            *
*******************************************************************/
UINT32 testbank()
{
   JUMPBUF;
   UINT32 rc;
 
   KC (COMP,COMPSBTDCBRAND) KEYSTO(K2);
   KC (DOMTOOL,DomTool_IdentifySegment) KEYSFROM(PSB,K2) RCTO(rc);
   if (rc >= KT) {
     KC (DOMTOOL,DomTool_IdentifyStart) KEYSFROM(PSB,K2) RCTO(rc);
     if(rc >= KT) return 1;
   }
   return 0;
}
/*****************************************************************
  MAKEDOM()    returns domain key in K0  Bank in PSB
*****************************************************************/
UINT32 makedom()
{
   JUMPBUF;
   UINT32 rc;
 
   KC (PSB,SB_CreateThreeNodes) KEYSTO(ROOT,KEYS,K2) RCTO(rc);
   if(rc) return 2;
   KC (ROOT,Node_Swap+0) KEYSFROM(KEYS) RCTO(rc);
   KC (K2,Node_Swap+0) KEYSFROM(ROOT) RCTO(rc);
   KC (PSB,SB_CreateThreeNodes) KEYSTO(ROOT,KEYS,REGS) RCTO(rc);
   if (rc) {
      KC (PSB,SB_DestroyThreeNodes) KEYSFROM(ROOT,KEYS,K2) RCTO(rc);
      return 2;
   }
   KC (REGS,Node_Swap+0) KEYSFROM(K2) RCTO(rc);
   KC (ROOT,Node_Swap+0) KEYSFROM(DOMKEY) RCTO(rc);
   KC (ROOT,Node_Swap+14) KEYSFROM(KEYS)  RCTO(rc);
   KC (ROOT,Node_Swap+15) KEYSFROM(REGS)  RCTO(rc);
   KC (PSB,SB_CreateNode) KEYSTO(REGS) RCTO(rc);
   KC (ROOT,Node_Swap+9) KEYSFROM(REGS) RCTO(rc);  /* floating point node */
   KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(ROOT) KEYSTO(K0)
       RCTO(rc);
   return 0;
}
/*****************************************************************
  DOID(id1,id2)    returns domain key in K0, rc=databyte
                   returns node key   in K1
*****************************************************************/
UINT32 doid(ocid1,ocid2)
   UINT32 ocid1,ocid2;
{
   JUMPBUF;
   UINT32 rc,urc;
 
   KC (DOMTOOL,ocid1) KEYSFROM(K0,DOMKEY) KEYSTO(K1,,K0) RCTO(urc);
   if(urc >= KT) return urc;    /* K0 and K1 are DK0 */
   KC (K1,(urc>>8)) KEYSTO(K0) RCTO(rc);  /* fetch keeper */
   KC (DOMTOOL,ocid2) KEYSFROM(K0,DOMKEY) KEYSTO(K0) RCTO(rc);
   if(rc >= KT) return rc;      /* get node to keeper */
   KC (DOMTOOL,DomTool_MakeDomainKey) KEYSFROM(K0) KEYSTO(K0)
             RCTO(rc);          /* make domain key from node */
   return urc&0xFF;
}
