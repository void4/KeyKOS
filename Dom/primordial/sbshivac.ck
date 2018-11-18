/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*******************************************************************
  SBSHIVA - Domain to destroy a space bank
 
********************************************************************/
 
#include "kktypes.h"
#include "keykos.h"
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
 
JUMPBUF;
 int bootwomb=0;
 int stacksiz=4096;
 
char title [] = "SBSHIVAC";
 
LLI llione = {0,1};
 
/********************************************************************
 
Initial state - To be set up by womb macros or equilivant
 
Key slots set up -
     dom, returner, memnode, sbint, nrange, & prange
 
The memory node slots contain -
     slot 0   - Structure to define program code, statics, and stack
 
The registers, psw etc. contain those values which will permit the
     compiler output to run directly without any additional KeyTECH
     initialization. (KeyTECH initialization tends to try to call
     a space bank for space.)
********************************************************************/
 
     KEY guardkey     = 1;
     KEY temp1        = 2;
     KEY dom          = 3;
     KEY returner     = 4;
     KEY k1           = 6;
     KEY k2           = 7;
     KEY k3           = 8;
     KEY k4           = 9;
     KEY k5           = 10;
     KEY memnode      = 11;   /* See memory tree and node below */
     KEY fen          = 12;   /* Receives red front end node key */
     KEY sbint        = 13;   /* Internal space bank communications */
     KEY prange       = 14;   /* Page range key */
     KEY nrange       = 15;   /* Node range key */
 
     KEY kstack[5];         /* Initialized to k1, k2, k3, k4, k5 */
 
 
/* Define the memory tree and node - Must be set up before entry */
   /* Each segment is 1 megabyte, node is black lss=5 */
 
/* Code 0-0FFFFF - The program storage */
#define GUARD     0x500000u  /* Data page(s) from front end node */
#define MEM_GUARDKEY  5
#define ADDGUARDW 0x600000u  /* Additional window for GUARD data */
#define MEM_ADDGUARD  6
 
 
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
     char npp;             /* node presence presence bits */
     char ppp;             /* page presence presence bits */
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
 
#define GPP (0x1000+GUARD) /* Guard's 4 page presence pages */
#define GPP_SLOT 1
#define GNP (0x5000+GUARD) /* Guard's 4 node presence pages */
#define GNP_SLOT 5
 
 
/* Define the working storage of the space bank destroyer */
 
 
 
/* Prototypes of internal routines */
 
 
void free_guard_nodes(
   KEY,
   int);
 
void free_cdas(
   uint32 *,              /* Pointer to the bit map */
   uint32,                /* CDA corrisponding to first bit in map */
   int,                   /* Number of uint32 words in the map */
   KEY);                  /* Range key to use in freeing cdas */
 
sint32 findonebit(
   sint32, sint32,     /* Start and end + 1 bit indexes */
   uint32*);           /* Pointer to map */
 
void get_guardkey(
   uint32,
   int);
 
uint32 *map_guard_page(
   uint32,             /* CDA to map */
   int);               /* 1=node, 0=page */
 
uint32 next_t1guarded_cda(
   uint32,             /* CDA to map */
   char,               /* The presence presence bits */
   uint32 *);          /* Pointer to the presences pages */
 
 
 
 
