/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports VCSF and VCS
 
*************************************************************/
#include "keykos.h"
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "vcs.h"
#include "datacopy.h"
#include "page.h"
#include "factory.h"
#include "consdefs.h"
#include "discrim.h"


   KEY COMP        = 0;
#define COMPSEG       0
#define COMPZEROSEG   1
#define COMPDISCRIM   2
#define COMPCOPY      3

#define COMPCONSOLE   15
   KEY SB          = 1;    /* Space bank parameter */
   KEY CALLER      = 2;
   KEY DOMKEY      = 3;
   KEY PSB         = 4;
   KEY METER       = 5;
   KEY DC          = 6;
   KEY NODE        = 7;   /* root segment node */

   KEY FACTORYB    = 8;   /* factory if frozen */
 
   KEY UPSLOT      = 9;
   KEY MEMNODE     = 10;
  
   KEY CONSOLE     = 11;

   KEY ZEROSEG     = 12;
 
   KEY K2          = 13;
   KEY K0          = 14;
   KEY K1          = 15;

#define NODEKEEPER  14
#define NODEFORMAT  15

#define NODEFAULT    KT+6
#define STORAGEFAULT KT+7
#define STOREFAULT KT+8
#define PARENTDEAD KT+9
 
    char title[]="VCSF    ";

    char *page=(char *)0x200000;  /* address of window */
                                  /* the segment key is in slot 3 */
 
    void crash();
    UINT32 dofault(unsigned long long);
    UINT16 unpack(unsigned long long,UCHAR *);

    int getlss();
    int testpage();
    int zappage(int);
    void makeinvalid();
    void makevalid();
    void getzerostem(int);
    unsigned long long treewalk(int,int,int,unsigned long long,
                                            unsigned long long,int);
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc,type;
   unsigned long long parm,oparm,tll;
   SINT16 db;
   char ddb;
   unsigned long backkey;
   struct Node_DataByteValue ndb;
   int i,lss;
   struct Node_KeyValues nkv;
   int frozen;
   static struct Node_KeyValues nkvformat={NODEFORMAT,NODEFORMAT,
        {FormatK(0,15,NODEKEEPER,NODEKEEPER,3)}
   };
   int actlen;

   frozen=0;

   KC (COMP,COMPCONSOLE) KEYSTO(CONSOLE);
   KC (CONSOLE,0) KEYSTO(,CONSOLE) RCTO(rc); 

