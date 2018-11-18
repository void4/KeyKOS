/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define WOMB 1

/**************************************************************
  This code supports FCC, FC, F, and the initial code of O
 
  An attempt has been made to make the code compatible with
  a factory product AND a WOMB object.  The define symbol WOMB
  will generate the WOMB version if defined.  Else the factory
  version will be generated for testing.
 
  databyte 2 of FCC is not currently supported
 
*************************************************************/
#include "keykos.h"
#include "kktypes.h"
#include <string.h>
#include "wombdefs.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "factory.h"
#include "discrim.h"
#include "dcc.h"
#include "kid.h"
#include "ocrc.h"
 
   KEY WOMBFACIL = 0;
   KEY WOMBMEM   = 1;
 
   KEY COMP        = 0;   /* the equivalent of wombfacil for testing */
   KEY SB          = 1;   /* Space bank parameter */
   KEY CALLER      = 2;    /* must be here for FORK */
   KEY DOMKEY      = 3;    /*          .            */
   KEY PSB         = 4;    /*          .            */
   KEY METER       = 5;    /*          .            */
   KEY DC          = 6;    /* must be here for FORK */
   KEY UM          = 7;    /*                    */
                           /* Meter Parameter */
   KEY TOOLS       = 8;    /* symbolseg from WOMB  */
                           /* global tools  */
   KEY SBT         = 9;    /* SBT   */
                           /* privnode  from womb */
   KEY RETURNER    = 10;
   KEY PTOOLS      = 11;   /* private tools node for FC and F */
   KEY K3          = 12;   /* a scratch around FORK */
   KEY K0          = 13;
   KEY K1          = 14;
   KEY K2          = 15;

#define MEMNODEDOMKEY 2     /* Place to stash DOMKEY as brand */
 
#define TOOLSFCC   0        /* entry to FCC for use by FCs */
#define TOOLSFCDC  1        /* Domain creator for FCs */
#define TOOLSKIDC  2        /* Kid creator */
#define TOOLSSBT   3        /* SBT for testing banks */
#define TOOLSDCC   4        /* DCC */
#define TOOLSFDC   5        /* Domain creator for each factory */
#define TOOLSWOMBFACIL 6    /* WOMBFACIL for keepers */
#define TOOLSFIRSTDISCREET 7
#define TOOLSDKC           7
#define TOOLSRETURNER      8
#define TOOLSLASTDISCREET  9
#define TOOLSDISCRIM       9
#define TOOLSMETER        10
#define TOOLSKEEPER       15   /* for test environment */
 
#define PTOOLSPSB  0        /* original */
#define PTOOLSM    1        /* original */
#define PTOOLSDC   2        /* DC of object FC F */
#define PTOOLSKID  3
#define PTOOLSCOMP 4
#define PTOOLSKEEPER 5
#define PTOOLSPROGRAM 6
#define PTOOLSSYMBOLS 7
 
#define COMPSBT    0
#define COMPDCC    1
#define COMPDKC    2
#define COMPRETR   3
#define COMPDISCRIM 4
#define COMPKIDC    5
 
    char title[]="FCC     ";
 
    int fork();
    void repmem(),crash();
    UINT32 dofc();
    int issensory(),isfactory(),isfactoryc();
    void instcomp(UINT32,UINT32,UCHAR *,UINT32*);
    void getkids(),getcomp(UINT32);

    int dobuilder(UINT32,UINT32,UCHAR *,UINT32 *,UINT32 *,UINT32 *,
        UINT32 *,UCHAR *,struct Factory_GetMiscellaneousValues *);

    UINT32 dof(int,UINT32,UCHAR *,UINT32,UINT32);

    UINT32 dorequest(UINT32,int,UINT32,UINT32,UINT32,UCHAR *,UINT32 *);

    int dofetcher(UINT32,int,UINT32,UINT32,UINT32,
        UINT32 *,UCHAR *,struct Factory_GetMiscellaneousValues *);

    int docopy(UINT32,UINT32,int,UINT32,UINT32,UINT32,
        UINT32 *,UCHAR *,struct Factory_GetMiscellaneousValues *);
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;   /* this is true for either version */
{
   JUMPBUF;
   UINT32 oc,rc,rc1;
 
#ifdef WOMB                 /* get stuff from WOMBFACIL */
/*******************************************************************
*   Key 9 (SBT)   has PRIV node
*
*******************************************************************/
 
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_RES) KEYSTO(K0);
   KC (K0,Node_Fetch+WOMB_FACIL_RES_SB) KEYSTO(SB);
 
   KC (SB,SB_CreateNode) KEYSTO(TOOLS);
   KC (TOOLS,Node_Swap+TOOLSWOMBFACIL) KEYSFROM(WOMBFACIL);
 
   KC (SBT,Node_Fetch+6) KEYSTO(K1);   /* KIDC from PRIV */
   KC (TOOLS,Node_Swap+TOOLSKIDC) KEYSFROM(K1);
   KC (K0,Node_Fetch+WOMB_FACIL_RES_METER) KEYSTO(METER);
 
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_FUND) KEYSTO(K0);
   KC (K0,Node_Fetch+WOMB_FACIL_FUND_DISCRIM) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSDISCRIM) KEYSFROM(K1);
   KC (K0,Node_Fetch+WOMB_FACIL_FUND_RETR) KEYSTO(RETURNER);
   KC (TOOLS,Node_Swap+TOOLSRETURNER) KEYSFROM(RETURNER);
   KC (K0,Node_Fetch+WOMB_FACIL_FUND_DKC) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSDKC) KEYSFROM(K1);
   KC (K0,Node_Fetch+WOMB_FACIL_FUND_SBT) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSSBT) KEYSFROM(K1);
 
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_RES) KEYSTO(K1);
   KC (K1,Node_Fetch+WOMB_FACIL_RES_DOMKEEP) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSKEEPER) KEYSFROM(K1);
 
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_SUPP) KEYSTO(K0);
   KC (K0,Node_Fetch+WOMB_FACIL_SUPP_DCC) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSDCC) KEYSFROM(K1);
 
   KC (DOMKEY,Domain_GetKey+SB) KEYSTO(PSB);    /* finally PSB */
