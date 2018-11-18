/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************/
/* CALLSEGC - Provide access to segment by callers                    */
/*                                                                    */
/*        CALLSEGF(0;SB,M=>c,CALLSEG)                                 */
/*            c = 0  -  It was allocated                              */
/*            c = 1  -  SB not "offical prompt"                       */
/*            c = 2  -  Not enough space in SB                        */
/*        CALLSEGF(kt=>X'...')                                        */
/*                                                                    */
/*        CALLSEG(0=>,SK)                                             */
/*            return segment key                                      */
/*                                                                    */
/*        CALLSEG(1,SK=>)                                             */
/*            accept new segment key                                  */
/*                                                                    */
/*        CALLSEG(2,(offset,length)=>0;data)                          */
/*            read segment data from offset address for length        */
/*                                                                    */
/*        CALLSEG(3,(offset,data)=>0)                                 */
/*            write segment data to offset address                    */
/*                                                                    */
/*        CALLSEG(kt=>X'110D')                                        */
/*        CALLSEG(kt+4=>0)                                            */
/*            destroys the CALLSEG domain                             */
/*                                                                    */
/*                                                                    */
/* This is what the memory tree will look like:                       */
/*                                                                    */
/*    0XXXXX Read only CALLSEG (and its keeper) code                  */
/*    1XXXXX Stack                                                    */
/*    2XXXXX First SLOTSIZE portion of segment (unfiltered)           */
/*    slot 3 used by keeper to access CALLSEG's stack as 3XXXXX       */
/*    slot 4 window one                                               */
/*    slot 5 window two                                               */
/*                                                                    */
/* Implementation notes:                                              */
/*   Code is written as a function of LSS value to change windowing   */
/*   size.  LSS=3 (page windowing) is not supported.                  */
/*   Current version of CFSTART automatically provides LSS=5 slots    */
/*   so no memory tree manipulation is required.                      */
/*   Rudimentary support for a window cache (CACHESIZE) exists        */
/*   but is not fully implemented.  Only one window pair is used now. */
/**********************************************************************/
 
#include "keykos.h"
#include "callseg.h"
#include "domain.h"
#include "node.h"
#include "dc.h"
#include "sb.h"
#include "setjmp.h"
#include <string.h>
#include "ocrc.h"
 
#define KT           0x80000000u
#define CACHESIZE    1
#define LSS          5
#define SLOTSIZE    (0x01<<(4*LSS))
#define OFFSETSIZE  (SLOTSIZE>>16)
 
#define LOCAL        0x02
#define BKGRND       0x03
#define READONLY     0x08
#define NOCALL       0x04
#define SLOT2        0x20
#define LASTBYTE    (SLOT2 | LOCAL)
 
/* Components node and its contents */
KEY COMPONENTSNODE  =  0;          /* fetch key to components node */
 
/* Domain key slots */
KEY DK              =  1;          /* passed to keeper on trap */
KEY CALLER          =  2;          /* normal resume key        */
KEY MYDOMAIN        =  3;
KEY BANK            =  4;
KEY METER           =  5;
KEY DOMCRE          =  6;
KEY STARTKEY        =  7;
KEY MYKEEPER        =  8;          /* to get error code and destroy */
KEY MYMEMTREE       =  9;
KEY K0              = 10;          /* utility */
KEY K1              = 11;          /* utility */
KEY CSDOMAIN        = 12;          /* left by callseg for keeper */
KEY CSKEEPER        = 13;          /* for use by keeper */
KEY CSMEMTREE       = 14;          /* for use by keeper */
KEY DK0             = 15;
 
struct cache_entry {
       uint32 offset;     /* offset address from start of segment */
       char   fmt;        /* not used when cachesize = 1 */
       char   state;      /* not used when cachesize = 1 */
       char   slotnum;    /* points to window key pair for this entry */
       char   filler;
};
 
