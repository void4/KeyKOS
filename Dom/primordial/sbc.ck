/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*******************************************************************
  SB - Prime Space Bank, Space Bank, and Space Bank Transformer
 
  SB(0==>c;N1)                        - Create node
  SB(1;N1==>c)                        - Destroy node
  SB(2;N1==>c;Nn)                     - Sever node
  SB(5==>c,qty)                       - Query nodes available
  SB(6==>c,(4,buys),(4,sells))        - Query nodes statistics
  SB(7==>c;N1,N2)                     - Create 2 nodes
  SB(8==>c;N1,N2,N3)                  - Create 3 nodes
  SB(9;N1,N2==>c)                     - Destroy 2 nodes
  SB(10;N1,N2,N3==>c)                 - Destroy 3 nodes
  SB(11,(4,delta)==>c,(4,newlimit))   - Change node limit
  SB(12,(16,limit)==>c;(16,newlimit)) - Change node range limit
  SB(13,(8,delta)==>c,(8,newlimit))   - Change node limit (big)
  SB(14==>c,(8,qty))                  - Query nodes available (big)
 
  SB(16==>c;N1)                       - Create page
  SB(17;N1==>c)                       - Destroy page
  SB(18;N1==>c;Nn)                    - Sever page
  SB(21==>c,qty)                      - Query pages available
  SB(22==>c,(4,buys),(4,sells))       - Query pages statistics
  SB(23==>c;N1,N2)                    - Create 2 pages
  SB(24==>c;N1,N2,N3)                 - Create 3 pages
  SB(25;N1,N2==>c)                    - Destroy 2 pages
  SB(26;N1,N2,N3==>c)                 - Destroy 3 pages
  SB(27,(4,delta)==>c,(4,newlimit))   - Change page limit
  SB(28,(16,limit)==>c;(16,newlimit)) - Change page range limit
  SB(29,(8,delta)==>c,(8,newlimit))   - Change page limit (big)
  SB(30==>c,(8,qty))                  - Query pages available (big)
 
  SB(33==>0;SBn)                    - Preclude destroy
  SB(34==>0;SBn)                    - Preclude query
  SB(35==>0;SBn)                    - Preclude destroy & query
  SB(36==>0;SBn)                    - Preclude change limits
  SB(37==>0;SBn)                    - Preclude change limits & destroy
  SB(38==>0;SBn)                    - Preclude change limits & query
  SB(39==>0;SBn)                    - Preclude chg lims, dstry, & query
 
  SB(65==>c,(8,nbuys),(8,nsells),(8,pbuys),(8,psells))
                                    - Query statistics (big)
  SB(66;SB2==>c)                    - Test space bank
  SB(67==>;SBn)                     - Create sub bank
  {ni}SB(68;k==>c)                      - Test zap effects
 
  SB(kt ==> 0x0C)                   - Alleged type
  SB(kt+4 ==> c)                    - Destroy
  SB(64 ==> c)                      - Destroy
 
  SBT(0;SB==>c)                     - Test space bank
  sbT(1,(16,limit);SB==>c;SBn)      - Create range limited bank
  SBT(2;SB==>c;SBn)                 - Create unlimited bank
  SBT(5,(4,nl),(4,pl);SB==>c;SBn)   - Create space limited bank
  SBT(kt ==> 0x40C)                 - Alleged type
 
********************************************************************/
 
#include "kktypes.h"
#include "keykos.h"
JUMPBUF;
#include "dc.h"
#include "sb.h"
#include "sbt.h"
#include "node.h"
#include "page.h"
#include "domain.h"
#include <string.h>
#include "lli.h"
#include "discrim.h"
#define ALL_ONES 0xffffffffu

void crash(char*);       /* Prototype for the crash routine */
 
char title[] = "SBC     ";
 
LLI llione = {0,1};
 
/********************************************************************
 
Initial state - To be set up by womb macros or equilivant
 
Key slots set up -
     dom, returner, domcreator, memnode, storage_node, nrange, & prange
 
The memory node slots contain -
     slot 0   - Structure to define program code, statics, and stack
     NODEMAP0 - A R/W page key to an unshared all zero page
     PAGEMAP0 - A R/W page key to a different unshared all zero page
     SBDDATA  - A R/W page key to yet another unshared all zero page
 
The registers, psw etc. contain those values which will permit the
     compiler output to run directly without any additional KeyTECH
     initialization. (KeyTECH initialization tends to try to call
     a space bank for space.)
********************************************************************/
 
     KEY temp1        = 0;
     KEY temp2        = 1;
     KEY caller       = 2;
     KEY dom          = 3;
     KEY returner     = 4;
     KEY gtemp2       = 5;
     KEY domcreator   = 6;
     KEY storage_node = 7;    /* Slots allocated as follows */
#define SN_SHIVA            0    /* Destroyer helper resume key */
#define SN_SHIVA_FIRST     14    /* First waiting for SHIVA */
#define SN_SHIVA_LAST      15    /* Last waiting for SHIVA */
     KEY k1           = 8;
     KEY k2           = 9;
     KEY k3           = 10;
     KEY memnode      = 11;   /* See memory tree and node below */
     KEY fen          = 12;   /* Receives red front end node key */
     KEY gtemp1       = 13;
     KEY prange       = 14;   /* Page range key */
     KEY nrange       = 15;   /* Node range key */
 
 
/* Define the memory tree and node - Must be set up before entry */
   /* Each segment is 1 megabyte, node is black lss=5 */
 
/* Code 0-0FFFFF - The program storage */
#define NODEMAP0  0x100000u  /* First meg of the node alloction map */
                             /*   The head of the segment node tree */
#define NODEMAP0SLOT 1       /* Memnode slot for node alloction map */
#define NODEMAPP  0x200000u  /* Some additional meg of node map */
#define NODE_MAP_SLOT 2
#define PAGEMAP0  0x300000u  /* First meg of the page alloction map */
                             /*   The head of the segment node tree */
#define PAGEMAP0SLOT 3       /* Memnode slot for page alloction map */
#define PAGEMAPP  0x400000u  /* Some additional meg of page map */
#define PAGE_MAP_SLOT 4
#define GUARD     0x500000u  /* Data page(s) from front end node */
#define MEM_GUARDKEY  5
#define ADDGUARDW 0x600000u  /* Additional window for GUARD data */
#define MEM_ADDGUARD  6
#define SBDDATA   0x700000u  /* The array of data associated with */
                             /* each space bank containing limit and */
                             /* subrange information */
#define MEM_SBDDATA   7
#define SBDW1     0x800000u  /*  Windows over the limit data array */
#define MEM_SBDW1     8
#define SBDW2     0x900000u
#define MEM_SBDW2     9
#define SBDW3     0xA00000u
#define MEM_SBDW3     0xA
#define SBDW4     0xB00000u
#define MEM_SBDW4     0xB
#define SBDW5     0xC00000u
#define MEM_SBDW5     0xC
#define SBDW6     0xD00000u
#define MEM_SBDW6     0xD
#define SBDW7     0xE00000u
#define MEM_SBDW7     0xE
/*                0xf00000u      The stack page */
 
 
/* Define the data bytes of start keys to this domain */
 
#define SB_DATABYTE     0
#define SBT_DATABYTE    1
#define SBINT_DATABYTE  2
 
 
/* Define the type codes passed to subroutines */
 
#define PAGE            0
#define NODE            1
 
 
/* Define the data byte in the node key to the red segment front end */
 
#define NO_DESTROY_DB       0x80
#define NO_QUERY_DB         0x20
#define NO_CHANGE_LIMITS_DB 0x10
 
 
/* Define the slots in the red segment front end node */
 
#define FEN_GuardKey       0
#define FEN_Caller         8      /* Resume key to notify by shiva */
#define FEN_Parm2          9
#define FEN_ForwardKey    10      /* Initially a key to self */
#define FEN_BackwardKey   11      /* ditto */
#define FEN_DescendentKey 12      /* Initially DK(0) */
#define FEN_AncestorKey   13
#define FEN_KeeperKey     14
#define FEN_FormatKey     15
 
 
/* Define the format of the "Guard segment" for each space bank */
 
#define T0GUARDSIZE 500
   struct GS {
     uint32 segid, index;  /* segment ID & index in sbd struct */
     uchar npp;             /* node presence presence bits */
     uchar ppp;             /* page presence presence bits */
     uint32 node_cursor;   /* next node CDA to try to allocate */
     uint32 page_cursor;   /* next node CDA to try to allocate */
 
              /* The following fields are only used if npp is zero */
     uint32 first_node;   /* CDA of first bit in node_guard */
                          /* Must be a multiple of 32 */
     uint32 node_guard[T0GUARDSIZE];
 
              /* The following fields are only used if ppp is zero */
     uint32 first_page;   /* CDA of first bit in page_guard */
                          /* Must be a multiple of 32 */
     uint32 page_guard[T0GUARDSIZE];
   };
 
/* If GS.npp is not zero then there are up to 4 pages of guard page
   presence pages. The first one is present if the 0x8 bit is one,
   the second if the 0x4 bit is one etc.  Each of these pages describes
   the presence of the actual guard pages one bit/page.         */
 
#define GPP (uint32 *)(0x1000+GUARD) /* Guard's 4 page presence pages */
#define GPP_SLOT 1
#define GNP (uint32 *)(0x5000+GUARD) /* Guard's 4 node presence pages */
#define GNP_SLOT 5
 
 
/* Define the information associated with each space bank */
 
   union sbd {                /* Space Bank Data */
      struct {                  /* When element is allocated */
         uint32 node_lower_limit; /* For this bank only */
         uint32 node_upper_limit;
         uint32 page_lower_limit;
         uint32 page_upper_limit;
         LLI node_limit;
         LLI page_limit;
         LLI nodes_allocated;
         LLI node_sells;
         LLI pages_allocated;
         LLI page_sells;
         uint32 ancestor_segid;  /* Segment ID and index of the sbd */
         uint32 ancestor_index;  /* for the immediate space bank    */
                                 /* ancestor.  (The sbd at (0,0) is */
                                 /* primordial and has invalid      */
                                 /* ancestor pointers               */
      } a;
      struct {                  /* When element is free */
         uint32 next_segmentid;    /* Segment id of next free element */
         uint32 next_index;        /* Index of next free element */
      } f;
   };
#define SBDPERSEGMENT (0x100000 / sizeof (union sbd))
 
 
/* Define the structure used to control the allocation arrays */
 
   struct mad {                /* Master allocation data */
     uint32 cursor;            /* Cursor for creating new sub-banks */
     uint32 *first_meg;        /* First meg of the map */
     uint32 *other_meg;        /* Some other meg of the map */
     uint32 meg_number;        /* Number of meg in other_meg */
     uint32 slot1, slot2;      /* Start of Node_WriteData parameter */
     Node_KeyData windowkey;     /* Window key for movable window */
     uint32 presence[4096];  /* Page presence bits for alloc map */
   };
 
 
/* Define the working storage of the space bank */
 
     uint32 oc;                   /* The caller's order code */
     SINT16 databyte;             /* Databyte of start key */
     struct instr_s {             /* The string passed by callers */
       union {
         struct SB_Limits limits;
         struct SB_Values values;
         struct SB_Statistics statistics;
         struct SB_SmallChangeValue smalldelta;
         struct SB_ChangeValue delta;
         struct SBT_Limits sbtlimits;
         struct SBT_Subrange subrange;
         uint32 first_cda;
       } u;
     } instring;
     long actlen;
 
     struct mad node_alloc = {0,
                      (uint32*)NODEMAP0,
                      (uint32*)NODEMAPP,
                      0,                 /* Will cause window fill */
                      NODE_MAP_SLOT,
                      NODE_MAP_SLOT,
                      WindowM(0,0,NODEMAP0>>20,0,0),
                      {0x80000000ul}};   /* First page starts valid */
     uint32 global_node_cursor = 0;
 
     struct mad page_alloc = {0,
                      (uint32*)PAGEMAP0,
                      (uint32*)PAGEMAPP,
                      0,                 /* Will cause window fill */
                      PAGE_MAP_SLOT,
                      PAGE_MAP_SLOT,
                      WindowM(0,0,PAGEMAP0>>20,0,0),
                      {0x80000000ul}};   /* First page starts valid */
     uint32 global_page_cursor = 0;
 
/* Data to control windowing of Space Bank Data (sbd) entries */
 
     struct sbdmapctlelement {
        uint32 resident_id;
        union sbd *first;
        int slot;
        int age;
     };
#define SBDMAPCTLSIZE 8
     struct sbdmapctlelement sbdmapctl[SBDMAPCTLSIZE] =
             {{ALL_ONES, (union sbd *)SBDW1, MEM_SBDW1, 0},
              {ALL_ONES, (union sbd *)SBDW2, MEM_SBDW2, 0},
              {ALL_ONES, (union sbd *)SBDW3, MEM_SBDW3, 0},
              {ALL_ONES, (union sbd *)SBDW4, MEM_SBDW4, 0},
              {ALL_ONES, (union sbd *)SBDW5, MEM_SBDW5, 0},
              {ALL_ONES, (union sbd *)SBDW6, MEM_SBDW6, 0},
              {ALL_ONES, (union sbd *)SBDW7, MEM_SBDW7, 0}};
     int current_age;
 
/* Data to control allocation of Space Bank Data (sbd) entries */
 
     uint32 sbd_free_segment = 0;
     uint32 sbd_free_index = 0;
     uint32 high_sbd_segment = 0;
     uint32 high_sbd_index = 0;    /* sbd 0,0 is for prime bank */
 
 
/* Data to remember pages and nodes allocated for space bank use */
 
#define MAXLOCALPAGES 8
#define MAXLOCALNODES (5*MAXLOCALPAGES)
     int numlocalnodes = 0;          /* entries in localnodes list */
     uint32 localnodes[MAXLOCALNODES]; /* used but not allocated */
     int numlocalpages = 0;          /* entries in localpages list */
     uint32 localpages[MAXLOCALPAGES]; /* used but not allocated */
     uint32 localcdas[MAXLOCALPAGES];  /* a cda that each local page
                                         has the allocation bit for */
 
 
/* Data to remember whether we have a key to the Shiva domain */
 
     int shiva_here = 0;
 
 
/* Prototypes of internal routines */
 
 
void destroy_never_returned(
   KEY,               /* Slot holding key to destroy */
   int);              /* NODE for node, PAGE for page */
 
void make_limited_key(
   int mask);                 /* More limits for the new key */
 
void severlocals(
   struct mad *);             /* Allocation data structure */
 
void free_guarded(
    uint32 *,                 /* Bitmap of allocated cdas to free */
    uint32,                   /* CDA of bit zero in the map */
    int,                      /* Size of the map in uint32s */
    int);                     /* NODE=node cdas, PAGE=page cdas */
 
void destroy_unguarded_node(
   KEY,
   uint32);
 
void destroy_unguarded_page(
   KEY,
   uint32);
 
int old_range_highest(
   uchar *,                        /* Current high CDA */
   KEY);
 
void destroy_this_bank();
 
uint32 create_bank_for_sbt(
   KEY,                /* The slot holding the superior bank key */
   KEY);               /* The slot to receive the new bank key */
 
int check_node_allocation(
   uint32 *,           /* Place for the effective lower range limit */
   uint32 *);          /* Place for the effective upper range limit */
 
int check_page_allocation(
   uint32 *,           /* Place for the effective lower range limit */
   uint32 *);          /* Place for the effective upper range limit */
 
void change_range_limit(
   int,                /* NODE for node, PAGE for page */
   struct SB_Limits*); /* The new limits to impose */
 
int change_limit(
   int,                /* NODE for node, PAGE for page */
   LLI *,              /* The change value */
   uint32);            /* The over/underflow mask */
 