#endif
#ifndef WOMB                /* get stuff from components */
   KC (PSB,SB_CreateNode) KEYSTO(TOOLS);
 
   KC (DOMKEY,Domain_GetKeeper) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSKEEPER) KEYSFROM(K0);
 
   KC (COMP,Node_Fetch+COMPSBT) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSSBT) KEYSFROM(K0);
   KC (COMP,Node_Fetch+COMPDCC) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSDCC) KEYSFROM(K0);
   KC (COMP,Node_Fetch+COMPDKC) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSDKC) KEYSFROM(K0);
   KC (COMP,Node_Fetch+COMPRETR) KEYSTO(RETURNER);
   KC (TOOLS,Node_Swap+TOOLSRETURNER) KEYSFROM(RETURNER);
   KC (COMP,Node_Fetch+COMPDISCRIM) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSDISCRIM) KEYSFROM(K0);
   KC (COMP,Node_Fetch+COMPKIDC) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSKIDC) KEYSFROM(K0);
#endif
 
 /* common prolog */
 
   KC (TOOLS,Node_Fetch+TOOLSDCC) KEYSTO(K0);
   KC (K0,DCC_Create) KEYSFROM(PSB,METER) KEYSTO(K1);
   KC (TOOLS,Node_Swap+TOOLSFDC) KEYSFROM(K1);
   KC (K0,DCC_Create) KEYSFROM(PSB,METER) KEYSTO(DC);
   KC (TOOLS,Node_Swap+TOOLSFCDC) KEYSFROM(DC);
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   KC (TOOLS,Node_Swap+TOOLSFCC) KEYSFROM(K0);
   KC (TOOLS,Node_Swap+TOOLSMETER) KEYSFROM(METER);
   KC (TOOLS,Node_Fetch+TOOLSSBT) KEYSTO(SBT);
 
   KC (TOOLS,Node_MakeFetchKey) KEYSTO(TOOLS);  /* freeze */
   LDEXBL(RETURNER,0) KEYSFROM(K0,,,CALLER);
   for (;;) {   /* FCC LOOP */
     LDENBL OCTO(oc) KEYSTO(SB,UM,,CALLER);
     RETJUMP();
 
     if(oc==KT) LDEXBL (RETURNER,FCC_AKT) KEYSFROM(,,,CALLER);
     else if(oc) LDEXBL (RETURNER,KT+2)  KEYSFROM(,,,CALLER);
     else {
       /***************************************************
       *                  FCC                             *
       ***************************************************/
       KC (SBT,0) KEYSFROM(SB) RCTO(rc);
       if(!(rc & 0x80)) {        /* is official */
         KC (DOMKEY,Domain_SwapKey+PSB) KEYSFROM(SB);
         KC (TOOLS,Node_Fetch+TOOLSKEEPER) KEYSTO(K3);
         KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
 
         if (!(rc=fork())) {   /******** FC code ** = OK *****/
 
            /**************************************************
            *              FC                                 *
            *                                                 *
            *    Set up initial conditions before calling     *
            *    DOFC().  May run out of space as in FORK     *
            *                                                 *
            *    Return from DOFC() means that the FC is to   *
            *    dissolve.  This is done by returning from    *
            *    this main procedure (CFSTART then handles    *
            *    tearing down the domain). NOTE  DOFC will    *
            *    also return if DOF returns as there is       *
            *    similar code in DOF.                         *
            *                                                 *
            **************************************************/
 
            KC (DOMKEY,Domain_SwapMeter) KEYSFROM(UM);
            KC (DOMKEY,Domain_SwapKey+METER) KEYSFROM(UM);
            KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(DC); /* for factory */
            KC (DOMKEY,Domain_SwapKey+COMP);  /* drop this */
            KC (PSB,SB_CreateNode) KEYSTO(PTOOLS) RCTO(rc);
            if(!rc) {
              KC (PTOOLS,Node_Swap+PTOOLSM)   KEYSFROM(METER);
              KC (PTOOLS,Node_Swap+PTOOLSPSB) KEYSFROM(PSB);
              rc=dofc();    /* FC CODE */
 /*  It looks like  PTOOLS is always DK0 at this point */
 /*           KC (PTOOLS,Node_Fetch+PTOOLSM)   KEYSTO(METER);  */
 /*           KC (PTOOLS,Node_Fetch+PTOOLSPSB) KEYSTO(PSB);    */
              KC (TOOLS,Node_Fetch+TOOLSFDC)  KEYSTO(DC); /* for death*/
            }
            KC (PSB,SB_DestroyNode) KEYSFROM(PTOOLS) RCTO(rc1);
 
            /*************************************************
            *  The FC returned either because it recieved    *
            *  KT+4, failed OR F returned. (OR no space for  *
            *  FC's private node)                            *
            *  F is also a clone of this domain spawned in   *
            *  FC so it can return as well.                  *
            *************************************************/
 
            return rc;    /* death with rc */
         }                /********* end FC code ********/
         /****************************************************
         *   The FORK worked (rc=1) or the FORK failed (rc>1)*
         *   because of space problems.  If FORK worked then *
         *   the new domain has the CALLER key else we have  *
         *   it and must return to the caller                *
         ****************************************************/
         rc=rc-1;  /* transform  1=ok */
 
         KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
 
         if(!rc) KC (DOMKEY,Domain_SwapKey+CALLER);  /* drop this */
         LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
       }
       else LDEXBL(RETURNER,NONPROMPTSB_RC) KEYSFROM(,,,CALLER);
     }
   }  /* FCC LOOP */