struct private_ws {
       struct  cache_entry cache[CACHESIZE];
       struct {                 /* the message string */
         char  parm[6];
         char  data[4000];
       } s;
};
char title[] = "CALLSEGC";    /* program name      */
int stacksiz = 8192;           /* desired stacksize */
 
/* Function declarations */
char *setwindow(struct private_ws *);
void make_window_keys(int, uint32, int);
int fork();
int exit();
 
/*********************************************************************/
/* Begin Program Code                                                */
/*********************************************************************/
factory()
{
   JUMPBUF;
 
 /* Define local (stack) variables */
 
 uint32  oc, rc;                           /* for key calls           */
 uint32  errcode;                          /* of the addressing fault */
 int     i,j;                              /* utility                 */
 short   is;                               /* utility                 */
 unsigned short tc;                        /* utility                 */
 char    *stack_frame_p;                   /* for parameter addresses */
 jmp_buf jump_buffer;                      /* for setjmp, longjmp     */
 struct  Domain_SPARCRegistersAndControl drac;  /* for keeper         */
 struct  private_ws pws;                   /* to pass to functions    */
 
 /* Put MYDOMAIN in slot where keeper can get access to it. */
 KALL(MYDOMAIN,Domain_GetKey+MYDOMAIN) KEYSTO(CSDOMAIN);
 
 /* Now create and fork a clone domain to be the keeper */
 if (!(rc=fork()) ) {
/*********************************************************************/
/* KEEPER_CODE  --  fork() returned a zero.                          */
/* Has its own copy of all variables in its own stack.               */
/* If invoked by a trap, high order bit of order code is on.         */
/*********************************************************************/

   errcode = 0;                                        /* initialize */
 
   /* Install start key as CS domain keeper, and save its old keeper */
   KALL(MYDOMAIN,Domain_MakeStart) KEYSTO(K0,DK);
   KALL(CSDOMAIN,Domain_SwapKeeper) KEYSFROM(K0) KEYSTO(CSKEEPER);
   LDEXBL(DK, 0);
   
   while(1) {  /* main keeper loop; once per fault */
     LDENBL OCTO(oc) KEYSTO(DK0,,DK,CALLER) STRUCTTO(drac);
     RETJUMP();
  Panic();
 
     if (oc == 0) {                      /* return fault information */
       LDEXBL(CALLER,errcode) STRUCTFROM(drac);
     }
     else if (oc == 4) {          /* destroy domain, KT+4 substitute */
       exit();
     }
     else {                        /* this must be a trap to process */
       errcode = drac.Control.TRAPCODE;

#ifdef Later
       tc = (drac.Control.TRAPCODE) & 0x0FFF;  /* get TC0 of trap code */
       if (tc == 0x20C) {                     /* addressing fault (Read) */
         errcode = drac.Control.TRAPCODE;
       }
       else if (tc == 0x100) {         /* rejected parameter word */
         errcode = drac.Control.TRAPCODE;
       }
       else {               /* this is not a fault for me to process */
         /* Fork original keeper with parameters passed to me */
         KFORK(CSKEEPER,oc) KEYSFROM(,,DK,CALLER) STRUCTFROM(drac);
         LDEXBL(DK0,0);       /* load exit block to become available */
         continue;                          /* go to top of for loop */
       }
#endif
       {uint32 fpc = drac.Control.PC;
        typedef struct{uint32 oldpc, newPc, newNPc;} pcTripple;
  //      extern pcTripple* hyperGo;.....
        pcTripple* wlk = 0; // hyperGo;  .....
        while(wlk->oldpc){if (wlk->oldpc == fpc) {
             drac.Control.PC = wlk-> newPc;
             drac.Control.NPC = wlk-> newNPc;
             drac.Regs.o[0] = drac.Control.TRAPCODE;
             goto resume;}
           ++wlk;}
       }
       {     /* this is not a fault for me to process */
         /* Fork original keeper with parameters passed to me */
         KFORK(CSKEEPER,oc) KEYSFROM(,,DK,CALLER) STRUCTFROM(drac);
         LDEXBL(DK0,0);       /* load exit block to become available */
         continue;                          /* go to top of for loop */
       }
       resume:
       LDEXBL(DK,Domain_ResetSPARCStuff) KEYSFROM(,,,CALLER) STRUCTFROM(drac);

     } /* end oc tests */
   } /* end keeper invocation loop */
  } /* end fork-a-keeper */
  if(rc > 1) { // failure say no space
      exit(NOSPACE_RC); 
  }
 
/*********************************************************************/
/* MAIN LINE CODE                                                    */
/* The fork() operation returned as non-zero.                        */
/*********************************************************************/
 
 /* Get memory key for segment and window key manipulation. */
 KALL(MYDOMAIN,Domain_GetMemory) KEYSTO(MYMEMTREE);
 
 /* For each cache_entry in cache, initialize the window keys     */
 /* offset is the number of the SLOTSIZE block in the segment.    */
 /* Initially, offset starts at 0 and increments by 2*OFFSETSIZE  */
 /* slotnum = offset+4  (i.e. slots 4,5  6,7  8,9  A,B  C,D  E,F) */
 for (i=0; i<CACHESIZE; i++) {
    j = 2*i;
    pws.cache[i].offset  = j * OFFSETSIZE;       /* window address */
    pws.cache[i].fmt     = '\0';
    pws.cache[i].state   = '\0';
    pws.cache[i].slotnum = j + 4;                  /* its slot pair */
    make_window_keys(j + 4,j,LASTBYTE);      /* for this cache slot */
 }
 
 Panic();
 
 /* Main Program Loop; Once per invocation of callseg key */
 while(1) {
   uint32 len;
int CopyIfYouCan(void* to, void* from, int cc, void ** failAddr){} // .....
   LDENBL OCTO(oc) KEYSTO(K0,,,CALLER)
                   CHARTO(pws.s.parm,6+4000,len);
   RETJUMP();

   /* test order codes and process appropriately */
   if (oc == Callseg_ReturnSegmentKey ) {
      KALL(MYMEMTREE,Node_Fetch+2) KEYSTO(K0);
      LDEXBL(CALLER,0) KEYSFROM(K0);
   }
   else if (oc == Callseg_ReplaceSegmentKey ) {
      KALL(MYMEMTREE,Node_Swap+2) KEYSFROM(K0);
      LDEXBL(CALLER,0) KEYSFROM(K0);
   }
   else if (oc == Callseg_ReadSegmentData) {   /* offset(i6),length(i2)*/
      len = 0;
      memcpy((char *)&len + 2, pws.s.data, 2); /* get l'data to return */
      if (len > 4000) LDEXBL(CALLER,FORMATERROR_RC); /* length too big */
      else {
         void * fail;
         char* seg_p = setwindow(&pws);       /* get address in window */
         int trouble = CopyIfYouCan(&pws.s.data, seg_p, len, &fail);
         /* byte=*seg_p;  shouldn't be necessary! */
         LDEXBL(CALLER,trouble) CHARFROM(pws.s.data, trouble?0:len);   
      }
   }
   else if (oc == Callseg_WriteSegmentData ) { /* offset(i6),data(xv) */
      void * fail;
      char* seg_p = setwindow(&pws);         /* get address in window */
      int trouble = CopyIfYouCan(seg_p,pws.s.data,len-6, fail); // move data into seg
      LDEXBL(CALLER,trouble);
   }
   else if (oc == KT) {                            /* return KT value */
      LDEXBL(CALLER,Callseg_AKT);
   }
   else if (oc == DESTROY_OC) {                    /* destroy domain */
      KALL(MYDOMAIN,Domain_GetKeeper) KEYSTO(K0);       /* get keeper */
      KFORK(K0,4);                                      /* destroy it */
      exit();                                         /* destroy self */
   }
   else LDEXBL(CALLER,INVALIDOC_RC);                /* bad order code */
 
 } /* end callseg invocation loop */
 
}/* end callseg */
 