uint32 calculate_available(
   int);               /* NODE for node, PAGE for page */
 
struct SB_Statistics calculate_short_statistics(
   int);               /* NODE for node, PAGE for page */
 
uint32 next_t1guarded_cda(
   uint32,             /* CDA to map */
   uint32,             /* Last cda in subrange */
   char,               /* The presence presence bits */
   uint32 *);          /* Pointer to the presences pages */
 
void deallocate(
    uint32,
    struct mad *);
 
int not_guarding_node(
   KEY,                 /* Key to test */
   uint32 *,            /* Pointer to place for CDA */
   int);                /* 1 to reset the guard bit, 0 not to */
 
int not_guarding_page(
   KEY,                 /* Key to test */
   uint32 *,            /* Pointer to place for CDA */
   int);                /* 1 to reset the guard bit, 0 not to */
 
union sbd *map_bank_data(
    uint32, uint32);    /* segment ID and index in segment */
 
sint32 findzerobit(
    sint32, sint32,     /* Start and end + 1 bit indexes */
    uint32*);           /* Pointer to map */
 
sint32 findonebit(
    sint32, sint32,     /* Start and end + 1 bit indexes */
    uint32*);           /* Pointer to map */
 
uint32 next_available(
    uint32,             /* First CDA in the guard range */
    uint32,             /* Last+1 CDA to try to allocate */
    KEY,                /* Slot to receive the allocated object */
    KEY,                /* Range key to use in allocation */
    struct mad*);       /* Allocation data structure */
 
uint32 alloc_next(
    uint32,             /* First CDA in the guard range */
    uint32,             /* Last+1 CDA to try to allocate */
    KEY,                /* Slot to receive the allocated object */
    KEY,                /* Range key to use in allocation */
    struct mad*);       /* Allocation data structure */
 
uint32 *map_alloc_page(
    uint32,             /* CDA to map */
    struct mad*);       /* Allocation data structure */
 
uint32 *map_guard_word(
   uint32,              /* CDA to map */
   int);                /* NODE for node, PAGE for page */
 
int getinternalnode(    /* Get a node for internal use */
   KEY);                /* Slot to return key in */
 
int getinternalpage(    /* Get a page for internal use */
   uint32,              /* a CDA that page will act as alloc map for */
   KEY);                /* Slot to return key in */
 
int getinternalkey(     /* Get a page or node for internal use */
   KEY,                 /* slot to return key in */
   uint32*,             /* list of local cdas to record/test in */
   int*,                /* number of entries in that list */
   struct mad*,         /* master allocation data */
   KEY);                /* range key to use */

int allocatet0(
   uint32 *cursor,     /* First CDA to try to allocate */
   uint32 first,       /* First CDA in the guard range */
   uint32 last,        /* Last CDA+1 to try */
   uint32 *guard,      /* Pointer to the guard array */
   uint32 *fgcda,      /* The first CDA guarded by the guard array */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc,  /* Allocation data structure */
   int not_1st);       /* Non-zero if fgcda must remain fixed */
 
int allocatet1(
   uchar *pp,          /* Presence presence bits */
   uint32 *cursor,     /* First CDA to try to allocate */
   uint32 first,       /* First CDA in the limit range */
   uint32 last,        /* Last CDA+1 in the limit range */
   uint32 *ppages,     /* Pointer to the presence pages */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc); /* Allocation data structure */

int makenewkey(
   uint32 cda,         /* CDA to make a key for */
   uint32 *next,       /* next CDA to try iff allocation failure */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range);         /* Range key to use in allocation */
 
   int bootwomb=0;
   int stacksiz=4096;
 
