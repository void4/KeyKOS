/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*
   This is a SIK/SOK simulator for the UART device
*/

/****************************************************************************
 WARNING WARNING WARNING

 Use of outsok() destroys "sb"

 WARNING WARNING WARNING
****************************************************************************/


#include "kktypes.h"         /* KeyKOS data types                     */
#include "keykos.h"
#include "domain.h"
#include "sb.h"
#include "node.h"
#include "kuart.h"
#include "cck.h"
#include "ocrc.h"
#include "tssf.h"
#include "primekeys.h"
#include "dc.h"
#include "consdefs.h"
#include "discrim.h"
#include "snode.h"
#include "fs.h"


KEY comp      = 0;
#define compdiscrim  1
#define compreturner 2
#define compsnodef   3
#define compfsf      4
#define compconscck  15
KEY sb        = 1;
KEY caller    = 2;
KEY domkey    = 3;
KEY psb       = 4;
KEY m         = 5;
KEY domcre    = 6;

/* TMMK has totally different key assignments for 7-15 */

KEY comkey    = 7;   /* ZMK store connection waiter here */
KEY sikdom    = 8;
KEY sokdom    = 9;
KEY upsik     = 10;  /* ZMK stores waiting ZSIK here for connection */
KEY upsok     = 11;  /* ZMK stores waiting ZSOK here for connection */
KEY cck       = 12;  /* ZMK stores disconnection waiter here */

KEY dnsik     = 13;  /* ZMK stores Connecting sik here MUST BE THE SAME */
KEY dnsok     = 14;  /* ZMK stores Connecting sok here MUST BE THE SAME */
KEY dncck     = 15;  /* ZMK stores Connecting cck here MUST BE THE SAME */

/* SAVE slots in MEMNODE for keys used during FORK */
#define SAVE13 9
#define SAVE14 10
#define SAVE15 11

/* SAVE for dncck for ZMK */
#define ZMKDNCCK 12

/* SAVE for connection keys */
#define SAVESIK2 13
#define SAVESOK2 14
#define SAVECCK2 15

       char title[]="TSSF";
       int stacksiz=4096;

