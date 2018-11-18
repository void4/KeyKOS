/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_keyh
#define _H_keyh

#include "keytypeh.h"  /* Values for keytypemask */
/* This file is specific for the Sun 4m with TI SuperSparc Processor(s) */

/* N.B. fields for Memoryc.c are still for 88K.  They need to be checked */



#include "kktypes.h"
#include "cpus.h"
#define pagesize 4096
typedef unsigned char PAGE[pagesize];
typedef unsigned char CDA[6];

extern int cdacmp(const unsigned char *cda1, const unsigned char *cda2);
 
typedef struct Node NODE;
typedef struct CoreTableEntry CTE;
typedef unsigned long ME /* Map Entry */;
typedef uint32 MI; /* Map index */
typedef unsigned short csid; /* for chargesetid's */

struct RangeLoc {short range; unsigned short offset;};
typedef struct RangeLoc RANGELOC;
 
struct Key {
   union {
 
      /* The following three components apply if
         type&keytypemask == datakey, misckey, or chargesetkey */
      struct {unsigned char fill1,    databody11[11];} dk11;
      struct {unsigned char fill2[6], databody6[6];} dk6;
      struct {unsigned char fill3[5], databody[7];} dk7;
 
      /* if type&keytypemask == devicekey : */
      struct { unsigned char fill1,
                    slot,       /* Slot for I/O board (MCPU=255) */
                    device,     /* Device number on board */
                    type,       /* Key subtype (e.g. sik,sok,cck) */
                    serial[8];} devk;  /* Key serial number */
 
      /* if type&keytypemask == prangekey or nrangekey : */
      struct { char fill4; CDA rangecda;
               unsigned char rangesize[5];} rangekey;
 
      /* if type&keytypemask == pagekey, segmentkey, nodekey,
            meterkey, fetchkey, startkey, resumekey, domainkey,
            hookkey, sensekey : */
      struct {
         union {
 
            /* if ! type&prepared : */
            struct { char fill5[2]; CDA cda;
                     unsigned long allocationid;} upk;
 
            /* if type&prepared : */
            struct { union Item *leftchain, *rightchain, *subject;
                     } pk;
         } item;
      } ik;
   } nontypedata;
   char checkmark, fill7;
   unsigned char databyte;
   unsigned char type;
 
/* Values for type */
#define prepared 0x80
#define involvedr 0x40
#define involvedw 0x20
#define keytypemask 0x1f
} /* End definition of Key */;

 
enum FrameType {
   PageFrame,
   NodePotFrame,
   AlocPotFrame,
   FreeFrame,
/* All the following are "in use by kernel". */
   InTransitFrame,
   CheckpointFrame,
   PDRFrame,
   DiskDirFrame
};
struct CoreTableEntry {
   union {
      struct {
         union Item *leftchain, *rightchain;  /* these must be
             at the beginning of the CTE to match GenericItem */
         unsigned long allocationid;
         MI maps; /* produced maps */
         unsigned dontcheckpoint :1; /* defined if busaddress >= endmemory */
         unsigned fill2 :15;
         CDA cda;
      } page;   /* PageFrame */

      struct {
         struct RangeLoc potaddress;
         CTE *homechain;     /* Next home node pot for migration */
         unsigned char nodepottype; /* NodePotFrame only. Contains an
                enum NodePotType. 3 bits would suffice. */
                /* Used for statistics only. */
         unsigned char nodepotnodecounter;
      } pot;    /* NodePotFrame or AlocPotFrame */

   } use;   /* three major types of use of a page frame - see type */
   uint32 busaddress;  /* High 32 bits of real address of page for map */
   CTE *hashnext;
   uchar ctefmt;  /* actually an enum FrameType; char is shorter */
   uchar corelock;
   uchar flags;
   uchar devicelockcount;
   uchar extensionflags;
   uchar iocount;
   unsigned global :1; /* All page table entries to this page have this
                          value for the global bit. It is a funcion of
                          cachedr and cachedw. */
   unsigned  zero :1;
   uchar unlockid;
};
 
