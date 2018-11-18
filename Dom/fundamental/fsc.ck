/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define usebanks 1
/**************************************************************
  This code supports FSF and the Fresh segment keeper
 
*************************************************************/
#include "keykos.h"
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "fs.h"
#include "lli.h"
#include "datacopy.h"
#include "page.h"
#include "ocrc.h"


   KEY COMP        = 0;
#define COMPDATACOPY   2
#define COMPKEYBITS    3
#define COMPCONSOLE   15
   KEY SB          = 1;    /* Space bank parameter */
   KEY CALLER      = 2;
   KEY DOMKEY      = 3;
   KEY PSB         = 4;
   KEY METER       = 5;
   KEY DC          = 6;
   KEY NODE        = 7;   /* root segment node */
 
   KEY TESTSLOT    = 8;
   KEY UPSLOT      = 9;
   KEY MEMNODE     = 10;
  
   KEY CONSOLE     = 11;
   KEY COPY        = 12;
 
   KEY K2          = 13;
   KEY K0          = 14;
   KEY K1          = 15;

/***************************************************************************
   BEWARE - NODESB must be a multiple of 3 because pages are bought
   in units of 3 when populating an FSC.  NOTE this doesn't work
   when the pages are faulted randomly..... WOW  ditch this

   Buy of 3 pages is disabled.  NODESB can be any value
***************************************************************************/
 
#define NODESB      9    /* spacebank for node, lowest slot used */
/* space for CopyOnWrite stuff */
#define NODEPARENT  10
#define NODECOPYPARMS 11
#define NODEMETA    12   /* meta data for Unix */
#define NODEMASTERSB 13
#define NODEKEEPER  14
#define NODEFORMAT  15
 
#define MEMNODESLAVE 3
#define MEMNODESAVE  4
 
    char title[]="FS      ";
    char *page=(char *)0x200000;
 
    void crash();
    UINT32 dofault(LLI,unsigned long,unsigned long);
    UINT32 makeseg(int,LLI);
    UINT16 unpack(LLI,UCHAR *);
    UINT32 docopy(LLI);
    void fixlss(LLI),pack(UCHAR *,LLI *);
    int testpage();
    int getlss();
    void makeinvalid();
    void makevalid();

    unsigned long long treewalk(int,int,int,unsigned long long,
                                            unsigned long long);
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc,type;
   char inparm[12];
   LLI parm;
   SINT16 db;
   char ddb;
   int sibcount;
   unsigned long long tll;
   LLI tlli;
 
   static LLI dz = {0,0};
   static LLI dmax = {0xFFFFFFFF,0xFFFFFFFF};
   struct Node_DataByteValue ndb;
   LLI oparm;
   UINT32 copylow,copyhigh;
   int i,lss;
   char buf[64];
   struct Node_KeyValues nkv;
   int actlen;

#ifdef debugging
   KC (COMP,15) KEYSTO(CONSOLE);
   KC (CONSOLE,0) KEYSTO(,CONSOLE);