#ifdef cantreach
   KC (PSB,SB_DestroyNode) KEYSFROM(TOOLS);
#endif
}
/*******************************************************************
*    FC code
*
*******************************************************************/
UINT32 dofc()
{
  JUMPBUF;
 
  struct Domain_DataByte ndb;
  UCHAR flags;
  UINT32 oc,rc,rc1;
  SINT16 db;            /* 0 unrestricted, 1 no builder recall */
                        /*                 2 no recall         */
  struct Factory_HoleCapacity holes;
  UCHAR ff[19] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 
  KC (PTOOLS,Node_Swap+PTOOLSDC)  KEYSFROM(DC);
  KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
  LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
  for (;;) {   /* FC loop */
    holes.Capacity=300;
    LDENBL OCTO(oc) KEYSTO(PSB,UM,SB,CALLER) STRUCTTO(holes)
      DBTO(db);
/*  UM holds  FDC if there is one */
    RETJUMP();
    if(oc==DESTROY_OC) {
      KC (PTOOLS,Node_Fetch+PTOOLSPSB) KEYSTO(PSB);
      KC (PTOOLS,Node_Fetch+PTOOLSM)   KEYSTO(METER);
      KC (PTOOLS,Node_Fetch+PTOOLSDC)  KEYSTO(DC);
      KC (PSB,SB_DestroyNode) KEYSFROM(PTOOLS);
      return 0;
    }
    if(oc==KT) {LDEXBL (CALLER,FC_AKT);continue;}
    if(oc == FC_DisableFetcher) {  /* Disable Recall */
      ndb.Databyte=DBFETCHER;
      if((int)db>(int)ndb.Databyte) ndb.Databyte=db;  /* can't make less */
      KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K0);
      LDEXBL (CALLER,OK_RC) KEYSFROM(K0);
      continue;
    }
    if(oc == FC_DisableBuilder) { /* Disable recall builder */
      ndb.Databyte=DBREQUESTOR;
      if((int)db>(int)ndb.Databyte) ndb.Databyte=db;  /* can't make less */
      KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K0);
      LDEXBL (CALLER,OK_RC) KEYSFROM(K0);
      continue;
    }
    if(oc == FC_RecallFetcher || oc == FC_RecallBuilder) {
      if((oc == FC_RecallFetcher) && (db == DBNORECALL))
                   {LDEXBL (CALLER,INVALIDOC_RC);continue;}
      if((oc == FC_RecallBuilder) && db)
                   {LDEXBL (CALLER,INVALIDOC_RC);continue;}
        KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(K0);  /* F DC */
        KC (K0,DC_IdentifyStart) KEYSFROM(PSB) KEYSTO(PSB) RCTO(rc);
        if(rc > 3) {LDEXBL (CALLER,0xFFFFFFFFu);continue;}
//        KC (PSB,Domain_Get+11) KEYSTO(K0);  /* FC DOMKEY */
        KC (PSB,Domain_GetMemory) KEYSTO(K0);
        KC (K0,Node_Fetch+MEMNODEDOMKEY) KEYSTO(K0);
//
        KC (TOOLS,Node_Fetch+TOOLSDISCRIM) KEYSTO(K1);
        KC (K1,Discrim_Compare) KEYSFROM(DOMKEY,K0) RCTO(rc);
        if(rc) {LDEXBL (CALLER,rc); continue;}
        if(oc == FC_RecallFetcher) ndb.Databyte=DBFETCHER;
        if(oc == FC_RecallBuilder) ndb.Databyte=DBBUILDER;
        KC (PSB,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K0);
        LDEXBL (CALLER,OK_RC) KEYSFROM(K0);
        continue;
    }
    if((oc == FC_Create) || (oc == FC_CreateDC)) {
        holes.Capacity=300;
        KC (DOMKEY,Domain_GetKey+PSB) KEYSTO(SB);
    }
    if( (oc==FC_Create) || (oc==FC_CreateDC) ||
        (oc==FC_CreateMHF) || (oc==FC_CreateDCMHF) ) {
       KC(SBT,0) KEYSFROM(PSB) RCTO(rc);
       if(rc & 0x80) {LDEXBL (CALLER,NONPROMPTSB_RC);continue;}  /* bad SB */
       KC (TOOLS,Node_Fetch+TOOLSMETER) KEYSTO(METER);
 
       if( (oc==FC_Create) || (oc==FC_CreateMHF) ) { /* new DC */
         KC (TOOLS,Node_Fetch+TOOLSDCC) KEYSTO(UM);
         KC (UM,DCC_Create) KEYSFROM(PSB,METER) KEYSTO(UM);
         flags=0;
       }
       else {
         KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(K0);
         KC (K0,DC_IdentifyStart) KEYSFROM(UM) KEYSTO(K0) RCTO(rc);
         if ((rc == 0xFFFFFFFFu) || (rc == DBREQUESTOR) )
             {LDEXBL (CALLER,FactoryDCNotValid);continue;}
         KC (K0,Domain_GetKey+DC) KEYSTO(UM);
         flags=SHAREDDC;
       }
       KC (DOMKEY,Domain_GetKey+DOMKEY) KEYSTO(RETURNER);/* for brand */
       KC (TOOLS,Node_Fetch+TOOLSKEEPER) KEYSTO(K3);
       KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
 
   /* UM has Domain Creator for use by new factory */
   /* RETURNER has My domain key for new factory brand   */
 
       if(!(rc=fork())) {  /**********  F code ** = OK **/
          /****************************************************
          *    The FORK worked and this is the Factory Domain *
          *    which must buy some space.   DOF is used by    *
          *    a COPYKEY created object (another FORK )       *
          *    so initial conditions are set up here          *
          *    If DOF returns, F failed or was asked to       *
          *    destroy itself.  This means that DOFC will     *
          *    return and death follows.                      *
          ****************************************************/
 
//          KC (DOMKEY,Domain_Swap+11) KEYSFROM(RETURNER); /* brand */
          KC (DOMKEY,Domain_GetMemory) KEYSTO(PTOOLS);
          KC (PTOOLS,Node_Swap+MEMNODEDOMKEY) KEYSFROM(RETURNER);
//
          KC (DOMKEY,Domain_SwapKey+DC) KEYSFROM(UM); /* DC */
          KC (TOOLS,Node_Fetch+TOOLSRETURNER) KEYSTO(RETURNER);
 
          KC (PSB,SB_CreateNode) KEYSTO(PTOOLS) RCTO(rc);
          if(!rc) {  /* got node */
            KC (PSB,SB_CreateNode) KEYSTO(COMP) RCTO(rc);
            if(!rc) {  /* got node */
              KC (PTOOLS,Node_Swap+PTOOLSPSB) KEYSFROM(PSB);
              KC (PTOOLS,Node_Swap+PTOOLSM) KEYSFROM(METER);
              KC (PTOOLS,Node_Swap+PTOOLSDC) KEYSFROM(DC);
              KC (PTOOLS,Node_Swap+PTOOLSCOMP) KEYSFROM(COMP);
 
              KC (TOOLS,Node_Fetch+TOOLSKIDC) KEYSTO(K0);
              KC (K0,KIDC_Create) STRUCTFROM(holes)
                  KEYSFROM(PSB,METER,SB)  KEYSTO(K0) RCTO(rc);
              if (!rc) {  /* got KID */
                KC (PTOOLS,Node_Swap+PTOOLSKID) KEYSFROM(K0);
                KC (DC,DC_Weaken) KEYSTO(DC);
                ndb.Databyte=DBCOPY;
                KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb)
                   KEYSTO(K0);
                KC (COMP,Node_Swap+3) KEYSFROM(K0);
 
                dof(flags,FR_AKT,ff,0,0);  /* factory code */
 
                /***********************************************
                *  DOF returned. This means death is requested *
                *  RC=0 in this case as it is not changed after*
                *  creating the KID.                           *
                ***********************************************/
 
                KC (PTOOLS,Node_Fetch+PTOOLSKID) KEYSTO(K0);
                KC (K0,DESTROY_OC) RCTO(rc1);
              }   /* rc=0 means KT+4 on factory */
 
              /************************************************
              *  If RC=0 then this is the Factory domain      *
              *  returning home to die.  If RC != 0 then this *
              *  is the new domain having failed to get space *
              ************************************************/
 
              KC (DOMKEY,Domain_GetKey+DC) KEYSTO(UM); /* O dc */
              KC (PTOOLS,Node_Fetch+PTOOLSPSB) KEYSTO(PSB);
              KC (PTOOLS,Node_Fetch+PTOOLSM) KEYSTO(METER);
              KC (PTOOLS,Node_Fetch+PTOOLSDC) KEYSTO(DC); /* my dc */
              KC (PTOOLS,Node_Fetch+PTOOLSCOMP) KEYSTO(COMP);
              KC (TOOLS,Node_Fetch+RETURNER) KEYSTO(RETURNER);
 
              if (!(flags & SHAREDDC)) KC (UM,DESTROY_OC) RCTO(rc1);
              KC (PSB,SB_DestroyNode) KEYSFROM(COMP) RCTO(rc1);
            }
            KC (PSB,SB_DestroyNode) KEYSFROM(PTOOLS) RCTO(rc1);
          }
 
          /***************************************************
          *  The Factory object is to be no longer.  Return  *
          *  from dofc()                                     *
          ***************************************************/
 
          if(rc) return 2;  /* no space - no keys */
          return 0;   /* death of factory, return from dofc */
       }              /********** end F code ********/
 
       /******************************************************
       *  If RC=1 then the FORK was successful and the new   *
       *  domain is responsible for returning to the caller  *
       *  else the FORK failed and we must return to the     *
       *  caller and await further orders                    *
       ******************************************************/
       rc=rc-1;    /* 1=ok 3->out of space */
 
       KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3); /* FC's keeper */
       KC (TOOLS,Node_Fetch+RETURNER) KEYSTO(RETURNER);
       if(!rc) KC (DOMKEY,Domain_SwapKey+CALLER);  /* drop this */
       LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
       continue;
    }
    LDEXBL (CALLER,KT+2);
  }
}
/*******************************************************************
*    F      -  The FACTORY
*
*******************************************************************/
UINT32 dof(iflags,iakt,ff,isa,iord)
   UCHAR iflags;
   UINT32 iakt,isa,iord;
   UCHAR *ff;
{
   JUMPBUF;

   struct Domain_DataByte ndb;
   struct Factory_GetMiscellaneousValues misc;
   UINT32 oc,rc,parm,ord,sa,akt;
   SINT16 db;
   UCHAR flags;
 
   akt=iakt;
   sa=isa;
   ord=iord;
   flags=iflags;
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   LDEXBL (RETURNER,0) KEYSFROM(K0,,,CALLER);
   for (;;) {     /* F loop */
     parm=0;
     LDENBL OCTO(oc) KEYSTO(SB,UM,K3,CALLER) DBTO(db)
       STRUCTTO(parm);
     RETJUMP();
     switch (db) {
       case  DBBUILDER:
          switch(dobuilder(oc,parm,&flags,&akt,&ord,&sa,&rc,ff,&misc)){
            case 0:  /* return rc */
               LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
               continue;
            case 1:  /* return rc,K0,K1 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,K1,,CALLER);
               continue;
            case 2:  /* return rc,misc,K0 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,,,CALLER)
                  STRUCTFROM(misc);
               continue;
            case 3:  /* destruct request */
                return 0;
          }
       case DBREQUESTOR:
          if(dorequest(oc,flags,akt,ord,sa,ff,&rc)) return rc;
 
          /**************************************************
          *   Return from here means either that RC=0 and   *
          *   there is an object or RC=n and there is no    *
          *   new object. Or the new object died            *
          **************************************************/
 
          LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
          continue;
       case DBFETCHER:
          switch(dofetcher(oc,flags,akt,ord,sa,&rc,ff,&misc)) {
            case 0:  /* return rc */
               LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
               continue;
            case 1:  /* return rc,K0,K1 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,,,CALLER);
               continue;
            case 2:  /* return rc,misc,K0 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,,,CALLER)
                  STRUCTFROM(misc);
               continue;
            case 3:
               LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
               continue;
          }
       case DBCOPY:
          switch(docopy(oc,parm,flags,akt,ord,sa,&rc,ff,&misc)) {
            case 0:  /* return rc */
               LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
               continue;
            case 1:  /* return rc,K0,K1 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,,,CALLER);
               continue;
            case 2:  /* return rc,misc,K0 */
               LDEXBL (RETURNER,rc) KEYSFROM(K0,,,CALLER)
                  STRUCTFROM(misc);
               continue;
            case 3: /* KT+4 on COPY/Builder */
               exit(0); /* we are done done */
          }
     }
   }  /* end F loop */
}
/*******************************************************************
*
*  DOBUILDER - process builder key calls
*
*******************************************************************/
int dobuilder(oc,parm,flags,akt,ord,sa,rc,ff,misc)
   UINT32 oc,parm,*sa;
   UCHAR *flags,*ff;
   UINT32 *akt,*ord,*rc;
   struct Factory_GetMiscellaneousValues *misc;
{
   JUMPBUF;