factory(factoc,factord)
   int factoc,factord;
{
   JUMPBUF;

   uint32 oc,rc;
   char switchchar[32];
   uint32 actlen;

   oc = (UINT32)factoc;

/* This domain becomes the CCK domain within the makeXXX routines */

   if (oc == EXTEND_OC) { /* not every call requires extended jump */
       {OC(EXTEND_OC);XB(0x00200000);RC(oc);NB(0x08F0DEF2);cjcc(0x00000000,&_jumpbuf); }
       if(oc == EXTEND_OC) {  /* tmmk requires one more */
          {OC(EXTEND_OC);XB(0x00200000);RC(oc);RS3(switchchar,32,actlen);NB(0x0B90C002);cjcc(0x00100000,&_jumpbuf); }
       }
   }
   switch(oc) {
   case TSSF_CreateCCK2:
       rc=makecck2();   /* the CCK domain returns here on death */
       break;
   case TSSF_CreateZMK:
       rc=makezmk();    /* the CCK domain returns here on death */
       break;
   case TSSF_CreateTMMK:  /* dnsik(tm13),dnsok(tm14),dncck(tm15),cck(tm12) */
       rc=maketmmk(switchchar,actlen);   /* the TMMK domain returns here on death */
       break;
   default:             /* other types go here */
       exit(INVALIDOC_RC);
   }
   exit(rc);
}
/**********************************************************************/
/* This is a CCK domain for level 2                                   */
/* return from here upon death request                                */
/**********************************************************************/
makecck2()
{
   JUMPBUF;
   UINT32 oc,rc;
   UINT16 db;
   struct Domain_DataByte ddb={1};
   UINT32 comakt;
   char parm[256];
   int  actlen;
   int type;
#define TYPEUART   1
#define TYPECONS   2
#define TYPESOCKET 3

/* make the SIK and SOK domains */

   {OC(Domain_GetKey+dnsik);XB(0x00300000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }   /* for this level the */
                                                     /* first key is the communication key */
   {OC(KT);XB(0x00700000);RC(comakt);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                      /* what type of com key */
   if (comakt == Uart_AKT) {
       type=TYPEUART;
   }
   else if (comakt == Console_AKT) {
       {OC(concck__wait_for_connect);XB(0x00700000);RC(rc);NB(0x08C0AB00);cjcc(0x00000000,&_jumpbuf); }
                      /* dnsik and dnsok are keys used in the FORK operation and */
                      /* are not properly sent to the child */
                      /* we cheat here and put them in the wrong place so they can */
                      /* be moved in the child */
       type=TYPECONS;
   }
   else {
       return(TSSF_Unsupported);                    /* other types go here */
   }

   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }  /* internal cck key */
   if(makesik2(type)) {            /* puts upsik into upsik  */
       return(NOSPACE_RC);
   }
   if(makesok2(type)) {            /* puts upsok into upsok  */
       return(NOSPACE_RC);
   }

/* BEGIN the CCK code, all types of cck2 are basically the same  */

   {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }        /* public cck key */

   {OC(0);XB(0xE020ABC0); }     /* return the keys to requestor */
   for(;;) {
       {RC(oc);DB(db);RS3(parm,256,actlen);NB(0x0F100002); }
       {rj(0x00100000,&_jumpbuf); }

       if(db == ddb.Databyte) {   /* internal caller, must be signalling death */
           return 0;  /* just die, first one to call kills me, second one gets dk(0) */
       }

       if(oc == KT) {
           {OC(CCK2_AKT);XB(0x00200000); }
           continue;
       }

       if(oc == DESTROY_OC) {               /* destroy */
           zapsikdom();                     /* forks his upsik with KT+1 blows upsik,upsok */
           zapsokdom();                     /* forks his upsok with KT+1 blows upsik,upsok */

           switch(type) {                   /* zap circuit */
           case TYPEUART:
                break;                      /* nothing */
           case TYPECONS:
                break;                      /* nothing */
           case TYPESOCKET:
                break;                      /* FOR NOW */
           }
           return 0;                        /* return to caller destroying domain */
       }

       if(oc == CCK_Disconnect) {           /* zap circuit */
//outsok("CCK2: Disconnecting Circuit\n");
           zapsikdom();                     /* forks his upsik with KT+1 blows upsik,upsok */
           zapsokdom();                     /* forks his upsok with KT+1 blows upsik,upsok */
                                            /* no down to zap */

           switch(type) {                   /* zap circuit */
           case TYPEUART:
                break;                      /* nothing */
           case TYPECONS:
                break;                      /* nothing */
           case TYPESOCKET:
                break;                      /* FOR NOW */
           }
//outsok("CCK2: Going away\n");
           return 0;                        /* return to caller destroying domain */
       }

       if((oc == CCK_RecoverKeys) || (oc == CCK_TAP)) {           /* TAP */
//outsok("CCK2: Recover keys\n");
           zapsikdom();                      /* forks upsik with KT+1 blows upsik,upsok */
           zapsokdom();                      /* forks upsok with KT+1 blows upsik,upsok */
                                             /* no down  to TAP */
                                             /* but must recover correct keys from down level */
           if(type == TYPEUART) {
              {OC(UART_MakeCurrentKey);XB(0x00700000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
              {OC(UART_EnableInput);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
           }
           if(type == TYPECONS) {
              {OC(concck__wait_for_connect);XB(0x00700000);RC(rc);NB(0x08C0AB00);cjcc(0x00000000,&_jumpbuf); }
                      /* dnsik and dnsok are keys used in the FORK operation and */
                      /* are not properly sent to the child */
                      /* we cheat here and put them in the wrong place so they can */
                      /* be moved in the child */
           }

           {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }  /* internal cck key */
           if(makesik2(type)) {         /* returns upsik in upsik */
               {OC(NOSPACE_RC);XB(0x00200000); }
               continue;
           }
           if(makesok2(type)) {         /* returns upsok in upsok */
               {OC(NOSPACE_RC);XB(0x00200000); }
               continue;
           }

           {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); } /* public cck */
           {OC(0);XB(0xE020ABC0); } /* return recovered keys */
           continue;
       }
//sprintf(parm,"CCK2: OtherOC = %d, type=%d\n",oc,type);
//outsok(parm);
       if(type == TYPEUART) {  /* ignore most CCK calls, not processed */
           {OC(UART_MakeCurrentKey);XB(0x00700000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
           if(oc == CCK_ActivateNow) {
              {OC(UART_WakeReadWaiter);XB(0x10700002); }
              continue;
           }
           {OC(0);XB(0x00200000); }
           continue;
       }
       if(type == TYPECONS)  {  /* pass down to console cck key (com key) */
           if(actlen > 256) actlen=256;
           {OC(oc);PS2(parm,actlen);XB(0x14700002); }
           continue;
       }
       {OC(INVALIDOC_RC);XB(0x00200000); }
       continue;
   }
}
/********************************************************************************/
/*  Makes a SIK domain and returns key in upsik, sikdom                         */
/*                                                                              */
/*                USES KEYS   sikdom, upsik                                     */
/********************************************************************************/

makesik2(type)
   int type;
{
   JUMPBUF;
   UINT32 rc;

   save131415(sikdom);

   if (!(rc=fork())) {                           /* SIK domain */
        {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* zap the copy of the caller key */
        if(type == TYPEUART) {
           rc=dosik2uart();                 /* run the SIK2 for UART */
           exit(rc);                        /* if eof detected  */
        }
        if(type == TYPECONS) {
           rc=dosik2cons();                 /* run the SIK2 for CONSOLE */
           exit(rc);                        /* if eof detected */
        }
   }
   if(rc > 1) {  /* no one calling back */
      restore131415();
      return 1;
   }

   {OC(0);XB(0x00000000); }                          /* convenient key to return to with no side effect */
   {RC(rc);NB(0x0890800A); }  /* sik2 calls back, sikup is the SIK2 key */
   {rj(0x00000000,&_jumpbuf); }                               /* wait for return with domain key */

   restore131415();
   return 0;
}

/******************************************************************************/
/* Makes a SOK domain and returns key in upsok and sokdom                     */
/*                                                                            */
/*                 USES KEYS  sokdom, upsok                                   */
/******************************************************************************/

makesok2(type)
   int type;
{
   JUMPBUF;
   UINT32 rc;

   save131415(sokdom);

   if (!(rc=fork())) {                           /* SOK domain */
        {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* zap the copy of the caller key */
        if(type == TYPEUART) {
           rc=dosok2uart();                 /* run the SOK2 for UART */
           exit(rc);                        /* if eof detected */
        }
        if(type == TYPECONS) {
           rc=dosok2cons();                 /* run the SOK2 for CONSOLE */
           exit(rc);                        /* if eof detected */
        }
   }

   if(rc > 1) { /* no one returning call */
      restore131415();
      return 1;
   }
   {OC(0);XB(0x00000000); }                          /* convenient key to return to with no side effect */
   {RC(rc);NB(0x0890900B); }  /* sok2 calls back, upsok is the SOK2 key */
   {rj(0x00000000,&_jumpbuf); }

   restore131415();
   return 0;
}
/****************************************************************************************/
/* ZAPSIKDOM     - destroy the domain held in key slot sikdom                           */
/*                                                                                      */
/*                 USES KEYS      upsik,upsok                                           */
/*                                                                                      */
/*                                which have no significance in CCK domain              */
/*                                and are sik and sok waiters in ZMK domain             */
/*                                                                                      */
/* Note dnsik is not Signaled.  We always tear down the circuit top to bottom           */
/* so the next step is to destroy the lower domains unless they are comm keys           */
/****************************************************************************************/
zapsikdom()
{
   JUMPBUF;
   UINT32 rc;

   {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }            /* freeze domain */
   {OC(Domain_GetKey+upsik);XB(0x00800000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }   /* get the upsik key from domain */
   {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); } /* get returner */
   {OC(KT+1);XB(0x10B0000A); }          /* fork upsik through returner */
   {fj(0x00000000,&_jumpbuf); }

   {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }      /* get domain's memory node */
   {OC(Node_Fetch+1);XB(0x00A00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }           /* get stack page */
   {OC(SB_DestroyPage);XB(0x8040B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }         /* sell stack page */
   {OC(SB_DestroyNode);XB(0x8040A000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }         /* sell memory node */
   {OC(DC_DestroyDomain);XB(0xC0608400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); } /* destroy domain */
}
/****************************************************************************************/
/* ZAPSOKDOM     - destroy the domain held in key slot sokdom                           */
/*                                                                                      */
/*                 USES KEYS      upsik,upsok                                           */
/*                                                                                      */
/*                                which have no significance in CCK domain              */
/*                                and are sik and sok waiters in ZMK domain             */
/*                                                                                      */
/* Note dnsok is not Signaled.  We always tear down the circuit top to bottom           */
/* so the next step is to destroy the lower domains unless they are comm keys           */
/****************************************************************************************/

zapsokdom()
{
   JUMPBUF;
   UINT32 rc;

   {OC(Domain_MakeBusy);XB(0x00900000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }            /* freeze domain */
   {OC(Domain_GetKey+upsok);XB(0x00900000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }   /* get the upsok key from domain */
   {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); } /* get returner */
   {OC(KT+1);XB(0x10A0000B); }          /* fork upsok through returner */
   {fj(0x00000000,&_jumpbuf); }

   {OC(Domain_GetMemory);XB(0x00900000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }      /* get domain's memory node */
   {OC(Node_Fetch+1);XB(0x00A00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }           /* get stack page */
   {OC(SB_DestroyPage);XB(0x8040B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }         /* sell stack page */
   {OC(SB_DestroyNode);XB(0x8040A000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }         /* sell memory node */
   {OC(DC_DestroyDomain);XB(0xC0609400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); } /* destroy domain */
}

dosik2uart()
{
   JUMPBUF;
   uint32 oc,rc;
   char buf[1024];
   int len,type;
   char *ptr;
   int accum;
   char pbuf[64];

   {OC(0);XB(0x80C03000);RC(oc);NB(0x0810000A);cjcc(0x00000000,&_jumpbuf); } /* this returns keys to parent */

   {OC(UART_MakeCurrentKey);XB(0x00700000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
   {OC(UART_EnableInput);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }

   for(;;) {                                              /* SIK2 called for input */
      if(oc == KT+1) {                                    /* gone */
          {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                          /* check in to internal key */
          return(0);                                      /* return for death */
      }
      len=oc & 0xFFF;                                     /* length limited */
      type=oc >> 12;   /* 0 means no echo, no BS, activate on cr  */
                       /* 1 means no echo, no bs, activate each character */
                       /* 2 means echo, bs, activate on CR */
      ptr=buf;         /* start here */
      accum=0;         /* accumulate this amount */

      while(len) {

         {OC(UART_WaitandReadData+1);XB(0x00700000);RC(rc);RS2(ptr,1);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }  /* read from UART */

         if(rc == 100) {
            {OC(100);XB(0x00A00000);RC(oc);NB(0x0810000A);cjcc(0x00000000,&_jumpbuf); }       /* give chars */
            goto loop;
         }

         if(rc == KT+3) {   /* this should be  KT+1 but UART and CONSOLE keys don't do this */
             {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* notify (oc doesn't matter) */
             return(0);              /* return for death */
         }
         if(rc == 1) { /* activate now */
             break;   /* leave while loop */
         }

         *ptr=*ptr & 0x7F;           /* mask for ascii */
         switch(type) {
           case 1:                   /* raw */
              len=0;                 /* forces activation */
              accum=1;               /* on each character */
              ptr++;                 /* accumulation buffer */
              break;
           case 2:   /* echo, bs, activate on CR */
              if(*ptr==8 & accum) {                                     /* backspace */
                {OC(UART_WriteData);PS2(ptr,1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* backup */
                {OC(UART_WriteData);PS2(" ",1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* overwrite */
                {OC(UART_WriteData);PS2(ptr,1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* backup */
                ptr--;                                                  /* backup */
                accum--;                                                /* backup */
                len++;                                                  /* get extra to read */
                break;
              }
              if( *ptr != 0x1b) {  /* don't echo escape */
                 {OC(UART_WriteData);PS2(ptr,1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }       /* echo char */
              }
              if(*ptr==13) {OC(UART_WriteData);PS2("\n",1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); } /* if cr */
              ptr++;                                                    /* next place */
              accum++;                                                  /* count character */
              len--;                                                    /* count # left */
              if(*(ptr-1)==13) len=0;                                   /* activate on CR */
              break;
           case 0:  /* no echo, no bs, activate on CR */
              accum++;                                                  /* count it */
              ptr++;                                                    /* next place */
              len--;                                                    /* count # left */
              if(*(ptr-1)==13) len=0;                                   /* activate on CR */
              break;
         }
         if(accum >= 1024) len=0;                                       /* force activation */
      }
      *ptr=0;              /* add terminating 0  BOY, assumes space DONT do this */
      {OC(0);PS2(buf,accum);XB(0x04A00000);RC(oc);NB(0x0810000A);cjcc(0x08000000,&_jumpbuf); }       /* give chars */
loop: ;
   }
}

dosok2uart()
{
   JUMPBUF;
   uint32 oc,rc;
   char buf[1024];
   int len;
   char *ptr;
   char pbuf[64];

   {OC(0);XB(0x80C03000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }   /* return keys to parent (cck) */
   len=0;                                                  /* initial limit is zero */

   {OC(UART_MakeCurrentKey);XB(0x00700000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }

   for(;;) {
     if(oc == KT+1) {                                      /* gone */
        {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                            /* check in */
        return 0;                                          /* return for death */
     }
     ptr=buf;                                              /* start output here */
     while(len) {                                          /* for this much */
        {OC(UART_WriteData);PS2(ptr,1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); } /* write a character */
        if(0x0A==*ptr && !oc) {OC(UART_WriteData);PS2("\r",1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); } /* add lf */

        if(KT+3 == rc) {                                   /* should be KT+1 */
           {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                         /* check in */
           return 0;                                       /* return for death */
        }
        ptr++;                                             /* next character */
        len--;                                             /* keep count of done */
     }
     {OC(1024);XB(0x00B00000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); } /* return limit */
   }
}
/*************************************************************************************/
/*  SIK2 for the CONSOLE                                                             */
/*************************************************************************************/

dosik2cons()
{
   JUMPBUF;
   uint32 oc,rc,trc;
   char buf[1024];
   int len,type;
   int actlen;
   char *ptr;
   int accum;
   char pbuf[64];

   {OC(Domain_GetKey+upsik);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
         /* had to have this in upsik because of fork issues */
   {OC(Domain_GetKey+upsok);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }   /* this is because CONSOLE is not really coroutine */
   {OC(0);XB(0x80C03000);RC(oc);NB(0x0810000A);cjcc(0x00000000,&_jumpbuf); } /* keys to parent */

   for(;;) {
      if(oc == KT+1) {                                    /* gone */
          {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                          /* check in to internal key */
          return(0);                                      /* return for death */
      }
      len=oc & 0xFFF;                                     /* length limited */
      type=oc >> 12;   /* 0 means no echo, no BS, activate on cr  */
                       /* 1 means no echo, no bs, activate each character */
                       /* 2 means echo, bs, activate on CR */
      ptr=buf;         /* start here */
      accum=0;         /* accumulate this amount */

      while(len) {

         {OC(1);XB(0x00D00000);RC(rc);RS3(ptr,1,actlen);NB(0x0B000000);cjcc(0x00100000,&_jumpbuf); }  /* read from CONSOLE 1 char no echo */

         if(rc == KT+3) {   /* this should be  KT+1 but UART and CONSOLE keys don't do this */
             {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* notify (oc doesn't matter) */
             return(0);              /* return for death */
         }
         if(!actlen) { /* activate now */
             break;   /* leave while loop */
         }

         *ptr=*ptr & 0x7F;           /* mask for ascii */
         switch(type) {
           case 1:                   /* raw */
              len=0;                 /* forces activation */
              accum=1;               /* on each character */
              ptr++;                 /* accumulation buffer */
              break;
           case 2:   /* echo, bs, activate on CR */
              if(*ptr==8 && accum) {                                    /* backspace */
                {OC(0);PS2(ptr,1);XB(0x04E00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* backup */
                {OC(0);PS2(" ",1);XB(0x04E00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* overwrite */
                {OC(0);PS2(ptr,1);XB(0x04E00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }     /* backup */
                ptr--;                                               /* backup */
                accum--;                                             /* backup */
                len++;                                               /* get extra to read */
                break;
              }
              if( *ptr != 0x1b) {  /* don't echo escape */
                 {OC(0);PS2(ptr,1);XB(0x04E00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }       /* echo char */
              }
              if(*ptr==13) {OC(0);PS2("\n",1);XB(0x04E00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); } /* if cr */
              ptr++;                                                    /* next place */
              accum++;                                                  /* count character */
              len--;                                                    /* count # left */
              if(*(ptr-1)==13) len=0;                                   /* activate on CR */
              break;
           case 0:  /* no echo, no bs, activate on CR */
              accum++;                                                  /* count it */
              ptr++;                                                    /* next place */
              len--;                                                    /* count # left */
              if(*(ptr-1)==13) len=0;                                   /* activate on CR */
              break;
         }
         if(accum >= 1024) len=0;                                       /* force activation */
      }
      *ptr=0;              /* add terminating 0  BOY, assumes space DONT do this */
      {OC(0);PS2(buf,accum);XB(0x04A00000);RC(oc);NB(0x0810000A);cjcc(0x08000000,&_jumpbuf); }       /* give chars */
loop1: ;
   }
}

dosok2cons()
{
   JUMPBUF;
   uint32 oc,rc;
   char buf[1024];
   int len,tl;
   char *ptr;
   int limit;

//   char obuf[64];

   {OC(Domain_GetKey+upsok);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
         /* had to have this in upsok because of fork issues */
   {OC(0);XB(0x00E00000);RC(rc);NB(0x0810000E);cjcc(0x00000000,&_jumpbuf); }   /* get console output limit */
   limit=rc;
   len=0;                                   /* my initial limit is 0 */

   {OC(1024);XB(0x80C03000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }  /* keys to parent */
   for(;;) {
     if(oc == KT+1) {                       /* gone */
        {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }             /* check in */
        return 0;                           /* return for death */
     }
     ptr=buf;                               /* start here */
     while(len) {                           /* until done */
       tl = len;
       if (tl > limit) tl=limit;            /* don't exceed console limit */
       {OC(oc);PS2(ptr,tl);XB(0x04E00000);RC(oc);NB(0x0810000E);cjcc(0x08000000,&_jumpbuf); }
       if (oc == KT+3) {                    /* gone */
           {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
           return 0;
       }
       limit=oc;                            /* new console limit */
       len -= tl;                           /* what we did */
       ptr += tl;                           /* new location for output */
     }                                      /* till done */
     {OC(1024);XB(0x00B00000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); } /* call giving limit */
   }
}
/**************************************************************************************/
/*   Build the ZMK keys                                                               */
/**************************************************************************************/

KEY zmkdisconwaiter    = 7;   /* resume key of domain waiting for disconnect notification */
/* KEY sikdom    = 8;      ZMK stores ZSIKDOM here */
/* KEY sokdom    = 9;      ZMK stores ZSOKDOM here */
KEY zmkzsik     = 10;      /* ZSIK waiting for connection*/
KEY zmkzsok     = 11;      /* ZSOK waiting for connection */
/* KEY cck       = 12;     ZMK stores the connection CCK here */
KEY zmknewsik   = 13;      /* NEW SIK waiting for ZSIK to notice need */
KEY zmknewsok   = 14;      /* NEW SOK waiting for ZSOK to notice need */
KEY zmknewcck   = 15;      /* NEW CCK waiting for test of CCK */

/**************************************************************************************/
/* This becomes the CCK key and ZMK key for the Zapper (level 3)                      */
/**************************************************************************************/

#define DBZMK 2
#define DBICCK 1

#define SIKWAITING 1
#define SOKWAITING 0

makezmk()
{
    JUMPBUF;
    UINT32 oc,rc;
    struct Domain_DataByte icckdb={DBICCK};
    struct Domain_DataByte zmkdb={DBZMK};
    UINT16 db;
    int  havecircuit = 0;
    int  sikwaiting = 0;
    int  sokwaiting = 0;
    int  havewaiter = 0;
    int  havesik = 0;
    int  havesok = 0;

    {OC(Domain_MakeStart);PS2(&(icckdb),sizeof(icckdb));XB(0x04300000);NB(0x00E0CAB0);cjcc(0x08000000,&_jumpbuf); }
    if(makesik3()) {    /* leaves in UPSIK (zmkzsik)  */
       return NOSPACE_RC;
    }
    if(makesok3()) {    /* leaves in UPSOK (zmkzsok)  */
       return NOSPACE_RC;
    }

    {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_MakeStart);PS2(&(zmkdb),sizeof(zmkdb));XB(0x04300000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
    {OC(0);XB(0xF020FABC); }
    for(;;) {
       {RC(oc);DB(db);NB(0x0CF0DEF2); }
       {rj(0x00000000,&_jumpbuf); }

       switch(db) {
       case 0:    /* CCK3  - perhaps this should pass unknown requests to CCK 2 */

          if (oc == KT) {
               {OC(CCK3_AKT);XB(0x00200000); }
               continue;
          }
          if (oc == DESTROY_OC) {
             zapsikdom();  /* blows upsik,upsok */
             zapsokdom();  /* blows upsik,upsok */
             return OK_RC;
          }
          if ((oc == CCK_RecoverKeys) || (oc == CCK_TAP) ) {
//outsok("CCK3: Recover keys\n");
             zapsikdom();  /* blows upsik,upsok */
             zapsokdom();  /* blows upsik,upsok */

             {OC(Domain_MakeStart);PS2(&(icckdb),sizeof(icckdb));XB(0x04300000);NB(0x00E0CAB0);cjcc(0x08000000,&_jumpbuf); }
             makesik3();   /* UPSIK  is zmkzsik which is free now */
             makesok3();   /* UPSOK  is zmkzsok which is free now */

             havesik=0;
             havesok=0;      /* in case we don't get keys */

             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Fetch+ZMKDNCCK);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
             {OC(CCK_RecoverKeys);XB(0x00F00000);RC(rc);NB(0x08C0DE00);cjcc(0x00000000,&_jumpbuf); }
             if(!rc) {                   /* we got some keys dncck is still the same */
                 {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Swap+SAVESIK2);XB(0x80F0D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Swap+SAVESOK2);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                 havesik=1;
                 havesok=1;
             }
             sikwaiting=0;
             sokwaiting=0;
//outsok("CCK3: returned new keys\n");
             {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
             {OC(OK_RC);XB(0xE020ABF0); }
             continue;
          }
          if (oc == CCK_Disconnect) {
             if(havecircuit) {
//outsok("CCK3:Disonnecting Circuit\n");
                 {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Fetch+ZMKDNCCK);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(CCK_Disconnect);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
                 if (!rc) {       /* if DK(0) then the circuit we have is NEW not old */
                     havecircuit=0;  /* only if old circuit */
                 }
                 if (havewaiter) {   /* signal waiter */
                     havewaiter=0;
                     {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                     {OC(0);XB(0x10F00007); }
                     {fj(0x00000000,&_jumpbuf); }
                 }
                 {OC(OK_RC);XB(0x00200000); }
                 continue;
             }
             {OC(ZMK_NoCircuit);XB(0x00200000); }
             continue;
          }

          /* pass request to dncck */

          {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+ZMKDNCCK);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }

//outsok("CCK3: Other OC\n");
          {OC(oc);XB(0x10F00002); }
          continue;

       case DBICCK:   /* INTERNAL Call only */

          if (oc == DESTROY_OC) {  /* sik or sok noted upstream go away */
             return 0;
          }
          if (oc == SIKWAITING) {  /* sik noticed downstream loss */
//outsok("SIKWAITING\n");
             if(havesik) {
                  havesik=0;
                  sikwaiting=0;
                  {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
                  {OC(Node_Fetch+SAVECCK2);XB(0x00D00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                  {OC(Node_Swap+ZMKDNCCK);XB(0x80D0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* set dncck */
                  {OC(Node_Fetch+SAVESIK2);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }   /* get sik */
//outsok("SIKGiven\n");
                  {OC(OK_RC);XB(0x8020D000); }     /* return sik to caller */
                  continue;
             }
             sikwaiting=1;                                       /* no keys must wait */
             havecircuit=0;                                      /* must be gone */
             {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }  /* save waiter */

             /* some disconnect has happened, we can signal the disconwaiter */
             /* this will turn disconwaiter into DK(0) if we have one */

             havewaiter=0;
             {OC(OK_RC);XB(0x00700000); }  /* disconwaiter or dk(0) */
             continue;
          }
          if (oc == SOKWAITING) {  /* sok noticed downstream loss */
//outsok("SOKWAITING\n");
             if(havesok) {
                  havesok=0;
                  sokwaiting=0;
                  {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
                  {OC(Node_Fetch+SAVECCK2);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                  {OC(Node_Swap+ZMKDNCCK);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* set dncck */
                  {OC(Node_Fetch+SAVESOK2);XB(0x00E00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }   /* get sok  */
//outsok("SOKGiven\n");
                  {OC(OK_RC);XB(0x8020E000); }       /* return sok to caller */
                  continue;
             }
             sokwaiting=1;                                         /* no keys must wait */
             havecircuit=0;                                        /* must be gone */
             {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }    /* save waiter */

             /* some disconnect has happened, we can signal the disconwaiter */
             /* this will turn disconwaiter into DK(0) if we have one */

             havewaiter=0;
             {OC(OK_RC);XB(0x00700000); } /* disconwaiter or dk(0) */
             continue;
          }

          {OC(0);XB(0x00200000); }   /* oddly nothing to do */
          continue;

       case DBZMK:
          if (oc == KT) {
               {OC(ZMK_AKT);XB(0x00200000); }
               continue;
          }
          if (oc == DESTROY_OC) {
             zapsikdom();  /* blows upsik,upsok */
             zapsokdom();  /* blows upsik,upsok */
             return OK_RC;
          }
          if (oc == ZMK_Connect) {
             if(!havecircuit) {   /* easy case */
                 havecircuit=1;
//outsok("Connect without circuit\n");
                 if(sokwaiting) {  /* give key directly to waiter */
                     sokwaiting=0;
                     havesok=0;
//outsok("SOKGiven\n");
                     {OC(OK_RC);XB(0x80B0E000); }
                     {fj(0x00000000,&_jumpbuf); }
                 }
                 else {           /* else stash for future call */
                     havesok=1;
                     {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
                     {OC(Node_Swap+SAVESOK2);XB(0x80B0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                 }
                 if(sikwaiting) {  /* give key directly to waiter */
                     sikwaiting=0;
                     havesik=0;
//outsok("SIKGiven\n");
                     {OC(OK_RC);XB(0x80A0D000); }
                     {fj(0x00000000,&_jumpbuf); }
                 }
                 else {           /* else stash for future call */
                     havesik=1;
                     {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
                     {OC(Node_Swap+SAVESIK2);XB(0x80A0D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                 }
                 /* put new cck key away since we have a new circuit put both places */
                 {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Swap+SAVECCK2);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Swap+ZMKDNCCK);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

                 {OC(OK_RC);XB(0x00200000); }
                 continue;
             }

         /* it is possible that no one has noticed the circuit going away, must test dncck */
         /* Since we still think we have a circuit zmkzsik ans zmkzsok are free */
         /* had sik or sok noticed they would have cleared havecircuit when saving */
         /* keys in zmkzsok or zmkzsik */

//outsok("No one waiting, saving keys\n");

             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }        /* save all new keys */
             {OC(Node_Swap+SAVESIK2);XB(0x80A0D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Swap+SAVESOK2);XB(0x80A0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Swap+SAVECCK2);XB(0x80A0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

         /* now test the existing dncck to see if the circuit has gone */

             {OC(Node_Fetch+ZMKDNCCK);XB(0x00A00000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
             {OC(compdiscrim);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Discrim_Type);XB(0x80B0A000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
             if (rc == Discrim_TypeOther) { /* we still have a circuit */
                 {OC(ZMK_AlreadyConnected);XB(0x00200000); }
                 continue;
             }

        /* ah hah, the circuit is gone, lets set new dncck and mark circuit as available */

             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Swap+ZMKDNCCK);XB(0x80A0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
             havesik=1;   /* indicate we have a circuit for whoever cares */
             havesok=1;   /* indicate we have a circuit for whoever cares */
             havecircuit=1;
             {OC(OK_RC);XB(0x00200000); }
             continue;
          }
          if (oc == ZMK_Disconnect) {
             if(havecircuit) {    /* after we zap downstream ZSIK and ZSOK will check in */
                 {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Fetch+ZMKDNCCK);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(CCK_Disconnect);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
                 havecircuit=0;
                 {OC(OK_RC);XB(0x00200000); }
                 continue;
             }
             {OC(ZMK_NoCircuit);XB(0x00200000); }
             continue;
          }
          if (oc == ZMK_WaitForDisconnect) {
//outsok("ZMK:WaitForDisconnect");
             if(!havecircuit) {                  /* no circuit to wait for zap */
//outsok("ZMK: - no wait, no circuit");
                 {OC(ZMK_NoCircuit);XB(0x00200000); }
                 continue;
             }
             if(havewaiter) {                    /* already have one */
//outsok("ZMK: - no wait, have waiter");
                 {OC(ZMK_AlreadyWaiting);XB(0x00200000); }
                 continue;
             }
//outsok("ZMK: - waiter is waiting");
             havewaiter=1;                       /* mark as having waiter */
             {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }  /* stash key */
             {OC(OK_RC);XB(0x00200000); }  /* now dk(0) */
             continue;
          }

          {OC(INVALIDOC_RC);XB(0x00200000); }
          continue;

       }
    }
}

/******************************************************************************/
/* Makes a SIK3 domain and returns key in upsik and sikdom                     */
/*                                                                            */
/*                 USES KEYS  sikdom, upsik                                   */
/******************************************************************************/
makesik3()
{
   JUMPBUF;
   UINT32 rc;

   save131415(sikdom);

   if (!(rc=fork())) {                           /* SIK domain */
        {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* zap the copy of the caller key */
        {OC(Domain_GetKey+upsik);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
        rc=dosik3();                        /* run the SIK3 */
        exit(rc);                           /* if eof detected */
   }
   if(rc > 1) {  /* no one returning */
      restore131415();
      return 1;
   }
   {OC(0);XB(0x00000000); }                          /* convenient key to return to with no side effect */
   {RC(rc);NB(0x0890800A); }  /* sik3 calls back, upsik is the SIK3 key */
   {rj(0x00000000,&_jumpbuf); }

   restore131415();
   return 0;
}

/******************************************************************************/
/* Makes a SOK3 domain and returns key in upsok and sokdom                    */
/*                                                                            */
/*                 USES KEYS  sokdom, upsok                                   */
/******************************************************************************/

makesok3()
{
   JUMPBUF;
   UINT32 rc;

   save131415(sokdom);

   if (!(rc=fork())) {                           /* SOK domain */
        {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* zap the copy of the caller key */
        {OC(Domain_GetKey+upsok);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        rc=dosok3();                        /* run the SOK3 */
        exit(rc);                           /* if eof detected */
   }
   if(rc > 1) { /* no one returning */
        restore131415();
        return 1;
   }
   {OC(0);XB(0x00000000); }                          /* convenient key to return to with no side effect */
   {RC(rc);NB(0x0890900B); }  /* sok3 calls back, upsok is the SOK3 key */
   {rj(0x00000000,&_jumpbuf); }

   restore131415();
   return 0;
}

/******************************************************************************/
/*  SOK3 domain just passes through - no processing                           */
/******************************************************************************/
dosok3()
{
    JUMPBUF;
    UINT32 rc,oc;
    char buf[1024];   /* my staging buffer */
    int  limit = 0;   /* initial limit of dnsok */
    int  len,tl;
    char *ptr;
//    char obuf[64];

    {OC(1024);XB(0x80C03000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }  /* keys to ZMK given upstream */

    for (;;) {                       /* sok loop */
        if(oc == KT+1) {
            {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* check in prior to death */
            return 0;                /* return for death */
        }
        if(!len) { /* return my limit without probing circuit */
           {OC(1024);XB(0x00B00000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }
           continue;
        }
        if(!limit) {                 /* must get limit of lower sok */
            {OC(0);XB(0x00E00000);RC(rc);NB(0x0810000E);cjcc(0x00000000,&_jumpbuf); }
            if(rc == KT+1) {         /* not connected */
                limit=getsok2key();  /* wait for connection */
            }
            else {
                limit=rc;            /* set limit */
            }
        }
        if (limit < 0) {             /* error */
            crash("Internal Error");
        }
        ptr=buf;                     /* starting point */
        while(len) {                 /* until done */
           tl=len;                   /* this much this time */
restartsok:
           if(tl > limit) tl=limit;  /* can't do all at once */
           {OC(oc);PS2(ptr,tl);XB(0x04E00000);RC(rc);NB(0x0810000E);cjcc(0x08000000,&_jumpbuf); }
           if(rc == KT+1) {          /* circuit died, must wait */
                limit=getsok2key();  /* get new limit */
                goto restartsok;     /* and try again */
           }
           len -= tl;                /* done this much */
           ptr += tl;                /* next place for data */
        }
                                     /* all done, go back for next call */
        {OC(1024);XB(0x00B00000);RC(oc);RS3(buf,1024,len);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }
    }
}

getsok2key()
{
    JUMPBUF;
    UINT32 rc,oc;
    char obuf[64];

    {OC(SOKWAITING);XB(0x00C00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);XB(0x00E00000);RC(rc);NB(0x0810000E);cjcc(0x00000000,&_jumpbuf); }
    return rc;
}

getsik2key()
{
    JUMPBUF;
    UINT32 rc,oc;

    {OC(SIKWAITING);XB(0x00C00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
}


/******************************************************************************/
/*  SIK3 domain just passes through - no processing                           */
/******************************************************************************/
dosik3()
{
    JUMPBUF;
    UINT32 oc,rc;
    char buf[1024];   /* my staging buffer */
    int actlen,tl;
    char *ptr;

    {OC(0);XB(0x80C03000);RC(oc);NB(0x0810000A);cjcc(0x00000000,&_jumpbuf); } /* keys to parent */
    if(oc == KT+1) {                           /* upsik died don't see this often */
       {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                 /* check in */
       return 0;                               /* return for death */
    }

    for(;;) {                                    /* sik loop */
      {OC(oc);XB(0x00D00000);RC(rc);RS3(buf,1024,actlen);NB(0x0B10000D);cjcc(0x00100000,&_jumpbuf); }  /* get some data */
      if (rc == KT+1) {                          /* SIK2 died */
         getsik2key();                           /* wait for reconnect */
         continue;                               /* try again */
      }
      ptr = buf;                                 /* starting point for delivery */
      while(actlen) {                            /* until done */
         tl = actlen;                            /* try this much */
         if (tl > (oc & 0xFFF)) {                /* unless too much */
             tl = (oc & 0xFFF);                  /* then use smaller amount */
         }
         {OC(0);PS2(ptr,tl);XB(0x04A00000);RC(oc);NB(0x0810000A);cjcc(0x08000000,&_jumpbuf); }  /* deliver */
         if(oc == KT+1) {                        /* upsik died must stop */
             {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }             /* notify cck */
             return 0;                           /* return for death */
         }
         actlen -= tl;                           /* did this much
         ptr += tl;                              /* next location */
      }
    }
}
/**************************************************************************
   TERMINAL MULTIPLEXOR BEGINS HERE
**************************************************************************/

 KEY tmsnode = 7;
 KEY tmcck   = 8;
 KEY tmsiksok = 9;   /* depends on which domain */
 KEY tm10    = 10;
 KEY tmreturner  = 11;
 KEY tm12    = 12;

/* KEYS below are saved in FORK but not shared with child */

 KEY tm13    = 13;
 KEY tm14    = 14;
 KEY tm15    = 15;

/*************************************************************************
   MAKETMMK

   INPUT KEYS:   psb,m,sb,dnsik,dnsok,dncck,bid
                          tm13  tm14  tm15  tm12
*************************************************************************/

/* MAXBRANCH can never be greater than 0xFFF because of ordercode restrictions */
/* of the BIO key which includes the Branch ID number in the ordercode         */
/* the 16 meg address space limits the number of branches to much less         */
/* and the restricted memory based on the use of memnode slots for saved keys  */
/* limits the memory to 8 meg.  each branch uses 2 pages for buffers limiting  */
/* the number of branches to 128*8 or 1000 (actually less)                     */

#define TMMAXBRANCH 500

/* Base slots in supernode for storage of extra keys */

#define TMREADER 0
#define TMWRITER 1
#define TMWAITER 2
#define TMRENDEZVOUS  3
#define TMCCK 4
#define TMREADERDOM 5
#define TMWRITERDOM 6
#define TMRENDEZVOUSDOM 7
#define TMSB 8

/* number of base slots reserved */
#define TMBASE  16

/* number of slots for each branch */
#define TMSLOTS  8

/* slots for branch specific keys */
#define TMSLOTSIK 0
#define TMSLOTSOK 1
#define TMSLOTCCK 2
#define TMSLOTBID 3
#define TMSLOTSIKDOM 4
#define TMSLOTSOKDOM 5
#define TMSLOTCCKDOM 6

/* databytes for various facets of arbitrator */
#define TMRDDB     1
#define TMWRDB     2
#define TMBIODB    3
#define TMTMMKDB   0

/* BIO ordercodes.  SIK/SOK/CCK ordercodes are added to these */
#define BIOREAD  0x10000000
#define BIOWRITE 0x20000000
#define BIOCCK   0x30000000
#define BIODIED  0x40000000
#define BIOREPEAT 0x70000000

   struct  tmbranch {
      short id;          /* determines slot numbers in snode */
      short readlength;  /* set by sik */
      char echo;      /* echo rules  0 no echo- activate cr, 1 raw, 2 echo-activate CR */
      char activate;  /* set when activation rule met */
      char activein,activeout;    /* if has output and not current, or input and not current */
      char havesok;   /* blocked, key in supernode */
      char havesik;   /* blocked, key in supernode */
      char havecck;   /* blocked, key in supernode */
#define TMMAXBUF 4096
#define TMMAXREAD 1024
      char *ininptr,*inoutptr;
      char *outinptr, *outoutptr;

      char inbuf[TMMAXBUF],outbuf[TMMAXBUF];
   };

#define MAXECHO 128
   struct tmmcontrol {
      short currentin,currentout;   /* current branch ids */
      short echoindex;
      char switchchar;  /* switch character */
      char writerwaiting;  /* was nothing to write */
      char waiterwaiting;
      char map[TMMAXBRANCH];
      char echobuf[MAXECHO];
   };

   struct tmmcontrol *tmmcntl = (struct tmmcontrol *)0x00200000;
   struct tmbranch *tmbranches = (struct tmbranch *)0x00300000;

   struct Node_KeyValues tmnkv = {3,8,
      {WindowM(0,0x00000100,2,0,0),
       WindowM(0,0x00000200,2,0,0),
       WindowM(0,0x00000300,2,0,0),
       WindowM(0,0x00000400,2,0,0),
       WindowM(0,0x00000500,2,0,0),
       WindowM(0,0x00000600,2,0,0)}
    };

/* slots 9-15 used to save keys at various times by various domains */

maketmmk(switchchar,actlen)
   char *switchchar;
   int actlen;
{

/* this domain becomes the TMMK domain.  The branch returned is the */
/* control branch                                                   */

/* CCK keys for the branches are also used as Branch ID keys for    */
/* switching.  These keys are FE keys with the CCK domain as the    */
/* keeper and a data key that identifies the branch                 */

/* BID keys are saved for the user and presented back to the user   */
/* on the Wait for active branch call.   This return give the user  */
/* the User's BID for his benefit and the CCK3 key of the branch for*/
/* the benefit of TMMK                                              */

/* KEYS INPUT: dnsik(tm13),dnsok(tm14),dncck(tm15),cck(tm12) */

    JUMPBUF;
    UINT32 oc,rc;
    UINT16 db;
    int i;
    struct Node_DataByteValue ndb;
    char inbuf[2];
    char outbuf[TMMAXREAD];
    int inbuflen,outbuflen;
    int slot;
    int echo;
    char buf[64];

    if(actlen != 1) {
        return TMMF_InvalidSwitch;
    }
    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+compfsf);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
    {OC(FSF_Create);XB(0xE0B04510);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+2);XB(0x80A0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_WriteData);PS2(&(tmnkv),sizeof(tmnkv));XB(0x04A00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Node_Fetch+compsnodef);XB(0x00000000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SNodeF_Create);XB(0xE0704510);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

    slot = TMCCK;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470F000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }  /* save tmcck */

    slot = TMSB;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84701000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    /* everything in tmmcntl and tmbranches is zero initially */

    tmmcntl->switchchar = *switchchar;

    save131415(tmcck);
    {OC(Domain_GetKey+dnsik);XB(0x00300000);NB(0x00809000);cjcc(0x00000000,&_jumpbuf); }  /* sik for reader */
    ndb.Byte=TMRDDB;
    {OC(Domain_MakeStart);PS2(&(ndb),sizeof(ndb));XB(0x04300000);NB(0x00808000);cjcc(0x08000000,&_jumpbuf); }

/***************************************************************************
    Reader Domain
***************************************************************************/
    if(!(rc=fork())) {  /*  Reader domain */
        inbuf[0]=0;
        inbuflen=0;
        {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* kill copy of caller */
        rc=0;
        while(1) {
           /* rc from sik key might indicate activate now (1 if Uart) but length will be zero */
           {OC(rc);PS2(inbuf,inbuflen);XB(0x84803000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
           {OC(1);XB(0x00900000);RC(rc);RS3(inbuf,1,inbuflen);NB(0x0B100009);cjcc(0x00100000,&_jumpbuf); }
           if(rc == KT+1) break;
        }
        {OC(1);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        exit(0);
    }
    restore131415();

    if(rc > 1) {  /* opps no space */
       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+2);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       return 2;
    }
    {RC(rc);NB(0x0890B00A); }
    {OC(0);XB(0x00000000); }
    {rj(0x00000000,&_jumpbuf); }        /* wait for reader to check in */

    slot = TMREADER;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470A000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMREADERDOM;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470B000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

    save131415(tmcck);
    {OC(Domain_GetKey+dnsok);XB(0x00300000);NB(0x00809000);cjcc(0x00000000,&_jumpbuf); }  /* sok for writer */
    ndb.Byte=TMWRDB;
    {OC(Domain_MakeStart);PS2(&(ndb),sizeof(ndb));XB(0x04300000);NB(0x00808000);cjcc(0x08000000,&_jumpbuf); }

/* THIS ASSUMES THAT THE NEXT LEVEL DOWN ALWAYS ACCEPTS TMMAXREAD characters */

/***************************************************************************
    Writer Domain
****************************************************************************/
    if(!(rc=fork())) {  /* Writer Domain */
       UINT32 limit;
       char buf[64];

//       {OC(Node_Fetch+compdiscrim);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
//       {OC(0);XB(0x80F09000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }

//sprintf(buf,"Writer started siksok type =%d\n",rc);
//outsok(buf);
       {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* kill copy of caller */
       limit=0;

//       {OC(0);XB(0x80F09000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
//sprintf(buf,"Sok Primed type=%d\n",rc);
//outsok(buf);
       while(1) {
          {OC(TMMAXREAD);XB(0x80803000);RC(rc);RS3(outbuf,TMMAXREAD,outbuflen);NB(0x0B000000);cjcc(0x00100000,&_jumpbuf); }
//sprintf(buf,"Writer instructions %d characters '%x'\n",outbuflen,outbuf[0]);
//outsok(buf);
                           /* call to get instructions */
          if(!limit) {OC(0);XB(0x00900000);RC(limit);NB(0x08100009);cjcc(0x00000000,&_jumpbuf); }   /* get limit */
          {OC(0);PS2(outbuf,outbuflen);XB(0x04900000);RC(rc);NB(0x08100009);cjcc(0x08000000,&_jumpbuf); }
          if(rc == 100 ) {
/* HACK HACK - should be a crash */
              outsok("TWO WRITERS **********************\n");
          }
//sprintf(buf,"SOK2 return rc=%X\n",rc);
//outsok(buf);
          if (rc == KT+1) break;
       }
       {OC(1);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       exit(0);
    }
    restore131415();
    if(rc > 1) {  /* opps no space */
       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+2);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       return 2;
    }

    {RC(rc);NB(0x0890B00A); }
    {OC(0);XB(0x00000000); }
    {rj(0x00000000,&_jumpbuf); }   /* wait for writer to check in */

    slot = TMWRITER;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470A000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMWRITERDOM;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470B000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

    tmmcntl->writerwaiting=1;

/*************************************************************************************************
    Rendezvous domain for MakeBranch
*************************************************************************************************/
    save131415(tmcck);
    {OC(Domain_MakeStart);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }

    if(!(rc=fork())) {  /* Rendezvous */
       int havetm=0;
       int havebr=0;
#define RENDTM 1
#define RENDBR 2

/* the TM resume key is stored in tm 13 */
/* the BR keys enter as 10,caller and are stored as 14,15 */

       {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);XB(0xC080A300); }
       while(1) {
          {RC(oc);NB(0x0890A002); }   /* collect Domain and caller from br */
          {rj(0x00000000,&_jumpbuf); }

          switch(oc) {
          case RENDTM:
             if(havebr) {  /* can return immediately */
                havebr=0;
                {OC(0);XB(0x9020E00F); }
                continue;
             }
             havetm=1;  /* note we are here first */
             {OC(Domain_SwapKey+tm13);XB(0x80302000);NB(0x00200020);cjcc(0x00000000,&_jumpbuf); }
             {OC(0);XB(0x00200000); }
             continue;

          case RENDBR:
             if(havetm) {  /* can meet immediately */
                 havetm=0;
                 {OC(0);XB(0x90D0A002); }
                 continue;
             }
             /* we must wait for tm */
             havebr=1;
             {OC(Domain_SwapKey+tm14);XB(0x8030A000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Domain_SwapKey+tm15);XB(0x80302000);NB(0x00200020);cjcc(0x00000000,&_jumpbuf); }
             {OC(0);XB(0x00200000); }
             continue;
          }
       }
    }
    restore131415();
    if(rc > 1) {
       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+2);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       {OC(DESTROY_OC);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       return 2;
    }
    {RC(rc);NB(0x08C0AB00); }   /* get rendezvous entry key */
    {OC(0);XB(0x00000000); }
    {rj(0x00000000,&_jumpbuf); }

    slot=TMRENDEZVOUS;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470A000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    slot=TMRENDEZVOUSDOM;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470B000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    {OC(Node_Fetch+compreturner);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

/* tm10,tm13,tm14,tm15 are now free, tm 12 contains the BID for the control branch */

    makebranch(0,1);   /* make control branch (0) with CCK */

/* now send the reader off, reading with control branch echo rules */

//outsok("Starting Reader\n");

    slot = TMREADER;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    {OC(tmbranches[0].echo);XB(0x00C00000); }
    {fj(0x00000000,&_jumpbuf); }

/***************************************************************************
    TMMK Domain
***************************************************************************/

    {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
    slot = TMBASE+TMSLOTS*0+TMSLOTSIK;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMBASE+TMSLOTS*0+TMSLOTSOK;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMBASE+TMSLOTS*0+TMSLOTCCK;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }

//outsok("Begin TMMK\n");

    {OC(0);XB(0xF020CDEF); }
    while(1) {
       {RC(oc);DB(db);RS3(outbuf,TMMAXREAD,actlen);NB(0x0FF0CDE2); }
       {rj(0x00100000,&_jumpbuf); }

//sprintf(buf,"TMM(%d) %08X len=%d\n",db,oc,actlen);
//outsok(buf);

       switch(db) {
       case TMRDDB:
          if(oc == 100) {
/* HACK HACH should be a crash */
              outsok("TWO READERS **********************");
              {OC(0);XB(0x00000000); }
              break;
         }
          echo=doreader(oc,outbuf,actlen,0);
          {OC(echo);XB(0x00200000); }
          break;
       case TMWRDB:
          rc=dowriter(oc,outbuf,&actlen,TMMAXREAD);
          {OC(rc);PS2(outbuf,actlen);XB(0x04200000); }
          break;
       case TMBIODB:
          rc=dobio(oc,outbuf,&actlen,TMMAXREAD);
//sprintf(buf,"TMBIODB return actlen=%d rc=%X\n",actlen,rc);
//outsok(buf);
          {OC(rc);PS2(outbuf,actlen);XB(0xF420CDEF); }
          break;
       case TMTMMKDB:
          rc=dotmmk(oc,outbuf,&actlen,TMMAXREAD);
          if(rc == KT+4) {  /* we were asked to go away */
              exit(0);
          }
          {OC(rc);PS2(outbuf,actlen);XB(0xF420CDEF); }
          break;
       }
    }
}

/***************************************************************************
    DOREADER   - reading 1 character at a time

    len is 1 or 0 if activate now
***************************************************************************/
doreader(oc,buf,len,id)
    UINT32 oc;
    char *buf;
    int len;
{
    struct tmbranch *tmbr;
    int used,space,slot;
    JUMPBUF;
    char pbuf[64];
    char *ptr;

    if(!id) { /* reader domain calling, as opposed to GenerateASCII for a branch  */

       if(len && (tmmcntl->switchchar == *buf)) {

//outsok("Doreader: switch character\n");

           tmmcntl->currentin=0;  /* switch to control branch input */
       }
       tmbr = &tmbranches[tmmcntl->currentin];
    }
    else {  /* GenerateASCIIInput - probably only used for esc (don't echo) */
       tmbr = &tmbranches[id];
    }

//sprintf(pbuf,"Doreader: BR(%d) len=%d inin %X outin %X buf %x\n",
//        tmmcntl->currentout,len,tmbr->ininptr,tmbr->inoutptr,tmbr->inbuf);
//outsok(pbuf);

    if(len && (tmbr->echo == 2) && (*buf == 8)) {  /* supposed to handle backspace */
       len=0;  /* don't present */
       if(tmbr->ininptr != tmbr->inoutptr) {  /* there is something */
          if(ptr == tmbr->inbuf) {
              ptr=tmbr->inbuf+(TMMAXBUF-1);  /* last */
          }
          else {
              ptr=tmbr->ininptr-1;
          }
          if(*ptr != '\r') {  /* don't backspace over cr */
              tmbr->ininptr=ptr;  /* back up input cursor, this could empty buffer */
              if( !id && (tmmcntl->echoindex < MAXECHO-2)) {  /* room for 3 characters */
                  tmmcntl->echobuf[tmmcntl->echoindex] = 0x08;
                  tmmcntl->echobuf[tmmcntl->echoindex+1] = ' ';
                  tmmcntl->echobuf[tmmcntl->echoindex+2] = 0x08;
                  tmmcntl->echoindex += 3;
              }
          }
       }
    }
    if(len) {   /* if activate now don't do anything */
       used = (tmbr->ininptr-tmbr->inoutptr);
       if(used < 0) {
          used = TMMAXBUF-used;
       }
       space = TMMAXBUF-used;
       if(space) { /* there is room for 1 character */
          *tmbr->ininptr = *buf;
          tmbr->ininptr++;
          if((tmbr->ininptr - tmbr->inbuf) == TMMAXBUF) {
             tmbr->ininptr = tmbr->inbuf;
          }
/* TEST for activation */
          if(*buf == tmmcntl->switchchar) {
              tmbr->activate = 1;
          }
          switch (tmbr->echo) {
          case 0:   /* no echo, no BS processing, Activate on CR (or length) */
              if(*buf == '\r') {
                   tmbr->activate = 1;
              }
              if(used+1 >= tmbr->readlength) {
                   tmbr->activate = 1;
              }
              break;
          case 2:  /* echo, BS processes, Acrivate on CR or length */
              if(!id & (tmmcntl->echoindex < MAXECHO) && (*buf != 0x1b)) {  /* no escapes echoed */
                 tmmcntl->echobuf[tmmcntl->echoindex] = *buf;
                 tmmcntl->echoindex++;
              }

              if(*buf == '\r') {
                   tmbr->activate = 1;
                   if(!id & (tmmcntl->echoindex < MAXECHO)) {
                       tmmcntl->echobuf[tmmcntl->echoindex] = '\n';
                       tmmcntl->echoindex++;
                   }

              }
              if(used+1 >= tmbr->readlength) {
                   tmbr->activate = 1;
              }
              break;
          case 1:  /* no echo, no bs, activate each char regardless of length */
              tmbr->activate = 1;
              break;
          }
       }  /* else we simply ignore character */

//sprintf(pbuf,"Doreader space = %d(%d) activate %d readlen %d,havesik %d\n",space,used,tmbr->activate,
// tmbr->readlength,tmbr->havesik);
// outsok(pbuf);

       if(tmbr->activate && tmbr->havesik) {
          int slot;

//sprintf(pbuf,"Wake Reader(%d) SIK\n",tmmcntl->currentin);
//outsok(pbuf);

          slot = TMBASE + TMSLOTS*tmbr->id + TMSLOTSIK;
          {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
          {OC(BIOREPEAT);XB(0x10B0000C); }
          {fj(0x00000000,&_jumpbuf); }

          tmbr->havesik=0;
       }
    }  /* end of process input */

//sprintf(pbuf,"DoreaderEnd: %X %X %X\n",tmbr->ininptr,tmbr->inoutptr,tmbr->inbuf);
//outsok(pbuf);

    if(tmmcntl->echoindex && tmmcntl->writerwaiting) {
       slot=TMWRITER;
       {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
       {OC(0);XB(0x00C00000); }  /* no characters to write, will come back for more */
       {fj(0x00000000,&_jumpbuf); }
       tmmcntl->writerwaiting=0;
    }

    return 0;   /* continue reading, using current echo rules */
}

/***************************************************************************
    DOWRITER - writer calling for work.  see if any output queued on current branch
     if not save caller key in snode slot TMWRITER and set tmmcntl->writerwaiting;
***************************************************************************/
dowriter(oc,buf,len,maxlen)
    UINT32 oc;
    char *buf;
    int *len;
    int maxlen;
{
    struct tmbranch *tmbr;
    JUMPBUF;
    int l;
    int slot;
    char pbuf[256];

    if(tmmcntl->echoindex) {
        strncpy(buf,tmmcntl->echobuf,tmmcntl->echoindex);
        *len=tmmcntl->echoindex;
        tmmcntl->echoindex=0;
        return 0;
    }

    tmbr = &tmbranches[tmmcntl->currentout];

//sprintf(pbuf,"Dowriter: BR(%d) outin %X outout %X buf %x\n",
//        tmmcntl->currentout,tmbr->outinptr,tmbr->outoutptr,tmbr->outbuf);
//outsok(pbuf);

    if(tmbr->outinptr == tmbr->outoutptr) {  /* there is no output */

//outsok("Writer going to sleep\n");

        if(tmbr->havesok) {   /* let SOK go */
           slot = TMBASE + TMSLOTS*tmmcntl->currentout + TMSLOTSOK;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(0);XB(0x10B0000C); }
           {fj(0x00000000,&_jumpbuf); }
           tmbr->havesok=0;
        }

        slot = TMWRITER;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
        tmmcntl->writerwaiting = 1;
        return 0;
    }
/* the current branch has some output.   OC has the number of bytes we can write */
/* maxlen has the maximum number we can return to the writer domain              */
/* some smarter code might avoid this extra copy                                 */

    if(oc > maxlen) oc = maxlen;

/* oc now has max we can copy into buf.  set copied amount in *len               */

    if(tmbr->outoutptr < tmbr->outinptr)  { /* normal case */
       l=tmbr->outinptr-tmbr->outoutptr;
       if(l > oc) l = oc;
       memcpy(buf,tmbr->outoutptr,l);
       tmbr->outoutptr += l;
       *len=l;
    }
    else {   /* output buffer wraps, do output in pieces (lazy) */
       l = tmbr->outbuf+TMMAXBUF - tmbr->outoutptr;
       if(l > oc) l = oc;
       memcpy(buf,tmbr->outoutptr,l);
       *len = l;
       tmbr->outoutptr += l;
       if(tmbr->outoutptr >= (tmbr->outbuf+TMMAXBUF)) {  /* went to end */
           tmbr->outoutptr = tmbr->outbuf;
       }
    }
    if(tmbr->outoutptr == tmbr->outinptr) {  /* did it all */
        tmbr->outoutptr = tmbr->outinptr = tmbr->outbuf;

        if(tmbr->havesok) {   /* let SOK go */
           slot = TMBASE + TMSLOTS*tmmcntl->currentout + TMSLOTSOK;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(0);XB(0x10B0000C); }
           {fj(0x00000000,&_jumpbuf); }
           tmbr->havesok=0;
        }
    }

//sprintf(pbuf,"DowriterEnd: %X %X %X\n",tmbr->outinptr,tmbr->outoutptr,tmbr->outbuf);
//outsok(pbuf);

    return 0;
}
/***************************************************************************
    DOBIO
***************************************************************************/
dobio(boc,buf,len,maxlen)
    UINT32 boc;
    char *buf;
    int *len;
    int maxlen;
{
    int id;
    UINT32 oc,rc;
    JUMPBUF;

//outsok("DOBIO Called\n");

    switch((boc & 0x30000000)) {
    case BIOREAD:   /* sik call by some branch */
       rc=dobioread(boc,buf,len,maxlen);
       return rc;
    case BIOWRITE:  /* sok call by some branch */
       rc=dobiowrite(boc,buf,len);
       return rc;
    case BIOCCK:    /* cck call by some branch */
       rc=dobiocck(boc,buf,len);
       return rc;
    case BIODIED:   /* some piece of some branch died, kill branch */
    }
    return INVALIDOC_RC;
}

/***************************************************************************
    DOTMMK  Keys passed in tm12 and tm13
***************************************************************************/
dotmmk(oc,buf,len,maxlen)
    UINT32 oc;
    char *buf;
    int *len;
    int maxlen;
{
    JUMPBUF;
    UINT32 rc;
    int id,slot,reason;
    struct tmbranch *tmbr;
    char pbuf[64];
    char *ptr;

    if(oc == KT) {
       return  TMMK_AKT;
    }
    if(oc == KT+4) {
       oc = 4;
    }

    switch(oc) {
    case TMMK_CreateBranch:   /* BID is in tm12 */
       for(id=0;id<TMMAXBRANCH;id++) {
          if(!tmmcntl->map[id]) {
              if(makebranch(id,1)) {
                 return 1;
              }
              slot = TMBASE + TMSLOTS*id + TMSLOTSIK;
              {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
              slot = TMBASE + TMSLOTS*id + TMSLOTSOK;
              {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }
              slot = TMBASE + TMSLOTS*id + TMSLOTCCK;
              {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00C0EF00);cjcc(0x08000000,&_jumpbuf); }
              return 0;
          }
       }
       return TMMK_NOSPACE;

    case TMMK_DestroyBranch:
       id=findbranch();
       if(id == -1) return TMMK_NOTABRANCH;
       if(!id) return TMMK_CANTDESTROYCONTROL;

       zapsiksok(id);
       zapcck(id);

       tmmcntl->map[id]=0;

       if(tmmcntl->currentin == id) {
          tmmcntl->currentin=0;
          slot = TMCCK;
          {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00808000);cjcc(0x08000000,&_jumpbuf); }
          {OC(CCK_ActivateNow);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* cause echo rule switch */
       }
       if(tmmcntl->currentout == id) tmmcntl->currentout=0;
       return 0;

    case TMMK_SwitchInput:
       id=findbranch();

//sprintf(pbuf,"TMMK-switch Input ID %d\n",id);
//outsok(pbuf);

       if(id == -1) return TMMK_NOTABRANCH;
       tmbr = &tmbranches[id];
       tmbr->activein=0;
       tmmcntl->currentin = id;
       slot = TMCCK;
       {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00808000);cjcc(0x08000000,&_jumpbuf); }
       {OC(CCK_ActivateNow);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }   /* cause switch of echo mode */

       if(tmbr->havesik) {
           slot = TMBASE + TMSLOTS*id + TMSLOTSIK;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(BIOREPEAT);XB(0x10B0000C); }
           {fj(0x00000000,&_jumpbuf); }
           tmbr->havesik=0;
       }

       return 0;

    case TMMK_SwitchOutput:
       id=findbranch();
       if(id == -1) return TMMK_NOTABRANCH;

//sprintf(pbuf,"TMMK-switch output ID %d\n",id);
//outsok(pbuf);

       if(id != tmmcntl->currentout) {
            tmbr= &tmbranches[tmmcntl->currentout];
            if((tmbr->outinptr != tmbr->outoutptr) ||  /* something queued */
                tmbr->havesok) {                       /* or someone waiting to write */

                tmbr->activeout=1;
                if(tmmcntl->waiterwaiting) {
                    wakewaiter(tmmcntl->currentout,1);
                    tmbr->activeout=0;
                    tmbr->activein=0;
                }
            }
       }

       tmbr = &tmbranches[id];
       tmbr->activeout = 0;
       tmmcntl->currentout = id;
       if(tmmcntl->writerwaiting) {
           slot=TMWRITER;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(0);XB(0x00C00000); }  /* no characters to write, will come back for more */
           {fj(0x00000000,&_jumpbuf); }
           tmmcntl->writerwaiting=0;
       }
       if(tmbr->havesok) {
           slot = TMBASE + TMSLOTS*id + TMSLOTSOK;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(BIOREPEAT);XB(0x10B0000C); }
           {fj(0x00000000,&_jumpbuf); }
           tmbr->havesok=0;
       }
       return 0;

    case TMMK_DestroyTMM:
       for(id=0;id<TMMAXBRANCH;id++) {
          if(tmmcntl->map[id]) {  /* there is a branch */
              zapsiksok(id);
              zapcck(id);
              tmmcntl->map[id]=0;
          }
       }
       zapdom(TMREADERDOM);
       zapdom(TMWRITERDOM);
       zapdom(TMRENDEZVOUSDOM);

       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+2);XB(0x00C00000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
       {OC(KT+4);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* zap fs */

       /* return keys to caller */
       slot = TMCCK;
       {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00808000);cjcc(0x08000000,&_jumpbuf); }
       {OC(CCK_RecoverKeys);XB(0x00800000);RC(rc);NB(0x08E0DEF0);cjcc(0x00000000,&_jumpbuf); }
       {OC(rc);XB(0xF0B0DEF2); }
       {fj(0x00000000,&_jumpbuf); }

       {OC(KT+4);XB(0x00700000);RC(rc);NB(0x08200020);cjcc(0x00000000,&_jumpbuf); }

       return KT+4;   /* go quietly into the night */

    case TMMK_GenerateASCIIInput:
       /* buf for len has data to put onto input queue */

       id=findbranch();
       if(id == -1) return TMMK_NOTABRANCH;
       ptr=buf;
//sprintf(pbuf,"TMMK Generate for ID %d\n",id);
//outsok(pbuf);
       while(*len) {
         doreader(0,ptr,1,id);
         ptr++;
         (*len)--;
       }
//outsok("TMMK Generate done\n");
       return 0;

    case TMMK_WaitForActiveBranch:
       if(tmmcntl->waiterwaiting) {
          {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
          return TMMK_ALREADYWAITING;
       }
       for(id=0;id<TMMAXBRANCH;id++) {
          if(tmmcntl->map[id]) {  /* possible branch */
             tmbr = &tmbranches[id];
             if(tmbr->activein || tmbr->activeout) {
                  slot = TMBASE + TMSLOTS*id + TMSLOTCCK;
                  {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
                  slot = TMBASE + TMSLOTS*id + TMSLOTBID;
                  {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00E0DEF0);cjcc(0x08000000,&_jumpbuf); }
                  reason = tmbr->activeout + 2*tmbr->activein;
                  tmbr->activein = tmbr->activeout = 0;
//sprintf(pbuf,"TMMK-WAIT return (%d) %d\n",id,reason);
//outsok(pbuf);
                  return reason;
             }
          }
       }
       slot = TMWAITER;
       {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
       tmmcntl->waiterwaiting=1;
       return 0;   /* returning to nothing */

    case TMMK_BranchStatus:
       id=findbranch();
       if(id == -1) return TMMK_NOTABRANCH;
       tmbr = &tmbranches[id];
       reason=0;
       if(!id) reason = 2;   /* start here */
       if(id == tmmcntl->currentin)  reason |= 0x10;
       if(id == tmmcntl->currentout) reason |= 0x08;
       if(tmbr->havesik) reason |= 0x20;  /* waiting for input */
       if(tmbr->outinptr != tmbr->outoutptr) reason |= 0x04;
       {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
       return reason;
    }
    return INVALIDOC_RC;
}

/***************************************************************************
    FINDBRANCH   - input is CCK key in tm12

    This could be done better IF:
       The CCK key were a FE key
       We used DC_IdentifySegment to retrieve the FE Node
       The FE node contained a data key with the branch ID
***************************************************************************/
findbranch()
{
    JUMPBUF;
    UINT32 rc;
    int i,slot;

    {OC(Node_Fetch+compdiscrim);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }

    for(i=0;i<TMMAXBRANCH;i++) {
       if(tmmcntl->map[i]) {  /* possible choice */
          slot = TMBASE + TMSLOTS*i +TMSLOTCCK;
          {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
          {OC(Discrim_Compare);XB(0xC0F0CE00);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
          if (!rc) {
             return i;
          }
       }
    }
    return -1;
}

/***************************************************************************
    DOBIOCCK
***************************************************************************/
dobiocck(boc,buf,len)
    UINT32 boc;
    char *buf;
    int *len;
{
    JUMPBUF;
    UINT32 oc,rc;
    int id,slot;
    struct tmbranch *tmbr;

    oc = boc & 0x8000FFFF;  /* can support KT */
    id = (boc >> 16) & 0xFFF;
    tmbr = &tmbranches[id];

    if(oc == KT) {
        *len=0;
        {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
        return CCK3_AKT;
    }

    if(oc == CCK_ActivateNow) {  /* even if not current branch we can do this */
        {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
        if(tmbr->havesik) {
           tmbr->activate=1;   /* force activation */
           slot = TMBASE + TMSLOTS*id + TMSLOTSIK;
           {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
           {OC(BIOREPEAT);XB(0x00C00000); }
           {fj(0x00000000,&_jumpbuf); }   /* send off with no characters and BIOREPEAT */
           tmbr->havesik=0;
        }
        return 0;
    }
    if((oc == CCK_RecoverKeys) || (oc == CCK_TAP))  {  /* we can do this... sometime */
        zapsiksok(id);
        slot = TMBASE +TMSLOTS*id +TMSLOTBID;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        makebranch(id,0);   /* make new branch leaving CCK alone */
        slot = TMBASE + TMSLOTS*id + TMSLOTSIK;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        slot = TMBASE + TMSLOTS*id + TMSLOTSOK;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }
        slot = TMBASE + TMSLOTS*id + TMSLOTCCK;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
        return 0;
    }

    /* OTHERS HERE */

    if(oc == concck__start_log) {
        slot=TMCCK;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        {OC(concck__start_log);XB(0x00C00000);RC(rc);NB(0x082000C0);cjcc(0x00000000,&_jumpbuf); }
        return 0;
    }
    if(oc == concck__stop_log) {
        slot=TMCCK;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        {OC(concck__stop_log);XB(0x00C00000);RC(rc);NB(0x082000C0);cjcc(0x00000000,&_jumpbuf); }
        return 0;
    }

    /* pass through to lower CCK ? */

    {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }  /* clean key regs */
    return 0;
}

/***************************************************************************
    zapsiksok
***************************************************************************/
zapsiksok(id)
    int id;
{
    JUMPBUF;
    int slot;
    UINT32 rc;

    slot = TMBASE + TMSLOTS*id + TMSLOTSIKDOM;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMBASE + TMSLOTS*id + TMSLOTSOKDOM;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Domain_MakeBusy);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_MakeBusy);XB(0x00D00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(Domain_GetMemory);XB(0x00C00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); } /* memory node */

    {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }     /* stack page */
    {OC(SB_DestroyPage);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(DC_DestroyDomain);XB(0xC060C400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(Domain_GetMemory);XB(0x00D00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); } /* memory node */

    {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }     /* stack page */
    {OC(SB_DestroyPage);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(DC_DestroyDomain);XB(0xC060D400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    return 0;
}

/***************************************************************************
    zapcck
***************************************************************************/
zapcck(id)
    int id;
{
    int slot;
    JUMPBUF;
    UINT32 rc;

    slot = TMBASE + TMSLOTS*id + TMSLOTCCKDOM;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Domain_MakeBusy);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_GetMemory);XB(0x00C00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); } /* memory node */
    {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }     /* stack page */
    {OC(SB_DestroyPage);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(DC_DestroyDomain);XB(0xC060C400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
}

/***************************************************************************
    zapdom
***************************************************************************/
zapdom(slot)
    int slot;
{
    JUMPBUF;
    UINT32 rc;

    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Domain_MakeBusy);XB(0x00C00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_GetMemory);XB(0x00C00000);RC(rc);NB(0x0880E000);cjcc(0x00000000,&_jumpbuf); } /* memory node */
    {OC(Node_Fetch+1);XB(0x00E00000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }     /* stack page */
    {OC(SB_DestroyPage);XB(0x8040F000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_DestroyNode);XB(0x8040E000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(DC_DestroyDomain);XB(0xC060C400);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
}

/***************************************************************************
    DOBIOWRITE
***************************************************************************/
dobiowrite(boc,buf,len)
    UINT32 boc;
    char *buf;
    int *len;
{
     JUMPBUF;
     int slot,reason,id;
     UINT32 oc,rc;
     int used,space;
     struct tmbranch *tmbr;
     char pbuf[256];

     oc = boc & 0x8000FFFF;  /* can support KT */
     id = (boc >> 16) & 0xFFF;
     tmbr = &tmbranches[id];

//sprintf(pbuf,"DOBIOWRITE(%d) %X(%d) current=%d\n",id,oc,*len,tmmcntl->currentout);
//outsok(pbuf);

     reason=0;
     if(tmmcntl->currentout != id) {
        if(tmbr->activein) reason=2;  /* there is output than made branch active */
        tmbr->activeout=1;
        tmbr->havesok=1;
//outsok("SOK waiting\n");
        slot = TMBASE+TMSLOTS*id+TMSLOTSOK;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
        if(tmmcntl->waiterwaiting) {
            wakewaiter(id,reason+1);
            tmbr->activeout=0;
            tmbr->activein=0;
        }
        return 0;
     }

     /* now put data into buffer  */

     used = (tmbr->outinptr-tmbr->outoutptr);
     if(used < 0) {
        used = TMMAXBUF-used;
     }
     space = TMMAXBUF-used;

//sprintf(pbuf,"DOBIOWRITE space=%d buf,in,out %X,%x,%x\n",space,
//     tmbr->outbuf,tmbr->outinptr,tmbr->outoutptr);
//outsok(pbuf);

     if(space < *len) { /* will not fit, block */
        tmbr->havesok=1;
        slot = TMBASE+TMSLOTS*id+TMSLOTSOK;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
        return 0;
     }

     /* ok have space, move data, possibly in two parts */

     if(tmbr->outinptr < tmbr->outoutptr) {  /* no wrap possible */
        memcpy(tmbr->outinptr,buf,*len);
        tmbr->outinptr += (*len);
     }
     else {  /* wrap is possible */
        int l,ll;
        char *ptr;

        ll=*len;
        ptr=buf;
        l=(tmbr->outbuf + 4096 - tmbr->outinptr);
        if(l > *len) l=ll;
        memcpy(tmbr->outinptr,ptr,l);
        tmbr->outinptr += l;

        ll -= l;
        ptr += l;
        if(ll) {  /* some to go at begininng */
           memcpy(tmbr->outbuf,ptr,ll);
           tmbr->outinptr = tmbr->outbuf+ll;
        }
     }

     if(tmmcntl->writerwaiting) {
        slot = TMWRITER;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        tmmcntl->writerwaiting = 0;

//        {OC(0);XB(0x00C00000); }    /* send no characters, will write 0 and call back for more */
/* must send some as it may be the interjected bell */
        dowriter(1024,buf,len,TMMAXREAD);
        {OC(0);PS2(buf,*len);XB(0x04C00000); }
        {fj(0x08000000,&_jumpbuf); }
        *len = 0;
     }

/* we never return immediately.  Always allow writer to free this */
/* NOPE a problem not allowing writing on non-current branch */
/* FIXED by calling wakewaiter if havesok=1 on switch output */

     if(tmbr->outinptr != tmbr->outoutptr) {  /* if something queued to write */
        tmbr->havesok=1;
        slot = TMBASE+TMSLOTS*id+TMSLOTSOK;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
     }

//sprintf(pbuf,"DOBIOWRITEEND: %X %X %X\n",tmbr->outinptr,tmbr->outoutptr,tmbr->outbuf);
//outsok(pbuf);

     *len=0;
     return 0;
}

/***************************************************************************
    DOBIOREAD
***************************************************************************/
dobioread(boc,buf,len,maxlen)
     UINT32 boc;
     char *buf;
     int *len;
     int maxlen;
{
     JUMPBUF;
     int slot,reason,id,echo,oldecho;
     struct tmbranch *tmbr;
     UINT32 oc,rc;
     char *inptr,*outptr;
     char pbuf[256];
     int activate;
     int readlen;

     oc = boc & 0x8000FFFF;  /* can support KT */
     id = (boc >> 16) & 0xFFF;
     echo = (oc >> 12) & 0x00F;
     readlen = oc & 0xFFF;
     tmbr = &tmbranches[id];

//sprintf(pbuf,"DOBIOREAD(%d) oc=%X len %d, echo %d\n",id,oc,maxlen,echo);
//outsok(pbuf);

     oldecho = tmbr->echo;
     tmbr->echo=echo;                  /* set for reader */
     tmbr->readlength=readlen;
     reason=0;
     if(tmmcntl->currentin != id) {

//outsok("DBBIOREAD: Wake waiter\n");

        if(tmbr->activeout) reason=1;  /* there is output then made branch active */
        tmbr->activein=1;
        tmbr->havesik=1;
        slot = TMBASE+TMSLOTS*id+TMSLOTSIK;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
        if(tmmcntl->waiterwaiting) {
            wakewaiter(id,reason+2);
            tmbr->activein=0;
            tmbr->activeout=0;
        }
        return 0;
     }
 /* there is input to provide, give it up, must honor activation rules */
 /* the switch character always activates                              */

     /* echo = 0 activate on CR, echo = 1 activate every char, echo = 2 activate CR */
     /* activate on switch character */

     /* start at tmbr->inoutptr  -> tmbr->ininptr looking for wrap and activation */

     activate = 0;
     if((oldecho != echo) || !tmbr->activate) {  /* must test for activate with new rule */
         int tlen=0;

         inptr = tmbr->inoutptr;
         while(inptr != tmbr->ininptr) {
            if(tlen == maxlen) {activate=1;break;}
            if(tlen == readlen) {activate=1;break;}
            if(*inptr == tmmcntl->switchchar) {activate=1;break;}
            if(echo == 1) {activate=1;break;}
            if((echo == 0 || echo == 2) && (*inptr == '\r')) {activate=1;break;}
            tlen++;
            inptr++;
            if((inptr - tmbr->inbuf) == TMMAXBUF) {  /* wrap point */
                inptr = tmbr->inbuf;
            }
         }
     }
     else {
         activate = 1;
     }

     tmbr->activate=0;

     if(!activate) {  /* hold key */
        slot = TMBASE + TMSLOTS*id + TMSLOTSIK;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84702000);NB(0x00200020);cjcc(0x08000000,&_jumpbuf); }
        tmbr->havesik=1;
//sprintf(pbuf,"SIK (%d) sleeping\n",id);
//outsok(pbuf);
        {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
        return 0;
     }

/* we have enough to satisfy the read */

     *len=0;
     inptr = tmbr->inoutptr;
     outptr = buf;

//sprintf(pbuf,"DOBIOREADSTART: ininptr %X inoutptr %X \n",tmbr->ininptr,tmbr->inoutptr);
//outsok(pbuf);

     while(inptr != tmbr->ininptr) {
        if(*len == maxlen) {
            break;    /* force return */
        }
        if(*len == readlen) {
            break;
        }
        *outptr = *inptr;
        (*len)++;
        inptr++;
        outptr++;

        if(*(outptr-1) == tmmcntl->switchchar) {
            break;
        }
        if(echo == 1) {
           break;
        }
        if(*(outptr-1) == '\r') {
            break;
        }

        if((inptr - tmbr->inbuf) == TMMAXBUF) {  /* wrap point */
           inptr = tmbr->inbuf;
        }
     }
     tmbr->inoutptr=inptr;

//sprintf(pbuf,"DOBIOREAD: moved %d bytes \n",*len);
//outsok(pbuf);

     {OC(0);XB(0x00B00000);NB(0x00F0CDEF);cjcc(0x00000000,&_jumpbuf); }
     return 0;
}

/***************************************************************************
    WAKEWAITER
***************************************************************************/
wakewaiter(id,reason)
    int id;
    int reason;
{
    JUMPBUF;
    int slot;
    char pbuf[64];

//sprintf(pbuf,"WakeWAITER(%d) %d\n",id,reason);
//outsok(pbuf);

    slot=TMWAITER;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }

    slot=TMBASE+TMSLOTS*id+TMSLOTCCK;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }

    slot=TMBASE+TMSLOTS*id+TMSLOTBID;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }

    {OC(reason);XB(0xD0B0DE0C); }
    {fj(0x00000000,&_jumpbuf); }

    tmmcntl->waiterwaiting=0;

    return 0;
}
/*************************************************************************
   MAKEBRANCH(id)

   BID is in tm12
**************************************************************************/

makebranch(id,makecck)
    int id;
    int makecck;  /* will be zero of remaking branch for TAP operation */
{
    JUMPBUF;
    UINT32 oc,rc;
    int slot;
    struct Node_DataByteValue ndb;
    char buf[TMMAXREAD];
    int actlen;

    slot = TMBASE+id*TMSLOTS+TMSLOTBID;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470C000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    ndb.Byte = TMBIODB;
    {OC(Domain_MakeStart);PS2(&(ndb),sizeof(ndb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMRENDEZVOUS;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080A000);cjcc(0x08000000,&_jumpbuf); }
/***********************************************************************
    Branch SIK
***********************************************************************/
    if(!(rc=fork())) {
//outsok("SIK domain ready\n");
       {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* kill copy of caller */
       {OC(Domain_GetKey+tm12);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }  /* BIO key */
       {OC(RENDBR);XB(0x80A03000);RC(rc);NB(0x08100002);cjcc(0x00000000,&_jumpbuf); }  /* RENDEZVOUS */
       while(1) {
readagain:
           {OC(BIOREAD+(id << 16)+rc);XB(0x00800000);RC(oc);RS3(buf,TMMAXREAD,actlen);NB(0x0B000000);cjcc(0x00100000,&_jumpbuf); }
//sprintf(buf+512,"SIK return from BIOREAD rc=%X\n",oc);
//outsok(buf+512);
           if(oc == BIOREPEAT) {
//outsok("READER REPEAT read\n");
               goto readagain;
           }

           {OC(oc);PS2(buf,actlen);XB(0x04200000);RC(rc);NB(0x08100002);cjcc(0x08000000,&_jumpbuf); }
           if(rc == KT+1) break;
       }
       {OC(BIODIED+(id << 16));XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       exit(0);
    }
    if(rc > 1) return 1;

    {OC(RENDTM);XB(0x00A00000);NB(0x0090C00D);cjcc(0x00000000,&_jumpbuf); }

    slot = TMBASE+id*TMSLOTS+TMSLOTSIK;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470D000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMBASE+id*TMSLOTS+TMSLOTSIKDOM;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470C000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    ndb.Byte = TMBIODB;
    {OC(Domain_MakeStart);PS2(&(ndb),sizeof(ndb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMRENDEZVOUS;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080A000);cjcc(0x08000000,&_jumpbuf); }
/***********************************************************************
    Branch SOK
***********************************************************************/
    if(!(rc=fork())) {
       {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   /* kill copy of caller */
//outsok("BranchSOK created\n");
       {OC(Domain_GetKey+tm12);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* BIO key */
       {OC(RENDBR);XB(0x80A03000);RC(rc);RS3(buf,TMMAXREAD,actlen);NB(0x0B100002);cjcc(0x00100000,&_jumpbuf); }
       while(1) {
writeagain:

//outsok("BranchSOK called\n");
          if(actlen) {   /* don't stall if just asking for limit */
             {OC(BIOWRITE+(id << 16)+rc);PS2(buf,actlen);XB(0x04800000);RC(oc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
             if(oc == BIOREPEAT) goto writeagain;
          }

          {OC(TMMAXREAD);XB(0x00200000);RC(rc);RS3(buf,TMMAXREAD,actlen);NB(0x0B100002);cjcc(0x00100000,&_jumpbuf); }
          if(rc == KT+1) break;
       }
       {OC(BIODIED+(id << 16));XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       exit(0);
    }
    if(rc > 1) return 1;

    {OC(RENDTM);XB(0x00A00000);NB(0x0090C00D);cjcc(0x00000000,&_jumpbuf); }

    slot = TMBASE+id*TMSLOTS+TMSLOTSOK;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470D000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
    slot = TMBASE+id*TMSLOTS+TMSLOTSOKDOM;
    {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470C000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    if(makecck) {  /* need new CCK object (not TAP operation) */
       ndb.Byte = TMBIODB;
       {OC(Domain_MakeStart);PS2(&(ndb),sizeof(ndb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
       slot = TMRENDEZVOUS;
       {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x0080A000);cjcc(0x08000000,&_jumpbuf); }
/***********************************************************************
    Branch CCK
***********************************************************************/
       if(!(rc=fork())) {
          {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* kill copy of caller */
          {OC(Domain_GetKey+tm12);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* BIO key */

          {OC(RENDBR);XB(0x80A03000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* TM will end up with resume key */

          {OC(0);XB(0x00000000); }
          while(1) {
             {RC(oc);RS3(buf,TMMAXREAD,actlen);NB(0x0B100002); }
             {rj(0x00100000,&_jumpbuf); }
cckagain:
             {OC(BIOCCK+(id << 16)+oc);PS2(buf,actlen);XB(0x04800000);RC(rc);RS3(buf,TMMAXREAD,actlen);NB(0x0BE0CDE0);cjcc(0x08100000,&_jumpbuf); }

             if(rc == BIOREPEAT) goto cckagain;

             {OC(rc);PS2(buf,actlen);XB(0xE420CDE0); }
          }
       }
       if(rc > 1) return 1;

       {OC(RENDTM);XB(0x00A00000);NB(0x0090C00A);cjcc(0x00000000,&_jumpbuf); }
       {OC(Domain_MakeStart);XB(0x00C00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }   /* Branch CCK */
       {OC(0);XB(0x00A00000); }   /* return to CCK */
       {fj(0x00000000,&_jumpbuf); }

       slot = TMBASE+id*TMSLOTS+TMSLOTCCK;
       {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470D000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
       slot = TMBASE+id*TMSLOTS+TMSLOTCCKDOM;
       {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x8470C000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

    }

    tmbranches[id].id=id;
    tmbranches[id].echo=2;  /* assume echo at start */
    tmbranches[id].ininptr = tmbranches[id].inoutptr = tmbranches[id].inbuf;
    tmbranches[id].outinptr = tmbranches[id].outoutptr = tmbranches[id].outbuf;
    if(makecck) {  /* if not on then tapping and must leave current alone */
       tmbranches[id].activate = 0;  /* not met yet */
    }
    tmbranches[id].readlength = TMMAXREAD;   /* current max max */
    tmbranches[id].havesik=0;
    tmbranches[id].havesok=0;
    tmbranches[id].activein=0;
    tmbranches[id].activeout=0;

    tmmcntl->map[id]=1;
    return 0;
}
/***************************************************************************
   UTILITIES
***************************************************************************/

   KEY key13 = 13;
   KEY key14 = 14;
   KEY key15 = 15;

/***********************************************************************************/
/*    SAVE keys 13, 14, 15  in the memory node                                     */
/*                                                                                 */
/*                 USES the key passed as scatch                                   */
/***********************************************************************************/
save131415(temp)
    KEY temp;
{
    JUMPBUF;

    {OC(Domain_GetMemory);XB(0x00300000);KRN(_0,_0,_0,temp);NB(0x00800000);cj(0x00020000,&_jumpbuf); }
    {IK(temp);OC(Node_Swap+SAVE13);KP(_0,_0,_0,key13);XB(0x80000000);NB(0x00000000);cj(0x02000000,&_jumpbuf); }
    {IK(temp);OC(Node_Swap+SAVE14);KP(_0,_0,_0,key14);XB(0x80000000);NB(0x00000000);cj(0x02000000,&_jumpbuf); }
    {IK(temp);OC(Node_Swap+SAVE15);KP(_0,_0,_0,key15);XB(0x80000000);NB(0x00000000);cj(0x02000000,&_jumpbuf); }
}
/*********************************************************************************/
/*   RESTORE keys 13, 14, 15 from the memory node                                */
/*                                                                               */
/*                USES no other keys                                             */
/*********************************************************************************/
restore131415()
{
    JUMPBUF;

    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+SAVE13);XB(0x00F00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+SAVE14);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+SAVE15);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
}

outsok(str)
    char *str;
{
    JUMPBUF;
    UINT32 oc,rc;
    int len;
    int slot;

    len=strlen(str);

    {OC(compconscck);XB(0x00000000);NB(0x00801000);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);XB(0x00100000);RC(rc);NB(0x08400100);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);PS2(str,strlen(str));XB(0x04100000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

    slot = TMSB;
    {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04700000);NB(0x00801000);cjcc(0x08000000,&_jumpbuf); }

    return 0;
}
