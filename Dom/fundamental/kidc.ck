/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define WOMB 1
/**************************************************************
  This code supports KIDC and KID
 
  An attempt has been made to make the code compatible with
  a factory product AND a WOMB object.  The define symbol WOMB
  will generate the WOMB version if defined.  Else the factory
  version will be generated for testing.
 
  Unlike the Assembly version of KID only one domain is
  used for both KIDC and KID
 
*************************************************************/
#include "keykos.h"
#include <string.h>
#include "wombdefs.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "kid.h"
#include "discrim.h"
#include "dcc.h"
#include "ocrc.h"
#include "sbt.h"
 
   KEY WOMBFACIL = 0;
   KEY WOMBMEM   = 1;
 
   KEY COMP        = 0;   /* the equivalent of wombfacil for testing */
   KEY SB          = 1;   /* Space bank parameter */
   KEY CALLER      = 2;    /* must be here for FORK */
   KEY DOMKEY      = 3;    /*          .            */
   KEY PSB         = 4;    /*          .            */
   KEY METER       = 5;    /*          .            */
   KEY DC          = 6;    /* must be here for FORK */
   KEY HISNODE     = 7;
   KEY RETURNER    = 8;
   KEY DISCRIM     = 9;
   KEY SBT         = 10;
   KEY NODE        = 11;
   KEY KEYBITS     = 12;
   KEY RCF         = 13;
   KEY K0          = 14;
   KEY K1          = 15;
 
#define COMPSBT     0
#define COMPDCC     1
#define COMPRETR    2
#define COMPDISCRIM 3
#define COMPTDOCODE 4
 
#define NODEPSB     0
#define NODESB      1
#define NODERC      2
#define NODEKEYS    2
#define NODEDATA    3
#define NODEKEEPER  12
#define NODEMAP     13
#define NODEVERSION 14
 
#define FB          6
#define MB          80 
 
    char title[]="KID     ";
 
 union keymap {
   Node_KeyData k;
   UCHAR map[16];
 };
 
 union version {
   Node_KeyData k;
   struct {
      UCHAR unused[9];
      UCHAR type;
      UINT16 count;
      UINT32 version;
   } vkey;
 };
 
 union id {
   Node_KeyData k;
   struct {
      UCHAR unused[12];
      UINT32 id;
   } data;
 };
 
 struct kbits {
  UINT32 version;
  UCHAR bytes[32];
 };
 
    UINT32 makekid(UINT32),openkid(void),included(UINT32,union version);
    UINT32 add(UINT32,union version),identify(UINT32 *,union version);
    UINT32 delete(union version),test(union version);
    UINT32 makeunion(union version);
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;   /* this is true for either version */
{
   JUMPBUF;

   UINT32 oc,rc,type;
   UINT32 parm,oparm,nkids;
   SINT16 db;
 static struct Domain_DataByte db1={1};
   union version v;
   int i;
 
#ifdef WOMB                 /* get stuff from WOMBFACIL */
/*******************************************************************
*   Key 8  (KEYBITS from WOMB)
*   Key 9  (TDO code from WOMB) probably not, but....
*******************************************************************/
   KC (DOMKEY,Domain_GetKey+8) KEYSTO(KEYBITS);         /* key 8 */
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_RES) KEYSTO(K1);
   KC (K1,Node_Fetch+WOMB_FACIL_RES_SB) KEYSTO(SB);
   KC (SB,SB_CreateNode) KEYSTO(NODE);  /* temp COMP node */
   KC (NODE,Node_Swap+COMPTDOCODE) KEYSFROM(DISCRIM);   /* key 9 */
 
   KC (K1,Node_Fetch+WOMB_FACIL_RES_METER) KEYSTO(METER);
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_FUND) KEYSTO(K1);
   KC (K1,Node_Fetch+WOMB_FACIL_FUND_SBT) KEYSTO(K0);
   KC (NODE,Node_Swap+COMPSBT) KEYSFROM(K0);
   KC (K1,Node_Fetch+WOMB_FACIL_FUND_DISCRIM) KEYSTO(K0);
   KC (NODE,Node_Swap+COMPDISCRIM) KEYSFROM(K0);
   KC (K1,Node_Fetch+WOMB_FACIL_FUND_RETR) KEYSTO(K0);
   KC (NODE,Node_Swap+COMPRETR) KEYSFROM(K0);
   KC (WOMBFACIL,Node_Fetch+WOMB_FACIL_SUPP) KEYSTO(K1);
   KC (DOMKEY,Domain_GetKey+SB) KEYSTO(PSB);
   KC (DOMKEY,Domain_GetKey+NODE) KEYSTO(COMP); /* like test */