#endif

   sibcount=0;
   KC (DOMKEY,Domain_GetMemory) KEYSTO(MEMNODE);
   rc=makeseg(0,dz);
   if(!rc) sibcount++;
   LDEXBL (CALLER,rc) KEYSFROM(NODE);
   for (;;) {
     parm=dz;   /* structure move */
     LDENBL OCTO(oc) KEYSTO(SB,,NODE,CALLER) STRUCTTO(inparm) DBTO(db);
     RETJUMP();
     oparm=dz;
     memcpy(&parm,inparm,8);
 
     if (oc == KT) {LDEXBL (CALLER,FS_AKT);continue;}
     if ( (oc == DESTROY_OC)) { /* die die */
#ifdef debugging
  KC(CONSOLE,0) CHARFROM("FSC called with KT+4\n",21) RCTO(rc);
#endif

#ifdef usebanks
        KC (NODE,Node_Fetch+NODESB) KEYSTO(K1);
        KC (K1,SB_DestroyBankAndSpace) RCTO(rc);
#else
        lss = getlss(); 
        makeinvalid();
        KC (NODE,Node_Fetch+NODESB) KEYSTO(SB);
        tll = treewalk(-1,lss,NODESB-1,0,0);  /* leaves slot 0 nodes in place */
        makevalid();
#endif

        if(!sibcount) break;  /* last one or no siblings */

        sibcount--;
        if(!sibcount) break;  /* last sibling died */

/* get rid of sibling node */

        KC(PSB,SB_DestroyNode) KEYSFROM(NODE) RCTO(rc);
        LDEXBL (CALLER,OK_RC);
        continue;
     }
     if(oc >= KT) {
#ifdef debugging
         hexcvt(&parm,buf,8);
         strcat(buf," FSC ");
         KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
#endif
         llilsr(&parm,16);  /* turn into 8 byte number */

         KC (MEMNODE,Node_Swap+MEMNODESAVE) KEYSFROM(NODE);

         KC (NODE,Node_Fetch+NODECOPYPARMS) KEYSTO(K0);
         KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
         memcpy(&oparm,&nkv.Slots[0].Byte[8],8);  /* get these cause we may need */

         rc=dofault(parm,oparm.hi,oparm.low);  /* handle fault */
         if(rc == -1) {  /* retail copyme. used RO page */
              LDEXBL(CALLER,OK_RC);
              continue;
         }
#ifdef datacopy
         if(!rc) {  /* must check for copy function */

           KC (MEMNODE,Node_Fetch+MEMNODESAVE) KEYSTO(NODE);  /* recover the damn node */

           if(oparm.low) {  /* length of copy (32 bit limit )*/
              parm.low=parm.low & 0xFFFFF000;   /* address on page boundary */

              oparm.low = oparm.hi + oparm.low;  /* next not copied */
                                                 /* convert length to range */

              copylow=parm.low;  /* start with adjusted fault address */
              if(oparm.hi > copylow) copylow=oparm.hi;  /* lower than range */

              copyhigh=parm.low+0x1000;
              if(oparm.low < copyhigh) copyhigh=oparm.low; /* above range */

              if(copyhigh > copylow) { /* must do copy */
                 oparm.hi=copylow;  /* start */
                 oparm.low= copyhigh-copylow;  /* length */
                 KC (MEMNODE,Node_Fetch+MEMNODESLAVE) KEYSTO(UPSLOT);
                 KC (NODE,Node_Fetch+NODEPARENT) KEYSTO(TESTSLOT);
                 ddb=0xC0;  /* ro-nocall */
                 KC (TESTSLOT,Node_MakeSegmentKey) STRUCTFROM(ddb)
                     KEYSTO(TESTSLOT);
                 ddb=0x40;  /* nocall */
                 KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ddb)
                     KEYSTO(K0);  
                 LDEXBL (UPSLOT,0) STRUCTFROM(oparm) 
                    KEYSFROM(TESTSLOT,K0,,CALLER); /* return throug slave */
                 continue;
              }
              else {
                LDEXBL (CALLER,OK_RC);
                continue;
              }
           }
           else {
             LDEXBL (CALLER,OK_RC);
             continue;
           }
         }
         else {
#endif
           LDEXBL (CALLER,rc);
           continue;
#ifdef datacopy
         }
#endif
     }
 /* explicit call */

#ifdef debugging
     hexcvt(&oc,buf,4);
     strcat(buf," oc FSC ");
     KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
     hexcvt(&parm,buf,8);
     strcat(buf,"\n");
     KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