   struct Domain_DataByte ndb;
   UINT32 rc1,tt;
   UINT16 i;
/*
*  SB,UM,K3
*/
   if (oc == KT) {
      *rc=FB_AKT;
      return 0;
   }
   if (oc == DESTROY_OC) {
      *rc=0;
      return 3;
   }
   if((oc >= FactoryB_InstallSensory) &&
                       (oc <= FactoryB_InstallSensory+18)) {
      oc=oc-FactoryB_InstallSensory;
      if( issensory()) {   /* test SB */
        ff[oc]=0;        /* not scrutable (factory) */
        instcomp(oc,parm,flags,sa);
        *rc=0;
        return 0;
      }
      else {
        *rc=2;
        return 0;
      }
   }
 
   if((oc >= FactoryB_InstallFactory) &&
                       (oc <= FactoryB_InstallFactory+18)) {
      oc=oc-FactoryB_InstallFactory;
      if( isfactory()) {   /* test SB, return Domain Key in K3 */
         getkids();        /* Kids  Mine in K0, his in K1 */
         if(*flags & COMPLETE) {  /* test */
            KC (K0,KID_TestInclusion) KEYSFROM(K1) RCTO(rc1);
            if(rc1) {
               *rc=3;
               return 0;
            }
         }
         else {                   /* add */
            KC (K0,KID_MakeUnion) KEYSFROM(K1) RCTO(rc1);
            if(rc1) {
               *rc=rc1+3;
               return 0;
            }
         }
      }
      else if (!isfactoryc()) {  /* test SB */
        *rc=2;
        return 0;
      }
      ff[oc]=1;
      instcomp(oc,parm,flags,sa);
      *rc=0;
      return 0;
   }
 