/*********************************************************************/
/* MAKE_WINDOW_KEYS                                                  */
/*                                                                   */
/* Formats a pair of window keys (in struct wp) which window over    */
/*     a pair of LSS (sub)segments in the segment.                   */
/* A pair is used to cover exceeding the first window end address.   */
/* Valid slot pairs are 4, 6, 8, 10, 12, 14.                         */
/* LSS must be > 3.  This code does not support page windowing.      */
/*   Therefore, window_key.Byte[Node_KEYLENGTH-2] is always zero.    */
/*********************************************************************/
void make_window_keys(
     int    v_slot,                     /* first slot of window pair */
     uint32 v_offset,                   /* windowing address         */
     int    v_last_byte)                /* holds slot#, RO, NC bits  */
{
     JUMPBUF;
 struct window_pair {
        uint32       cntl1;                  /* starting slot number */
        uint32       cntl2;                  /* ending slot number   */
        Node_KeyData window_one;
        Node_KeyData window_two;
 } wp;
 
 memset(&wp,0,sizeof(wp));                         /* clear the area  */
 
 wp.cntl1 = v_slot;                               /* set slot number */
 wp.cntl2 = wp.cntl1 + 1;                         /* and its pair    */
 
 v_offset = v_offset & ~(OFFSETSIZE-1);       /* zero low order bits */
 memcpy(&wp.window_one.Byte[Node_KEYLENGTH-6],&v_offset,4);
 v_offset = v_offset + OFFSETSIZE;       /* next window, next offset */
 memcpy(&wp.window_two.Byte[Node_KEYLENGTH-6],&v_offset,4);
 
 wp.window_one.Byte[Node_KEYLENGTH-1] = v_last_byte;
 wp.window_two.Byte[Node_KEYLENGTH-1] = v_last_byte;
 
 KALL(MYMEMTREE,Node_WriteData) STRUCTFROM(wp);
 
 return;
} /* end make_window_keys */
 