/* Values in flags */
#define ctkernelreadonly     0x80
#define ctallocationidused   0x40
#define ctbackupversion      0x20
#define ctchanged            0x10
#define ctreferenced         0x08
#define ctcheckread          0x04    /* See adatacheckread */
#define ctvirtualzero        0x02    /* See adatavirtualzero */
#define ctgratis             0x01    /* See adatagratis */
 
/*
 * Each segment of physical memory is described by a memseg struct. Within
 * a segment, memory is considered contiguous. The segments from a linked
 * list to describe all of physical memory. The list is ordered by increasing
 * physical addresses.
 */
struct memseg {
        CTE *ctes, *ectes;         /* [from, to] in CTE array */
        unsigned int pages_base, pages_end;    /* [from, to] in page numbers */
        struct memseg *next;            /* next segment in list */
};
extern  struct  memseg *memsegs;        /* list of memory segments */
 
enum NodePotType {   /* values for nodepottype */
   nodepottypehome,
   nodepottypehomemigr,
   nodepottypeswapclean,
   nodepottypeswapcleanmigr,
   nodepottypeswapdirty};
 
/* Values in extensionflags */
#define ctmarkcleanok        0x80
#define ctkernellock         0x40
#define ctoncleanlist        0x20
#define ctwhichbackup        0x10    /* Meaningful iff ctbackupversion */
                                      /* 1 iff page is from SWAPAREA2 */
 
/*
  Note on bgnode:
  This field is to insure that those that share a map
  agree on the background key in effect.  It would be better
  to store the CDA & AC and thus require agreement on the key instead
  of agreement on the slot holding the key.  We did it as we did
  because we thought of that way sooner and also it perhaps takes
  less code.  The situation can be described by saying that
  the slot holding the background key is involved and related
  (via DEPEND) to tables that it influences merely as a way
  to remember the identity of the key, not the slot!
 
  CHARGESETID works in an analogous manner to ensure agreement on the
  charge set in effect at the address space block. When charge set IDs
  are re-used the address space block must be invalidated.
*/

 
struct Node {
   union Item *leftchain, *rightchain;
   unsigned char corelock;
   unsigned char flags;
   CDA cda;
 
   unsigned long allocationid;
   NODE *hashnext;
   union {                   /* Field use depends on prepcode */
      struct DIB *dib;          /* if prepcode == prepasdomain */
                                /*             == prepasgenkeys */
                                /*             == prepasstate */

      unsigned char drys;       /* if prepcode == prepasmeter  */
      MI maps;    /* produced maps, if prepcode == prepassegment */
   } pf;
   unsigned long callid;
   unsigned long fill1;
   unsigned char prepcode, preplock, meterlevel, fill2;
   struct Key keys[16];
#define dommeterkey keys[1]
#define domkeeper   keys[2]
#define dommemroot keys[3]
#define domtrapcode keys[5]
/* slots 4, 6, 7, and 8 reserved for additional state */
#define domfpstatekey keys[9]
#define domprio     keys[12]
#define domhookkey  keys[13]
#define domkeyskey  keys[14]
#define domstatekey keys[15]
};
 
/* Values in prepcode */
#define unpreparednode   0
#define prepasdomain     1
#define prepassegment    4
#define prepasstate      5
#define prepasgenkeys    6
#define prepasmeter      7
 
/* Values in flags */
#define NFREJECT           0x80
      /* This domain has a stall queue, valid when prepasdomain */
#define NFALLOCATIONIDUSED 0x40
#define NFCALLIDUSED       0x20
#define EXTERNALQUEUE      0x10
#define NFDIRTY            0x04
      /* The node frame has changed, must be saved in swap area */
#define NFNEEDSCLEANING    0x02
      /* The nodeframe has been selected for cleaning */
#define NFGRATIS           0x01


typedef union Produceru {
   NODE node;
   CTE cte;
} Producer; /* Producer of a map */