   if((oc >= FactoryB_InstallHole) &&
                        (oc <= FactoryB_InstallHole+18)) {
      oc=oc-FactoryB_InstallHole;
      KC (PTOOLS,Node_Fetch+PTOOLSKID) KEYSTO(K0);
      if(*flags & COMPLETE) {
        KC (K0,KID_Identify) KEYSFROM(SB) RCTO(rc1);
        if(rc1) {*rc=2;return 0;}
      }
      else {
        KC (K0,KID_AddEntry) KEYSFROM(SB) RCTO(rc1);
        if (rc1) {*rc=6;return 0;}
      }
      ff[oc]=0;
      instcomp(oc,parm,flags,sa);
      *rc=0;
      return 0;
   }
 
   if((oc >= FactoryB_GetMiscellaneous) &&
                   (oc <= FactoryB_GetMiscellaneous+18)) {
      oc=oc-FactoryB_GetMiscellaneous;
      getcomp(oc);     /* to K0 */
      misc->Address=*sa;
      misc->AKT=*akt;
      misc->Flags=*flags;
      misc->Ordinal=*ord;
      tt=0;
      for(i=0;i<19;i++) {
        if(ff[i]) tt |= (0x80000000u>>i);
      }
      memcpy(misc->Bits,&tt,3);
      *rc=0;
      return 2;
   }
 
   if (oc == FactoryB_AssignKT) {
     *akt=parm;
     *rc=0;
     return 0;
   }
 
   if (oc == FactoryB_AssignOrdinal) {
     *ord=parm;
     *rc=0;
     return 0;
   }
 