#endif 
     switch (oc) {
       case FS_CreateROSegmentKey:
          ndb.Byte=0x80;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(NODE);
          LDEXBL (CALLER,OK_RC) KEYSFROM(NODE);
          continue;
       case FS_Freeze:
#ifdef usebanks
          KC (NODE,Node_Fetch+NODESB) KEYSTO(SB);
#endif
          ndb.Byte=0xC0;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(NODE);
          LDEXBL (CALLER,OK_RC) KEYSFROM(NODE);
          FORKJUMP();
          goto die;
       case FS_ReturnLength:
          lss=getlss();
          KC (NODE,Node_Fetch+NODESB) KEYSTO(SB);
          makeinvalid();
          tll=treewalk(-1,lss,NODESB-1,0,0xFFFFFFFFFFFFFFFF);
          makevalid();
          tll++; /* length is highest address +1 */
#ifdef debugging
      hexcvt(&tll,buf,8);
      strcat(buf," FSC returnlength \n");
      KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
#endif
          oparm.hi = tll >> 32;
          oparm.low = tll;
          LDEXBL (CALLER,OK_RC) STRUCTFROM(oparm);
          continue;
       case FS_CreateNCSegmentKey:
          ndb.Byte=0x40;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(NODE);
          LDEXBL (CALLER,OK_RC) KEYSFROM(NODE);
          continue;
       case FS_CreateRONCSegmentKey:
          ndb.Byte=0xC0;
          KC (NODE,Node_MakeSegmentKey) STRUCTFROM(ndb) KEYSTO(NODE);
          LDEXBL (CALLER,OK_RC) KEYSFROM(NODE);
          continue;
       case FS_TruncateSegment:
#ifdef usebanks
          if((0 == parm.hi) && (0 == parm.low)) {
             KC (NODE,Node_Fetch+NODEMASTERSB) KEYSTO(SB);
             KC (NODE,Node_Fetch+NODESB) KEYSTO(K1);
             KC (K1,SB_DestroyBankAndSpace) RCTO(rc);
             KC (SB,SB_CreateBank) KEYSTO(K1);
             KC (NODE,Node_Swap+NODESB) KEYSFROM(K1);
             fixlss(dz);  /* set lss to 3, all nodes are zapped */
             LDEXBL (CALLER,OK_RC);
             continue;
          }
#endif
          KC (NODE,Node_Fetch+NODESB) KEYSTO(SB);
          lss = getlss();
          tll = parm.hi;
          tll = (tll << 32) + parm.low;
          makeinvalid();
          tll=treewalk(-1,lss,NODESB-1,0,tll);
          makevalid();
          LDEXBL (CALLER,OK_RC);
          continue;
       case FS_CreateSibling:   /* use bank passed */
          rc=makeseg(1,dz);
          if(!rc) sibcount++;
          LDEXBL (CALLER,rc) KEYSFROM(NODE);
          continue;
       case FS_SetLimit:
          KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
          KC (K0,1) STRUCTTO(nkv.Slots[0]) RCTO(rc);
          memcpy(buf,&parm,8); 
          memcpy(&nkv.Slots[0].Byte[5],buf+1,7);   /* include low byte not high */
          nkv.StartSlot=NODEFORMAT;
          nkv.EndSlot=NODEFORMAT;
          KC (NODE,Node_WriteData) STRUCTFROM(nkv);
          LDEXBL(CALLER,OK_RC);
          continue;
       case FS_GetLimit:
          KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
          KC (K0,1) STRUCTTO(nkv.Slots[0]) RCTO(rc);
          memcpy(buf+1,&nkv.Slots[0].Byte[5],7);
          *buf=0;
          memcpy(&oparm,buf,8);
          LDEXBL(CALLER,OK_RC) STRUCTFROM(oparm);
          continue;
       case FS_SetMetaData:
          KC (NODE,Node_Fetch+NODEMETA) KEYSTO(K0);
          KC (K0,1) STRUCTTO(nkv.Slots[0]) RCTO(rc);
          memcpy(&nkv.Slots[0].Byte[5],inparm+1,11);   /* include low byte not high */
          nkv.StartSlot=NODEMETA;
          nkv.EndSlot=NODEMETA;
          KC (NODE,Node_WriteData) STRUCTFROM(nkv);
          LDEXBL(CALLER,OK_RC);
          continue;
       case FS_GetMetaData:
          KC (NODE,Node_Fetch+NODEMETA) KEYSTO(K0);
          KC (K0,1) STRUCTTO(nkv.Slots[0]) RCTO(rc);
          memcpy(buf+1,&nkv.Slots[0].Byte[5],11);
          *buf=0;
          LDEXBL(CALLER,OK_RC) STRUCTFROM(buf,12);
          continue;
       case FS_CopyMe:    /* use bank passed */
          rc=docopy(parm);
          if(!rc) sibcount++;
/* return through child to caller */
          oparm=parm;
#ifdef xx
          if((oparm.low < 0x6000) || db) {  /* do copy immediately */
            LDEXBL (UPSLOT,0) STRUCTFROM(oparm) KEYSFROM(TESTSLOT,NODE,,CALLER);
            continue;
          }
          else {  /* delay the copy till the fault */
	    LDEXBL (CALLER,0) KEYSFROM(NODE);
            continue;
          }
#endif
            LDEXBL (CALLER,rc) KEYSFROM(NODE);  /* always delay till fault */
            continue;
       
       default:
          LDEXBL (CALLER,INVALIDOC_RC);
          continue;
     }
  }
  KC (PSB,SB_DestroyNode) KEYSFROM(NODE); /* sell segment node */