void factory()        /* Domain to destroy a space bank */
{
   kstack[0] = k1;    /* Initialize the key stack for the routine */
   kstack[1] = k2;    /*    free_guard_nodes                      */
   kstack[2] = k3;
   kstack[3] = k4;
   kstack[4] = k5;
 
   for(;;) {
      struct GS *g;
      uint32 rc;
 
      KC  (sbint, 0) KEYSTO(fen);
 
      /* Make the guard map addressable */
      KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO(k3);
      KC (memnode, Node_Swap + MEM_GUARDKEY) KEYSFROM(k3);
 
      g = (struct GS*)GUARD;
 
         /* Free guarded pages */
      if (g->ppp == 0) {
         free_cdas(g->page_guard, g->first_page,
                   T0GUARDSIZE, prange);
         KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO(guardkey);
         KC (sbint, 1) KEYSFROM (guardkey);
      } else {
         uint32 cda = 0;
         for (;; cda += 0x8000) {
            cda = next_t1guarded_cda(cda, g->ppp, (uint32*)GPP);
            if (ALL_ONES == cda)
         break;
            free_cdas(map_guard_page(cda, 0), cda, 1024, prange);
            get_guardkey(cda, 0);
            KC (sbint, 3) KEYSFROM (guardkey) STRUCTFROM (cda);
         }
      }
         /* Free guarded nodes */
      if (g->npp == 0) {
         free_cdas(g->node_guard, g->first_node,
                   T0GUARDSIZE, nrange);
         KC (fen, Node_Fetch + FEN_GuardKey) KEYSTO(guardkey);
         KC (sbint, 2) KEYSFROM (guardkey);
      } else {
         uint32 cda = 0;
         for (;; cda += 0x8000) {
            cda = next_t1guarded_cda(cda, g->npp, (uint32*)GNP);
            if (ALL_ONES == cda)
         break;
            free_cdas(map_guard_page(cda, 1), cda, 1024, nrange);
            get_guardkey(cda, 1);
            KC (sbint, 4) KEYSFROM (guardkey) STRUCTFROM (cda);
         }
      }
         /* Free up the node structure for the guard pages */
      KC (fen, Node_Fetch+FEN_GuardKey) KEYSTO (guardkey);
      KC (guardkey, KT) RCTO (rc);
      if (3 == rc) {           /* Clean up type 1 guard nodes */
         KC (guardkey, Node_Fetch+1) KEYSTO (temp1);
         free_guard_nodes(temp1, 0);
         KC (guardkey, Node_Fetch+2) KEYSTO (temp1);
         free_guard_nodes(temp1, 0);
      }
         /* free the presence pages */
      if (g->ppp) {
         int i;
         KC (guardkey, Node_Fetch+0) KEYSTO (k1);
         for (i=0; i<4; i++) {
            KC (k1, Node_Fetch + GPP_SLOT + i) KEYSTO (k2);
            KC (k2, KT) RCTO (rc);
            if (KT+1 != rc) KC (sbint, 5) KEYSFROM (k2);
         }
      }
      if (g->npp) {
         int i;
         KC (guardkey, Node_Fetch+0) KEYSTO (k1);
         for (i=0; i<4; i++) {
            KC (k1, Node_Fetch + GNP_SLOT + i) KEYSTO (k2);
            KC (k2, KT) RCTO (rc);
            if (KT+1 != rc) KC (sbint, 5) KEYSFROM (k2);
         }
      }
      KC (guardkey, KT) RCTO (rc);
      if (3 == rc) {
         KC (guardkey, Node_Fetch+0) KEYSTO (k1); /* Get lss3 node */
         KC (k1, Node_Fetch+0) KEYSTO (k2); /* Get guard page */
         KC (sbint, 5) KEYSFROM (k2);       /* Free guard page */
         KC (sbint, 6) KEYSFROM (k1);       /* and lss=3 node */
         KC (sbint, 6) KEYSFROM (guardkey); /* and lss=8 node */
      } else {
         KC (sbint, 5) KEYSFROM (guardkey); /* Free guard map page */
      }
      KC (fen, Node_Fetch+FEN_Caller) KEYSTO (temp1);
      KC (sbint, 6) KEYSFROM (fen);         /* Free front end node */
      KFORK (returner,0) KEYSFROM(,,,temp1);
 
   }  /* end for loop */
}  /* end sbshiva */
 
 
/******************************************************************
free_cdas - Sever, fork and clear exits in all nodes/pages guarded
            by a guard bitmap
   Input:
      *map      - Pointer to the bit map
      first     - CDA represented by bit zero of the map
      max       - Number of unit32 words in the map
      range     - Range key to use to free the cdas
*******************************************************************/
void free_cdas(
   uint32 *map,           /* Pointer to the bit map */
   uint32 first,          /* CDA corisponding to first bit in map */
   int    max,            /* Number of uint32 words in the map */
   KEY    range)          /* Range key to use in freeing cdas */
{
   uint32 rc;
   sint32 index;
   uint32 cda;
   uchar rcda[6] = "\0\0\0\0\0\0";
 
   for (index = findonebit(0, max<<5, map);
        index >= 0;
        index = findonebit(index+1, max<<5, map)) {
      cda = first + index;
      memcpy(rcda+2, &cda, 4);
         /* Sever, fork, & zero */
      KC (range, 8) CHARFROM (rcda, 6) RCTO (rc);
      if (0 != rc) {
             /* Get (and wait for) a key to the object */
         KC (range, 0) STRUCTFROM (cda, 4) KEYSTO (k1);
         KC (range, 2) KEYSFROM (k1); /* Sever it */
         KC (range, 0) STRUCTFROM (cda, 4) KEYSTO (k1);
         if (range == nrange) {
            int i;
            for (i=0; i<16; i++) {
               KC (k1, Node_Fetch + i) KEYSTO (k2);
               KFORK (returner, KT+1) CHARFROM ("\0\0\0\0\0\0",6)
                      KEYSFROM (,,,k2);
            }
         }
         KC (k1, Node_Clear);
#if (Node_Clear != Page_Clear)
#error "this clear operation must work on both pages and nodes"
#endif
      }
   }
}
 