   if (oc == FactoryB_MakeRequestor) {
     *flags |= COMPLETE;
     ndb.Databyte=DBFETCHER;
     KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K1);
     if(*flags & DOTPROGRAM) ndb.Databyte=DBREQUESTOR;
     KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K0);
     *rc=0;
     return 1;
   }
 
   if (oc == FactoryB_MakeCopy) {
     ndb.Databyte=DBCOPY;
     KC (DOMKEY,Domain_MakeStart) STRUCTFROM(ndb) KEYSTO(K0,K1);
     *rc=0;
     return 1;
   }
 
   if (oc == FactoryB_GetDC) {
     KC (DOMKEY,Domain_GetKey+DC) KEYSTO(K0,K1);
     *rc=0;
     return 1;
   }
}
/*******************************************************************
*
*    INSTCOMP - Install a component
*
*******************************************************************/
void instcomp(oc,parm,flags,sa)
  UINT32 oc,parm;
  UCHAR *flags;
  UINT32 *sa;
{
  JUMPBUF;

  if(oc<16) {
    KC (COMP,Node_Swap+oc) KEYSFROM(SB);
  }
  else {
    KC (PTOOLS,Node_Swap+PTOOLSKEEPER+(oc-16)) KEYSFROM(SB);
    if(oc==17) {
      *flags |= DOTPROGRAM;
      *sa=parm;
    }
  }
}
/*******************************************************************
*
*    GETCOMP - Fetch a component
*
*******************************************************************/
void getcomp(oc)
   UINT32 oc;
{
  JUMPBUF;

  if(oc<16) {
    KC (COMP,Node_Fetch+oc) KEYSTO(K0);
  }
  else {
    KC (PTOOLS,Node_Fetch+PTOOLSKEEPER+(oc-16)) KEYSTO(K0,K1);
  }
}
/*******************************************************************
*
*    ISSENSORY - Test for SB being a sensory key
*
*******************************************************************/
int issensory()
{
   JUMPBUF;
   int i;
   UINT32 rc;
 
   KC (TOOLS,Node_Fetch+TOOLSDISCRIM) KEYSTO(K0);
   KC (K0,Discrim_Discreet) KEYSFROM(SB) RCTO(rc);
   if (!rc) return 1;
   for(i=TOOLSFIRSTDISCREET;i<=TOOLSLASTDISCREET;i++) {
     KC (TOOLS,Node_Fetch+i) KEYSTO(K1);
     KC (K0,Discrim_Compare) KEYSFROM(SB,K1) RCTO(rc);
     if(!rc) return 1;   /* is discreet */
   }
   KC (K0,0) KEYSFROM(SB) RCTO(rc);
   if (rc == 1) return 1;
   return 0;
}
/*******************************************************************
*
*    ISFACTORY - Test for SB being a factory, Return Domain in K3
*
*******************************************************************/
int isfactory()
{
     JUMPBUF;
     UINT32 rc;
 
     KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(K3);
     KC (K3,DC_IdentifyStart) KEYSFROM(SB) KEYSTO(K3) RCTO(rc);
     if( (rc==1) || (rc==2)) return 1;  /* ok */
     return 0;  /* not ok */
}
/*******************************************************************
*
*    ISFACTORYC - Test for SB being a FactoryC or DC
*
*******************************************************************/
int isfactoryc()
{
     JUMPBUF;
     UINT32 rc;
 
     KC (TOOLS,Node_Fetch+TOOLSFCDC) KEYSTO(K3);
     KC (K3,DC_IdentifyStart) KEYSFROM(SB) RCTO(rc);
     if(!rc) return 0;  /* Powerfull FC */
     if( (rc==1) || (rc==2)) return 1; /* weak FC */
     KC (TOOLS,Node_Fetch+TOOLSFCC) KEYSTO(K3);
     KC (TOOLS,Node_Fetch+TOOLSDISCRIM) KEYSTO(K0);
     KC (K0,Discrim_Compare) KEYSFROM(SB,K3) RCTO(rc);
     if(!rc) return 1;  /* ok */
     KC (TOOLS,Node_Fetch+TOOLSDCC) KEYSTO(K3);
     KC (K3,DCC_Identify) KEYSFROM(SB) RCTO(rc);
     if(rc==1) return 1;
     return 0;
}
/*******************************************************************
*
*    GETKIDS    - Domain in K3, return - My Kid in K0, His in K1
*
*******************************************************************/
void getkids()
{
   JUMPBUF;
   UINT32 rc;
 
   KC (K3,Domain_GetKey+PTOOLS) KEYSTO(K3);
   KC (K3,Node_Fetch+PTOOLSKID) KEYSTO(K1);
   KC (PTOOLS,Node_Fetch+PTOOLSKID) KEYSTO(K0);
}
/*******************************************************************
*
*    DOREQUEST  - process requester call
*
*******************************************************************/
UINT32 dorequest(oc,flags,akt,ord,sa,ff,myrc)
 UINT32 oc,akt,ord,sa,*myrc;
 UCHAR flags,*ff;
{
  JUMPBUF;
  UINT32 rc;
 
  if(oc == FactoryR_Compare) {   /* test SB */
     if(isfactoryc()) {*myrc=0;return 0;}  /* ok by me */
     if(!isfactory()) {*myrc=2;return 0;}  /* domain to K3 */
     getkids();
     KC (K0,KID_TestInclusion) KEYSFROM(K1) RCTO(rc);
     if(rc) {*myrc=1;return 0;}
     *myrc=0;
     return 0;
  }
 
  if(oc == KT)          {   /* what am I */
     *myrc=akt;
     return 0;
  }
 
  if(oc > 0xC0000000u)  {*myrc=KT+2;return 0;}
 
  /****************************************************************
  *   Build and start a new object   SB,UM,K3                     *
  ****************************************************************/
 
  KC (SBT,0) KEYSFROM(SB) RCTO(rc);
  if(rc & 0x80)  {*myrc=1;return 0;}  /* no destruction */
  KC (DOMKEY,Domain_SwapKey+PSB) KEYSFROM(SB);     /* his bank */
  KC (PTOOLS,Node_Fetch+PTOOLSM) KEYSTO(METER);/* my meter */
 
  KC (DOMKEY,Domain_GetKey+K3) KEYSTO(SBT);
  KC (TOOLS,Node_Fetch+TOOLSKEEPER) KEYSTO(K3);
  KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
 
  if(!(rc=fork())) {   /* the new object runs here */
 
     /************************************************************
     *  This is the new object.  It starts with everything that  *
     *  the factory has.  It must switch to the Object           *
     *  environment.  Must wait till the end to switch meters    *
     ************************************************************/
 
//     KC (DOMKEY,Domain_Swap+11);           /* zap brand */
     KC (DOMKEY,Domain_GetMemory) KEYSTO(K2);
     KC (K2,Node_Swap+MEMNODEDOMKEY);
//
     KC (COMP,Node_MakeFetchKey) KEYSTO(COMP);
     KC (PTOOLS,Node_Fetch+PTOOLSKEEPER) KEYSTO(K0);
     KC (PTOOLS,Node_Fetch+PTOOLSPROGRAM) KEYSTO(K1);
     KC (PTOOLS,Node_Fetch+PTOOLSSYMBOLS) KEYSTO(K2);
     KC (DOMKEY,Domain_SwapKey+PSB) KEYSFROM(SB);
     KC (DOMKEY,Domain_SwapKey+METER) KEYSFROM(UM);
     KC (DOMKEY,Domain_SwapKey+SB)  KEYSFROM(SBT);
     KC (RETURNER,0) KEYSTO(UM,SBT,TOOLS,PTOOLS);
     KC (RETURNER,0) KEYSTO(RETURNER,K3);
 
     /***********************************************************
     *  New object no longer has any of factories keys          *
     *  Time to swap keepers and test the meter                 *
     ***********************************************************/
     if(ff[16]) { /* Keeper is a factory */
         KC (K0,2) KEYSFROM(PSB,METER,SB) KEYSTO(K0) RCTO(rc);
         if(rc) {*myrc=rc;return 0;}  /* death!! */
     }
     KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K0);
     KC (DOMKEY,Domain_Swap+10)    KEYSFROM(K2);
     KC (DOMKEY,Domain_SwapMeter)  KEYSFROM(METER);
     /***********************************************************
     *  if new object is still running, get program segment     *
     *  This code is known as BIRTHSECT.  Some or all of it     *
     *  could be part of REPMEM() which is in assembly          *
     ***********************************************************/
 
     if( (sa == 0xFFFFFFFF) || (sa == 0xFFFFFFFD) ) {  /* LSF */
        KC (DOMKEY,Domain_SwapKey+7) KEYSFROM(K1);
        KC (K1,0) KEYSTO(K1);    /* get component 0 */
        KC (DOMKEY,Domain_SwapKey+10) KEYSFROM(K1);
        sa=0x000000ACu;  /* start address of lsfsims */
     }
     else {
        if(ff[17])  { /* program segment is factory */
          KC (K1,2) KEYSFROM(PSB,METER,SB) KEYSTO(K1) RCTO(rc);
          if(rc) {*myrc=rc;return 0;}  /* death!! */
          KC (DOMKEY,Domain_SwapKey+7);
        }
     }
     repmem(oc,ord,sa);  /* slot K1(14)= new memory */
     crash("repmem returned unexpectedly");
  }
 
  /************************************************************
  *  if RC=1  the fork worked and we must lose the CALLER key *
  *  else there is no new object and F  has the CALLER key.   *
  *  Note above that the new object may RETURN before it      *
  *  switches to the new memory.  If so the new domain will   *
  *  run the code after rc=dorequester() and return to the    *
  *  requester with the return code as it destructs ( it      *
  *  returns from DOF which returns from DOFC, etc            *
  ************************************************************/
  KC (TOOLS,Node_Fetch+TOOLSSBT) KEYSTO(SBT);
  rc=rc-1;  /* transform */
  KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3);
  if(!rc) KC (DOMKEY,Domain_SwapKey+CALLER);  /* fork worked */
  *myrc=rc;
  return 0;
}
/*************************************************************
*   DOFETCHER -  Fetcher code                                *
*************************************************************/
int dofetcher(oc,flags,akt,ord,sa,myrc,ff,misc)
   UINT32 oc,akt,ord,sa;
   UINT32 *myrc;
   UCHAR flags,*ff;
   struct Factory_GetMiscellaneousValues *misc;
{
   JUMPBUF;
 