die:
  KC (MEMNODE,Node_Fetch+MEMNODESLAVE) KEYSTO(UPSLOT);
  KC (UPSLOT,DESTROY_OC) RCTO(rc);  /* destroy copy slave if exists */
}
/*******************************************************************
   DOCOPY(parm)   -  Makes a new sibling segment with some bits copied
*******************************************************************/
UINT32 docopy(parm)
    LLI parm;
{
    JUMPBUF;
    uint32 offset,length,rc,oc,did,error;
    struct Node_DataByteValue db={0xC0};  /* ro-nocall */
    unsigned long long limit,max;
    struct Node_KeyValues nkv;
    char buf[8];

    struct DatacopyArgs args;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0);
    KC (K0,1) STRUCTTO(nkv.Slots[0]) RCTO(rc);

    memcpy(&buf+1,&nkv.Slots[0].Byte[5],7); 
    *buf=0;
    memcpy(&limit,buf,8);

#ifdef xx
    KC (DOMKEY,Domain_GetKey+NODE) KEYSTO(TESTSLOT);  /* need this ..parent */
    KC (TESTSLOT,Node_MakeSegmentKey) STRUCTFROM(db) KEYSTO(TESTSLOT);
#endif
    error=makeseg(1,parm);   /* now have a sibling segment key in NODE */
    if(error) {
         return KT+7;
    } 

#ifdef datacopy

    KC (MEMNODE,Node_Fetch+MEMNODESLAVE) KEYSTO(UPSLOT);
    KC (UPSLOT, KT) RCTO(rc);

    if (KT+1 == rc) {  /* must make a child */
      if(!fork()) {  /* the child to do the copy */
       LDEXBL(COMP,0);  /* become available */
       while(1) {
         LDENBL OCTO(oc) STRUCTTO(parm) KEYSTO(TESTSLOT,NODE,,CALLER);
         RETJUMP();

         if(DESTROY_OC == oc) exit(0);
         if(KT == oc) {
              LDEXBL(CALLER,FS_AKT);
              continue;
         }
/* copy the segment */
         offset=parm.hi;
         offset=offset & 0xFFFFF000;
         length=parm.low;
         length=(length+4095) & 0xFFFFF000;

         max = offset+length-1;
         if(max > limit) {
             length = limit-offset;
             if(length < 0 ) length=0;
         }
 
         KC (COMP,Node_Fetch+COMPDATACOPY) KEYSTO(UPSLOT);

         while(length>0) {
           args.fromoffset=offset;
           args.tooffset=offset;
           args.length=length;
           KC (UPSLOT,0) KEYSFROM(TESTSLOT,NODE) STRUCTFROM(args)
              STRUCTTO(args) RCTO(rc);
           did=length-args.length;
           switch (rc) {
           case 0:
           case 1:
           case 2:
           case 3:
              if(!did) {  /* skip this page */
                   did=4096;
              }
              break;  /* some data was moved.. */
           case 4:
           case 5:
           case 6:
           case 7:
              error=FS_CopyError;
              length=0;
              break;
           }
           if(error) break;
           length -= did;
           offset += did;
         }

       /* after copy */
         LDEXBL (CALLER,error) KEYSFROM(NODE);  /* exit to original caller */
        }  /* while loop */
      }
      KC (K1,Domain_MakeStart) KEYSTO(UPSLOT);
      KC (MEMNODE,Node_Swap+MEMNODESLAVE) KEYSFROM(UPSLOT);
    }  /* end build child */