void factory()             /* Main space bank */
{
   uint32 rc;                   /* The return code for the caller */
   struct SB_Statistics short_stats;
 
   {  union sbd *b = map_bank_data(0,0);  /* Set up prime bank's sbd */
      b->a.node_upper_limit = ALL_ONES - 1;
      b->a.page_upper_limit = ALL_ONES - 1;
      b->a.node_limit.low = ALL_ONES;
      b->a.page_limit.low = ALL_ONES;
      b->a.ancestor_segid = -1;
      b->a.ancestor_index = -1;
   }
   LDEXBL (caller,0);
   for(;;) {
      LDENBL OCTO(oc) KEYSTO(k1,k2,fen,caller) DBTO(databyte)
           STRUCTTO(instring, sizeof instring, actlen);
      RETJUMP();
 
      switch (databyte) {
       case SB_DATABYTE:        /* The space bank */
 
         /* Make the guard map addressable */
         KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO(k3);
         KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k3);
 
         if (oc >= KT) {
            if (oc == KT) {
               LDEXBL (returner,0x0C) KEYSFROM (,,,caller);
       break;
            }
            else if (oc == KT+4) {
               if (test_rights(NO_DESTROY_DB)) {
                  LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
               } else {
                  destroy_this_bank();
                  LDEXBL (returner, 0);     /* N.B. caller is DK0 */
               }
            }
            else LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
       break;
         }
         switch (oc) {
          case SB_CreateNode:
            rc = create_node(k1);
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_DestroyNode:
            rc = destroy_node(k1);
            LDEXBL (returner, rc) KEYSFROM (,,,caller);
          break;
          case SB_SeverNode:
            rc = sever_node(k1);
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_QueryNodeSpace:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               rc = calculate_available(NODE);
               LDEXBL (returner, 0) CHARFROM (&rc,4)
                      KEYSFROM (,,,caller);
            }
          break;
          case SB_QueryNodeStatistics:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               short_stats = calculate_short_statistics(NODE);
               LDEXBL (returner,0) STRUCTFROM (short_stats)
                      KEYSFROM (,,,caller);
            }
          break;
          case SB_CreateTwoNodes:
            rc = create_node(k1);
            if (0 == rc) {
               rc = create_node(k2);
               if (0 != rc) destroy_never_returned(k1, NODE);
            }
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,k2,,caller);
          break;
          case SB_CreateThreeNodes:
            rc = create_node(k1);
            if (0 == rc) {
               rc = create_node(k2);
               if (0 != rc) destroy_never_returned(k1, NODE);
               else {
                  rc = create_node(k3);
                  if (0 != rc) {
                     destroy_never_returned(k1, NODE);
                     destroy_never_returned(k2, NODE);
                  }
               }
            }
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,k2,k3,caller);
          break;
          case SB_DestroyTwoNodes:
            rc = destroy_node(k1);
            rc += 2*destroy_node(k2);
            LDEXBL (returner, rc) KEYSFROM (,,,caller);
          break;
          case SB_DestroyThreeNodes:
              /* Get last key */
            KC (fen, Node_Fetch + FEN_Parm2) KEYSTO(k3);
            rc = destroy_node(k1);
            rc += 2*destroy_node(k2);
            rc += 4*destroy_node(k3);
            LDEXBL (returner, rc) KEYSFROM (,,,caller);
          break;
          case SB_ChangeSmallNodeLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM (,,,caller);
            } else {
               LLI newlimit;
               newlimit.low = instring.u.smalldelta.Delta;
               if (instring.u.smalldelta.Delta < 0)
                      newlimit.hi = ALL_ONES;
               else newlimit.hi = 0;
               rc = change_limit(NODE, &newlimit, ALL_ONES);
               LDEXBL (returner, rc) STRUCTFROM (newlimit.low, 4)
                        KEYSFROM (,,,caller);
            }
          break;
          case SB_SetNodeRangeLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM (,,,caller);
            } else {
               if (llicmp(&instring.u.limits.Lower,
                          &instring.u.limits.Upper) > 0) {
                  LDEXBL (returner, 2) KEYSFROM (,,,caller);
               } else {
                  change_range_limit(NODE, &instring.u.limits);
                  LDEXBL (returner, 0) KEYSFROM (,,,caller);
               }
            }
          break;
          case SB_ChangeNodeLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM (,,,caller);
            } else {
               LLI *newlimit;
               newlimit = &instring.u.delta.Delta;
               rc = change_limit(NODE, newlimit, 0xffff0000ul);
               LDEXBL (returner, rc) STRUCTFROM (*newlimit, 8)
                        KEYSFROM (,,,caller);
            }
          break;
          case SB_QueryNodesAvailable:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               instring.u.delta.Delta.low = calculate_available(NODE);
               instring.u.delta.Delta.hi = 0;
               LDEXBL (returner, 0) KEYSFROM (,,,caller)
                      STRUCTFROM (instring.u.delta.Delta);
            }
          break;
          case SB_CreatePage:
            rc = create_page(k1);
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_DestroyPage:
            rc = destroy_page(k1);
            LDEXBL (returner, rc) KEYSFROM (,,,caller);
          break;
          case SB_SeverPage:
            rc = sever_page(k1);
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_QueryPageSpace:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               rc = calculate_available(PAGE);
               LDEXBL (returner, 0) CHARFROM (&rc,4)
                        KEYSFROM (,,,caller);
            }
          break;
          case SB_QueryPageStatistics:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               short_stats = calculate_short_statistics(PAGE);
               LDEXBL (returner, 0) STRUCTFROM (short_stats)
                       KEYSFROM (,,,caller);
            }
          break;
          case SB_CreateTwoPages:
            rc = create_page(k1);
            if (0 == rc) {
               rc = create_page(k2);
               if (0 != rc) destroy_never_returned(k1, PAGE);
            }
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,k2,,caller);
          break;
          case SB_CreateThreePages:
            rc = create_page(k1);
            if (0 == rc) {
               rc = create_page(k2);
               if (0 != rc) destroy_never_returned(k1, PAGE);
               else {
                  rc = create_page(k3);
                  if (0 != rc) {
                     destroy_never_returned(k1, PAGE);
                     destroy_never_returned(k2, PAGE);
                  }
               }
            }
            if (rc) LDEXBL (returner, rc) KEYSFROM (,,,caller);
            else LDEXBL (returner, 0) KEYSFROM (k1,k2,k3,caller);
          break;
          case SB_DestroyTwoPages:
            rc = destroy_page(k1);
            rc += 2*destroy_page(k2);
            LDEXBL (returner, rc) KEYSFROM(,,,caller);
          break;
          case SB_DestroyThreePages:
              /* Get last key */
            KC (fen, Node_Fetch + FEN_Parm2) KEYSTO(k3);
            rc = destroy_page(k1);
            rc += 2*destroy_page(k2);
            rc += 4*destroy_page(k3);
            LDEXBL (returner, rc) KEYSFROM(,,,caller);
          break;
          case SB_ChangeSmallPageLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM(,,,caller);
            } else {
               LLI newlimit;
               newlimit.low = instring.u.smalldelta.Delta;
               if (instring.u.smalldelta.Delta < 0)
                      newlimit.hi = ALL_ONES;
               else newlimit.hi = 0;
               rc = change_limit(PAGE, &newlimit, ALL_ONES);
               LDEXBL (returner, rc) STRUCTFROM (newlimit.low, 4)
                        KEYSFROM (,,,caller);
            }
          break;
          case SB_SetPageRangeLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM(,,,caller);
            } else {
               if (llicmp(&instring.u.limits.Lower,
                          &instring.u.limits.Upper) > 0) {
                  LDEXBL (returner, 2) KEYSFROM(,,,caller);
               } else {
                  change_range_limit(PAGE, &instring.u.limits);
                  LDEXBL (returner, 0) KEYSFROM(,,,caller);
               }
            }
          break;
          case SB_ChangePageLimit:
            if (test_rights(NO_CHANGE_LIMITS_DB)) {
               LDEXBL (returner, 3) KEYSFROM(,,,caller);
            } else {
               LLI *newlimit;
               newlimit = &instring.u.delta.Delta;
               rc = change_limit(PAGE, newlimit, 0xffff0000ul);
               LDEXBL (returner, rc) STRUCTFROM (*newlimit, 8)
                        KEYSFROM (,,,caller);
            }
          break;
          case SB_QueryPagesAvailable:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               instring.u.delta.Delta.low = calculate_available(PAGE);
               instring.u.delta.Delta.hi = 0;
               LDEXBL (returner, 0) KEYSFROM (,,,caller)
                      STRUCTFROM (instring.u.delta.Delta);
            }
          break;
          case SB_ForbidDestroy:
             make_limited_key(NO_DESTROY_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          case SB_ForbidQuery:
             make_limited_key(NO_QUERY_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_ForbidDestroyAndQuery:
             make_limited_key(NO_DESTROY_DB | NO_QUERY_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_ForbidChangeLimits:
             make_limited_key(NO_CHANGE_LIMITS_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_ForbidDestroyAndChangeLimits:
             make_limited_key(NO_DESTROY_DB | NO_CHANGE_LIMITS_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_ForbidQueryAndChangeLimits:
             make_limited_key(NO_QUERY_DB | NO_CHANGE_LIMITS_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_ForbidDestroyQueryAndChangeLimits:
             make_limited_key(NO_DESTROY_DB | NO_QUERY_DB |
                              NO_CHANGE_LIMITS_DB);
             LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
          break;
          case SB_DestroyBankAndSpace:
            if (test_rights(NO_DESTROY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               destroy_this_bank();
               LDEXBL (returner, 0);     /* N.B. caller is DK0 */
            }
          break;
          case SB_QueryStatistics:
            if (test_rights(NO_QUERY_DB)) {
               LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
            } else {
               struct SB_FullStatistics stat;
               struct GS *g = (struct GS*)GUARD;
               union sbd *b = map_bank_data(g->segid, g->index);
 
               stat.NodeCreates = b->a.nodes_allocated;
               stat.NodeDestroys = b->a.node_sells;
               lliadd(&stat.NodeCreates, &stat.NodeDestroys);
 
               stat.PageCreates = b->a.pages_allocated;
               stat.PageDestroys = b->a.page_sells;
               lliadd(&stat.PageCreates, &stat.PageDestroys);
 
               LDEXBL (returner, 0) KEYSFROM (,,,caller)
                      STRUCTFROM (stat);
            }
          break;
          case SB_VerifyBank:
            {
               uint32 segdb;
               KC (domcreator, DC_IdentifySegment) KEYSFROM (k1)
                           RCTO (segdb);
               if (segdb > 255) {
                  rc = ALL_ONES;
               } else {
                  rc = 0;
                  if (segdb&NO_DESTROY_DB)       rc += 1;
                  if (segdb&NO_QUERY_DB)         rc += 2;
                  if (segdb&NO_CHANGE_LIMITS_DB) rc += 4;
               }
               LDEXBL (returner, rc) KEYSFROM (,,,caller);
            }
          break;
          case SB_CreateBank:
            {
               uint32 db;           /* Query limit */
               uchar dbc;
               KC (fen, Node_DataByte) RCTO (db);
               rc = create_subbank();
               if (0 == rc) {             /* Propagate query limits */
                  dbc = db & NO_QUERY_DB;
                  KC (fen, Node_MakeSegmentKey) STRUCTFROM (dbc,1)
                          KEYSTO (k1);
                  LDEXBL (returner, 0) KEYSFROM (k1,,,caller);
               } else {
                  LDEXBL (returner, 1) KEYSFROM (,,,caller);
               }
            }
          break;
          default:
            LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
          break;
         }  /* end switch on order code for databyte SB */
       break;
 
       case SBT_DATABYTE:       /* The space bank transformer */
         if (oc >= KT) {
            if (oc == KT) {
               LDEXBL (returner, 0x40C) KEYSFROM (,,,caller);
            }
            else LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
       break;
         }
         switch (oc) {
          uint32 segdb;
          case SBT_Verify:
            KC (domcreator, DC_IdentifySegment) KEYSFROM (k1)
                        RCTO (segdb);
            if (segdb > 255) {
               rc = ALL_ONES;
            } else {
               rc = 0;
               if (segdb&NO_DESTROY_DB)       rc += 1;
               if (segdb&NO_QUERY_DB)         rc += 2;
               if (segdb&NO_CHANGE_LIMITS_DB) rc += 4;
            }
            LDEXBL (returner, rc) KEYSFROM (,,,caller);
          break;
          case SBT_CreateSubrange:
            rc = create_bank_for_sbt(k1, k2);  /* Make a space bank */
            if (0 == rc) {
               struct GS *g = (struct GS*)GUARD;
               union sbd *b = map_bank_data(g->segid, g->index);
               if (actlen >= 4)
                  b->a.node_lower_limit =
                           instring.u.subrange.LowerNodeLimit;
               if (actlen >= 8)
                  b->a.node_upper_limit =
                           instring.u.subrange.UpperNodeLimit;
               if (actlen >= 12)
                  b->a.page_lower_limit =
                           instring.u.subrange.LowerPageLimit;
               if (actlen >= 16)
                  b->a.page_upper_limit =
                           instring.u.subrange.UpperPageLimit;
               LDEXBL (returner, 0) KEYSFROM (k2,,,caller);
            } else {
               LDEXBL (returner, rc) KEYSFROM (,,,caller);
            }
          break;
          case SBT_Create:
            rc = create_bank_for_sbt(k1, k2);  /* Make a space bank */
            if (0 == rc) {
               LDEXBL (returner, 0) KEYSFROM (k2,,,caller);
            } else {
               LDEXBL (returner, rc) KEYSFROM (,,,caller);
            }
          break;
          case SBT_CreateLimited:
            rc = create_bank_for_sbt(k1, k2);  /* Make a space bank */
            if (0 == rc) {
               struct GS *g = (struct GS*)GUARD;
               union sbd *b = map_bank_data(g->segid, g->index);
               if (actlen >= 4) {
                  b->a.node_limit.hi = 0;
                  b->a.node_limit.low =
                           instring.u.sbtlimits.NodeLimit;
               }
               if (actlen >= 8) {
                  b->a.page_limit.hi = 0;
                  b->a.page_limit.low =
                           instring.u.sbtlimits.PageLimit;
               }
               LDEXBL (returner, 0) KEYSFROM (k2,,,caller);
            } else {
               LDEXBL (returner, rc) KEYSFROM (,,,caller);
            }
          break;
          default:
            LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
         }  /* end switch on order code for databyte SBT */
       break;
 
       case SBINT_DATABYTE:     /* The internal entry from shiva */
         if (oc >= KT) {
            LDEXBL (caller, KT+2);
       break;
         }
         switch (oc) {
            struct GS *g;
            uint32 *p;
            uint32 cda;
          case 0:               /* Wait for work */
            KC (storage_node, Node_Fetch+SN_SHIVA_FIRST) KEYSTO (k1);
            KC (k1,KT) RCTO (rc);
            if (KT+1 == rc) {
               KC (storage_node, Node_Swap+SN_SHIVA) KEYSFROM (caller)
                       KEYSTO (,caller);  /* Save and set caller DK0 */
               shiva_here = 1;            /* Remember we have key */
               LDEXBL (caller, 0);  /* Return to DK0 */
            } else {
               KC (k1, Node_Fetch+FEN_ForwardKey) KEYSTO (k2);
                  /* Dequeue node */
               KC (storage_node, Node_Swap+SN_SHIVA_FIRST)
                       KEYSFROM (k2);
               LDEXBL (caller, 0) KEYSFROM (k1);
            }
          break;
          case 1:               /* Free type zero guarded pages */
                                /* Parameter is T0GuardPageKey */
            KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k1);
 
            g = (struct GS*)GUARD;
            free_guarded(g->page_guard, g->first_page,
                         T0GUARDSIZE, PAGE);
            LDEXBL (caller, 0);
          break;
          case 2:               /* Free type zero guarded nodes */
                                /* Parameter is T0GuardPageKey */
            KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k1);
 
            g = (struct GS*)GUARD;
            free_guarded(g->node_guard, g->first_node,
                         T0GUARDSIZE, NODE);
            LDEXBL (caller, 0);
          break;
          case 3:               /* Free a page of guarded pages */
                                /* Destroy guard page when done */
                                /* Parameters are PageKey & firstcda */
            KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k1);
 
            p = (uint32*)GUARD;
            free_guarded(p, instring.u.first_cda, 1024, PAGE);
            KC (prange, 1) KEYSFROM (k1) STRUCTTO(cda);
            destroy_unguarded_page(k1, cda);
            LDEXBL (caller, 0);
          break;
          case 4:               /* Free a page of guarded nodes */
                                /* Destroy guard page when done */
                                /* Parameters are PageKey & firstcda */
            KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k1);
 
            p = (uint32*)GUARD;
            free_guarded(p, instring.u.first_cda, 1024, NODE);
            KC (prange, 1) KEYSFROM (k1) STRUCTTO(cda);
            destroy_unguarded_page(k1, cda);
            LDEXBL (caller, 0);
          break;
          case 5:               /* Free a page */
            KC (prange, 1) KEYSFROM (k1) STRUCTTO(cda);
            destroy_unguarded_page(k1, cda);
            LDEXBL (caller, 0);
          break;
          case 6:               /* Free a node */
            KC (nrange, 1) KEYSFROM (k1) STRUCTTO(cda);
            destroy_unguarded_node(k1, cda);
            LDEXBL (caller, 0);
          break;
          default:
            LDEXBL (returner, KT+2) KEYSFROM (,,,caller);
         }  /* end switch on order code for databyte SBINT */
       break;
       default:
          KC (returner,databyte);  /* cause crash with unknown databyte */
       break;
 
      }  /* end switch on databyte */
   }  /* end for loop */
}  /* end sb */
 
 
/******************************************************************
test_rights - Test the rights bit passed against the node key databyte
   Input:
      mask    - The rights bit to test
   Return codes:
      0 - the bit is off in the data byte
      other - the bit is on in the data byte
*******************************************************************/
int test_rights(
   int mask)
{  uint32 db;
   KC (fen, Node_DataByte) RCTO (db);
   return (db & mask);
}  /* end test_rights */
 
 
/******************************************************************
make_limited_key - Make key for bank with rights limited
   Input:
      mask    - The rights bit to limit
   Output:
      k1 holds the new key
*******************************************************************/
void make_limited_key(
   int mask)
{  uint32 db;
   uchar cdb;
      /* Get current rights limit */
   KC (fen, Node_DataByte) RCTO (db);
   cdb = db | mask;   /* Merge new limits */
   KC (fen, Node_MakeSegmentKey) STRUCTFROM (cdb, 1) KEYSTO (k1);
}  /* end make_limited_key */
 
 
/******************************************************************
ensure_sbd_pages - Ensure pages in sbd segment for segid and index
   Input:
      segid   - The segment ID of the segment that holds the sbd
      index   - The index to the sbd within the segment
   Return codes:
      0 - Pages could not be obtained
      1 - Segment has pages defined
*******************************************************************/
int ensure_sbd_pages(
   uint32 segid,
   uint32 index)
{
   uint32 rc;
   int lss;
 
   KC (dom, Domain_GetKey+memnode) KEYSTO (gtemp1);
   if(!grow_tree(segid<<8 | index * sizeof (union sbd) >> 12,
                    MEM_SBDDATA))
        return 0;

   if(4096 - ((index * sizeof(union sbd)) & 0xFFF) < 
        sizeof(union sbd)) {  /* not fit on page */
     KC (dom, Domain_GetKey+memnode) KEYSTO (gtemp1);
     return grow_tree(segid<<8 | (index+1) * sizeof(union sbd) >> 12,
                    MEM_SBDDATA);
   }
   return 1;
}  /* end ensure_sbd_pages */
 
 
/******************************************************************
create_subbank - Create a bank subordinate to the bank-node in fen
   Input (not through the parameter list):
      fen        - Node key to the superior bank's front end node
                   That banks guard page is addressable through GUARD
   Output (not through the parameter list):
      fen        - Node key to the new bank's front end node (rc=0)
      fen        - destroyed (rc=1)
   Return codes:
      0 - New bank created, its guard segment is mapped
      1 - Not enough space for new bank
*******************************************************************/
int create_subbank()
{
   struct GS *g = (struct GS*)GUARD;
   uint32 old_segid = g->segid;
   uint32 old_index = g->index;
   union sbd *b;
   uint32 rc;
   uint32 nodecda, pagecda;
   uint32 cda, first, last;  /* CDA values for starting the subbank */
 
      /* Get a node for the space bank front end */
   nodecda = alloc_next(0, ALL_ONES, gtemp1, nrange, &node_alloc);
   if (ALL_ONES == nodecda) return 1;
 
      /* Get a page for the space bank guard */
   pagecda = alloc_next(0, ALL_ONES, gtemp2, prange, &page_alloc);
   if (ALL_ONES == pagecda) {
      destroy_unguarded_node(gtemp1, nodecda);
      return 1;
   }
 
      /* Install guard page into front end node */
   KC (gtemp1, Node_Swap + FEN_GuardKey) KEYSFROM (gtemp2);
      /* Make the guard map addressable */
   KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(gtemp2);
   g = (struct GS*)GUARD;
      /* Fill in rest of front end node */
   KC (fen, Node_Fetch + FEN_FormatKey) KEYSTO (gtemp2);
   KC (gtemp1, Node_Swap + FEN_FormatKey) KEYSFROM (gtemp2);
   KC (fen, Node_Fetch + FEN_KeeperKey) KEYSTO (gtemp2);
   KC (gtemp1, Node_Swap + FEN_KeeperKey) KEYSFROM (gtemp2);
   KC (gtemp1, Node_Swap + FEN_AncestorKey) KEYSFROM (fen);
 
      /* Allocate a sbd structure for this bank */
   if (sbd_free_segment | sbd_free_index) {  /* Element on free list */
      b = map_bank_data(sbd_free_segment, sbd_free_index);
      g->segid = sbd_free_segment;
      g->index = sbd_free_index;
      sbd_free_segment = b->f.next_segmentid;
      sbd_free_index = b->f.next_index;
   } else {
      if (high_sbd_index++ >= SBDPERSEGMENT) {
         high_sbd_index = 0;
         high_sbd_segment++;
      }
 
         /* Save new bank node in fen over call */
      KC (dom, Domain_GetKey + gtemp1) KEYSTO (fen);
      if (!ensure_sbd_pages(high_sbd_segment, high_sbd_index)) {
         /* Destroy the node and page and return failure to caller */
         KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO (gtemp2);
         destroy_unguarded_page(gtemp2, pagecda);
         destroy_unguarded_node(fen, nodecda);
         return 1;
      }
 
         /* Restore new bank node to gtemp1 and old one to fen */
      KC (dom, Domain_GetKey + fen) KEYSTO (gtemp1);
      KC (gtemp1, Node_Fetch + FEN_AncestorKey) KEYSTO (fen);
      b = map_bank_data(high_sbd_segment, high_sbd_index);
      g->segid = high_sbd_segment;
      g->index = high_sbd_index;
   }
 
      /* Chain bank node into space bank link structure */
   KC (fen, Node_Fetch + FEN_DescendentKey) KEYSTO (gtemp2);
   KC (gtemp2, Node_Fetch + FEN_ForwardKey) KEYSTO (temp1) RCTO (rc);
   if (rc) {          /* Ancestor has no other descendents */
      KC (gtemp1, Node_Swap + FEN_ForwardKey) KEYSFROM (gtemp1);
      KC (gtemp1, Node_Swap + FEN_BackwardKey) KEYSFROM (gtemp1);
   } else {           /* Add to descendent ring of ancestor */
      KC (gtemp2, Node_Swap + FEN_ForwardKey) KEYSFROM (gtemp1);
      KC (temp1, Node_Swap + FEN_BackwardKey) KEYSFROM (gtemp1);
      KC (gtemp1, Node_Swap + FEN_ForwardKey) KEYSFROM (temp1);
      KC (gtemp1, Node_Swap + FEN_BackwardKey) KEYSFROM (gtemp2);
   }
 
      /* Have ancestor bank point to new bank as its descendent */
   KC (fen, Node_Swap + FEN_DescendentKey) KEYSFROM (gtemp1);
 
      /* Make new bank the fen key bank */
   KC (dom, Domain_GetKey + gtemp1) KEYSTO (fen);
 
      /* Initialize the Space Bank Data */
   b->a.node_lower_limit = 0;
   b->a.node_upper_limit = ALL_ONES;
   b->a.page_lower_limit = 0;
   b->a.page_upper_limit = ALL_ONES;
   b->a.node_limit.hi = 0;
   b->a.node_limit.low = ALL_ONES;
   b->a.page_limit.hi = 0;
   b->a.page_limit.low = ALL_ONES;
   b->a.nodes_allocated.hi = b->a.nodes_allocated.low = 0;
   b->a.node_sells.hi = b->a.node_sells.low = 0;
   b->a.pages_allocated.hi = b->a.pages_allocated.low = 0;
   b->a.page_sells.hi = b->a.page_sells.low = 0;
   b->a.ancestor_segid = old_segid;
   b->a.ancestor_index = old_index;
 
      /* Set node allocation cursor */
   first = b->a.node_lower_limit;   /* Get effective limits */
   last  = b->a.node_upper_limit;
   for ( ;
         b;
         b = map_bank_data(b->a.ancestor_segid,b->a.ancestor_index)) {
      if (first < b->a.node_lower_limit) first = b->a.node_lower_limit;
      if (last  > b->a.node_upper_limit) last  = b->a.node_upper_limit;
   }
   b = map_bank_data(g->segid, g->index); /* Restore bankdata pointer */
 
   if (last-first) cda = first + global_node_cursor % (last-first);
   else cda = first;
   cda = next_available(cda, last, gtemp2, nrange, &node_alloc);
   if (ALL_ONES == cda) {
      cda = next_available(first, last, gtemp2, nrange, &node_alloc);
   }
   if (ALL_ONES != cda) g->node_cursor = cda & ~31ul;
   g->node_cursor = first & ~31ul;
 
/* The code that wraps CDA at the end of the range has not been tested */
/* and there is at least 1 bug.  At this time we will simply not advance */
/* global cursors for each sub bank keeping allocation dense            */

/*   global_node_cursor += 8000; */
      /* ensure cursor cda mounted */
/*   makenewkey(global_node_cursor, &global_node_cursor, gtemp2, nrange); */
 
      /* Set page allocation cursor */
   first = b->a.page_lower_limit;   /* Get effective limits */
   last  = b->a.page_upper_limit;
   for ( ;
         b;
         b = map_bank_data(b->a.ancestor_segid,b->a.ancestor_index)) {
      if (first < b->a.page_lower_limit) first = b->a.page_lower_limit;
      if (last  > b->a.page_upper_limit) last  = b->a.page_upper_limit;
   }
   b = map_bank_data(g->segid, g->index); /* Restore bankdata pointer */
 
   if (last-first) cda = first + global_page_cursor % (last-first);
   else cda = first;
   cda = next_available(cda, last, gtemp2, prange, &page_alloc);
   if (ALL_ONES == cda) {
      cda = next_available(first, last, gtemp2, prange, &page_alloc);
   }
   if (ALL_ONES != cda) g->page_cursor = cda & ~31ul;
   else g->page_cursor = first & ~31ul;
 
/*   global_page_cursor += 8000; */
      /* ensure cursor cda mounted */
/*   makenewkey(global_page_cursor, &global_page_cursor, gtemp2, prange); */
 
   return 0;
}  /* end create_subbank */
 
 
/******************************************************************
create_bank_for_sbt - Create a new bank subordinate to bank in slot
   Input:
      inslot     - The slot holding the superior bank key
      outslot    - The slot to receive the new bank key
   Return codes:
      3 - The input slot does not contain a space bank
      1 - No space for new bank
      0 - Success, outslot holds new bank key, its guard segment
          is mapped
*******************************************************************/
uint32 create_bank_for_sbt(
  KEY inslot,
  KEY outslot)
{
   uint32 rc, db;
   uchar dbc;
   KC (domcreator, DC_IdentifySegment) KEYSFROM (inslot)
               KEYSTO (,fen) RCTO (db);
   if (db > 255) return 3;
      /* Make the guard map addressable */
   KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO (temp1);
   KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM (temp1);
   rc = create_subbank();
   if (0 == rc) {                    /* Propagate query limits */
      dbc = db & NO_QUERY_DB;
      KC (fen, Node_MakeSegmentKey) STRUCTFROM (dbc,1) KEYSTO (outslot);
   }
   return rc;
}  /* end create_bank_for_sbt */
 
 
/******************************************************************
queue_for_shiva - Delink, sever and queue a bank for shiva
   Input:
      node       - Node key to the bank's front end node, must have
                   no descendents
      t1         - An available scratch key slot
      t2         - Another available scratch key slot
   Output:
      0          - Bank's ancestor correctly updated
      not zero   - Bank's ancestor key was DK0
*******************************************************************/
uint32 queue_for_shiva(
   KEY node,          /* Node key to bank's front end node */
   KEY t1,
   KEY t2)        /* Available work slots */
{
   uint32 rc, ourrc;
   uint32 cda;
   union sbd *b;
   struct GS *g;
 
      /* Unlink the front end node from the sibling ring */
   KC (node, Node_Fetch + FEN_BackwardKey) KEYSTO (t1);
   KC (node, Node_Fetch + FEN_ForwardKey) KEYSTO (t2);
   KC (t1, Node_Swap + FEN_ForwardKey) KEYSFROM (t2);
   KC (t2, Node_Swap + FEN_BackwardKey) KEYSFROM (t1);
      /* Ensure ancestor's descendent key remains valid */
      /* by replacing it with one of our siblings */
   KC (node, Node_Fetch + FEN_AncestorKey) KEYSTO (t1);
   KC (t1, Node_Swap + FEN_DescendentKey) KEYSFROM (t2) RCTO (ourrc);
      /* Sever the bank node so keys to that bank becomes invalid */
   KC (nrange, 1) KEYSFROM (node) STRUCTTO (cda);   /* Get CDA */
   KC (nrange, 2) KEYSFROM (node);                  /* Sever */
   KC (nrange, 0) STRUCTFROM (cda) KEYSTO (node);   /* Get new key */
 
      /* Queue node on the Shiva list */
   KC (node, Node_Swap + FEN_ForwardKey);  /* DK0 to next pointer */
   KC (storage_node, Node_Fetch + SN_SHIVA_LAST) KEYSTO (t1);
   KC (t1, Node_Swap + FEN_ForwardKey) KEYSFROM (node) RCTO (rc);
   if (rc) {           /* Last doesn't exist, therefor first doesn't */
      KC (storage_node, Node_Swap + SN_SHIVA_FIRST) KEYSFROM (node);
   }
   KC (storage_node, Node_Swap + SN_SHIVA_LAST) KEYSFROM (node);
      /* Make the guard map addressable */
   KC (node, Node_Fetch + FEN_GuardKey) KEYSTO (t1);
   KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM (t1);
   g = (struct GS*)GUARD;
      /* Free the union sbd structure */
   b = map_bank_data(g->segid, g->index);
      /* Adjust the allocated and sells on superior banks */
   {
      union sbd *bb;
      for (bb = map_bank_data(b->a.ancestor_segid,
                              b->a.ancestor_index);
           bb;
           bb = map_bank_data(bb->a.ancestor_segid,
                              bb->a.ancestor_index)) {
         llisub(&bb->a.nodes_allocated, &b->a.nodes_allocated);
         lliadd(&bb->a.node_sells, &b->a.nodes_allocated);
         llisub(&bb->a.pages_allocated, &b->a.pages_allocated);
         lliadd(&bb->a.page_sells, &b->a.pages_allocated);
      }
   }
   b->f.next_segmentid = sbd_free_segment;
   b->f.next_index     = sbd_free_index;
   sbd_free_segment = g->segid;
   sbd_free_index = g->index;
   return ourrc;
}  /* end queue_for_shiva */
 
 
/******************************************************************
destroy_this_bank - Arange to destroy a bank and its subbanks
   Input (not through the parameter list):
      fen        - Node key to the bank's front end node
   Output (not through the parameter list):
      fen        - DK0
      caller     - DK0
*******************************************************************/
void destroy_this_bank()
{
   uint32 rc;
 
      /* Ensure ancestor's descendent key remains valid */
      /* by replacing it with one of our siblings */
   KC (fen, Node_Fetch + FEN_AncestorKey) KEYSTO (temp1);
   KC (fen, Node_Fetch + FEN_ForwardKey) KEYSTO (temp2);
   KC (temp1, Node_Swap + FEN_DescendentKey) KEYSFROM (temp2);
      /* Remove key to ancestor to terminate loop below */
   KC (fen, Node_Swap + FEN_AncestorKey);
      /* Save caller in fen for shiva to fork when done */
      /* Set caller to DK0 */
   KC (fen, Node_Swap + FEN_Caller) KEYSFROM (caller) KEYSTO (,caller);
   for (;;) {            /* Sever and queue for shiva all subbanks */
                         /*   N.B must be done depth first */
         /* Start with fen and init its ancestor to DK0 */
      KC (dom, Domain_GetKey + fen) KEYSTO (temp1, gtemp1);
      for (;;) {            /* Go down desecendent chain to bottom */
         KC (temp1, Node_Fetch + FEN_DescendentKey) KEYSTO (temp2)
                 RCTO (rc);
         if (KT+1 == rc) {
            rc = queue_for_shiva(gtemp1, temp1, temp2);
      break;
         }
         KC (temp2, Node_Fetch + FEN_DescendentKey) KEYSTO (gtemp1)
                 RCTO (rc);
         if (KT+1 == rc) {
            rc = queue_for_shiva(temp1, temp2, gtemp1);
      break;
         }
         KC (gtemp1, Node_Fetch + FEN_DescendentKey) KEYSTO (temp1)
                 RCTO (rc);
         if (KT+1 == rc) {
            rc = queue_for_shiva(temp2, gtemp1, temp1);
      break;
         }
      }
      if (rc)
   break;
   }
      /* Start shiva if possible */
   if (shiva_here) {
         /* Dequeue the first entry */
      KC (storage_node, Node_Fetch + SN_SHIVA_FIRST) KEYSTO (temp2);
      KC (temp2, Node_Fetch + FEN_ForwardKey) KEYSTO (temp1);
      KC (storage_node, Node_Swap + SN_SHIVA_FIRST) KEYSFROM (temp1);
         /* Start Shiva */
      KC (storage_node, Node_Fetch + SN_SHIVA) KEYSTO (temp1);
      KFORK (temp1, 0) KEYSFROM (temp2);
      shiva_here = 0;
   }
}  /* end destroy_this_bank */
 
 
/******************************************************************
could_not_allocate - Reduce the allocated count if no CDA available
   Input:
      type   - NODE for node allocation, PAGE for page
   Return code:
      Always returns 1 (no space available)
*******************************************************************/
int could_not_allocate(
   int type)
{
   struct GS *g = (struct GS*)GUARD;
   union sbd *b;
 
      /* Undo check_node_allocation or check_page_allocation */
   for (b = map_bank_data(g->segid, g->index);
        b;
        b = map_bank_data(b->a.ancestor_segid,
                          b->a.ancestor_index)) {
      if (NODE == type) llisub(&b->a.nodes_allocated, &llione);
      else llisub(&b->a.pages_allocated, &llione);
   }
   return 1;
}  /* end destroy_this_bank */
 
 
/******************************************************************
create_node -  Create a new node and put the key in "slot"
   Input:
      slot   - Slot in which to return the new node key
   Return codes:
      0 - OK
      1 - No space available
      4 - Bank limit exceeded
*******************************************************************/
int create_node(slot)
   KEY slot;
{
   struct GS *g = (struct GS*)GUARD;
   uint32 lower, upper;    /* effective range limit */
   uint32 first, last;     /* subrange of rangelimit to try */
 
   if (!check_node_allocation(&lower, &upper)) return 4;
      /* check_node_allocation incremented number allocated */
   if (g->npp == 0) {
      union sbd *b = map_bank_data(g->segid, g->index);
      uint32 not_first = b->a.nodes_allocated.hi |
                         (b->a.nodes_allocated.low - 1);
      if (not_first) {
         if (lower < g->first_node) first = g->first_node;
         else first = lower;
         last = g->first_node + (T0GUARDSIZE<<5);
         if (last > upper + 1) last = upper + 1;
      } else {
         last = upper;
         first = lower;
      }
  first=g->first_node;  /* ..... KLUDGE for nodisk kernel */
      if (0 == allocatet0(&g->node_cursor, first, last,
                     g->node_guard, &g->first_node,
                     slot, nrange, &node_alloc, not_first))
         return 0;
      if (lower >= g->first_node && upper < g->first_node+T0GUARDSIZE)
         return could_not_allocate(NODE);
      if (convert_to_type1(NODE)) return could_not_allocate(NODE);
   }
   if (allocatet1(&g->npp, &g->node_cursor, lower, upper,
                        GNP, slot, nrange, &node_alloc))
      return could_not_allocate(NODE);
   return 0;
}
 
 
/******************************************************************
create_page(slot) Create a new page and put the key in "slot"
   Input:
      slot   - Slot in which to return the new page key
   Return codes:
      0 - OK
      1 - No space available
      4 - Bank limit exceeded
*******************************************************************/
int create_page(slot)
   KEY slot;
{
   struct GS *g = (struct GS*)GUARD;
   uint32 lower, upper;    /* effective range limit */
   uint32 first, last;     /* subrange of rangelimit to try */
 
   if (!check_page_allocation(&lower, &upper)) return 4;
      /* check_page_allocation incremented number allocated */
   if (g->ppp == 0) {
      union sbd *b = map_bank_data(g->segid, g->index);
      uint32 not_first = b->a.pages_allocated.hi |
                         (b->a.pages_allocated.low - 1);
      if (not_first) {
         if (lower < g->first_page) first = g->first_page;
         else first = lower;
         last = g->first_page + (T0GUARDSIZE<<5);
         if (last > upper + 1) last = upper + 1;
      } else {
         last = upper;
         first = lower;
      }
  first=g->first_page;  /*........ KLUDGE for nodisk kernel */
      if (0 == allocatet0(&g->page_cursor, first, last,
                     g->page_guard, &g->first_page,
                     slot, prange, &page_alloc, not_first))
         return 0;
      if (lower >= g->first_page && upper < g->first_page+T0GUARDSIZE)
         return could_not_allocate(PAGE);
      if (convert_to_type1(PAGE)) return could_not_allocate(PAGE);
   }
   if (allocatet1(&g->ppp, &g->page_cursor, lower, upper,
                        GPP, slot, prange, &page_alloc))
      return could_not_allocate(PAGE);
   return 0;
}
 
 
/******************************************************************
change_range_limit - Change the cda limits for allocation
   Input:
      type    - NODE for node, PAGE for page
      new     - New limit to set
   Return codes:
      0 - OK
      1 - Not acceptable to bank
*******************************************************************/
void change_range_limit(
   int type,
   struct SB_Limits *new)
{
   uint32 rc;
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
      /* Change input cda limits for implementation limits */
   if (new->Lower.hi) new->Lower.low = ALL_ONES;
   if (new->Upper.hi) new->Upper.low = ALL_ONES;
   if (NODE == type) {           /* Change node limits */
      b->a.node_lower_limit = new->Lower.low;
      b->a.node_upper_limit = new->Upper.low;
   } else {
      b->a.page_lower_limit = new->Lower.low;
      b->a.page_upper_limit = new->Upper.low;
   }
}
 
 
/******************************************************************
destroy_unguarded_node - Destroy node without checking guard
   Input:
      slot      - Slot holding node key to be severed
      cda       - CDA of the key
*******************************************************************/
void destroy_unguarded_node( KEY slot, uint32 cda)
{
   uint32 rc;
 
   KC (nrange,6) KEYSFROM (slot) RCTO (rc);
   if (0 != rc) {
      KC (nrange,2) KEYSFROM (slot);
      KC (nrange,0) STRUCTFROM (cda,4) KEYSTO (slot);
      KC (slot, Node_Clear);
   }
   deallocate(cda, &node_alloc); /* Turn off allocated bit */
}
 
 
/******************************************************************
destroy_node(slot) Destroy node whose key is in "slot"
   Return codes:
      0 - OK
      1 - Not acceptable to bank
*******************************************************************/
int destroy_node(slot)
   KEY slot;
{
   uint32 cda;
   union sbd *b;
   struct GS *g = (struct GS*)GUARD;
 
   if (not_guarding_node(slot, &cda, 1)) return 1;
   destroy_unguarded_node(slot, cda);
      /* update limit counters */
   for (b = map_bank_data(g->segid, g->index);
         b;
         b = map_bank_data(b->a.ancestor_segid, b->a.ancestor_index)) {
      llisub(&b->a.nodes_allocated, &llione);
      lliadd(&b->a.node_sells, &llione);
   }
   return 0;
}
 
 
/******************************************************************
destroy_unguarded_page - Destroy page without checking guard
   Input:
      slot      - Slot holding page key to be severed
      cda       - CDA of the key
*******************************************************************/
void destroy_unguarded_page(KEY slot, uint32 cda)
{
   uint32 rc;
 
   KC (prange,6) KEYSFROM (slot) RCTO (rc);
   if (0 != rc) {
      KC (prange,2) KEYSFROM (slot);
      KC (prange,0) STRUCTFROM (cda,4) KEYSTO (slot);
      KC (slot, Page_Clear);
   }
   deallocate(cda, &page_alloc); /* Turn off allocated bit */
}
 
 
/******************************************************************
destroy_page(slot) Destroy page whose key is in "slot"
   Return codes:
      0 - OK
      1 - Not acceptable to bank
*******************************************************************/
int destroy_page(slot)
   KEY slot;
{
   uint32 cda;
   union sbd *b;
   struct GS *g = (struct GS*)GUARD;
 
   if (not_guarding_page(slot, &cda, 1)) return 1;
   destroy_unguarded_page(slot, cda);
 
      /* update limit counters */
   for (b = map_bank_data(g->segid, g->index);
         b;
         b = map_bank_data(b->a.ancestor_segid, b->a.ancestor_index)) {
      llisub(&b->a.pages_allocated, &llione);
      lliadd(&b->a.page_sells, &llione);
   }
   return 0;
}
 
 
/******************************************************************
destroy_never_returned - Destroy CDA without increasing "sells"
   input:
      slot    - Slot holding the key
      type    - NODE for a node, PAGE for a page
*******************************************************************/
void destroy_never_returned(KEY slot, int type)
{
   uint32 cda;
   union sbd *b;
   struct GS *g = (struct GS*)GUARD;
 
   if (NODE == type) {
      if (not_guarding_node(slot, &cda, 1))
         crash("destroy_never_returned called for unguarded node");
      destroy_unguarded_node(slot, cda);
   } else {
      if (not_guarding_page(slot, &cda, 1))
         crash("destroy_never_returned called for unguarded page");
      destroy_unguarded_page(slot, cda);
   }
 
   could_not_allocate(type);
}
 
 
/******************************************************************
deallocate - Turn off the allocated bit in a master allocation map
   Input:
      cda    - The cda of the object
      *alloc - The map to update
   Return codes: None
*******************************************************************/
void deallocate(cda, alloc)
   uint32 cda;
   struct mad *alloc;
{
   uint32 *p = map_alloc_page(cda, alloc); /* Get page addr */
   p[(cda>>5)&0x3ff] &= ~(0x80000000ul>>(cda&31)); /* deallocate */
}
 
 
/******************************************************************
sever_node(slot) Sever the node whose key is in "slot"
         Return codes:
            0 - OK, slot holds new key to old data
            1 - Not acceptable to bank
*******************************************************************/
int sever_node(slot)
   KEY slot;
{
   uint32 cda;
 
   if (not_guarding_node(slot, &cda, 0)) return 1;
   KC (nrange,2) KEYSFROM (slot);                   /* Sever it */
   KC (nrange,0) STRUCTFROM (cda,4) KEYSTO (slot); /* Get new key */
   return 0;
}
 
 
/******************************************************************
sever_page(slot) Sever the page whose key is in "slot"
         Return codes:
            0 - OK, slot holds new key to old data
            1 - Not acceptable to bank
*******************************************************************/
int sever_page(slot)
   KEY slot;
{
   uint32 cda;
 
   if (not_guarding_page(slot, &cda, 0)) return 1;
   KC (prange,2) KEYSFROM (slot);                   /* Sever it */
   KC (prange,0) STRUCTFROM (cda,4) KEYSTO (slot); /* Get new key */
   return 0;
}
 
 
/******************************************************************
ensure_zeroes - Test an array of uint32s for zero and crash if not
   Input:
      words  - uint32 array to test
      size   - Size in uint32s
*******************************************************************/
void ensure_zeroes(
   uint32 *words,            /* Bitmap of allocated cdas to free */
   int    size)              /* Size of the map in uint32s */
{
   int i;
   for (i=0; i<size; i++) {
      if (words[i]) crash("Non-zero guard data w/o alloc map page");
   }
}
 
 
/******************************************************************
xorandtestmem - Turn off allocated bits and ensure they were on
   Input:
      amap  - Allocation bitmap
      gmap  - Bitmap of allocated cdas to free
      size - Size of the map in uint32s
*******************************************************************/
void xorandtestmem(
   uint32 *amap,             /* Allocation bitmap */
   uint32 *gmap,             /* Bitmap of allocated cdas to free */
   int    size)              /* Size of the map in uint32s */
{
   int i;
   for (i=0; i<size; i++) {
      amap[i] ^= gmap[i];
      if (amap[i] & gmap[i])
            crash("Shiva freeing already free cdas");
   }
}
 
 
/******************************************************************
free_guarded - Turn off allocated bits for a set of cdas
   Input:
      map  - Bitmap of allocated cdas to free
      cda  - CDA of bit zero in the map
      size - Size of the map in uint32s
      type - NODE for node, PAGE for page
*******************************************************************/
void free_guarded(map, cda, size, type)
   uint32 *map;              /* Bitmap of allocated cdas to free */
   uint32 cda;               /* CDA of bit zero in the map */
   int    size;              /* Size of the map in uint32s */
   int    type;              /* NODE=node cdas, PAGE=page cdas */
{
   struct mad *alloc = (NODE == type ? &node_alloc : &page_alloc);
   int len =  size;
   uint32 *p;
   uint32 *from = map;
 
   if ((cda&0xffff8000) !=
                 ((cda + size * sizeof(uint32) - 1) & 0xffff8000)) {
         /* must update two pages of allocation map */
      len = (((cda+0x8000)&0xffff8000) - cda)>>5;
      if (alloc->presence[cda>>10+5+5] &
                        (0x80000000ul>>((cda>>10+5)&31))) {
         p = map_alloc_page(cda, alloc) + ((cda>>5) & 0x3ff);
         xorandtestmem(p, from, len);
      } else ensure_zeroes(from, len);
      from += len;
      cda = (cda+0x8000)&0xffff8000;
      len = size - len;
   }
   if (alloc->presence[cda>>10+5+5] &
                        (0x80000000ul>>((cda>>10+5)&31))) {
      p = map_alloc_page(cda, alloc) + ((cda>>5) & 0x3ff);
      xorandtestmem(p, from, len);
   } else ensure_zeroes(from, len);
}
 
 
/******************************************************************
copy_guard - Copy data from a type 0 guard to at type 1 guard
 
   Input:
      cda  - first CDA guarded by the type 0 guard
      from - Pointer to the first uint32 of the type 0 guard
      type - NODE for node, PAGE for page
   Return codes:
      0 - OK, guard data copied
      1 - Can't get pages or nodes for type 1 guard
*******************************************************************/
int copy_guard(
   uint32 cda,           /* The first CDA of the type 0 guard */
   uint32 *from,         /* The uint32 map of the type 0 guard */
   int type)             /* NODE for node or PAGE for page */
{
   int len;
   uint32 *p;
 
   if (grow_guard(cda, type)) return 1;
   p = map_guard_word(cda, type);
   len = T0GUARDSIZE * sizeof(uint32);    /* Bytes to move */
   if ((cda&0xffff8000) !=
                 ((cda+T0GUARDSIZE*sizeof(uint32)-1)&0xffff8000)) {
      /* must copy into two pages */
      len = (((cda+0x8000)&0xffff8000) - cda)>>3;
      memcpy(p, from, len);
      cda = (cda+0x8000)&0xffff8000;
      from = from + (len / sizeof(uint32));
      len = (T0GUARDSIZE*sizeof(uint32)) - len;
      if (grow_guard(cda, type)) return 1;
      p = map_guard_word(cda, type);
   }
   memcpy(p, from, len);
   return 0;
}
 
 
/******************************************************************
convert_to_type1 - Convert a guard to a type 1 guard
 
   Input:
      type - NODE for node, PAGE for page
   Return codes:
      0 - OK, guard converted
      1 - Can't get pages or nodes for conversion
*******************************************************************/
int convert_to_type1(type)
   int type;
{
   struct GS *g = (struct GS*)GUARD;
   uint32 cda1, cda2;
   uint32 rc;
   char db=8;
 
   KC (fen, Node_Fetch+FEN_GuardKey) KEYSTO (temp1); /* get guard */
   KC (temp1, KT) RCTO (rc);            /* Get type of top */
   if (Page_AKT == rc) {                /* Only guard page exists */
         /* Allocate lss=8 and lss=3 nodes for guard segment */
      cda1 = alloc_next(0, ALL_ONES, gtemp1, nrange, &node_alloc);
      if (ALL_ONES == cda1) return 1;
      KC (gtemp1, Node_MakeNodeKey) STRUCTFROM (db) KEYSTO (gtemp1);
      cda2 = alloc_next(0, ALL_ONES, gtemp2, nrange, &node_alloc);
      if (ALL_ONES == cda2) {
         destroy_unguarded_node(gtemp1, cda1);
         return 1;
      }
      KC (gtemp2, Node_MakeNodeKey) CHARFROM ("\3",1) KEYSTO (gtemp2);
      KC (fen, Node_Fetch+FEN_GuardKey) KEYSTO (temp1); /* guard page */
      KC (gtemp2, Node_Swap+0) KEYSFROM (temp1);  /* page to lss=3 */
      KC (gtemp1, Node_Swap+0) KEYSFROM (gtemp2); /* lss=3 to lss=8 */
      KC (fen, Node_Swap+FEN_GuardKey) KEYSFROM (gtemp1);/* new guard */
         /* Make new guard structure addressable */
      KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM (gtemp1);
   }  /* End guard page to guard segment */
   if (NODE == type)                  /* Copy node guard */
      return copy_guard(g->first_node, g->node_guard, NODE);
   return copy_guard(g->first_page, g->page_guard, PAGE);
} /* End convert_to_type1 */
 
 
/******************************************************************
not_guarding_node - Test if this bank is guarding a node
   Input:
      slot    - The slot holding the key to test
      *rcda   - Place to return the cda of the node
      unguard - if 1 then reset the guard bit, if 0 then leave guarded
   Return codes:
      0 - Guarded by this bank, bit reset as per "unguard"
      1 - Not guarded by this bank, not a node key etc.
*******************************************************************/
int not_guarding_node(KEY slot, uint32 *rcda, int unguard)
{
   struct GS *g = (struct GS*)GUARD;
   uint32 cda;
   uint32 rc;
 
   KC (nrange, 1) KEYSFROM (slot) RCTO (rc) CHARTO ((char*)rcda, 4);
   if (1 < rc) return 1;        /* Key not identified by range key */
   cda = *rcda;
 
   if (g->npp == 0) {  /* Code for a type zero guard */
         /* Ensure that we are guarding it */
      uint32 off = cda - g->first_node;   /* offset in guard array */
 
      if (off >= T0GUARDSIZE*32) return 1;
      if (!(g->node_guard[off>>5] & (0x80000000ul>>(off&31))))
         return 1;
      if (unguard)
         g->node_guard[off>>5] &= ~(0x80000000ul>>(off&31));
 
   } else {            /* Code for a type 1 guard */
      uint32 *p;
      if ( !(g->npp & (0x8>>(cda>>30)))) return 1;
      if ( !((uint32 *)GNP)[cda>>15+5] & 0x80000000ul>>(cda>>15 & 31))
         return 1;
      p = map_guard_word(cda, NODE);
      if (!(*p & (0x80000000ul >> (cda & 31)))) return 1;
      if (unguard) *p &= ~(0x80000000ul >> (cda&31));
   }
   return 0;
}
 
 
/******************************************************************
not_guarding_page - Test if this bank is guarding a page
   Input:
      slot    - The slot holding the key to test
      *rcda   - Place to return the cda of the page
      unguard - if 1 then reset the guard bit, if 0 then leave guarded
   Return codes:
      0 - Guarded by this bank, bit reset as per "unguard"
      1 - Not guarded by this bank, not a R/W page key etc.
*******************************************************************/
int not_guarding_page(KEY slot, uint32 *rcda, int unguard)
{
   struct GS *g = (struct GS*)GUARD;
   uint32 cda;
   uint32 rc;
 
   KC (prange, 1) KEYSFROM (slot) RCTO (rc) CHARTO ((char*)rcda, 4);
   if (1 < rc) return 1;        /* Key not identified by range key */
   cda = *rcda;
 
   if (g->ppp == 0) {  /* Code for a type zero guard */
         /* Ensure that we are guarding it */
      uint32 off = cda - g->first_page;   /* offset in guard array */
 
      if (off >= T0GUARDSIZE*32) return 1;
      if (!(g->page_guard[off>>5] & (0x80000000ul>>(off&31))))
         return 1;
      if (unguard)
         g->page_guard[off>>5] &= ~(0x80000000ul>>(off&31));
 
   } else {            /* Code for a type 1 guard */
      uint32 *p;
      if ( !(g->ppp & (0x8>>(cda>>30)))) return 1;
      if ( !((uint32 *)GPP)[cda>>15+5] & 0x80000000ul>>(cda>>15 & 31))
         return 1;
      p = map_guard_word(cda, PAGE);
      if (!(*p & (0x80000000ul >> (cda & 31)))) return 1;
      if (unguard) *p &= ~(0x80000000ul >> (cda&31));
   }
   return 0;
}
 
 
/******************************************************************
check_node_allocation - Check node limit for bank, increment allocated
   Input:
      *lower  - Place to return the effective lower range limit
      *upper  - Place to return the effective upper range limit
   Return codes:
      1 - OK
      0 - Some (perhaps superior) bank's limit exceeded
*******************************************************************/
int check_node_allocation(
   uint32 *lower,
   uint32 *upper)
{
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
   *lower = b->a.node_lower_limit;
   *upper = b->a.node_upper_limit;
   for ( ;
         b;
         b = map_bank_data(b->a.ancestor_segid,b->a.ancestor_index)) {
      if (llicmp(&b->a.nodes_allocated, &b->a.node_limit) >= 0) {
            /* fix updated counters */
         union sbd *r = map_bank_data(g->segid, g->index);
         for ( ;
               r != b;
               r = map_bank_data(r->a.ancestor_segid,
                                 r->a.ancestor_index)) {
            llisub(&r->a.nodes_allocated, &llione);
         }
         return 0;
      }
      lliadd(&b->a.nodes_allocated, &llione);
      if (*lower < b->a.node_lower_limit)*lower = b->a.node_lower_limit;
      if (*upper > b->a.node_upper_limit)*upper = b->a.node_upper_limit;
   }
   return 1;
}
 
 
/******************************************************************
check_page_allocation - Check page limit for bank, increment allocated
   Input:
      *lower  - Place to return the effective lower range limit
      *upper  - Place to return the effective upper range limit
   Return codes:
      1 - OK
      0 - Some (perhaps superior) bank's limit exceeded
*******************************************************************/
int check_page_allocation(
   uint32 *lower,
   uint32 *upper)
{
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
   *lower = b->a.page_lower_limit;
   *upper = b->a.page_upper_limit;
   for ( ;
         b;
         b = map_bank_data(b->a.ancestor_segid,b->a.ancestor_index)) {
      if (llicmp(&b->a.pages_allocated, &b->a.page_limit) >= 0) {
            /* fix updated counters */
         union sbd *r = map_bank_data(g->segid, g->index);
         for ( ;
               r != b;
               r = map_bank_data(r->a.ancestor_segid,
                                 r->a.ancestor_index)) {
            llisub(&r->a.pages_allocated, &llione);
         }
         return 0;
      }
      lliadd(&b->a.pages_allocated, &llione);
      if (*lower < b->a.page_lower_limit)*lower = b->a.page_lower_limit;
      if (*upper > b->a.page_upper_limit)*upper = b->a.page_upper_limit;
   }
   return 1;
}
 
 
/******************************************************************
countzeroes - Count the number of zero bits in a word
   Input:
      word    -  The word to count
   Returns:
      The number of zero bits in that word
*******************************************************************/
int countzeros(
   uint32 word)                   /* The word to count */
{  int i;
   if (!word) return 32;
   word = ~word;                  /* Set up to count the 1 bits */
   for (i=0; word; i++) {
      word &= (-word ^ word);   /* turns off one bit per execution */
   }
   return i;
}
 
 
/******************************************************************
old_range_highest - Find highest mounted CDA for old range key
                    (Assumes all CDAs from 0 to highest are mounted)
   Input:
      high    -  (6, lowest CDA of interest)
      range   -  Slot holding range key to search
   Returns:
      0 - high has been updated to new high CDA
      1 - high is greater than or equal to highest mounted CDA
*******************************************************************/
int old_range_highest(uchar *high, KEY range)
{  uint32 cda  = 0x80000000ul;
   uint32 incr = 0x40000000ul;
   uint32 rc;
 
   for (; incr; incr >>= 1) {     /* Binary search CDA space */
      KC (range, 5) STRUCTFROM (cda) RCTO (rc);
      if (rc) cda -= incr;
      else cda += incr;
 
   }
   if (*(uint16*)(high+2) > cda>>16) return 1;
   if (*(uint16*)(high+4) >= (cda&0xffff)) return 1;
   *(uint16*)(high+2) = cda>>16;
   *(uint16*)(high+4) = (cda&0xffff);
   return 0;
}
 
 
/******************************************************************
count_available - Count available bits in a subrange
   Input:
      limit  - The effective maximum allocation for this bank
      lower  - The effective lower CDA limit
      upper  - The effective upper CDA limit
      range  - Range key for the type (page or node )
      alloc  - Master allocation data for the type
   Returns:
      The number that can be allocated
*******************************************************************/
uint32 count_available(
   uint32 limit,                  /* The allocation limit */
   uint32 lower,                  /* The lower cda limit */
   uint32 upper,                  /* The upper cda limit */
   KEY range,                     /* Range key for the range */
   struct mad *alloc)             /* The master allocation data */
{
   uchar rkp[6];
   uint32 count = 0;
   uint32 rc;
 