/* must start by copying moste of the node which is our component */

   KC (DOMKEY,Domain_GetMemory) KEYSTO(MEMNODE);
   KC (SB,SB_CreateNode) KEYSTO(NODE) RCTO(rc);
   if(rc) {
       exit(NODEFAULT);  /* Node Space unavailable */
   }

   KC (COMP,COMPZEROSEG) KEYSTO(ZEROSEG);
   KC (COMP,COMPSEG) KEYSTO(K0); 
   KC (K0,KT) RCTO(rc);   /* see if this is "from the box" Nothing to copy */
   if(rc == KT+1) {       /* yes build lss=3 node */

#ifdef debugging
       KC (ZEROSEG,KT) RCTO(rc);  /* see if we have a zero seg */
       if(rc == KT+1) {  /* must make the ZERO SEG */ 
           KC (SB,SB_CreatePage) KEYSTO(ZEROSEG);
           KC (ZEROSEG,Page_MakeReadOnlyKey) KEYSTO(ZEROSEG);
           for(lss=3;lss<12;lss++) {
              KC (SB,SB_CreateNode) KEYSTO(K0);
              ndb.Byte=lss;
              KC (K0,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K0);
              for(i=0;i<16;i++) {
                 KC (K0,Node_Swap+i) KEYSFROM(ZEROSEG);
              }
              KC (K0,Node_MakeSenseKey) KEYSTO(ZEROSEG);
           }
       }
#endif

       getzerostem(3);  /* this should be page keys */

       for(i=0;i<NODEKEEPER;i++) {
          KC (NODE,Node_Swap+i) KEYSFROM(K0);
       }
      
       KC (NODE,Node_WriteData) STRUCTFROM(nkvformat);  /* lss = 3 red segment key */
   }  /* if we have a Segment we also have a ZeroSegment */
   else {                 /* Copy the node and replace the keeper */
       for(i=0;i<16;i++) {  /* copy the node */
           KC (K0,Node_Fetch+i) KEYSTO(K1);
           KC (NODE,Node_Swap+i) KEYSFROM(K1);
       }
   } 

   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   KC (NODE,Node_Swap+NODEKEEPER) KEYSFROM(K0);

   KC (NODE,Node_MakeSegmentKey) KEYSTO(K2);

   LDEXBL (CALLER,0) KEYSFROM(K2);
   for (;;) {
     parm=0;   /* structure move */
     LDENBL OCTO(oc) KEYSTO(K2,,NODE,CALLER) CHARTO(&parm,8,actlen) DBTO(db);
     RETJUMP();
     oparm=0;
 
     if (oc == KT) {
          LDEXBL (CALLER,VCS_AKT);
          continue;
     }

     if ( (oc == KT+4)) { /* die die */
        if(frozen) {
            KC (DOMKEY,Domain_SwapKey+MEMNODE) KEYSTO(NODE) KEYSFROM(NODE);   /* get a copy with real keys */
        }
        lss = getlss(); 
        makeinvalid();
        tll = treewalk(-1,lss,NODEKEEPER-1,0,0,1);  /* only call to treewalk for frozen segment */
 
        KC (FACTORYB,KT+4) RCTO(rc);
        if(frozen) {
            KC (SB,SB_DestroyNode) KEYSFROM(NODE) RCTO(rc);  /* sell the copy with real keys */
/* can't really do anything interesting if this fails, we are trying to die anyway */
            KC (DOMKEY,Domain_GetKey+MEMNODE) KEYSTO(NODE);   /* get my copy for exit */
        }
        break;  /* node sold on exit */
     }

     if(oc >= KT) {
         if(frozen) {
            LDEXBL (CALLER,STOREFAULT); /* cannot modify */
            continue;
         }
         parm = parm >> 16;  /* adjust for 6 byte fault address */

         if(0) {
            char buf[256];
            sprintf(buf,"VCSF: %llX\n",parm);
            KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
         }

         rc=dofault(parm);  /* handle fault */

         LDEXBL (CALLER,rc);
         continue;
     }
 /* explicit call */

     switch (oc) {
       case VCS_CreateROSegmentKey:
          ndb.Byte=0x80;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(K0);
          LDEXBL (CALLER,0) KEYSFROM(K0);
          continue;

       case VCS_CreateNCSegmentKey:
          ndb.Byte=0x40;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(K0);
          LDEXBL (CALLER,0) KEYSFROM(K0);
          continue;

       case VCS_CreateRONCSegmentKey:
          ndb.Byte=0xC0;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(K0);
          LDEXBL (CALLER,0) KEYSFROM(K0);
          continue;

       case VCS_ReturnBaseSegmentKey:
          KC (COMP,COMPSEG) KEYSTO(K0);
          LDEXBL (CALLER,0) KEYSFROM(K0);
          continue;

       case VCS_TruncateSegment:
          if(frozen) {
             LDEXBL (CALLER,KT+2);
             continue;
          }
          if(actlen != 8) {
             LDEXBL (CALLER,KT+2);
             continue;
          }

          makeinvalid();
          lss=getlss();
          tll=treewalk(-1,lss,NODEKEEPER-1,0,parm,0);  /* will replace empty portions with zero seg stems */
          makevalid();

          LDEXBL (CALLER,0);
          continue;

        case VCS_ReturnLength:
          if(frozen) {
             LDEXBL (CALLER,KT+2);
             continue;
          }

          makeinvalid();
          lss=getlss();
          tll=treewalk(-1,lss,NODEKEEPER-1,0,-1,0);   /* trim only, sell real nodes and zero pages */
/* what about KT+7 return codes */
          tll += 1;
          makevalid();

          LDEXBL (CALLER,0) CHARFROM(&tll,8);
          continue;

        case VCS_Freeze:
          if(frozen) {                 /* return factory previously made */
             KC (FACTORYB,FactoryB_MakeRequestor) KEYSTO(K0);
             LDEXBL (CALLER,0) KEYSFROM(K0);
             continue;
          }
          KC (COMP,COMPCOPY) KEYSTO(K0);
          KC (K0,0) KEYSFROM(K2,,K2) KEYSTO(FACTORYB) RCTO(rc); /* banks only used for factory */
          if(rc) {
             LDEXBL (CALLER,2);
             continue;
          }

          makeinvalid();

          KC (SB,SB_CreateNode) KEYSTO(MEMNODE) RCTO(rc);  /* won't need this */
          if(rc) {
             LDEXBL (CALLER,STORAGEFAULT);
             continue;
          }
          KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
          KC (MEMNODE,Node_Swap+NODEFORMAT) KEYSFROM(K0);

          for(i=0;i<NODEKEEPER;i++) {  /* make any node or page keys sensory */
             KC (NODE,Node_Fetch+i) KEYSTO(K0);
             KC (MEMNODE,Node_Swap+i) KEYSFROM(K0);  /* make a good copy for KT+4 */
             KC (K0,KT) RCTO(rc);
             if(rc == Node_NODEAKT) {    /* Node key */
                 KC(K0,Node_MakeSenseKey) KEYSTO(K0);
                 KC(NODE,Node_Swap+i) KEYSFROM(K0);
             }
             if(rc == Page_AKT) {  /* page key */
                 KC (K0,Page_MakeReadOnlyKey) KEYSTO(K0);
                 KC (NODE,Node_Swap+i) KEYSFROM(K0);
             }
          }

          makevalid();

          KC (NODE,Node_MakeSenseKey) KEYSTO(K1);    

          KC (FACTORYB,FactoryB_InstallSensory+COMPSEG) KEYSFROM(K1);
          KC (FACTORYB,FactoryB_MakeRequestor) KEYSTO(K2);

          frozen=1;   /* freeze */

          LDEXBL (CALLER,0) KEYSFROM(K2);
          continue;

       default:
          LDEXBL (CALLER,KT+2);
          continue;
     }
  }
  KC (PSB,SB_DestroyNode) KEYSFROM(NODE); /* sell segment node */
}
/*****************************************************************
  DOFAULT(address)  - handle fault at ADDRESS

  INPUT SLOTS:   NODE - root of segment
                 SB   - a space bank
  CHANGED SLOTS: K0,K1
*****************************************************************/
UINT32 dofault(unsigned long long address)
{
   JUMPBUF;
   UINT16 lss,i,didexpand,compseglss;
   unsigned long long backkey;
   struct Node_KeyValues nkv,nkv1;
   UINT32 rc;
 
   UCHAR slots[12];
   struct Node_DataByteValue ndb;
   int actlen;

   lss=unpack(address,slots);                    /* get lss needed */
 
/* slots is the unpacked address */
/* lss is what is required to address the fault address */
 
   KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
   KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);