#endif

    /* must send the child to do the copy in main loop */

    return 0; 
}
/*******************************************************************
   MAKESEG(databyte)   -  Makes a new segment, databyte of keeper
*******************************************************************/
UINT32 makeseg(db,parm)
   int db;
   LLI parm;
{
   JUMPBUF;
   UINT32 rc;
   char buf[64];
 
   struct Domain_DataByte db1;
   struct Node_KeyValues nkv={NODEFORMAT,NODEFORMAT,
        {FormatK(0,15,NODEKEEPER,NODESB,3)}
   };
   unsigned long long maxlimit = 0xFFFFFFFFFFFFFFFFull;
   struct Node_KeyValues nkv1;
   unsigned long copyend;

/* The segment node is from PSB so it lasts as long as the keeper */
 
   KC (DOMKEY,Domain_GetKey+NODE) KEYSTO(K2);  /* parent node */

   KC (PSB,SB_CreateNode) KEYSTO(NODE) RCTO(rc);
   if(rc) return NOSPACE_RC;
#ifdef usebanks
   KC (SB, SB_CreateBank) KEYSTO(K1) RCTO(rc);
   if(rc) return NOSPACE_RC;
   KC (NODE,Node_Swap+NODESB) KEYSFROM(K1) RCTO(rc); /* bank for mem */
   KC (NODE,Node_Swap+NODEMASTERSB) KEYSFROM(SB);  /* user bank */
#else
   KC (NODE,Node_Swap+NODESB) KEYSFROM(SB);
#endif
   db1.Databyte=db;
   KC (DOMKEY,Domain_MakeStart) STRUCTFROM(db1) KEYSTO(K0);
   KC (NODE,Node_Swap+NODEKEEPER) KEYSFROM(K0) RCTO(rc);
   if(rc) return FS_InternalError;
   memcpy(&nkv.Slots[0].Byte[5],&maxlimit,7);
   KC (NODE,Node_WriteData) STRUCTFROM(nkv) RCTO(rc);
   if(rc) return FS_InternalError;

   copyend=parm.hi+parm.low;
   copyend=copyend+0xfff;
   copyend=copyend & 0xFFFFF000;
   parm.hi = parm.hi & 0xFFFFF000;
   parm.low = copyend-parm.hi;
    
   KC (NODE,Node_Swap+NODEPARENT) KEYSFROM(K2);   /* put in parent */
   nkv1.StartSlot=NODECOPYPARMS;
   nkv1.EndSlot=NODECOPYPARMS;
   for(rc=0;rc<16;rc++) nkv1.Slots[0].Byte[rc]=0;
   memcpy(&nkv1.Slots[0].Byte[8],&parm,8);
   KC (NODE,Node_WriteData) STRUCTFROM(nkv1);

   KC (NODE,Node_MakeSegmentKey) KEYSTO(NODE) RCTO(rc);
   if(rc) return FS_InternalError;
   return 0;
}
/*****************************************************************
  DOFAULT(address)  - handle fault at ADDRESS
*****************************************************************/
UINT32 dofault(address,copyaddr,copylen)
  LLI address;
  unsigned long copyaddr,copylen;
{
   JUMPBUF;
   UINT16 lss,hostlss,i,topslot;
   struct Node_KeyValues nkv;
   UINT32 rc;
 
   UCHAR slots[12];
   struct Node_DataByteValue ndb;
   LLI limit;
   char buf[64];
   int needupdate;
   int actlen;
 
   lss=unpack(address,slots);                    /* get lss needed */
 
/* slots is the unpacked address */
/* lss is what is required to address the fault address */
 
   KC (NODE,Node_Fetch+NODESB) KEYSTO(SB) RCTO(rc);
   if(rc) return KT+1;

   KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0) RCTO(rc);
   if(rc) return KT+1;
   KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
   memcpy(buf+1, &nkv.Slots[0].Byte[5], 7);
   *buf=0;
   memcpy(&limit.hi,buf,8);
#ifdef debugging
   hexcvt(&limit.hi,buf,8);
   strcat(buf, " Limit ");
   KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
#endif
   if(address.hi > limit.hi) {
        return KT+7;
   }
   if(!address.hi && !limit.hi) {
        if(address.low > limit.low) {
           return KT+7;
        }
   }

