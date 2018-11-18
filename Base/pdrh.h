/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_pdrh
#define _H_pdrh
/**********************************************************************
   Proprietary material of Key Logic  COPYRIGHT (C) 1990 Key Logic
**********************************************************************/
/*
   Defines Pack Descriptor Record
*/
#include "keyh.h" /* for CDA */
#include "kktypes.h"
#include "lli.h"

struct PDRangeDesc {
   CDA first, last;      /* first and last (not last+1) CDAs in range */
   uchar offset[6];    /* Page offset on the pack */
   uchar nplex[2];     /* Expected number of range instances */
   uchar type;           /* Type of range as follows: */
#define PDRDNORMAL             0   /* Ordinary pages or nodes */
#define PDRDDUMP               1   /* Pages for a kernel dump */
#define PDRDIPL                2   /* Pages for a bootable kernel */
#define PDRDCHECKPOINTHEADER 253   /* A checkpoint header */
#define PDRDSWAPAREA1        254   /* Frames are part of swap area 1 */
#define PDRDSWAPAREA2        255   /* Frames are part of swap area 2 */
   uchar migrationtod[sizeof(LLI)];
                         /* Time of last migration to this range */
};
typedef struct PDRangeDesc PDRD;
 
struct PackDescData {
   uchar seedid[8];    /* ID number to identify this pack set */
   short rangecount;     /* Number of ranges on this pack */
   uchar packserial[8]; /* Serial number/name of the pack */
   uchar version;        /* Version number of the pack format */
   uchar integrity;      /* Write integrity check bits */
#define PDINTEGRITYCOUNTER 0x07  /* These bits are used as a counter */
                                 /* to check that the pack descriptor */
                                 /* record was correctly written. The */
                                 /* counter in the header must match */
                                 /* counter in the last byte of the */
                                 /* pot. */
};
 
#define PDRANGES ((pagesize-sizeof(struct PackDescData)-2)/sizeof(PDRD))
 
struct PackDescRecord {
   struct PackDescData pd;      /* Pack description data */
   PDRD ranges[PDRANGES];     /* Range descriptions */
   uchar precheck;              /* See checkbyte */
   uchar checkbyte;             /* Check byte for correct write. The
                                 last 3 bits must match the last
                                 3 bits of pd.integrity. The
                                 first 5 bits are the inverse of the
                                 same bits in the precheck byte, to
                                 check for controlers which
                                 replicate bytes or errors (e.g.
                                 the IBM 3880 on an overrun error) */
};
typedef struct PackDescRecord PDR;

/* The page before the PDR contains PDR_MAGIC in the word at
   offset PDR_MAGIC_OFFSET.
   This magic number distinguishes the PDR from a Unix 
   file system superblock. */
#define PDR_MAGIC 0x82646
#define PDR_MAGIC_OFFSET 0x55c

#define PDRPAGE 3 /* The page offset of the pdr on the pack. */
#endif