   if(oc == KT) {*myrc=akt;return 0;}
 
   if(oc == FactoryF_Compare)
     return dorequest(oc,flags,akt,ord,sa,ff,myrc);
   if(oc == FactoryF_MakeRequestor)
     return dobuilder(oc,0,&flags,&akt,&ord,&sa,myrc,ff,misc);
   if( (oc >= FactoryF_Fetch) && (oc <= FactoryF_Fetch+18)) {
      getcomp(oc);
      KC (PTOOLS,Node_Fetch+0) KEYSTO(,,,K1);
      *myrc=0;
      return 1;
   }
   if( (oc >= FactoryF_GetMiscellaneous) &&
                 (oc <= FactoryF_GetMiscellaneous+18))
     return dobuilder(oc,0,&flags,&akt,&ord,&sa,myrc,ff,misc);
   *myrc=KT+2;
   return 0;
}
/*************************************************************
*   DOCOPY    -  Copy    code                                *
*************************************************************/
int docopy(oc,parm,flags,akt,ord,sa,myrc,ff,misc)
    UINT32 oc,parm,akt,ord,sa,*myrc;
    UCHAR flags,*ff;
    struct Factory_GetMiscellaneousValues *misc;
{
    JUMPBUF;
    UCHAR lflags;
    UINT16 i;
    UINT32 rc;
    struct Domain_DataByte cdb;
 
  /* parm has hole count */
    if(!parm) parm=300;
    lflags=flags;
    lflags = lflags & ~COMPLETE;
    cdb.Databyte=DBCOPY;
 