/* nkv.Slots[0] has the format data */
/*******************************************************************************
  segment is expanded filling in all keys from the zero seg so that any subsequent
  fault must require a page to be acquired
*******************************************************************************/
   makeinvalid(); 
   didexpand=0;
   while( (int)lss > (int)(nkv.Slots[0].Byte[15] & 0x0F)) {   /* grow tree */
      KC (SB,SB_CreateNode) KEYSTO(K1) RCTO(rc);
      if(rc) return STORAGEFAULT; 
      for(i=0;i<NODEKEEPER;i++) {                 /* copy first I keys */
         KC (NODE,Node_Swap+i) KEYSTO(K0);       /* data key back */
         KC (K1,Node_Swap+i) KEYSFROM(K0);
      }
      ndb.Byte=nkv.Slots[0].Byte[15] & 0x0F; /* lss of new node */

/* must fill in the rest of the keys with zero keys of correct size */
  
      getzerostem(ndb.Byte);
    
      for(i=NODEKEEPER;i<16;i++) {
          KC (K1,Node_Swap+i) KEYSFROM(K0); 
      }

      KC (K1,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K1);
      KC (NODE,Node_Swap+0) KEYSFROM(K1);

      nkv.Slots[0].Byte[15]++;              /* bump lss of root */

      getzerostem(nkv.Slots[0].Byte[15] & 0x0F);

      for(i=1;i<NODEKEEPER;i++) {
          KC (NODE,Node_Swap+i) KEYSFROM(K0);
      }
    
      nkv.StartSlot=NODEFORMAT;
      nkv.EndSlot=NODEFORMAT;
      KC (NODE,Node_WriteData) STRUCTFROM(nkv);        /* update node */
      KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
      KC (NODE,Node_Swap+NODEKEEPER) KEYSFROM(K0);
      didexpand=1;
   }
   makevalid();
   if(didexpand) return 0;  /* could be a read that will now work */
 
/* at this point the tree is deep enough to support the new page */
/* now find hole and fill it by walking down the tree to the new */
/* page.  Nodes may have to be obtained to fill in the path to the */
/* to the page.  A queued fault must be recognized */

/* the tree always remains valid */

/* there will be sense keys in this structure */
 