/**********************************************************************
free_guard_nodes - Free the nodes in a type1 guard structure
    Input:
       top   - The top key in the tree
       intno - Interation number, first caller uses zero
   Output:
       none
**********************************************************************/
void free_guard_nodes(
   KEY top,
   int intno)
{
   uint32 rc;
   int i;
   KEY temp;
 
   KC (top, KT) RCTO (rc);
   if (3 != rc) return;
   temp = kstack[intno];
   for (i=0; i<16; i++) {
      KC (top, Node_Fetch + i) KEYSTO (temp);
      free_guard_nodes(temp, intno+1);
   }
   KC (sbint, 6) KEYSFROM (top);
}
 
/**********************************************************************
get_guardkey - Get a page key from the guard structure
    Input:
       cda   - cda in the guard page
       type  - 1=node, 0=page
   Output:
       guardkey - Slot holds guard key
**********************************************************************/
void get_guardkey(
   uint32 cda,
   int    type)
{
   uint32 rc;
 
   KC (fen, Node_Fetch+FEN_GuardKey) KEYSTO (guardkey);
      /* Get proper tree */
   KC (guardkey, Node_Fetch+2-type) KEYSTO (guardkey);
   for (;;) {
      KC (guardkey, KT) RCTO (rc);
      if (0x202 == rc) return;      /* Got to the page key */
      KC (guardkey, Node_DataByte) RCTO (rc); /* Get lss */
      KC (guardkey, Node_Fetch+(cda>>rc*4+3&0xf)) KEYSTO (guardkey);
   }
}
 
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
map_guard_page - Map a t1 guard page into memory
   Input:
      cda    - CDA of the object whose guard map is needed
      type   - 1=node, 0=page
   Output: A pointer to the page containing the guard bit for cda
**********************************************************************/
uint32 *map_guard_page(
   uint32 cda,         /* CDA to map */
   int type)           /* Type of guard to map, 1=node, 0=page */
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
   if (type) {                    /* This is a node guard */
      /* Need the guard at 2**32 in guard segment */
      *((uint32 *)&wwk.window + 2) = 1;  /* Set window to 2**32 */
   } else {                       /* This is a page guard */
      /* Need the guard at 2 * 2**32 in guard segment */
      *((uint32 *)&wwk.window + 2) = 2;  /* Set window to 2 * 2**32 */
   }
   KC (memnode, Node_WriteData) STRUCTFROM (wwk);
   return (uint32 *)ADDGUARDW + ((cda&0x007f8000)>>5);
}
 
/**********************************************************************
next_t1guarded_cda - Get the next cda >= input with present guard page
   Input:
      cda     - CDA to start from
      pp      - The presence presence bits
      *ppages - The base address of the presence pages
   Output:
      The CDA or ALL_ONES if no mapped cdas in the subrange
**********************************************************************/
uint32 next_t1guarded_cda(
   uint32 cda,         /* CDA to map */
   char   pp,          /* The presence presence bits */
   uint32 *ppages)     /* Pointer to the presences pages */
{
   int startpp = cda>>30;          /* start presence presence mask */
   int i;                          /* Current presence presence mask */
   for (i=startpp; i<4; i++) {
      if (pp & (8>>i)) {            /* presence presence is true */
         sint32 off = i<<15;
         if (off < cda>>15) off = cda>>15;
         off = findonebit(off, (off&0x18000)+0x8000, ppages);
         if (-1 != off) {
            /* presence is true - we have a guard page */
            return off<<15;
         }
      }
   }
   return ALL_ONES;
}