   rkp[0] = 0;
   rkp[1] = 0;
   memcpy(rkp+2, &lower, 4);
   for (; count<limit; ) {   /* Loop to count all mounted ranges */
      uint32 first, last;    /* first & last CDAs in mounted range */
      uint32 cda;            /* current CDA in count loop */
      int al;                /* To keep runtime from klobbering str */
                                     /* Find next mounted */
      KC (range,10) CHARFROM (rkp,6) CHARTO (rkp,6,al) RCTO (rc);
      if (KT+2 == rc) rc = 0;     /* Old range is solid from 0 to max */
      if (rc || rkp[0] || rkp[1]) /* None or too large a CDA */
   break;                               /* Exit */
      memcpy (&first, rkp+2, 4);
      if (first >= upper)
   break;
      KC (range,11) CHARFROM (rkp,6) CHARTO (rkp,6,al) RCTO (rc);
      if (KT+2 == rc) {
          rc = (uint32)old_range_highest(rkp, range);
          if (rc)
   break;
      }
      if (rc || rkp[0] || rkp[1]) /* and next not mounted */
             last = ALL_ONES;              /* All above mounted */
      else memcpy (&last, rkp+2, 4);       /* Get last in subrange */
      if (last > upper) {
         if (ALL_ONES == upper) last = upper;
         else last = upper+1;
      }
         /* Loop to count allocations */
      for (cda=first; cda >= first && cda < last && count < limit; ) {
         int off = cda&0x7fff;        /* Get offset on page */
         int end;
 
         if ((cda & 0xffff8000u) == (last & 0xffff8000u))
              end = last&0x7fff;  /* offset for last on this page */
         else end = 0x8000;       /* offset for last beyond this page */
 
         if (alloc->presence[cda>>10+5+5] &
                        (0x80000000ul>>((cda>>10+5)&31))) {
               /* Allocation map page is present */
            uint32 *p = map_alloc_page(cda, alloc); /* Get map page */
               /* Count zero bits in first word of range */
            int i = off >> 5;
            int bitno = off & 31;
            int lastword = end >> 5;
            uint32 map_word;
 
            if (0==bitno) map_word = p[i];
            else map_word = p[i] | (ALL_ONES << (32-bitno));
            if (i != lastword) {         /* First word not last word */
               if (ALL_ONES != map_word)    /* zeros in first word */
                  count += countzeros(map_word); /* Count them */
                  /* Count middle words or range */
               bitno = 0;
               for (i++; i < lastword; i++) {
                  if (ALL_ONES != p[i])    /* found some zero bits */
                     count += countzeros(p[i]);
               }
                  /* Count last word of range */
               bitno = end & 31;
               if (bitno) {              /* Check partial final word */
                  map_word = p[i] | (ALL_ONES >> bitno);
                  if (ALL_ONES != map_word)    /* zeros in last word */
                     count += countzeros(map_word); /* Count them */
               }
            } else {                       /* First word is last word */
               bitno = end & 31;
               map_word |= (ALL_ONES >> bitno);
               if (ALL_ONES != map_word)    /* zeros in last word */
                  count += countzeros(map_word); /* Count them */
            }
         }  /* End allocation map page is present */
 
         else {  /* map page not present */
            count += end - off;
         }  /* End map page not present */
         cda += end - off;              /* Next CDA to consider */
      }  /* End loop to count allocatable CDAs */
   }  /* End of loop to count CDAs in mounted ranges */
   if (count<limit) return count;
   return limit;
} /* End count_available */
 
 
/******************************************************************
calculate_available - Calculate number of CDAs that can be allocated
   Input:
      type - NODE for node, PAGE for page
   Returns:
      The number that can be allocated
*******************************************************************/
uint32 calculate_available(type)
   int type;             /* NODE for node, PAGE for page */
{
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
   uint32 available = ALL_ONES;
   LLI temp;
   uint32 lower = b->a.node_lower_limit;
   uint32 upper = b->a.node_upper_limit;
 
   if (NODE == type) {        /* We want the nodes available */
      for ( ;
            b;
            b = map_bank_data(b->a.ancestor_segid,
                              b->a.ancestor_index)) {
         if (lower < b->a.node_lower_limit)lower=b->a.node_lower_limit;
         if (upper > b->a.node_upper_limit)upper=b->a.node_upper_limit;
         temp = b->a.node_limit;
         llisub(&temp,&b->a.nodes_allocated);
         if (temp.hi) temp.low = 0;  /* more allocated than limit */
         if (available > temp.low) available = temp.low;
      }
         /* We have most restrictive range and allocation limits */
      available = count_available(available, lower, upper, nrange,
                                  &node_alloc);
   } else {           /* Do the pages available */
      for ( ;
            b;
            b = map_bank_data(b->a.ancestor_segid,
                              b->a.ancestor_index)) {
         if (lower < b->a.page_lower_limit)lower=b->a.page_lower_limit;
         if (upper > b->a.page_upper_limit)upper=b->a.page_upper_limit;
         temp = b->a.page_limit;
         llisub(&temp,&b->a.pages_allocated);
         if (temp.hi) temp.low = 0;  /* more allocated than limit */
         if (available > temp.low) available = temp.low;
      }
         /* We have most restrictive range and allocation limits */
      available = count_available(available, lower, upper, prange,
                                  &page_alloc);
   }
   return available;
} /* End calculate_available */
 
 
/******************************************************************
calculate_short_statistics - Calculate the page or node statistics
   Input:
      type - NODE for node, PAGE for page
   Returns:
      The page or node allocation statistics
*******************************************************************/
struct SB_Statistics calculate_short_statistics(type)
   int type;             /* NODE for node, PAGE for page */
{
   struct SB_Statistics ans;
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
   if (NODE == type) {              /* if node */
      LLI creates;
      creates = b->a.nodes_allocated;
      lliadd(&creates, &b->a.node_sells);
      if (creates.hi) {                /* over 2**32 operations */
         LLI adj, val;
         adj.hi = creates.hi-1;
         adj.low = creates.low;
         lliadd(&adj, &llione);
         llisub(&creates, &adj);
         ans.Creates = creates.low;
         val = b->a.node_sells;
         llisub(&val, &adj);
         ans.Destroys = val.low;
      } else {
         ans.Creates = creates.low;
         ans.Destroys = b->a.node_sells.low;
      }
   } else {
      LLI creates;
      creates = b->a.pages_allocated;
      lliadd(&creates, &b->a.page_sells);
      if (creates.hi) {                /* over 2**32 operations */
         LLI adj, val;
         adj.hi = creates.hi-1;
         adj.low = creates.low;
         lliadd(&adj, &llione);
         llisub(&creates, &adj);
         ans.Creates = creates.low;
         val = b->a.page_sells;
         llisub(&val, &adj);
         ans.Destroys = val.low;
      } else {
         ans.Creates = creates.low;
         ans.Destroys = b->a.page_sells.low;
      }
   }
   return ans;
} /* End calculate_short_statistics */
 
 
/******************************************************************
change_limit - Change the node or page limit for this bank
   Input:
      type    - NODE for node, PAGE for page
      *delta  -  the signed change value for the limit
      mask    -  Mask for over/underflow (0xffff0000 or ALL_ONES)
   Returns:
      0  - Limit changed
      1  - Under flow, limit not changed
      2  - Over flow, limit not changed
      *delta is set to the (new) limit in all cases
*******************************************************************/
int change_limit(type, delta, mask)
   int type;             /* NODE for node, PAGE for page */
   LLI *delta;
   uint32 mask;
{
   struct GS *g = (struct GS*)GUARD;
   union sbd *b = map_bank_data(g->segid, g->index);
   LLI *limptr;
   LLI temp;
 
   if (NODE == type)                /* if node */
      limptr = &b->a.node_limit;
   else limptr = &b->a.page_limit;
 
   if (delta->hi & mask &&   /* is abs(delta) is too large? */
            (delta->hi & mask) != mask){  /* yes */
      if (delta->hi & 0x80000000ul) {
         *delta = *limptr;
         return 1;
      }
      *delta = *limptr;
      return 2;
   }
   temp = *limptr;
   lliadd(&temp, delta);
   if (temp.hi & mask) {             /* Over or under flow */
      if (delta->hi & 0x80000000ul) {
         *delta = *limptr;
         return 1;
      }
      *delta = *limptr;
      return 2;
   }
   *delta = (*limptr = temp);
   return 0;
} /* End change_limit */
 
 
/******************************************************************
allocatet0 - Allocate a page or node with a type 0 guard
   Input:
      cursor* - next CDA to try pointer
      first   - lowest CDA to try
      last    - highest CDA+1 to try to allocate (for subrange bank)
                (must be in the range of the guard array)
      guard   - Pointer to the guard array
      fgcda*  - Pointer to CDA represented by the first bit in guard
      slot    - slot to return key in
      range   - range key to use in allocation
      alloc*  - Master allocation data for rangepointer
      not_1st - Non zero if this is not the first allocation of the type
   Function values:
      0 - Allocated, cursor is its CDA
      1 - None available for allocation
   Other output (iff function value is zero)
      cursor is updated for allocated CDA
      key holds key to the newly allocated page or node
      allocation map and guard map are updated.
*******************************************************************/
int allocatet0(
   uint32 *cursor,     /* First CDA to try to allocate */
   uint32 first,       /* First CDA in the guard range */
   uint32 last,        /* Last CDA+1 to try */
   uint32 *guard,      /* Pointer to the guard array */
   uint32 *fgcda,      /* The first CDA guarded by the guard array */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc,  /* Allocation data structure */
   int not_1st)        /* Non-zero if fgcda must remain fixed */
{
   int i,j;
   uint32 cda;
 
   if (*cursor<first || *cursor>=last) *cursor = first;
   *cursor= first;   /* .... KLUDGE for nodisk kernel */
   cda = alloc_next(*cursor, last, slot, range, alloc);
   if (ALL_ONES == cda) {
      cda = alloc_next(first, *cursor, slot, range, alloc);
      if (ALL_ONES == cda) return 1;
   }
   *cursor = cda;
   if (!not_1st) *fgcda = cda & ~31ul;
   guard[(cda-*fgcda) >> 5] |=
                         0x80000000ul >> ((cda-*fgcda) & 31);
   return 0;
}
 
 
/******************************************************************
allocatet1 - Allocate a page or node with a type 1 guard
   Input:
      pp      - Presence presence bits (in one byte)
      *cursor - next CDA to try pointer
      first   - first CDA in subrange
      last    - Last CDA+1 in subrange
      *ppages - Address of the presence pages
      slot    - slot to return key in
      range   - range key to use in allocation
      *alloc  - Master allocation data for rangepointer
   Function values:
      0 - Allocated, cursor is its CDA
      1 - None available for allocation
   Other output (iff function value is zero)
      cursor is updated for allocated CDA
      key holds key to the newly allocated page or node
      allocation map and guard map are updated.
*******************************************************************/
int allocatet1(
   uchar *pp,          /* Presence presence bits */
   uint32 *cursor,     /* First CDA to try to allocate */
   uint32 first,       /* First CDA in the limit range */
   uint32 last,        /* Last CDA+1 in the limit range */
   uint32 *ppages,     /* Pointer to the presence pages */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc)  /* Allocation data structure */
{
   int i,j;
 
   uint32 try;
   uint32 cda;
   uint32 block_end;
   uint32 rc;
   uint32 *p;
   uint32 pageneeded;
   int lss;
   int slotno;
   int type = (&node_alloc == alloc ? NODE : PAGE);
   struct GS *g = (struct GS*)GUARD;
 
   if (*cursor<first || *cursor>=last) *cursor = first;
         /* Try already guarded pieces */
   for (cda = *cursor; cda < last; ) {
      cda = next_t1guarded_cda(cda, last, *pp, ppages);
      if (ALL_ONES == cda)
   break;
      block_end = (cda&0xffff8000) + 0x8000;
      if (block_end>last) block_end = last;
      cda = alloc_next(cda, block_end, slot, range, alloc);
      if (ALL_ONES != cda) {
            /* Update guard, return cursor, key w/rc=0 */
         uint32 *p = map_guard_word((*cursor = cda), type);
         *p |= 0x80000000ul >> (cda & 31);
         return 0;
      }
      cda = block_end+1;
   }
   for (cda = first; cda < *cursor; ) {
      cda = next_t1guarded_cda(cda, *cursor, *pp, ppages);
      if (ALL_ONES == cda)
   break;
      block_end = (cda&0xffff8000) + 0x8000;
      if (block_end>last) block_end = last;
      cda = alloc_next(cda, block_end, slot, range, alloc);
      if (ALL_ONES != cda) {
            /* Update guard, return cursor, key w/rc=0 */
         p = map_guard_word((*cursor = cda), type);
         *p |= 0x80000000ul >> (cda & 31);
         return 0;
      }
      cda = block_end+1;
   }
      /* Can't allocate on an already allocated guard page */
   cda = alloc_next(*cursor, last, slot, range, alloc);
   if (ALL_ONES == cda) {
      cda = alloc_next(first, *cursor, slot, range, alloc);
      if (ALL_ONES == cda) return 1;
   }
   if (alloc == &node_alloc) {   /* Update node guard */
      if (grow_guard(cda, NODE)) {
         destroy_unguarded_node(slot, cda); /* destroy gotten node */
         return 1;
      }
   } else {                      /* Update page guard */
      if (grow_guard(cda, PAGE)) {
         destroy_unguarded_page(slot, cda); /* destroy gotten page */
         return 1;
      }
   }
   p = map_guard_word((*cursor = cda), type);
   *p |= 0x80000000ul >> (cda & 31);
   return 0;
}
 
 
/******************************************************************
get_slotlss - Get the lss from the key in "slot" of node "gtemp1"
    Input:
       slot    - The slot number in gtemp1 node of the top of the tree
       gtemp1  - Slot holdings a node describing the segment
    Function values:
       the lss of the key (non-node key (e.g. page or data key) is 2)
    Other output:
       slot "gtemp2" holds the key from "slot " in "gtemp1"
*******************************************************************/
int get_slotlss(
   int slot)           /* Slot in gtemp1 holding top of tree */
{
   uint32 rc;
   KC (gtemp1, Node_Fetch + slot) KEYSTO (gtemp2);
   KC (gtemp2, KT) RCTO (rc);
   if (Node_NODEAKT != rc) return 2; /* not a node key */
   KC (gtemp2, Node_DataByte) RCTO (rc);
   return rc;
}
 
 
/******************************************************************
grow_tree - Ensure a page is present in a memory tree
    Input:
       page    - The page number needed (address / 4096)
       slot    - The slot number in gtemp1 node of the top of the tree
       gtemp1  - Slot holdings node describing the segment
    Function values:
       1 - Page is present in memory tree
       0 - Unable to grow the tree
    Other output
       allocation map is updated for all pages and nodes allocated
*******************************************************************/
int grow_tree(
   uint32 page,        /* The page number in segment needed */
   int slot)           /* Slot in gtemp1 holding top of tree */
{
   int slotno = slot;   /* Slot of interest in gtemp1 */
   int lss;             /* Current lss of input page number */
   uint32 slotlss;      /* Lss of the key in slotno of gtemp1 */
   uint32 rc;
 
   slotlss = get_slotlss(slotno);   /* Sets up gtemp2 */
   for (lss=10; lss >= 2; lss--) {
      if (lss < slotlss) {          /* We're below place in tree */
         KC (dom, Domain_GetKey + gtemp2) KEYSTO (gtemp1);
         slotno = (page >> 4*(lss-2)) & 0xf;
         slotlss = get_slotlss(slotno);  /* Sets up gtemp2 */
      }
         /* Now slotlss <= lss */
      if (slotlss < lss && (page>>4*(lss-3)) & 0xf) {
            /* Have no node and non-slot 0 at lss - must add a node */
         uchar db;            /* Databyte character */
         rc = alloc_next(0, ALL_ONES, gtemp2, nrange, &node_alloc);
         if (ALL_ONES == rc) return 0;
         db = lss;
         KC (gtemp2, Node_MakeNodeKey) STRUCTFROM (db)
                       KEYSTO(gtemp2);
         KC (gtemp1, Node_Fetch+slotno) KEYSTO (temp1);
         KC (gtemp2, Node_Swap+0) KEYSFROM (temp1);
         KC (gtemp1, Node_Swap+slotno) KEYSFROM (gtemp2);
            /* New node --> gtemp1, dk0 --> gtemp2 */
         KC (dom, Domain_GetKey + gtemp2) KEYSTO (gtemp1,gtemp2);
         slotno = (page >> 4*(lss-3)) & 0xf; /* New node slot number */
         slotlss = 2;               /* Slot holds a data key */
      } /* End must add a node */
   } /* End loop to lss 3 */
      /* gtemp1 is node that needs page key and slotno is the slot */
   KC (gtemp1, Node_Fetch + slotno) KEYSTO (gtemp2);
   KC (gtemp2, KT) RCTO (rc);
   if (KT+1 == rc) {
      rc = alloc_next(0, ALL_ONES, gtemp2, prange, &page_alloc);
      if (ALL_ONES == rc) return 0;
      KC (gtemp1, Node_Swap + slotno) KEYSFROM (gtemp2);
   }
   return 1;
}
 
 
/******************************************************************
grow_guard - Allocate a page in a guard segment
    Input:
       cda     - the CDA to be guarded
       type    - NODE for node, PAGE for page
    Function values:
       0 - Page has been allocated and installed in guard segment
       1 - Unable to grow the tree
    Other output
       allocation map is updated for all pages and nodes allocated
       The guard segment has NOT been marked for cda
*******************************************************************/
int grow_guard(cda, type)
   uint32 cda;         /* The CDA to be guarded */
   int type;           /* 0 for node, 1 for page */
{
   uint32 pageneeded = cda >> (12+3);
   int lss;
   int slotno;
   struct GS *g = (struct GS*)GUARD;
   uint32 rc;
 
      /* Allocate a page in the guard map for the cda */
   KC (fen,Node_Fetch+FEN_GuardKey) KEYSTO (gtemp1); /* lss=8 node */
   if (NODE == type) {                   /* Update node guard */
      if ( !(g->npp & (0x8 >> (cda>>30)))) {  /* No presence page */
         rc = alloc_next(0, ALL_ONES, gtemp2, prange, &page_alloc);
         if (ALL_ONES == rc) return 1;
         KC (gtemp1, Node_Fetch+0) KEYSTO (temp1); /* Get lss=3 */
         KC (temp1, Node_Swap + (cda>>30) + GNP_SLOT) KEYSFROM (gtemp2);
         g->npp |= (0x8 >> (cda>>30));    /* Mark pp now present */
      }
      slotno = 1;
   } else {                      /* Update page guard */
      if ( !(g->ppp & (0x8 >> (cda>>30)))) {  /* No presence page */
         rc = alloc_next(0, ALL_ONES, gtemp2, prange, &page_alloc);
         if (ALL_ONES == rc) return 1;
         KC (gtemp1, Node_Fetch+0) KEYSTO (temp1); /* Get lss=3 */
         KC (temp1, Node_Swap + (cda>>30) + GPP_SLOT) KEYSFROM (gtemp2);
         g->ppp |= (0x8 >> (cda>>30));    /* Mark pp now present */
      }
      slotno = 2;
   }
   if (!grow_tree(cda>>3+12, slotno)) return 1;
   if (NODE == type) {
      ((uint32 *)GNP)[pageneeded>>5] |=
                           0x80000000ul >> (pageneeded & 31);
   } else {
      ((uint32 *)GPP)[pageneeded>>5] |=
                           0x80000000ul >> (pageneeded & 31);
   }
   return 0;
} /* End grow_guard */
 
 
/******************************************************************
next_available - Find next available CDA first <= CDA < last
    Input:
       first   - first CDA to try
       last    - last+1 CDA of the CDAs to try (> first)
       slot    - slot to return key in
       range   - range key to use in allocation
       alloc*  - Master allocation data for rangepointer
    Function values:
       ALL_ONES  - None available for allocation in range
       all others  - Value is next available cda
*******************************************************************/
uint32 next_available(
   uint32 first,       /* First CDA in try */
   uint32 last,        /* Last+1 CDA to try */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc)  /* Allocation data structure */
{
   uint32 cda;          /* CDA to try next */
   uint32 *p;
   uint32 end;
   sint32 off;
   uint32 rc;
         /* Loop to try allocations */
   for (cda=first; (cda >= first && cda < last); ) {
      if (alloc->presence[cda>>10+5+5] &
                     (0x80000000ul>>((cda>>10+5)&31))) {
            /* Allocation map page is present */
         p = map_alloc_page(cda, alloc); /* Get addr for map page */
         if ((cda & 0xffff8000u) == (last & 0xffff8000u))
              end = last&0x7fff;  /* End is on this page */
         else end = 0x8000;       /* End is beyond this page */
 
         off = findzerobit(cda&0x7fff, end, p);
         if (off >= 0) {      /* Have offset in allocation map page */
            cda = off + (cda & 0xffff8000);  /* convert off to CDA */
            if (makenewkey(cda, &cda, slot, range)) {
               return cda;
            }
         }
         else {  /* didn't find a bit - can we try more */
            cda = (cda & 0xffff8000) + end;
         }  /* didn't find a bit - here we try next alloc map page */
      }  /* End allocation map page is present */
 
      else {  /* map page not present */
         if (makenewkey(cda, &cda, slot, range)) { /* CDA in range */
            if (!getallocationmappage(cda, alloc)) {
               severlocals(alloc);  /* Clean up partially used */
               return ALL_ONES;
            }
         } /* cda has next CDA to try, perhaps with new allocation
              map page that will allow it to be allocated */
      }  /* End map page not present */
   }  /* End loop to try allocations */
   return ALL_ONES;  /* searched all the bits */
} /* End next_available */
 
 
/******************************************************************
alloc_next - Allocate next available where first <= CDA < last
    Input:
       first   - first CDA to try
       last    - last+1 CDA of the CDAs to try (> first)
       slot    - slot to return key in
       range   - range key to use in allocation
       alloc*  - Master allocation data for rangepointer
    Function values:
       ALL_ONES  - None available for allocation in range
       all others  - Allocated, value is cda of node or page
    Other output (iff function value is >= zero)
       key holds key to the newly allocated page or node
       allocation map is updated, guard map is not updated.
*******************************************************************/
uint32 alloc_next(
   uint32 first,       /* First CDA in the guard range */
   uint32 last,        /* Last+1 CDA to try to allocate */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range,          /* Range key to use in allocation */
   struct mad *alloc)  /* Allocation data structure */
{
   uint32 cda;          /* CDA to try next */
   uint32 *p;
   uint32 end;
   sint32 off;
   uint32 rc;
         /* Loop to try allocations */
   cda = next_available(first, last, slot, range, alloc);
   if (ALL_ONES == cda) return ALL_ONES;
   p = map_alloc_page(cda, alloc); /* Get addr for map page */
   off = cda&0x7fff;
      /* Mark allocated and return offset */
   p[off >> 5] |= 0x80000000ul >> (off&31);
   return cda;
}
 
 
/******************************************************************
makenewkey - Make a key with new allocation ID to CDA passed
    Input:
       cda     - CDA to allocate
       next*   - next CDA to try
       slot    - slot to return key in
       range   - range key to use in allocation
    Function values:
       1  - Allocated
       0  - Not mounted or I/O error
    Other output:
       value=1 - slot holds newly allocated key, next unchanged
       value=0 - next has been set to next CDA to try
*******************************************************************/
int makenewkey(
   uint32 cda,         /* CDA to make a key for */
   uint32 *next,       /* next CDA to try iff allocation failure */
   KEY slot,           /* Slot to receive the allocated object */
   KEY range)          /* Range key to use in allocation */
{
   uint32 rc;
   uchar rcda[6] = "\0\0\0\0\0";
 
   *(uint16*)(rcda+2) = *(uint16*)&cda;
   *(uint16*)(rcda+4) = *((uint16*)&cda+1);
   KC (range, 9) CHARFROM (rcda,6) KEYSTO (slot) RCTO (rc);
   if (rc > 3) {          /* Range key doesn't allocate and clear */
      KC (range, 5) CHARFROM (rcda+2,4) KEYSTO (slot) RCTO (rc);
      if (rc > 3) {          /* Range key doesn't work - crash */
         KC (range, 5) CHARFROM (rcda+2,4) KEYSTO (slot);
         rc = 0;             /* Incase it worked second time???? */
      }
      if (rc == 0) KC (slot, Node_Clear);
#if (Node_Clear != Page_Clear)
#error "this clear operation must work on both pages and nodes"
#endif
   }
   switch (rc) {
    case 0: return 1;     /* Key made and object zeroed */
    case 1:               /* Beyond end of range */
      rc = ALL_ONES;
      break;
    case 3:               /* I/O error */
      rc = cda+1;
      break;
    case 2:               /* CDA not mounted */
      KC (range, 10) CHARFROM (rcda,6) RCTO (rc) CHARTO (rcda,6);
      if (rc == 0)
         rc = *(uint16*)(rcda+2)<<16 | *(uint16*)(rcda+4);
      else rc = ALL_ONES;
   }  /* End switch */
   *next = rc;            /* Set next to try */
   return 0;
}
 
 
/******************************************************************
restoreallocationtree - Restore the master allocation map when it
                        can't be expanded
    Input:
       lss     - The lss of the top key in the old map (page==2)
       alloc*  - Master allocation data (for page or node info)
*******************************************************************/
void restoreallocationtree(
   int lss,            /* lss of the old top key */
   struct mad *alloc)  /* Allocation data structure */
{  int i;
   uint32 rc;
 