    if(oc == KT) {*myrc=FCopy_AKT;return 0;}
 
    if( (oc >= FactoryC_GetMiscellaneous) &&
                       (oc <= FactoryC_GetMiscellaneous+18)) {
      return dobuilder(oc,0,&flags,&akt,&ord,&sa,myrc,ff,misc);
    }
    if( oc==FactoryC_Copy || oc == 1) {
       KC (SBT,0) KEYSFROM(SB) RCTO(rc);
       if (!(rc & 0x80)) {
         KC (DOMKEY,Domain_SwapKey+PSB)  KEYSFROM(SB);
         KC (DOMKEY,Domain_SwapKey+SB)   KEYSFROM(K3);
         KC (TOOLS,Node_Fetch+TOOLSMETER) KEYSTO(METER);
         KC (DOMKEY,Domain_GetKey+DC) KEYSTO(UM);
         KC (PTOOLS,Node_Fetch+PTOOLSKID) KEYSTO(RETURNER);
         KC (DOMKEY,Domain_GetKey+PTOOLS) KEYSTO(SBT);
         KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(DC);
         KC (TOOLS,Node_Fetch+TOOLSKEEPER) KEYSTO(K3);
         KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
         if(!(rc=fork())) {    /* the new factory */
            *myrc=2;
            KC (PSB,SB_CreateNode) KEYSTO(PTOOLS) RCTO(rc);
            if(!rc) {
               KC (PTOOLS,Node_Swap+PTOOLSDC) KEYSFROM(DC);
               KC (PTOOLS,Node_Swap+PTOOLSM) KEYSFROM(METER);
               KC (PTOOLS,Node_Swap+PTOOLSPSB) KEYSFROM(PSB);
               KC (PSB,SB_CreateNode) KEYSTO(K0) RCTO(rc);
               if(!rc) {
                 for (i=0;i<16;i++) {
                    KC (COMP,Node_Fetch+i) KEYSTO(K1);
                    KC (K0,Node_Swap+i) KEYSFROM(K1);
                 }
                 KC (DOMKEY,Domain_MakeStart) STRUCTFROM(cdb)
                     KEYSTO(K1);  /* new copy key */
                 KC (K0,Node_Swap+3) KEYSFROM(K1);  /* replace C3 */
                 KC (PTOOLS,Node_Swap+PTOOLSCOMP) KEYSFROM(K0);
                 KC (DOMKEY,Domain_GetKey+K0) KEYSTO(COMP);
                 KC (SBT,Node_Fetch+PTOOLSKEEPER) KEYSTO(K1);
                 KC (PTOOLS,Node_Swap+PTOOLSKEEPER) KEYSFROM(K1);
                 KC (SBT,Node_Fetch+PTOOLSPROGRAM) KEYSTO(K1);
                 KC (PTOOLS,Node_Swap+PTOOLSPROGRAM) KEYSFROM(K1);
                 KC (SBT,Node_Fetch+PTOOLSSYMBOLS) KEYSTO(K1);
                 KC (PTOOLS,Node_Swap+PTOOLSSYMBOLS) KEYSFROM(K1);
                 KC (TOOLS,Node_Fetch+TOOLSSBT) KEYSTO(SBT);
                 *myrc=3;
                 KC (TOOLS,Node_Fetch+TOOLSKIDC) KEYSTO(K0);
                 KC (K0,KIDC_Create) STRUCTFROM(parm)
                     KEYSFROM(PSB,METER,SB)  KEYSTO(K0) RCTO(rc);
                 if (!rc) {  /* got KID */
                    KC (K0,KID_MakeUnion) KEYSFROM(RETURNER);
                    KC (PTOOLS,Node_Swap+PTOOLSKID) KEYSFROM(K0);
                    KC (TOOLS,Node_Fetch+TOOLSRETURNER)
                          KEYSTO(RETURNER);
                    KC (DOMKEY,Domain_SwapKey+DC) KEYSFROM(UM);
                    if(oc == 1) lflags = lflags & ~DOTPROGRAM;
 
                    dof( (lflags | SHAREDDC),akt,ff,sa,ord);
 
                    *myrc=0;  /* KT+4 on copy of Builder */
                    KC (TOOLS,Node_Fetch+TOOLSFDC) KEYSTO(DC);
                    KC (PTOOLS,Node_Fetch+PTOOLSPSB) KEYSTO(PSB);
                    KC (PTOOLS,Node_Fetch+PTOOLSM) KEYSTO(METER);
                    KC (PTOOLS,Node_Fetch+PTOOLSKID) KEYSTO(K0);
                    KC (K0,DESTROY_OC) RCTO(rc);
                    KC (PTOOLS,Node_Fetch+PTOOLSCOMP) KEYSTO(COMP);
                 }
                 KC (PSB,SB_DestroyNode) KEYSFROM(COMP) RCTO(rc);
               }
               KC (PSB,SB_DestroyNode) KEYSFROM(PTOOLS) RCTO(rc);
            }
            return 3; /* signal death of domain MYRC to caller */
         }
         rc=rc-1;
         KC (DOMKEY,Domain_SwapKeeper) KEYSFROM(K3) KEYSTO(K3);
         KC (DOMKEY,Domain_GetKey+UM) KEYSTO(DC);
         KC (TOOLS,Node_Fetch+TOOLSRETURNER) KEYSTO(RETURNER);
         KC (TOOLS,Node_Fetch+TOOLSSBT) KEYSTO(SBT);
         if(!rc) KC (DOMKEY,Domain_SwapKey+CALLER);  /* fork worked */
         *myrc=rc;
         return 0;
       }
       *myrc=1;
       return 0;
    }
    *myrc=KT+2;
    return 0;
}
