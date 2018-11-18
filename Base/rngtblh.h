/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "lli.h"
 
typedef struct RangeList RANGELIST;
 
struct RangeList {
   RANGELIST *next;       /* Next in list for range or NULL */
   struct Device *device; /* Device on which range instance exists */
   uint32 firstpage;      /* Offset on device to first page of range */
   uchar flags;           /* Flags as follows: */
#define RANGELISTOBSOLETE          0x80
                              /* Range is not in valid current state */
#define RANGELISTRESYNCINPROGRESS  0x40
                                /* Obsolete range being re-introduced*/
#define RANGELISTWAITFORCHECKPOINT 0x20
                                    /* Range resynced - wait for ckpt*/
#define RANGELISTUPDATEPDR         0x10
                               /* Update PDR entry on this pack with */
                               /* "current" TOD stamp.  Has been     */
                               /* resynced iff marked obsolete.      */
#define RANGELISTUPGRADE           0x08
                      /* Range is to be upgraded for new disk format */
};
 
struct CDAFirstLast {
   CDA first, last;       /* First and last (not last+1) cdas */
};
 
struct SBAFirstLast {
   uint32 first, last;    /* First and last (not last+1) swap addrs */
};
 
typedef struct RangeTable RANGETABLE;
 
struct RangeTable {
   union {
      struct CDAFirstLast cda;  /* First and last for pages & nodes */
      struct SBAFirstLast sba;  /* First and last for swap ranges */
   } dac;                 /* Disk address codes */
   RANGELIST *devlist;    /* Devices on which this range exists */
   sint16 nplex;          /* iff >= 0 then number of copies of range */
                          /* -1 ==> swap area 1, -2 ==> swap area 2 */
   uchar type;            /* Type of range as follows: */
#define RANGETABLENORMAL        0
                              /* Everyday kind of page or node range */
#define RANGETABLEDUMP          1
                                      /* Page range for kernel dumps */
#define RANGETABLEIPL           2
                                /* Page range for an IPL-able kernel */
#define RANGETABLECHECKPOINTHDR 253
                                     /* Range is a checkpoint header */
#define RANGETABLESWAPAREA2     254
                                     /* Range is part of swap area 2 */
#define RANGETABLESWAPAREA1     255
                                     /* Range is part of swap area 1 */
 
   uchar flags;           /* Flags as follows: */
#define RANGETABLEOBSOLETEHAVETHISTOD 0x80
                     /* One of the PDRs for an obsolete copy of this */
                     /* range may have the "current" TOD stamp.      */
#define RANGETABLEUPDATEPDR           0x20
                               /* On iff the UPDATEPDR flag is on in */
                               /* a RANGELIST under this entry       */
#define RANGETABLEPOOL1               0x01
                               /* This swap range is in swap space   */
                               /* pool 1 for determining checkpoint  */
                               /* limit. Otherwise pool 0            */
   uint64 migrationtod;           /* Time of last partial migration */
};
 
extern uint32 swapaloc[]; /* Allocations by range */
extern char swapused[];   /* Flag for already returned */
extern RANGETABLE swapranges[];
extern RANGETABLE *userranges;
#define rangetable userranges            /* Base address for indexing */
 
extern RANGELIST *rangelistbase;         /* Available rangelists */