#ifdef debugging
   hexcvt(&lss,buf,2);
   strcat(buf," lss -> format ");
   KC  (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
   hexcvt(&nkv.Slots[0].Byte[12],buf,4);
   strcat(buf,"\n");
   KC  (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
#endif
/* nkv.Slots[0] has the format data */
 
   needupdate=0;
   while((int)lss > (int)(nkv.Slots[0].Byte[15] & 0x0F)) {   /* grow tree */
      KC (SB,SB_CreateNode) KEYSTO(K0) RCTO(rc);
      if(rc) return NONODES_RC;
      topslot=NODESB;
      if(needupdate) topslot=1;
      for(i=0;i<topslot;i++) {                 /* copy first 11 keys */
         KC(NODE,Node_Swap+i) KEYSTO(K1) RCTO(rc);
         if(rc) return FS_InternalError;
         KC(K0,Node_Swap+i) KEYSFROM(K1) RCTO(rc);
         if(rc) return FS_InternalError;
      }
      ndb.Byte=nkv.Slots[0].Byte[15] & 0x0F; /* lss of new node */
      KC (K0,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K0) RCTO(rc);
      if(rc) return FS_InternalError;
      KC (NODE,Node_Swap+0) KEYSFROM(K0) RCTO(rc);
      if(rc) return FS_InternalError;
      nkv.Slots[0].Byte[15]++;              /* bump lss of root */
      needupdate=1;
   }
   if(needupdate) {
      nkv.StartSlot=NODEFORMAT;
      nkv.EndSlot=NODEFORMAT;
      KC (NODE,Node_WriteData) STRUCTFROM(nkv);        /* update node */
   }
 
/* at this point the tree is deep enough to support the new page */
/* now find hole and fill it by walking down the tree to the new */
/* page.  Nodes may have to be obtained to fill in the path to the */
/* to the page.  A queued fault must be recognized */
 
/* lss is what is needed for the address.  We must start at the */
/* top of the tree */
 
   lss=nkv.Slots[0].Byte[15] & 0x0F;   /* depth of tree */
 
   while(lss > 2) {                        /* till lss=3 (done) */
     KC (NODE,Node_Fetch+slots[11-lss]) KEYSTO(K1) RCTO(rc);
     if(rc) return FS_InternalError;
     KC (K1,KT) RCTO(rc);                  /* what is here */
     if((rc == KT+1) || (rc == Page_ROAKT)) {   /* a hole */
        if(lss == 3) {  /* the leaf page */

          if(copylen) {

/* this is a copyme segment.  Therefore we start by fetching  a  */
/* readonly copy of the page from the parent and return rc=-1 (no copy) */
/* we need to check the range actually and buy the page if outside the range */
/*  copyaddr <i= address < copyaddr+copylen */ 

            if(rc == Page_ROAKT) {
                KC (MEMNODE,Node_Swap+2) KEYSFROM(K1);  /* parent to my addr */
                KC (SB,SB_CreatePage) KEYSTO(K1) RCTO(rc);
                if (rc) return NOPAGES_RC;
                KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1) RCTO(rc);
                KC (K1,Page_WriteData) CHARFROM(page,4096);
                return -1;  /* nothing to do */
            }
            if(address.low < copyaddr) goto buypage;
            if(address.low > copyaddr+copylen) goto buypage;

            KC (MEMNODE,Node_Fetch+MEMNODESAVE) KEYSTO(K1);
            KC (K1,Node_Fetch+NODEPARENT) KEYSTO(K1);
            KC (K1,Node_Fetch+NODEFORMAT) KEYSTO(K0);
            KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
            hostlss=nkv.Slots[0].Byte[15] & 0x0F;
      
            while(hostlss > 2) {  /* all the way to K1 contain page key */
                KC (K1,Node_Fetch+slots[11-hostlss]) KEYSTO(K1) RCTO(rc);
                if(rc) break;
                hostlss--;
            }
            if(rc) {
buypage:
               KC (SB,SB_CreatePage) KEYSTO(K1) RCTO(rc);
               if (rc) return NOPAGES_RC;
               KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1) RCTO(rc);
            }
            else {
#define cow 1
#ifdef cow
                KC (K1,Page_MakeReadOnlyKey) KEYSTO(K1) RCTO(rc);
                if(rc) goto buypage;
                KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1) RCTO(rc);
                if(rc) return FS_InternalError;
                return -1;  /* indicate used RO page */
#else
                KC (MEMNODE,Node_Swap+2) KEYSFROM(K1);
                KC (SB,SB_CreatePage) KEYSTO(K1) RCTO(rc);
                if (rc) return NOPAGES_RC;
                KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1);
                KC (K1,Page_WriteData) CHARFROM(page,4096);
                return -1;  /* no action */
#endif
            }
          }
          else {
            i=slots[11-lss];
#ifdef BADIDEA
            i= (i/3)*3;          /* will fill slots in chunks of 3 */
            if(15==i) {  /* only 1 page */
#endif
               KC (SB,SB_CreatePage) KEYSTO(K0) RCTO(rc);
               if (rc) return NOPAGES_RC;
               KC (NODE,Node_Swap+i) KEYSFROM(K0) RCTO(rc);
#ifdef BADIDEA
            }
            else  {
               KC (SB,SB_CreateThreePages) KEYSTO(K0,K1,K2) RCTO(rc);
               if (rc) return NOPAGES_RC;
               KC (NODE,Node_Swap+i) KEYSFROM(K0) RCTO(rc);
               KC (NODE,Node_Swap+i+1) KEYSFROM(K1) RCTO(rc);
               KC (NODE,Node_Swap+i+2) KEYSFROM(K2) RCTO(rc);
            }
#endif
          } 
          if(rc) return FS_InternalError;
          return 0;
        }
        else {                             /* must add node */
          KC (SB,SB_CreateNode) KEYSTO(K1) RCTO(rc);
          if (rc) return NONODES_RC;
          ndb.Byte=lss-1;
          KC (K1,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K1) RCTO(rc);
          if (rc) return FS_InternalError;
          KC (NODE,Node_Swap+slots[11-lss]) KEYSFROM(K1) RCTO(rc);
          if (rc) return FS_InternalError;
        }
      }
      else {  /* found something, best be node or page */
 
/* if two domains fault on the same page, the second will queue  */
/* while the keeper fills in the page.  When the queued fault is */
/* processed, it will find the page already present.             */
 
        if(rc == Page_AKT) return 0; /* a page, filled in before */
        if(rc != Node_NODEAKT) crash("Stange key in Node");
      }
      KC (DOMKEY,Domain_GetKey+K1) KEYSTO(NODE); /* move to new node */
      lss--;
   }
   return FS_InternalError;
}
/********************************************************************
  UNPACK(address,slots)  unpack address into slot array
                         return lss of node needed to address
********************************************************************/
UINT16 unpack(address,slots)
   LLI address;
   UCHAR *slots;
{
   UINT16 lss;
   int i;
 
   for(i=11;i>=0;i--) {
      slots[i]=address.low & 0x0F;
      llilsr(&address,4);
   }
   for(lss=11;lss>2;lss--) {   /* find left most address byte */
      if(slots[11-lss]) break;
   }
   if(lss==2) lss=3;           /* for zero loop goes too far */
   if(slots[11-lss] >= NODESB) lss++;  /* adjust for short node */
                               /* only 11 slots used in first node */
                               /* so must use larger node if need */
                               /* to address above that */
   return lss;
}
/*******************************************************************
   PACK(slots,address)  pack address from slot array
*******************************************************************/
void pack(slots,address)
    UCHAR *slots;
    LLI *address;
{
    int i;
    for(i=0;i<12;i++) {
      llilsl(address,4);
      address->low = (address->low & 0xFFFFFFF0) | slots[i];
    }
}
/************************************************************************/
/*  highestaddr=treewalk(slot,lss,startslot,addr,truncaddr)             */
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
/*     return is the  highest non zero address                          */
/*                                                                      */
/* assume an lss=6 top level red node with 12 initial slots             */
/*                                                                      */
/*  trim and return length                                              */
/*                 len = treewalk(-1,6,12,0,0xFFFFFFFFFFFFFFFF)         */
/*  empty                                                               */
/*                 treewalk(-1,6,12,0,0)                                */
/*                                                                      */
/************************************************************************/
unsigned long long treewalk(int slot,int lss,int startslot,
  unsigned long long addr,unsigned long long truncaddr)
{
     JUMPBUF;
     UINT32 rc,bytes;
     int i;
     unsigned long long pageaddr;
     unsigned long long highestaddr = -1;

     if((lss > 3) && (highestaddr == -1)) {  /* must continue plunge unless done */
         for(i=startslot;i>=0;i--) {  /* look at each slot */
            KC (NODE,Node_Swap+i)              KEYSFROM(UPSLOT) KEYSTO(K0);
            KC (K0,KT) RCTO(rc);
            if(rc != Node_NODEAKT) {  /* slot does not go deeper, this is weird */
               KC (NODE,Node_Swap+i) KEYSFROM(K0);  /* put back non-node key */
            }
            else {
               KC (DOMKEY,Domain_SwapKey+NODE) KEYSFROM(K0) KEYSTO(UPSLOT);

               highestaddr=treewalk(i,lss-1,15,((addr << 4) | i),truncaddr);

               KC (DOMKEY,Domain_SwapKey+NODE) KEYSFROM(UPSLOT) KEYSTO(K0);
               KC (NODE,Node_Swap+i)           KEYSFROM(K0) KEYSTO(UPSLOT);

	       if(highestaddr != -1) break;   /* cause early exit */
            }
         }
	 if((highestaddr == -1) && (slot > -1)) { /* Must have sold all the nodes, keep root */
            KC (SB,SB_DestroyNode) KEYSFROM(NODE);
         }
     }
     else {  /* leaf */
         for(i=startslot;i>=0;i--) {  /* Take action at leaf */
             KC (NODE,Node_Fetch+i) KEYSTO(K1);
             KC (K1,KT) RCTO(rc);

             if((rc == Page_AKT) || (rc == Page_ROAKT) ) {  /* some form of page key */

/************************************************************************************/
/*  what to do.  trim or truncate                                                   */
/*    trim if truncaddr = FFFFFFFFFFFFFFFF  else truncate to truncaddr              */
/************************************************************************************/
		 pageaddr = (((addr << 4) | i) << 12);  /* address of this page */
		 if(pageaddr > (truncaddr & 0xFFFFFFFFFFFFF000)) { /* definitely remove page  */
                     KC (SB,SB_DestroyPage) KEYSFROM(K1) RCTO(rc);  /* sell page, don't change highest */
                 }
                 else {  /* below truncaddr, maybe */
	             KC (MEMNODE,Node_Swap+2) KEYSFROM(K1);  /* map page into my memory at 0x00400000 */
		     bytes=testpage();  /* returns number non-zero bytes */
                     if(!bytes) {  /* zero page, remove and don't set hightest addr */
			 KC (SB,SB_DestroyPage) KEYSFROM(K1) RCTO(rc);
                     }
                     else {
		         if(bytes > truncaddr & 0xFFF) {  /* zero page above truncaddr, return highest */
                             zappage(truncaddr & 0xFFF);
			     return truncaddr;
                         }
			 return pageaddr+bytes-1;  /* this is the highest non-zero below truncaddr */
                     }
		 }
             }  
         }
	 /* must have sold all pages or there were none */
         if(slot > -1) {
             KC (SB,SB_DestroyNode) KEYSFROM(NODE);  /* don't sell root */
         }
     }

     return highestaddr;
}