/*********************************************************************/
/* SETWINDOW                                                         */
/*                                                                   */
/* The first four bytes of the six byte address (in parm) identify   */
/*   the "64K segment number" (LSS=4) in which the address is found. */
/* If using larger windows (LSS>4), zero low order bits of offset.   */
/*********************************************************************/
char *setwindow(struct private_ws * p)
{
 int i, j, k, seg_num;
 
 memcpy(&seg_num,&p->s.parm[0],4);       /* get 64K segment number */
 seg_num = seg_num & ~(OFFSETSIZE-1);         /* zero low order bits */
 for (i=0; i<CACHESIZE; i++) {              /* search thru cache for */
   if (seg_num == p->cache[i].offset) {   /* a window that matches */
      goto return_address;
   }
 } /* end for */
 
 /* No window keys match address, so make one that does.             */
 /* If CACHESIZE > 1, then first determine which slot to purge. {ni} */
 /* Else just use slot pair (4,5) all the time.                      */
 make_window_keys(4, seg_num, LASTBYTE); // NOTE HARDCODED SLOTNUM
 p->cache[0].offset = seg_num;          /* set new offset in cache */
 i = 0;                                 /* and point to it for below */
 
 return_address:
 /* Format the return address as:                                    */
 /*   0x000saaa  (for LSS=4)      0x00saaaaa  (for LSS=5)            */
 /* The following describes LSS=5:                                   */
 /* The high order byte is zero.  The next nibble (4 bits) is the    */
 /*   slot number of the window key which maps the desired address.  */
 /* The remainder is the low order 5 nibbles of the desired address. */
 /* Then stuff it in an int so setwindow returns it as the address.  */
 
 memcpy(&j,&p->s.parm[2],4);        /* get last 4 bytes of address */
 j = j & (SLOTSIZE-1);                   /* turn off high order bits */
 k = p->cache[i].slotnum;         /* get slot number of window key */
 k = k * SLOTSIZE;             /* put slot number in proper position */
 j = j | k;                                    /* slap them together */
 return((char *)j);                       /* return it as an address */
} /* end setwindow */
