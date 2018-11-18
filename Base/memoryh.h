/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_memoryh
#define _H_memoryh
int args_overlap_with_page(        /* Check for overlap with page */
   CTE *p                             /* The page to check */
          );
/* Returns 1 if arg and passed pages overlap, else 0 */

extern int memory_args_overlap(void);
             /* Returns 1 if arg and parm pages overlap, else 0 */
 
 
extern char *map_parm_string(
    /* Maps user "dib"'s page(s) "addr" for "len" bytes for "actor" */
      unsigned long addr, /* Domain's virtual address */
      struct DIB *dib,    /* Domain's DIB for page(s) to be mapped */
      int len             /* max length of string */
      );
/*
    Returns NULL if it can't map, otherwise pointer (within window) to
    the first byte selected by user's address "addr".
    cpudibp holds the dib of the actor
    If NULL is returned, either (1) the actor's map situation is better
    and its dib remains in cpudibp, OR (2) the domain root referenced by
    pointer "actor" is on a wait queue and select_domain_to_run has
    been called, OR (3) the domain referenced by the dib pointer "dib"
    has a non-zero trap code set in its DIB,
    OR (4) a segment keeper is needed and it
    has been invoked with keepjump(  ).
*/
 
typedef struct {
/* MapHeader-Size in sparc_asm.s thinks it knows sizeof(MapHeader). 
   memorys checks that opinion. */
      union Produceru *producer;
      NODE *bgnode;
      uint32 coProd; /* For product chains */
      int PurgeAge; /*The value of the global purge counter when this
           map was last made Xvalid */
      int StepCount; /* The step count of the superior CCB when this map
          was last made Xvalid. This is also used to link unallocated frames. */
      unsigned long address;   /* Address defined by the table */
                               /* Valid iff context is not the kernel map */
      unsigned short context;  /* The ID of the context which uses this map */
                               /* If map is used by more than one context, or */
                               /* at more than one address, == KernCtx */
      csid chargesetid;
      uchar format;    /* Databyte of the key to the top node */
      uchar slot_origin   :3; /* first slot no. for this table
             in producer node, Bits 18&19 in Sparc. */
      unsigned  producernode  :1; /* producer points to 1=NODE, 0=CTE */
      unsigned  multichargeset:1; /* more than one charge set
                                     under this table */
      unsigned  checkbit      :1; /* Used by check */
      unsigned lock           :1; /* to lock during a transaction */
      char State; /* Ageing mechanism:
         State is 0 for unallocated but unsafe frames.
           A frame is unsafe while there may be superior map entries to it.
         State is 1 for allocated frames in normal use.
         State is 2 to Rgn##N for maps under test for use. */
      char allCount; /* Incremented each time this frame is reallocated.
          This is 0 when frame is unallocated, especially for forsaken
          frames when there may be outstanding Xvalid pointers to it. */
      } MapHeader;

 
extern void release_parm_pages(void);
             /* Releases the pages mapped into windows 3 and 4 */
 
 
extern char *map_arg_string(
   unsigned long addr, int len
   );
   /* Maps cpudibp page(s) at "addr" for "len" into windows 0 (and 1) */
 
/*
    Returns NULL if it can't map, otherwise pointer (within window 0)
    to the first byte selected by user's address "addr".
 
    If NULL is returned, either the domain root referenced by the
    pointer "actor" is on a wait queue and select_domain_to_run has
    been called, or the domain referenced by the dib pointer "dib" has
    a non-zero trap code set in its DIB,
    or a segment keeper is needed and it has
    been invoked with keepjump(  ).
*/

extern void release_arg_pages(void);
             /* Releases the pages mapped into windows 1 and 2 */

void detpage(          /* Reclaim map entries which reference page */
   CTE *cte               /* Coretable entry of page to be reclaimed */
   );
/*
  Output -
    All of the page keys to the page are changed to uninvolved.
      (The order of the backchain of page keys is unchanged.)
    All ASBs generated from this page will be invalidated.
*/
void unprseg(NODE *node); /* unprepare a segment node */

void call_seg_keep(int);

void fault2(         /* Set up trap to seg or dom keeper */
   int code,                /* Error code from access */
   int rw);
   
typedef enum {
   memfault_nokeep,     /* No segment keeper (trap domain) */
   memfault_redispatch, /* No error found, redispatch domain */
   memfault_keeper      /* There is a segment keeper */
   } memfault_result;
   
void kldge(int);

memfault_result memfault(
   unsigned long addr,    /* Failing virtual address */
   struct DIB *dib,       /* Domain's DIB */
   struct Key *memroot);  /* Key defining memory tree root */

typedef enum {
   mem_ok,        /* or at least better */
   mem_wait,      /* Domain is blocked */
   mem_fault      /* Access fault */
   } mem_result;
   
extern mem_result search_trunk(  /* search memory tree from root */
   int targetlss,     /* stop at this lss */
   struct Key *root,  /* the root */
   struct DIB *dib,           /* DIB of domain to resolve space in */
   int rw);                   /* rw=1 if R/W access needed, else 0 */

extern mem_result search_portion(
   uint32 addr,
   int rw,                    /* rw=1 if R/W access needed, else 0 */
   MapHeader *asb,     /* pointer to a structure with the following 
                                 fields:
        producer
        bgnode
        chargesetid
        format
        producernode */
   int newmemlss);     /* target lss */
/* Returns:
   0 if ok, values in memitem, memispage, chargesetid, seapformat,
                      bgnode, segkeeperslot.
            caller must call depend_xxx_entries
   otherwise as from fault1 */
   
extern int memispage;               /* 0==memitem is node, */
                                    /* 1==memitem is cte */
extern Producer *memitem;   /* Object of current mem key */
extern csid chargesetid;   /* The charge set id in effect */
extern int seapformat;              /* ro+nc plus the lss */
extern int error_code;
extern NODE *bgnode;      /* Node with the background key in effect */
extern struct Key *segkeeperslot;
uchar * accessSeg(struct Key *, int, uint64, char, int);
void savesegkeeperaddr(void);
void restoresegkeeperaddr(void);
void init_mem(void);
#endif