/* lss is what is needed for the address.  We must start at the */
/* top of the tree */
 
   lss=nkv.Slots[0].Byte[15] & 0x0F;   /* depth of tree */
 
   while(lss > 2) {                        /* till lss=3 (done) */
     KC (NODE,Node_Fetch+slots[11-lss]) KEYSTO(K1);
     KC (K1,KT) RCTO(rc);                  /* what is here */
     if((rc == Page_ROAKT) || (rc == Node_SENSEAKT) || 
                              ((rc & Node_SEGMENTMASK) == Node_SEGMENTAKT)) {   /* must replace */
        if(lss == 3) {  /* the leaf page, must have been ro page (as fetched through sense key) */
            KC (SB,SB_CreatePage) KEYSTO(K2) RCTO(rc);
            if (rc) return STORAGEFAULT;

            KC (MEMNODE,Node_Swap+2) KEYSFROM(K1);
            KC (K2,Page_WriteData) CHARFROM(page,4096) RCTO(rc);  /* if fails, leaves zero */
            KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K2);

            return 0;  /* problem fixed try again to store here */
        } 
        else {                             /* must have been a node sense key, add node */
                                           /* copy the contents of the sensory node to this new one */
          KC (SB,SB_CreateNode) KEYSTO(K2) RCTO(rc);
          if (rc) return NODEFAULT;

          for(i=0;i<16;i++) {
             KC (K1,Node_Fetch+i) KEYSTO(K0);
             KC (K2,Node_Swap+i) KEYSFROM(K0); 
          } 

          ndb.Byte=lss-1;
          KC (K2,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K1);  /* leave in K1 */
          KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1);  /* just like we got it from here */
        }
     }
     else {  /* found something, best be node or page */
 
/* if two domains fault on the same page, the second will queue  */
/* while the keeper fills in the page.  When the queued fault is */
/* processed, it will find the page already present.             */
 
        if(rc == 0x0202) return 0; /* a page, filled in before */
        if(rc != 0x03) crash("Stange key in Node");
      }
      KC (DOMKEY,Domain_GetKey+K1) KEYSTO(NODE); /* move to new node */
      lss--;
   }
   return STOREFAULT;
}
/********************************************************************
  UNPACK(address,slots)  unpack address into slot array
                         return lss of node needed to address
********************************************************************/
UINT16 unpack(unsigned long long address,UCHAR *slots)
{
   UINT16 lss;
   int i;
 
   for(i=11;i>=0;i--) {
      slots[i]=address & 0x0F;
      address = address >> 4;
   }
   for(lss=11;lss>2;lss--) {   /* find left most address byte */
      if(slots[11-lss]) break;
   }
   if(lss==2) lss=3;           /* for zero loop goes too far */
   if(slots[11-lss] >= NODEKEEPER) lss++;  /* adjust for short node */
                               /* only NODEKEEPER slots used in first node */
                               /* so must use larger node if need */
                               /* to address above that */
   return lss;
}
/************************************************************************/
/*  highestaddr=treewalk(slot,lss,startslot,addr,truncaddr,zero)        */
/*     Node in NODE                                                     */
/*     UPSLOT contains parent of NODE                                   */
/*     UPSLOT[slot] contains the parent of UPSLOT                       */
/*           slot is -1 at top of tree                                  */
/*     lss is the lss of NODE                                           */
/*                                                                      */
/*     At each descent NODE[slot] is moved to NODE                      */
/*        and NODE[slot] filled with UPSLOT                             */
/*        NODE is moved to UPSLOT                                       */
/*                                                                      */
/*     At each ascent  UPSLOT[slot] is moved to UPSLOT                  */
/*        and UPSLOT[slot] is filled with NODE                          */
/*        UPSLOT is moved to NODE                                       */
/*                                                                      */
/*     startslot is the highest slot in the node to look at             */
/*     addr is the cumulative page number                               */
/*     truncaddr is the truncation address                              */
/*     zero is a flag for KT+4, don't descend non-node slots            */
/*     return is the  highest non zero address                          */
/*                                                                      */
/* assume an lss=6 top level red node with 12 initial slots             */
/*                                                                      */
/*  trim and return length                                              */
/*                 high = treewalk(-1,6,12,0,0xFFFFFFFFFFFFFFFF)        */
/*  empty                                                               */
/*                 treewalk(-1,6,12,0,0)                                */
/*                                                                      */
/*  INPUT SLOTS:  NODE - starts at root                                 */
/*                UPSLOT - starts arbitrarily at root                   */
/*                ZEROSEG - the zero segment root                       */
/*                MEMNODE - root of keeper memory                       */
/*                DOMKEY  - domain key of keeper                        */
/*  CHANGED SLOTS:  NODE/UPSLOT - walks the tree                        */
/*                  K0,K1,K2                                            */
/************************************************************************/
unsigned long long treewalk(int slot,int lss,int startslot,
  unsigned long long addr,unsigned long long truncaddr,int zero)
{
     JUMPBUF;
     UINT32 rc,bytes,pagerc;
     int i,j;
     unsigned long long pageaddr;
     unsigned long long highestaddr = -1;
     struct Node_DataByteValue ndb;
     char debug[128];

/* if this node is above the truncation address and it is sensory, there is nothing of interest */
/* below this node.  Replace this node with the correct piece of the zero segment */

     getzerostem(lss);
     KC (DOMKEY,Domain_GetKey+K0) KEYSTO(K2);

     if((lss > 3) && (highestaddr == -1)) {  /* must continue plunge unless done */
         for(i=startslot;i>=0;i--) {  /* look at each slot */
            KC (NODE,Node_Swap+i)              KEYSFROM(UPSLOT) KEYSTO(K0);

            KC (COMP,COMPDISCRIM) KEYSTO(K1);
            KC (K1,Discrim_Compare) KEYSFROM(K0,K2) RCTO(rc);   /* is this a zero stem */
            if(!rc) {  /* keys are identical */
                KC (NODE,Node_Swap+i) KEYSFROM(K0) KEYSTO(UPSLOT);
                continue; /* put node back and continue on loop */
            }
            KC (K0,KT) RCTO(rc);
            if(rc != Node_NODEAKT) {  /* if it goes deeper, we can't go there until we replace sense key */
                if( (rc != Node_SENSEAKT) && ((rc & Node_SEGMENTMASK) != Node_SEGMENTAKT) ) {
                   crash("Funny key in tree");
                }

                if(zero) continue;   /* don't descend there are no pages to free */

                KC (SB,SB_CreateNode) KEYSTO(K1) RCTO(rc);
                if(rc) return NODEFAULT;
                for(j=0;j<16;j++) {
                   KC (K0,Node_Fetch+j) KEYSTO(K2);
                   KC (K1,Node_Swap+j) KEYSFROM(K2);
                }
                ndb.Byte=lss-1;
                KC (K1,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K0);
            }
            KC (DOMKEY,Domain_SwapKey+NODE) KEYSFROM(K0) KEYSTO(UPSLOT);

            highestaddr=treewalk(i,lss-1,15,((addr << 4) | i),truncaddr,zero);

            KC (DOMKEY,Domain_SwapKey+NODE) KEYSFROM(UPSLOT) KEYSTO(K0);
            KC (NODE,Node_Swap+i)           KEYSFROM(K0) KEYSTO(UPSLOT);

	    if(highestaddr != -1) break;   /* cause early exit */
            else {  /* the node we just visited is empty (lss-1), replace with getzerostem(lss) */
                 KC (K0,KT) RCTO(rc);  /* K0 is the node we just visited */
                 if(rc == Node_NODEAKT) KC (SB,SB_DestroyNode) KEYSFROM(K0) RCTO(rc);
                 if(!zero) {
                    getzerostem(lss);  /* to K0 */
                    KC (NODE,Node_Swap+i) KEYSFROM(K0);
                 }
            }
            getzerostem(lss);
            KC (DOMKEY,Domain_GetKey+K0) KEYSTO(K2);
         }
     }
     else {  /* leaf */
         for(i=startslot;i>=0;i--) {  /* Take action at leaf */
             KC (NODE,Node_Fetch+i) KEYSTO(K1);
             KC (K1,KT) RCTO(pagerc);

             if((pagerc == Page_AKT) || (pagerc == Page_ROAKT) ) {  /* some form of page key */

/************************************************************************************/
/*  what to do.  trim or truncate                                                   */
/*    trim if truncaddr = FFFFFFFFFFFFFFFF  else truncate to truncaddr              */
/************************************************************************************/
		 pageaddr = (((addr << 4) | i) << 12);  /* address of this page */
		 if((pageaddr > (truncaddr & 0xFFFFFFFFFFFFF000)) || !truncaddr) { /* definitely remove page  */
                     if(pagerc == Page_AKT) {
                         KC (SB,SB_DestroyPage) KEYSFROM(K1) RCTO(rc);  /* sell page, don't change highest */
                     }
/* Replace with a zero page */
                     if(!zero) {
                        getzerostem(3);
                        KC (NODE,Node_Swap+i)  KEYSFROM(K0);
                     }
                 }
                 else {  /* below truncaddr, maybe */
	             KC (MEMNODE,Node_Swap+2) KEYSFROM(K1);  /* map page into my memory at 0x00400000 */
		     bytes=testpage();  /* returns number non-zero bytes */

                     if(!bytes) {  /* zero page, remove and don't set hightest addr */
                         if(pagerc == Page_AKT) { 
			    KC (SB,SB_DestroyPage) KEYSFROM(K1) RCTO(rc);
                            getzerostem(3);
                            KC (NODE,Node_Swap+i) KEYSFROM(K0);
                         }
                     }
                     else {
/* only want to come here if (truncaddr & 0xFFF) is non-zero */
		         if((bytes-1) > (truncaddr & 0xFFF)) {  /* zero page above truncaddr, return highest */
                             if(pagerc == Page_AKT) zappage(truncaddr & 0xFFF);
                             else {  /* we must buy a page so that we can copy the first bytes bytes */
                                KC (SB,SB_CreatePage) KEYSTO(K0) RCTO(rc);
                                if(rc) {
                                    return STORAGEFAULT;
                                }
                                KC (K0,Page_WriteData) CHARFROM(page,bytes) RCTO(rc);
                                KC (NODE,Node_Swap+i) KEYSFROM(K0);
                             }
			     return truncaddr;
                         }
			 return pageaddr+bytes-1;  /* this is the highest non-zero below truncaddr */
                     }
		 }
             }  
         }
     }

     return highestaddr;
}