struct pcfa {
   CDA cda;
   unsigned char flags;
   unsigned long allocationid;
};
typedef struct pcfa PCFA;
/* Bits in flags: */
#define adatacheckread   0x04      /* Same as ctcheckread */
#define adatavirtualzero 0x02      /* Same as ctvirtualzero */
#define adatagratis      0x01      /* Same as ctgratis */
 
 
struct QueueHead {
   union Item *tail, *head;
      /* The next item after the head is *head.rightchain */
      /* *(queuehead.head).leftchain == queuehead */
      /* If the queue is empty, queuehead.tail == &queuehead
            and queuehead.head == &queuehead */
};
 
 
/* WARNING: The definitions of the structures in the union "Item" */
/*   must be coordinated so the left/rightchain pointers are at the */
/*   same relative offset for the linking code to work. */
 
/* GenericItem provides concise access to the item structures without
   implying the type of item. */
 
struct GenericItem {
   union Item *leftchain, *rightchain;
};
 
union Item {
   CTE cte;
   NODE node;
   struct QueueHead queuehead;
   struct Key key;     /* key must be prepared */
   struct GenericItem item;
};
 
typedef struct{
   unsigned long address;
   unsigned long instruction;
} pipe_entry;

typedef struct {
   unsigned long regs[16];            /* l0-l7, i0-i7 for the window */
} backwindow;

struct DIB {          /* This DIB is for the Sun4m SuperSparc  machine */
   unsigned long regs[16];            /* Y, g1-g7, o0-o7 */
   unsigned long pc, npc;             /* PC and nPC */
   unsigned long psr;

   unsigned long fsr;                 /* Zero if no domain floating point */
   /* the queue of deferred floating point operations */
   pipe_entry deferred_fp[4];         /* Zero if no deferred fp */
   unsigned long fp_regs[32];         /* floating point registers */

   signed char backmax;       /* Maximum number of windows permitted */
   signed char backdiboldest; /* oldest window in dib and not stack */
   signed char backhwoldest;  /* oldest window in H/W registers */
   signed char backalloc;     /* newest entry allocated to H/W window */

   unsigned char readiness;
#define BUSY 0x80
  /* Domain is logically busy (see Principles of Operation) */
#define LOWPRIORITY 0x40
  /* Domain's priority is worse than the domain at the head of the CPU queue */
  /*  (Set when this domain was being started by startdom.) */
#define STALECACHE 0x20
  /* CPU time cache has been scavenged */
#define ZEROCACHE 0x10
  /* dib->cpucache is zero */
#define HOOKED 0x08
  /* There is an involved hook in this domain */
#define TRAPPED 0x04
  /* If off, NFDOMTRAPCODE is zero. */
  /* If on, NFDOMTRAPCODE may not be zero. */
#define AGEING 0x2
  /* For aging the DIB */

   unsigned char permits;
#define GATEJUMPSPERMITTED 0x01
#define FPPERMITTED        0x02  /* iff has fpstatestore and psr ok */

   unsigned short Trapcode;

   long trapcodeextension[2];  /* Add'l info for traps */

   csid chargesetid;
   unsigned long cpucache;      /* Meter/time slice fields */
   ME map;                      /* Data segment table or NULL */
   union Item *lastinvolved;    /* Process management fields */
   NODE *rootnode;              /* root node.
                                   Also used to chain free dibs */
   NODE *keysnode;      /* NULL iff dib is free */
   NODE *statestore;
   long filler;                 /* Align backset on 32 byte boundry */
   long long dom_cycles, dom_instructions, ker_cycles, ker_instructions;
   backwindow backset[32];
};

/*static inline uint64 midLng(struct Key * k){
  union {struct {int hi, lo;} w; uint64 l;} u;
  u.w.hi = *(int*)&k->nontypedata.dk11.databody11[3];
  u.w.lo = *(int*)&k->nontypedata.dk11.databody11[7];
  return u.l;}
  inline uint64 lli2ull(LLI x){
  union {LLI w; uint64 l;} u;
  u.w = x;
  return u.l;}*/
#endif