   if (alloc == &page_alloc) { /* Get page allocation map */
      KC (memnode,Node_Fetch+PAGEMAP0SLOT) KEYSTO(temp1);
   } else {                    /* Get node allocation map */
      KC (memnode,Node_Fetch+NODEMAP0SLOT) KEYSTO(temp1);
   }
   KC (temp1,KT) RCTO (rc);
   if (3 == rc) {           /* Top of allocation map is a node */
      KC (temp1, Node_DataByte) RCTO (rc);
     i = rc;
   } else i = 2;   /* Call a page LSS=2 */
   for (; i > lss; i--) {
         /* Get original top memory key */
      KC (temp1, Node_Fetch+0) KEYSTO (temp1);
   }
   if (alloc == &page_alloc) { /* Restore old page allocation map */
      KC (memnode,Node_Swap+PAGEMAP0SLOT) KEYSFROM(temp1);
   } else {                    /* Restore old node allocation map */
      KC (memnode,Node_Swap+NODEMAP0SLOT) KEYSFROM(temp1);
   }
} /* End restoreallocationtree */
 
 
/******************************************************************
severlocals - Sever nodes and pages on the local list
    Input:
       alloc*  - Master allocation data (for page or node info)
       The lists in shared memory
*******************************************************************/
void severlocals(
   struct mad *alloc)  /* Allocation data structure */
{  int i;
      /* Sever used but not allocated nodes */
   for (i=0; i<numlocalnodes; i++){
      KC (nrange, 5) STRUCTFROM (localnodes[i],4) KEYSTO (temp1);
      KC (nrange, 2) KEYSFROM (temp1);
   }
      /* Sever used but not allocated pages */
   for (i=0;i<numlocalpages;i++){
      KC (prange, 5) STRUCTFROM (localpages[i],4) KEYSTO (temp1);
      KC (prange, 2) KEYSFROM (temp1);
      alloc->presence[ localcdas[i] >> 10+5+5 ] &=
                    ~(0x80000000ul >> ((localcdas[i] >> 10+5)&31));
   }
      /* Now turn off any allocated bits that are on for CDAs */
   for (i=0; i<numlocalnodes; i++){
      uint32 cda = localnodes[i];
      if (node_alloc.presence[cda>>10+5+5] &
                   (0x80000000ul >> ((cda>>10+5) & 31))) {
         uint32 *p = map_alloc_page(cda, &node_alloc);
         p[(cda>>5)&0x3ff] &= ~(0x80000000ul >> (cda & 31));
      }
   }
   for (i=0;i<numlocalpages;i++){
      uint32 cda = localpages[i];
      if (page_alloc.presence[cda>>10+5+5] &
                   (0x80000000ul >> ((cda>>10+5) & 31))) {
         uint32 *p = map_alloc_page(cda, &page_alloc);
         p[(cda>>5)&0x3ff] &= ~(0x80000000ul >> (cda & 31));
      }
   }
   numlocalnodes = 0;
   numlocalpages = 0;
} /* End severlocals */
 
 
/******************************************************************
marklocalsallocated - Mark nodes and pages on the local list as
                        allocated
    Input:  The lists in shared memory
    Output:
       0 - Could not get pages/nodes for allocation maps
       1 - All are marked as allocated
*******************************************************************/
int marklocalsallocated()
{  int nodeindex = 0;
   int pageindex = 0;
   uint32 cda;
   uint32 *p;
   while (nodeindex < numlocalnodes || pageindex < numlocalpages) {
      for (; nodeindex < numlocalnodes; nodeindex++){
            /* Mark the nodes */
         cda = localnodes[nodeindex];
         if (!(node_alloc.presence[cda>>10+5+5] &
                      (0x80000000ul >> ((cda>>10+5) & 31)))) {
            if (!getallocationmappage(cda, &node_alloc)) return 0;
         }
         p = map_alloc_page(cda, &node_alloc);
         p[(cda>>5)&0x3ff] |= 0x80000000ul >> (cda & 31);
      }                    /* End mark nodes */
      for (; pageindex < numlocalpages; pageindex++){
            /* Mark the pages */
         cda = localpages[pageindex];
         if (!(page_alloc.presence[cda>>10+5+5] &
                      (0x80000000ul >> ((cda>>10+5) & 31)))) {
            if (!getallocationmappage(cda, &page_alloc)) return 0;
         }
         p = map_alloc_page(cda, &page_alloc);
         p[(cda>>5)&0x3ff] |= 0x80000000ul >> (cda & 31);
      }                    /* End mark pages */
   }
   numlocalnodes = 0;
   numlocalpages = 0;
   return 1;
} /* End mark_locals_allocated */
 
 
/******************************************************************
getallocationmappage - Get a page for a master allocation map
    Input:
       cda     - the CDA that needs a map page (for addr in segment)
       alloc*  - Master allocation data for rangepointer
    Function values:
        1  - Page allocated
        0  - Page could not be allocated
    Other output (iff function value is != zero)
       page and node master allocation maps have been updated for
       the page(s) and node(s) needed to define the new allocation
       map page(s) in the appropriate master allocation segment
    N.B. The cda passed may be used to support allocating it, so
       callers must re-check its availability after the return
       however, if the cda is on the local list, it will not be
       re-allocated.
*******************************************************************/
int getallocationmappage(
   uint32 cda,         /* CDA that needs an allocation map page */
   struct mad *alloc)  /* Allocation data structure */
{
   uint32 rc;
   int i, currentlss, startlss;
   char c;
 
