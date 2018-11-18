/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "pdrh.h"

unsigned long forminit(void);
   /* Returns the sizeof the read/write work area needed.
      The caller is responsible for allocating this area.
      Its address is passed on subsequent calls. */

struct packstring {
   char packserial[8];
   char packsetid[8];
   enum {
      NOCKPTHDR,
      CKPTHDR1,
      CKPTHDR2
   } ckptflag; /* flag for checkpoint header */
};

/* Define a pack */
int formdev(
   void *work,  /* Pointer to the workarea allocated by the caller */
   const struct packstring *packstr);
   /* 0 is the only possible return code. */

struct rangeinfo {
   enum {
      SWAP2  = PDRDSWAPAREA2,  /* SWAPAREA 2 SWAP RANGE */
      SWAP1  = PDRDSWAPAREA1,  /* SWAPAREA 1 SWAP RANGE */
      NORMAL = PDRDNORMAL,     /* NORMAL RANGE */
      DUMP   = PDRDDUMP,       /* RANGE FOR KERNEL DUMPS */
      IPL    = PDRDIPL         /* RANGE THAT CONTAINS AN IPLABLE KERNEL */
   } type;
   unsigned char firstcda[6];
   unsigned long numcdas;
   unsigned short multiplicity; /* number of copies of this range */
};

/* Define a range */
int formrng(
   void *work,  /* Pointer to the workarea allocated by the caller */
   const struct rangeinfo *range);
   /* Returns 0 if ok, 8 if some error (message issued). */

/* Start physically formating the pack. */
int formfmt(
   void *work);  /* Pointer to the workarea allocated by the caller */