#endif
 /* common prolog */
   KC (COMP,Node_Fetch+COMPSBT)     KEYSTO(SBT);
   KC (COMP,Node_Fetch+COMPDISCRIM) KEYSTO(DISCRIM);
   KC (COMP,Node_Fetch+COMPRETR)    KEYSTO(RETURNER);
 
   nkids=0;
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   KC (DOMKEY,Domain_MakeStart) STRUCTFROM(db1) KEYSTO(K1);
   type=0;  /* no rcf yet */
   LDEXBL(RETURNER,1) KEYSFROM(K0,K1,,CALLER);
   for (;;) {   /* KIDC + KID LOOP */
     parm=0;
     LDENBL OCTO(oc) STRUCTTO(parm) KEYSTO(PSB,,SB,CALLER) DBTO(db);
     RETJUMP();
 
     switch(db) {
       case 0:  /* KIDC */
          if(oc==KT) {
             LDEXBL (RETURNER,KIDC_AKT) KEYSFROM(,,,CALLER)
               STRUCTFROM(nkids);
             continue;
          }
          KC (SBT,SBT_Verify) KEYSFROM(PSB) RCTO(rc);
          if (rc & 0x80) {
             LDEXBL (RETURNER,NONPROMPTSB_RC) KEYSFROM(,,,CALLER);
             continue;
          }
          if(parm > 300) {  /* max holes */
            KC (SBT,SBT_Verify) KEYSFROM(SB) RCTO(rc);
            if (rc & 0x80000000) {
               LDEXBL (RETURNER,NONPROMPTSB_RC) KEYSFROM(,,,CALLER);
               continue;
            }
          }
          else KC (DOMKEY,Domain_GetKey+PSB) KEYSTO(SB);
          rc=makekid(type);
          nkids++;
          LDEXBL (RETURNER,rc) KEYSFROM(NODE,,,CALLER);
          continue;
       case 1:  /* KIDC take RCF */
          LDEXBL (RETURNER,KID_InternalError) KEYSFROM(,,,CALLER);
          continue;
       case 2: ; /* KID  */
          KC (DOMKEY,Domain_GetKey+SB) KEYSTO(NODE);
          KC (NODE,Node_Fetch+NODEVERSION) KEYSTO(K0);
          KC (K0,1) STRUCTTO(v.k) RCTO(rc);
          if(oc==KT) {
             LDEXBL (RETURNER,KID_AKT) KEYSFROM(,,,CALLER);
             continue;
          }
          if(oc==DESTROY_OC) {
             nkids--;
             if(v.vkey.type == 0) {
               KC (NODE,Node_Fetch+NODEPSB) KEYSTO(PSB) RCTO(rc);
               KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(K0) RCTO(rc);
               for (i=0;i<5;i++) {   /* kill the database */
                    KC (K0,Node_Fetch+i) KEYSTO(K1);
                    KC (PSB,SB_DestroyNode) KEYSFROM(K1) RCTO(rc);
               }
               KC (PSB,SB_DestroyNode) KEYSFROM(K0) RCTO(rc);
               KC (NODE,Node_Fetch+NODEDATA) KEYSTO(K0) RCTO(rc);
               for (i=0;i<5;i++) {   /* kill the database */
                    KC (K0,Node_Fetch+i) KEYSTO(K1);
                    KC (PSB,SB_DestroyNode) KEYSFROM(K1) RCTO(rc);
               }
               KC (PSB,SB_DestroyNode) KEYSFROM(K0) RCTO(rc);
               KC (PSB,SB_DestroyNode) KEYSFROM(NODE) RCTO(rc);
             }
             else  {
               LDEXBL (RETURNER,KID_InternalError) KEYSFROM(,,,CALLER);
               continue;
             }
 
             LDEXBL (RETURNER,OK_RC) KEYSFROM(,,,CALLER);
             continue;
          }
          rc=0;
          switch(oc) {       /* PSB has Key or KID */
                             /* parm has number */
            case KID_AddEntry:
              if(included(parm,v)) break;
              rc=add(parm,v);  /* Add PSB to KID */
              break;
            case KID_Identify:
              rc=identify(&oparm,v);
              if(rc) break;   /* rc = 1  not in KID */
              LDEXBL (RETURNER,OK_RC) STRUCTFROM(oparm) KEYSFROM(,,,CALLER);
              continue;
            case KID_DeleteEntry:
              rc=delete(v);
              break;
            case KID_TestInclusion:
              rc=test(v);
              break;
            case KID_PerformIntersection:
              return KID_InternalError;
            case KID_MakeUnion:
              rc=makeunion(v);
              break;
            default:
              LDEXBL (RETURNER,INVALIDOC_RC) KEYSFROM(,,,CALLER);
              continue;
          } /* ordercode switch */
          LDEXBL (RETURNER,rc) KEYSFROM(,,,CALLER);
          continue;
     } /* databyte switch */
   }  /* KIDC LOOP */
}
/*************************************************************
*   MAKEKID(type)   Makes a new kid of correct type          *
*************************************************************/
UINT32 makekid(type)
   UINT32 type;  /* 0=node 1=rcf */
{
   JUMPBUF;
 
 struct Node_KeyValues nkv = {13,15,
      { {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* byte map */
       {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* version */
       FormatK(0,15,12,0,3)}
 };
 struct Domain_DataByte db2={2};
  union version kv;
  struct kbits kb;
  UINT32 actlen,rc;
  UINT16 i;
 
  KC (KEYBITS,0) STRUCTTO(kb,sizeof(kb),actlen) RCTO(rc);
  kv.vkey.count=actlen-4;
  kv.vkey.version=kb.version;
  kv.vkey.type=0;     /* node type for now */
  for(i=0;i<9;i++) kv.vkey.unused[i]=0;
 
  nkv.Slots[1]=kv.k;
  KC (PSB,SB_CreateNode) KEYSTO(NODE) RCTO(rc);
  if(rc) return NOSPACE_RC;
  KC (NODE,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (DOMKEY,Domain_MakeStart) STRUCTFROM(db2) KEYSTO(K0);
  KC (NODE,Node_Swap+NODEKEEPER) KEYSFROM(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (NODE,Node_Swap+NODEPSB) KEYSFROM(PSB) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (NODE,Node_Swap+NODESB)  KEYSFROM(SB) RCTO(rc);
  if(rc) return KID_InternalError;
  if (type==0) {
    KC (PSB,SB_CreateNode) KEYSTO(K0) RCTO(rc);  /* keys */
    if(rc) return NOSPACE_RC;
    KC (NODE,Node_Swap+NODEKEYS) KEYSFROM(K0) RCTO(rc);
    if(rc) return KID_InternalError;
    KC (PSB,SB_CreateNode) KEYSTO(K0) RCTO(rc);  /* data */
    if(rc) return KID_InternalError;
    KC (NODE,Node_Swap+NODEDATA) KEYSFROM(K0) RCTO(rc);
    if(rc) return KID_InternalError;
  }
  else return KID_InternalError;
 
  KC (NODE,Node_MakeSegmentKey) KEYSTO(NODE) RCTO(rc);
  if(rc) return KID_InternalError;
  return OK_RC;
}
/****************************************************************
*  INCLUDED(parm,v) - if present replace number return 1        *
*                     if not present return 0                   *
****************************************************************/
UINT32 included(parm,v)
  UINT32 parm;   /* his number */
  union version v;
{
  JUMPBUF;
  union keymap map;
  union id id;
  UINT16 i,j;
  UINT32 rc;
  struct Node_KeyValues nkv;
 
  KC (NODE,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return 0;
  KC (K0,1) STRUCTTO(map.k) RCTO(rc);

  for (i=0;i<MB;i++) {  /* last MB bits */
    if(!(i % 16))  { /* node boundary in database */
        KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(K0) RCTO(rc);
        KC (K0,Node_Fetch+(i/16)) KEYSTO(K0);
    }
    if(map.map[FB+(i/8)] & (0x80u >>(i%8)) ) {
      KC (K0,Node_Fetch+(i % 16)) KEYSTO(K1) RCTO(rc);
      if(rc) return 0;
      KC (DISCRIM,Discrim_Compare) KEYSFROM(K1,PSB) RCTO(rc);
      if(!rc) { /* a match */
         KC (NODE,Node_Fetch+NODEDATA) KEYSTO(K0) RCTO(rc);
         KC (K0,Node_Fetch+(i/16)) KEYSTO(K0);
         if(rc) return 0;
         for(j=0;j<12;j++) id.data.unused[j]=0;
         id.data.id=parm;
         nkv.StartSlot=(i %16);
         nkv.EndSlot=(i % 16);
         nkv.Slots[0]=id.k;
         KC (K0,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
         if(rc) return 0;
         return 1;
      }
    }
  }
  return 0;
}
/****************************************************************
* ADD(parm,v) -  add if room           PSB has key              *
*                return 1 2 3 if cant add                       *
****************************************************************/
UINT32 add(parm,v)
  UINT32 parm;   /* his number */
  union version v;
{
  JUMPBUF;
  union keymap map;
  union id id;
  UINT16 i,j;
  UINT32 rc;
  struct Node_KeyValues nkv;
 
  KC (NODE,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (K0,1) STRUCTTO(map.k) RCTO(rc);

  for (i=0;i<MB;i++) {  /* last MB bits */
    if(!(map.map[FB+(i/8)] & (0x80u >>(i%8))) ) {
      KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(K1) RCTO(rc);
      if(rc) return KID_InternalError;

      KC (K1,Node_Fetch+(i/16)) KEYSTO(K0) RCTO(rc);
      if(rc) return KID_InternalError;

      KC (K0,KT) RCTO(rc);
      if (rc != Node_NODEAKT) {
          KC (NODE,Node_Fetch+NODEPSB) KEYSTO(K0);
          KC (K0,SB_CreateNode) KEYSTO(K0) RCTO(rc);
          if(rc) return 1;  /* no space */
          KC (K1,Node_Swap+(i/16)) KEYSFROM(K0);
      }

      KC (K0,Node_Swap+(i % 16)) KEYSFROM(PSB) RCTO(rc);
      if(rc) return KID_InternalError;
      KC (NODE,Node_Fetch+NODEDATA) KEYSTO(K1) RCTO(rc);
      if(rc) return KID_InternalError;

      KC (K1,Node_Fetch+(i/16)) KEYSTO(K0) RCTO(rc);
      if(rc) return KID_InternalError;

      KC (K0,KT) RCTO(rc);
      if (rc != Node_NODEAKT) {
          KC (NODE,Node_Fetch+NODEPSB) KEYSTO(K0);
          KC (K0,SB_CreateNode) KEYSTO(K0) RCTO(rc);
          if(rc) return 1;  /* no space */
          KC (K1,Node_Swap+(i/16)) KEYSFROM(K0);
      }

      for(j=0;j<12;j++) id.data.unused[j]=0;
      id.data.id=parm;
      nkv.StartSlot=(i % 16);
      nkv.EndSlot=(i % 16) ;
      nkv.Slots[0]=id.k;
      KC (K0,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
      if(rc) return KID_InternalError;

      map.map[FB+(i/8)] |= (0x80u >> (i%8));    /* allocate */
      nkv.StartSlot=NODEMAP;
      nkv.EndSlot=NODEMAP;
      nkv.Slots[0]=map.k;
      KC (NODE,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
      if(rc) return KID_InternalError;
      return OK_RC;
    }
  }
  return 1;
}
/*************************************************************
* IDENTIFY(&parm,v)- if present set parm return 0            *
*                    else return 1                           *
*************************************************************/
UINT32 identify(parm,v)
  UINT32 *parm;   /* number */
  union version v;
{
  JUMPBUF;
  union keymap map;
  union id id;
  UINT16 i,j;
  UINT32 rc;
  struct Node_KeyValues nkv;
 
  KC (NODE,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (K0,1) STRUCTTO(map.k) RCTO(rc);

  for (i=0;i<MB;i++) {  /* last MB bits */
    if(!(i % 16))  { /* node boundary in database */
        KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(K0) RCTO(rc);
        KC (K0,Node_Fetch+(i/16)) KEYSTO(K0);
    }

    if(map.map[FB+(i/8)] & (0x80u >>(i%8)) ) {
      KC (K0,Node_Fetch+(i % 16)) KEYSTO(K1) RCTO(rc);
      if(rc) return KID_InternalError;
      KC (DISCRIM,Discrim_Compare) KEYSFROM(K1,PSB) RCTO(rc);
      if(!rc) { /* a match */
         KC (NODE,Node_Fetch+NODEDATA) KEYSTO(K0) RCTO(rc);
         KC (K0,Node_Fetch+(i / 16)) KEYSTO(K0);
         if(rc) return KID_InternalError;
         KC (K0,Node_Fetch+(i % 16)) KEYSTO(K0) RCTO(rc);
         if(rc) return KID_InternalError;
         KC (K0,1) STRUCTTO(id.k) RCTO(rc);
         *parm=id.data.id;
         return OK_RC;
      }
    }
  }
  return 1;
}
/******************************************************************
* DELETE(v)        - remove entry return 0  not in 1              *
******************************************************************/
UINT32 delete(v)
  union version v;
{
  JUMPBUF;
  union keymap map;
  union id id;
  UINT16 i;
  UINT32 rc;
  struct Node_KeyValues nkv;
 
  KC (NODE,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (K0,1) STRUCTTO(map.k) RCTO(rc);

  for (i=0;i<MB;i++) {  /* last 16 bits */
    if(!(i % 16))  { /* node boundary in database */
        KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(K0) RCTO(rc);
        KC (K0,Node_Fetch+(i/16)) KEYSTO(K0);
    }
    if(map.map[FB+(i/8)] & (0x80u >>(i%8)) ) {

      KC (K0,Node_Fetch+(i % 16)) KEYSTO(K1) RCTO(rc);
      if(rc) return KID_InternalError;
      KC (DISCRIM,Discrim_Compare) KEYSFROM(K1,PSB) RCTO(rc);
      if(!rc) { /* a match */
        map.map[FB+(i/8)] &= ~(0x80u >> (i%8));
        nkv.StartSlot=NODEMAP;
        nkv.EndSlot=NODEMAP;
        nkv.Slots[0]=map.k;
        KC (NODE,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
        if(rc) return KID_InternalError;
        return OK_RC;
      }
    }
  }
  return 1;
}
/****************************************************************
* TEST(v)          - if all keys in PSB are in NODE 0           *
*                    some keys in PSB not in NODE  1            *
*                    PSB not a KID 4                            *
****************************************************************/
UINT32 test(v)
   union version v;
{
   JUMPBUF;
   union keymap mapmine,maphis;
 
   UINT16 i,j,match;
   UINT32 rc;
 
  if(!openkid()) return 4;
  KC (NODE,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (K0,1) STRUCTTO(mapmine.k) RCTO(rc);
  KC (PSB,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (K0,1) STRUCTTO(maphis.k) RCTO(rc);
  KC (PSB,Node_Fetch+NODEKEYS) KEYSTO(PSB) RCTO(rc);
  if(rc) return KID_InternalError;
  KC (NODE,Node_Fetch+NODEKEYS) KEYSTO(NODE) RCTO(rc);
  if(rc) return KID_InternalError;
  for(i=0;i<MB;i++) {  /* last MB bits */
    if(maphis.map[FB+(i/8)] & (0x80u >>(i%8)) ) {
      KC (PSB,Node_Fetch+(i/16)) KEYSTO(K0);
      KC (K0,Node_Fetch+(i % 16)) KEYSTO(K0) RCTO(rc);
      if(rc) return KID_InternalError;
      match=0;
      for(j=0;j<MB;j++) {  /* last MB bits */
        if(mapmine.map[FB+(j/8)] & (0x80u >>(j%8)) ) {
          KC(NODE,Node_Fetch+(j/16)) KEYSTO(K1) RCTO(rc);
          KC(K1,Node_Fetch+(j%16)) KEYSTO(K1) RCTO(rc);
          if(rc) return KID_InternalError;
          KC(DISCRIM,Discrim_Compare) KEYSFROM(K0,K1) RCTO(rc);
          if(!rc) {match=1;break;}
        }
      }
      if(!match) return KID_Error;  /* at least 1 key in his not in mine */
    }
  }
   return 0;
}
/******************************************************************
* MAKEUNION(v)     - if all keys in PSB added return 0            *
*                    no keys in PSB needed to be added   return 1 *
*                    PSB not a KID 4                              *
******************************************************************/
UINT32 makeunion(v)
   union version v;
{
   JUMPBUF;
 
   union keymap maphis;
   UINT16 i;
   UINT32 rc,parm;
   union id id;
 
   if(!openkid()) return KID_InternalError;
   KC (DOMKEY,Domain_GetKey+PSB) KEYSTO(SB);  /* copy his NODE */
 
   KC (SB,Node_Fetch+NODEMAP) KEYSTO(K0) RCTO(rc);
   if(rc) return KID_InternalError;
   KC (K0,1) STRUCTTO(maphis.k) RCTO(rc);
   for(i=0;i<MB;i++) {  /* last MB bits */
     if(maphis.map[FB+(i/8)] & (0x80u >>(i%8)) ) {
       KC (SB,Node_Fetch+NODEKEYS) KEYSTO(K0) RCTO(rc);
       if(rc) return KID_InternalError;
       KC (K0,Node_Fetch+(i/16)) KEYSTO(PSB) RCTO(rc);  /* get his key */
       KC (PSB,Node_Fetch+(i%16)) KEYSTO(PSB) RCTO(rc); /* his key */ 
       if(rc) return KID_InternalError;
       if(identify(&parm,v)) {  /* not in my kid */
         KC (SB,Node_Fetch+NODEDATA) KEYSTO(K0) RCTO(rc);
         if(rc) return KID_InternalError;
         KC (K0,Node_Fetch+(i/16)) KEYSTO(K0) RCTO(rc); /* his data */
         KC (K0,Node_Fetch+(i%16)) KEYSTO(K0) RCTO(rc);
         if(rc) return KID_InternalError;
         KC (K0,1) STRUCTTO(id.k) RCTO(rc);
         if (rc=add(id.data.id,v)) return rc;
       }
     }
   }
   return OK_RC;
}
 
/*****************************************************************
*  openkid()       - replaces PSB with NODE from KID2            *
*                  - returns 1 if did it else return 0           *
*****************************************************************/
UINT32 openkid()
{
  JUMPBUF;
  UINT32 rc;
 
  KC (DC,DC_IdentifySegment) KEYSFROM(PSB) KEYSTO(,PSB) RCTO(rc);
  if (rc & 0x80000000) return 0;  /* not work */
  return 1;
}