   if (alloc == &page_alloc) { /* Page allocation map needs page */
      KC (memnode,Node_Fetch+PAGEMAP0SLOT) KEYSTO(temp1);
   } else {
      KC (memnode,Node_Fetch+NODEMAP0SLOT) KEYSTO(temp1);
   }
   KC (temp1,KT) RCTO (rc);
   if (3 == rc) {           /* Top of allocation map is a node */
      KC (temp1, Node_DataByte) RCTO (rc);
      startlss = (currentlss = rc);
   } else startlss = (currentlss = 2);   /* Call a page LSS=2 */
   for (i=currentlss+1; cda>>(10+5-12+(i<<2)); i++) {
         /* Build nodes to support larger addresses if needed */
      if (!getinternalnode(temp2)) {
         restoreallocationtree(startlss, alloc);
         return 0;
      }
      c = i;
      KC (temp2,Node_MakeNodeKey) STRUCTFROM (c) KEYSTO (temp2);
      KC (temp2,Node_Swap+0) KEYSFROM (temp1);
      KC (dom,Domain_GetKey+temp2) KEYSTO (temp1);
      currentlss = i;
   }
      /* The memory tree is now deep enough to describe the page */
      /*  temp1 holds the key to the new top */
   if (alloc == &page_alloc) { /* Page allocation map needs page */
      KC (memnode,Node_Swap+PAGEMAP0SLOT) KEYSFROM (temp1);
   } else {
      KC (memnode,Node_Swap+NODEMAP0SLOT) KEYSFROM (temp1);
   }
      /* Descend the tree building the necessary nodes */
   for (i=currentlss; i>=4; i--) {
      KC (temp1, Node_Fetch+(cda>>10+5-12+(i<<2)&15)) KEYSTO (temp2);
      KC (temp2, KT) RCTO (rc);    /* Is this node missing? */
      if (KT+1 == rc) {            /* Yes - buy one */
         if (!getinternalnode(temp2)) {
            restoreallocationtree(startlss, alloc);
            return 0;
         }
         c = i-1;
         KC (temp2,Node_MakeNodeKey) STRUCTFROM (c) KEYSTO (temp2);
         KC (temp1,Node_Swap+(cda>>10+5-12+(i<<2)&15)) KEYSFROM (temp2);
      }
      KC (dom,Domain_GetKey+temp2) KEYSTO (temp1);
   }
   if (!getinternalpage(cda, temp2)) return 0;
   KC (temp1,Node_Swap+(cda>>10+5-12+(3<<2)&15)) KEYSFROM (temp2);
   alloc->presence[cda>>10+5+5] |= (0x80000000ul>>((cda>>10+5)&31));
   if (!marklocalsallocated()) {
      restoreallocationtree(startlss, alloc);
      return 0;
   }
   return 1;
}  /* End getallocationmappage */
 
 
/**********************************************************************
findonebit - Get bit offset to next one bit in a bit array
    Input:
       start - bit index to start at
       end   - bit index + 1 to end at
       map*  - array to search
   Function value:
       -1  - No one bits found
       >=0 - bit index of the next one bit
**********************************************************************/
sint32 findonebit(
   sint32 start,
   sint32 end,
   uint32 *map)
{
   sint32 rc, i, bitno, last;
   uint32 map_word;
 
   if (start >= end) return -1;
   i = start >> 5;
   bitno = start & 31;
   map_word = map[i] & (ALL_ONES >> bitno);
   if (0 == map_word) {
        /* set up last*/
      last = end >> 5;
      bitno = 0;
      for (i++; i < last; i++) {
         if (0 != map[i]) {
            map_word = map[i];
      break;   /* found a one, so leave the for loop */
         }
      }
      bitno = end & 31;
      if (i==last) {            /* Check bits in last partial word */
         if (!bitno) return -1;     /* No bits in final word */
         if ((map[i] & (ALL_ONES << (32-bitno))) == 0) return -1;
         map_word = map[i];
      }
      bitno = 0;
   }
      /* Current word has a one bit - Get its bit number */
   map_word <<= bitno;
   for (; bitno < 32; bitno++) {
     if (map_word & 0x80000000ul) break;
     map_word <<= 1;
   }
   return bitno + 32*i;
}
 
 
/**********************************************************************
findzerobit - Get bit offset to next zero bit in a bit array
   Input:
       start - bit index to start at
       end   - bit index + 1 to end at
       map*  - array to search
   Function value:
       -1  - No zero bits found
       >=0 - bit index of the next one bit
**********************************************************************/
sint32 findzerobit(
   sint32 start,
   sint32 end,
   uint32 *map)
{
   sint32 i, bitno, last;
   uint32 map_word;
 
   if (start >= end) return -1;
   i = start >> 5;
   bitno = start & 31;
   if (0==bitno) map_word = map[i];
   else map_word = map[i] | (ALL_ONES << (32-bitno));
   if (ALL_ONES == map_word) {  /* no zero bits in first word */
        /* set up last*/
      last = end >> 5;
      bitno = 0;
      for (i++; i < last; i++) {
         if (ALL_ONES != map[i]) {  /* found one with zero bit */
            map_word = map[i];
      break;   /* found a zero, so leave the for loop */
         }
      }
      bitno = end & 31;
      if (i==last) {            /* Check bits in last partial word */
         if (!bitno) return -1;     /* No bits in final word */
         if (ALL_ONES == (map[i] | (ALL_ONES << bitno))) return -1;
         map_word = map[i];
      }
      bitno = 0;
   }
      /* Current word has a zero bit - Get its bit number */
   map_word <<= bitno;
   for (; bitno < 32; bitno++) {
     if (!(map_word & 0x80000000ul)) break;
     map_word <<= 1;
   }
   i = bitno + 32*i;
   if (i<end) return i;
   return -1;
}
 
 
/**********************************************************************
alreadyusedlocally - See if a CDA is already in local use
   Input:
       list  - The list to scan
       max   - The highest element in the list
       cda   - The CDA to test
   Function value:
       0 - cda not in use
       1 - cda in use
**********************************************************************/
int alreadyusedlocally(uint32 *list, int max, uint32 cda)
{
   int i;
   for (i=0; i<max; i++) {
      if (list[i] == cda) return 1;
   }
   return 0;
}
 
 
/**********************************************************************
getinternalkey - Get a node or page for internal use
   Input:
       slot     - The slot to return the key in
       *list    - The local list to check and record it in
       *numlist - The number in the list
       *alloc   - The master allocation data
       range    - The range key to use
   Function value:
       0 - Object not available
       1 - Object key returned
**********************************************************************/
int getinternalkey(
   KEY slot,
   uint32 *list,
   int *numlist,
   struct mad *alloc,
   KEY range)
{
   uint32 *p;           /* Pointer to a bit map element */
   sint32 ppi;          /* Current page present index */
   sint32 ai;           /* Allocation index within allocation page */
   uint32 cda;          /* CDA to try to use */
   uint32 pbcda;        /* Bit zero of allocation page is this CDA */
   uint32 lastcda;      /* First CDA of next allocation page */
 
   ppi = 0;             /* Bit index to page present bit */
   for (; -1 != ppi; ppi++) {
      ppi = findonebit(ppi, sizeof(alloc->presence)*8,
                       alloc->presence);
      if (ppi < 0)
   break;
         /* We have an allocation page present, scan it */
      pbcda = ppi<<(10+5);      /* Base CDA for allocation page */
      lastcda = pbcda + 0x8000; /* And last+1 cda */
      p = map_alloc_page(pbcda, alloc);
      for (ai=0; ai<1024*32; ) {  /* Scan bits on allocation page */
         ai = findzerobit(ai, 1024*32, p); /* find next available */
         if (-1 == ai)
      break;
         cda = ppi*1024*32 + ai;
         if (!alreadyusedlocally(list, *numlist, cda) ) {
            if (makenewkey(cda, &cda, slot, range)) {  /* Got it */
               list[(*numlist)++] = cda;             /* Remember it */
               return 1;                              /* Return it */
            }  /* Here if not mounted or I/O error */
            if (ALL_ONES == cda) {   /* No higher cdas to try */
               ppi = -1;   /* Force "try new allocation page" */
      break;
            }
         } else {    /* Here if CDA is on local list */
            cda++;
         }
         if (cda >= lastcda)
      break;
         ai = cda - pbcda;
      } /* Here if we have exhausted the current allocation page */
      if (ppi < 0)
   break;
   } /* Searched all present allocation pages */
     /* try for new allocation page */
   for (ppi = 0; ppi < sizeof(alloc->presence)*8; ) {
      ppi = findzerobit(ppi, sizeof(alloc->presence)*8,
                       alloc->presence);
      if (ppi < 0)
   break;
         /* We have an allocation page not present, try its cdas */
      cda = (uint32)ppi<<10+5;
      lastcda = cda + 0x8000;
      for (; cda < lastcda; ){
         if (!alreadyusedlocally(list, *numlist, cda) ) {
            if (makenewkey(cda, &cda, slot, range)) {  /* Got it */
               list[(*numlist)++] = cda;             /* Remember it */
               return 1;                              /* Return it */
            }     /* Here if not mounted or I/O error */
            if (ALL_ONES == cda) return 0;   /* No higher cdas to try */
         } else {/* Here if CDA is on localnodes list */
            cda++;
         }
      } /* Here if (non-existant) allocation page exhausted */
      ppi = cda >> 10+5;
   } /* Searched all absent allocation pages */
   return 0;
}
 
 
/**********************************************************************
getinternalnode - Get a node for internal use
   Input:
       slot  - the slot to return the key in
   Function value:
       0 - No nodes available
       1 - Node allocated
**********************************************************************/
int getinternalnode(KEY slot)
{
   if (MAXLOCALNODES-1 == numlocalnodes) return 0;
   return getinternalkey(slot, localnodes, &numlocalnodes,
                         &node_alloc, nrange);
}
 
 
/**********************************************************************
getinternalpage - Get a page for internal use
   Input:
       slot  - the slot to return the key in
   Function value:
       0 - No pages available
       1 - Page allocated
**********************************************************************/
int getinternalpage(
   uint32 cda,
   KEY slot)
{
   if (MAXLOCALPAGES-1 == numlocalpages) return 0;
   if (!getinternalkey(slot, localpages, &numlocalpages,
                         &page_alloc, prange)) return 0;
   localcdas[numlocalpages-1] = cda;
   return 1;
}
 
 
/**********************************************************************
map_alloc_page - Map a page of an allocation map into memory
   Input:
      cda    - CDA of the object whose allocation map is needed
      *alloc - The allocation map to use
   Output: A pointer to the page
**********************************************************************/
uint32 *map_alloc_page(cda, alloc)
   uint32 cda;         /* CDA to map */
   struct mad *alloc;  /* Allocation data structure */
{
   uint32 megnumber = cda >> 23;  /* Megabyte number needed */
   if (megnumber) {               /* Not the first */
      if (alloc->meg_number != megnumber) { /* Need to re-map */
         *(uint16*)(alloc->windowkey.Byte+12) = megnumber<<20-16;
 
         KC (memnode, Node_WriteData) STRUCTFROM (alloc->slot1,24);
         alloc->meg_number = megnumber;
      }
      return alloc->other_meg + ((cda>>5)&0x0003fc00);
   }
   return alloc->first_meg + ((cda>>5)&0x0003fc00);
}
 
 
/**********************************************************************
map_guard_word - Map a t1 guard page into memory
   Input:
      cda    - CDA of the object whose guard map is needed
      type   - NODE for a node guard, PAGE for a page guard
   Output: A pointer to the uint32 containing the guard bit for cda
**********************************************************************/
uint32 *map_guard_word(cda, type)
   uint32 cda;         /* CDA to map */
   int type;           /* Type of guard to map */
{
   uint32 megnumber = cda >> 23;  /* Megabyte number needed */
   struct {                       /* Data to write a data key */
      uint32 slot1, slot2;
      Node_KeyData window;
   } wwk;
   wwk.slot1 = (wwk.slot2 = MEM_ADDGUARD);
   *(uint32 *)&wwk.window = 0;
   *((uint32 *)&wwk.window + 1) = 0;
   *((uint32 *)&wwk.window + 3) =
              (cda>>3)&0xfff00000 | MEM_GUARDKEY<<4 | 2;
   if (NODE == type) {            /* This is a node guard */
      /* Need the guard at 2**32 in guard segment */
      *((uint32 *)&wwk.window + 2) = 1;  /* Set window to 2**32 */
   } else {                       /* This is a page guard */
      /* Need the guard at 2 * 2**32 in guard segment */
      *((uint32 *)&wwk.window + 2) = 2;  /* Set window to 2 * 2**32 */
   }
   KC (memnode, Node_WriteData) STRUCTFROM (wwk);
   return (uint32 *)ADDGUARDW + ((cda>>5)&0x0003ffff);
}
 
 
/**********************************************************************
map_bank_data - Map the page that holds a banks limit/alloc etc. data
   Input:
      segid     - Segment ID of the segment holding the bank data
      segindex  - Index within the segment
   Output: A pointer bank data,
      null if segid id ALL_ONES (i.e. invalid)
**********************************************************************/
union sbd *map_bank_data(segid, segindex)
   uint32 segid;       /* Segment ID */
   uint32 segindex;    /* Index in segment */
{
   struct {                       /* Data to write a data key */
      uint32 slot1, slot2;
      Node_KeyData window;
   } wwk;
   int i, lru;
   if (0 == segid) return ((union sbd *)SBDDATA + segindex);
   if (ALL_ONES == segid) return NULL;
   current_age++;                 /* Increment current lru age */
   for (i=0; i<SBDMAPCTLSIZE; i++) {
      if (sbdmapctl[i].resident_id == segid) {
         sbdmapctl[i].age = current_age;  /* Update entry's lru */
         return (sbdmapctl[i].first + segindex);
      }
   }
      /* Not mapped, find LRU map control entry */
   lru = 0;                       /* Initial guess */
   for (i=0; i<SBDMAPCTLSIZE; i++) {
      if (current_age - sbdmapctl[i].age >
                current_age - sbdmapctl[lru].age)
         lru = i;
   }
      /* lru holds index of oldest mapping slot - map it into window */
   sbdmapctl[lru].resident_id = segid;
   sbdmapctl[i].age = current_age;          /* Set entry's lru */
   wwk.slot1 = (wwk.slot2 = sbdmapctl[lru].slot);
   *(uint32 *)&wwk.window = 0;
   *((uint32 *)&wwk.window + 1) = 0;
   *((uint32 *)&wwk.window + 2) = segid>>12;
   *((uint32 *)&wwk.window + 3) =
              (segid<<20) & 0xfff00000 | MEM_SBDDATA<<4 | 2;
   KC (memnode, Node_WriteData) STRUCTFROM (wwk);
   return (sbdmapctl[lru].first + segindex);
}
 
 
/**********************************************************************
next_t1guarded_cda - Get the next cda >= input with present guard page
   Input:
      cda     - CDA to start from
      last    - The last cda in the valid subrange
      pp      - The presence presence bits
      *ppages - The base address of the presence pages
   Output:
      The CDA or ALL_ONES if no mapped cdas in the subrange
**********************************************************************/
uint32 next_t1guarded_cda(
   uint32 cda,         /* CDA to start looking from */
   uint32 last,        /* Last cda in subrange */
   char   pp,          /* The presence presence bits */
   uint32 *ppages)     /* Pointer to the presences pages */
{
   int startpp = (cda>>30);        /* start presence presence mask */
   int i;                          /* current presence presence bit */
   for (i=startpp; i < 4; i++) {
      if (pp & (8>>i)) {              /* presence presence is true */
         sint32 off = i<<15;
         if (off < cda>>15) off = cda>>15;
         off = findonebit(off, (off&0x18000)+0x8000, ppages);
         if (-1 != off) {
            /* presence is true - we have a guard page */
            if (cda < off<<15) cda = off<<15;
            if (cda<last) return cda;
            return ALL_ONES;
         }
      }
   }
   return ALL_ONES;
}