/********************************************************************
  GETLSS()
  
  INPUT SLOTS:  NODE root node of segment
  CHANGED SLOTS:  K0
********************************************************************/
int getlss()
{
    JUMPBUF;
    struct Node_KeyValues nkv;
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    return (nkv.Slots[0].Byte[15] & 0xF);
}
/*******************************************************************
  MAKEINVALID() while tree walking

  INPUT SLOTS:  NODE root node of segment
  CHANGED SLOTS:  K0
*******************************************************************/
void makeinvalid()
{
    JUMPBUF;
    struct Node_KeyValues nkv;
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    nkv.Slots[0].Byte[15] = nkv.Slots[0].Byte[15] & 0x0F;  /* set I to zero */
    nkv.StartSlot=NODEFORMAT;
    nkv.EndSlot=NODEFORMAT;
    KC(NODE,Node_WriteData) STRUCTFROM(nkv);
}
 
/*******************************************************************
  MAKEVALID() after tree walking

  INPUT SLOTS:  NODE root node of segment
  CHANGED SLOTS:  K0
*******************************************************************/
void makevalid()
{
    JUMPBUF;
    struct Node_KeyValues nkv;
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    nkv.Slots[0].Byte[15] = (nkv.Slots[0].Byte[15] & 0x0F) | (NODEKEEPER << 4);
    nkv.StartSlot=NODEFORMAT;
    nkv.EndSlot=NODEFORMAT;
    KC (NODE,Node_WriteData) STRUCTFROM(nkv);
}
/******************************************************************
   TESTPAGE()  - return the number of non-zero bytes
******************************************************************/
testpage()      /* this needs to be a lot faster */
{
   int i;
   for(i=4095;i>=0;i--) if(*(page+i)) break;
   return i+1;
}
/*****************************************************************
   ZAPPAGE(bytes) - zero above bytes
*****************************************************************/
zappage(bytes)
{
   int i;
   for(i=bytes+1;i<4096;i++) *(page+i) =0;
}
/****************************************************************
   GETZEROSTEM(lss) - sets K0 to the correct chunk based on lss

   INPUT SLOTS:  ZEROSEG - root of zero segment
   CHANGEED SLOTS:   K0
****************************************************************/
void getzerostem(int lss)
{
    JUMPBUF;
    UINT32 rc;

#ifdef debugging
    log1var("VCS: GETZEROSTEM lss ",(char *)&lss,4,1);
#endif

    KC (DOMKEY,Domain_GetKey+ZEROSEG) KEYSTO(K0);
    KC (K0,Node_DataByte) RCTO(rc);
    rc &= 0x0F;  /* probably has ro-nocall seg */
    while(rc > lss) {  /* appropriate for node with databyte lss */
       KC (K0,Node_Fetch+0) KEYSTO(K0);
       KC (K0,Node_DataByte) RCTO(rc);
       rc &= 0x0F;
    }
    KC (K0,Node_Fetch+0) KEYSTO(K0); /* fetch the thing we want */
}