/********************************************************************
  GETLSS()
********************************************************************/
int getlss()
{
    JUMPBUF;
    struct Node_KeyValues nkv;
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0) RCTO(rc);
    if(rc) return 0;
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    return (nkv.Slots[0].Byte[15] & 0xF);
}
/*******************************************************************
  MAKEINVALID() while tree walking
*******************************************************************/
void makeinvalid()
{
    JUMPBUF;
    struct Node_KeyValues nkv={NODEFORMAT,NODEFORMAT,
        {FormatK(0,15,NODEKEEPER,NODESB,3)}
    };
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0) RCTO(rc);
    if(rc) return;
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    nkv.Slots[0].Byte[15] = nkv.Slots[0].Byte[15] & 0x0F;  /* set I to zero */
    KC(NODE,Node_WriteData) STRUCTFROM(nkv);
}
 
/*******************************************************************
  MAKEVALID() after tree walking
*******************************************************************/
void makevalid()
{
    JUMPBUF;
    struct Node_KeyValues nkv={NODEFORMAT,NODEFORMAT,
        {FormatK(0,15,NODEKEEPER,NODESB,3)}
    };
    int actlen;
    UINT32 rc;

    KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0) RCTO(rc);
    if(rc) return;
    KC (K0,1) STRUCTTO(nkv.Slots[0],,actlen) RCTO(rc);
    nkv.Slots[0].Byte[15] = (nkv.Slots[0].Byte[15] & 0x0F) | (NODESB << 4);
    KC(NODE,Node_WriteData) STRUCTFROM(nkv);
}

/********************************************************************
  FIXLSS(address)  - adjusts lss based on address
********************************************************************/
void fixlss(address)
   LLI address;
{
   JUMPBUF;
   UCHAR slots[12];
   UINT16 lss;
   UINT32 rc;
   int actlen;
   struct Node_KeyValues nkv={NODEFORMAT,NODEFORMAT,
        {FormatK(0,15,NODEKEEPER,NODESB,3)}
   };
   struct Node_KeyValues nkv1;
   
 
   lss=unpack(address,slots);

   KC (NODE,Node_Fetch+NODEFORMAT) KEYSTO(K0) RCTO(rc);
   if(rc) return;
   KC (K0,1) STRUCTTO(nkv1.Slots[0],,actlen) RCTO(rc);  // preserve the limit
   memcpy(&nkv.Slots[0].Byte[5],&nkv1.Slots[0].Byte[5],7);

   nkv.Slots[0].Byte[15] = nkv.Slots[0].Byte[15] & 0xF0 |
     lss;
   KC(NODE,Node_WriteData) STRUCTFROM(nkv);
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

