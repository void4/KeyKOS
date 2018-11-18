/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GDIRECT - Swap area directory module - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "kerinith.h"
#include "migrate.h"
#include "keyh.h"
#include "disknodh.h"
#include "wsh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "dskiomdh.h"
#include "spaceh.h"
#include "prepkeyh.h"
#include "getih.h"
#include "geteh.h"
#include "gckpth.h"
#include "gmigrath.h"
#include "gswapah.h"
#include "gsw2gdih.h"
#include "grangeth.h"
#include "grt2gdih.h"
#include "gdirecth.h"       /* Multi-caller routines */
#include "gdi2geth.h"       /* Routines called only by GETC */
#include "gdi2gswh.h"       /* Routines called only by GSWAPAC */
#include "gdi2gmih.h"       /* Routines called only by GMIGRATC */
#include "gdi2greh.h"       /* Routines called only by GRESTARC */
#include "gdi2grth.h"       /* Routines called only by GRANGETC */
#include "gdi2gckh.h"       /* Routines called only by GCKPTC */
#include "gdi2jmih.h"       /* Routines called only by JMIGRATC */
#include "gdi2spah.h"       /* Routines called only by SPACEC */
#include "memomdh.h"
#include "mi2mdioh.h"
#include "gdi2inih.h"  /* DIRENTRY is are here */
#include "cvt.h"
#include "consmdh.h"
#include "memutil.h"
 
 
struct PackedDiskDirEntry {
   uchar cda[sizeof(CDA)];
   uchar flags;
   uchar allocationid[sizeof(uint32)];
   uchar first[sizeof(uint32)];
   uchar second[sizeof(uint32)];
};
typedef struct PackedDiskDirEntry PDDE;

#define DISKDIRENTRYCOUNT ((pagesize-sizeof(LLI)) / sizeof(PDDE))

struct PackedDiskDir {
   uint64 checkpointtod;
   PDDE entries[DISKDIRENTRYCOUNT];
};
typedef struct PackedDiskDir PDD;
 
/* Bit definitions in direntry.pcfa.flags */
 
#define DIRENTPROCESSINNODE 0x80  /* Node has process in it */
                                  /* see disknode */
#define DIRENTCTEFIRST      0x40  /* DIRENTFIRST has ptr to a CTE */
#define DIRENTDEVREQFIRST   0x20  /* DIRENTFIRST has ptr to a devreq */
#define DIRENTDEVREQSECOND  0x10  /* DIRENTSECOND has ptr to a devreq */
#define DIRENTALLOCATIONPOTMIGRATED 0x08 /* Migration of this entry */
                                         /* caused the allocation pot */
                                         /* to change */
#define DIRENTCHECKREAD     0x04  /* Check for partial write on read */
                                  /* see ctcheckread & adatacheckread */
#define DIRENTVIRTUALZERO   0x02  /* Page contains all zeroes */
                                  /* see adatavirtualzero */
#define DIRENTGRATIS        0x01  /* Item is gratis */
                                  /* see ctgratis & adatagratis */
 
typedef struct Directory DIRECTORY;
 
struct Directory {
   uint32 pageentries;          /* Number of page entries */
   uint32 nodeentries;          /* Number of node entries */
   DIRENTRY *chains[1];       /* variable number of chain heads */
};
 
 
/* Local variables */
 
static
uint32 numhashheads;            /* Number of hash heads in directory */
                                /* Must be a power of 2 for hash */
static
uint32 directoryhash;           /* Hash value for above */
 
static
DIRECTORY *workingdirectory;    /* CDAs in current swap area */
static
DIRECTORY *unmigrateddirectory; /* Yet to be migrated cdas */
static
DIRECTORY *dataforapdirectory;  /* CDAs needing allocation pot update */
static
DIRECTORY *journaldirectory;    /* Places to update on journal calls */
 
static
DIRENTRY dummyentry;                    /* Ends all chains */
 
static
uint32 totaldirblocks = 0;  /* Total blocks formated by init */
static
uint32 dirblocksavailable;  /* Blocks currently available */
static
uint32 checkpointlimit;     /* Block left when checkpoint required */
static
uint32 safelimit;           /* Max direntries to be in use before */
                            /* start of a migration */
static
uint32 currentlimit;        /* Limit for current direntries, it */
                            /* ensures safelimit is obeyed */
static
DIRENTRY *dirfreehead;      /* Free list of direntries */
 
static
int hashlistofmigratememory = -1; /* Ptrs for gdimigr2 directory scan */
static
DIRENTRY **entryofmigratememory = &dummyentry.next;
                     /* Ptr to ptr to next direntry to return on scan */
 
static
DIRENTRY **lasthashchaintoreturn = NULL;  /* Cursors for gdindp */
static
DIRENTRY **nexthashchaintoreturn = NULL;     /* (used in checkpoint */
static
DIRENTRY *nextdirenttoreturn = &dummyentry;  /*  and restart) */
 
static
int hashlistofmigr34cursor = 0;   /* Cursors for gdimigr4 scan */
static const DIRENTRY *entryofmigr34cursor = &dummyentry;
 
static char gdirectflags;
#define MIGRATIONHIPRIORITY 0x80
 
 
/*********************************************************************
initdir - Allocate and initialize a directory
 
  Input -
     dirstart - Place to allocate the directory
     dir      - The directory being initialized
 
  Output -
     The next available space for a directory or direntry
*********************************************************************/
static DIRENTRY **initdir(char *dirstart, DIRECTORY *dir)
{
   int i;
 
   dir = (DIRECTORY *)dirstart;
   dir->pageentries = 0;
   dir->nodeentries = 0;
 
   for (i=0; i<numhashheads; i++) {
      dir->chains[i] = &dummyentry;
   }
   return &(dir->chains[i]);  /* Place for next object */
} /* End initdir */
 
 
/*********************************************************************
idirect - Directory initialization
 
  Input -
     dirstart - Start of the area allocated for the directories
     dirsize  - Size of the area allocated
 
  Output -
     The data structures in gdirectc have been initialized
*********************************************************************/
void idirect(char *dirstart, uint32 dirsize)
{
   int i;
   char *dirptr = dirstart;   /* Allocation cursor */
 
   if (!dirstart)
      crash("GDIRECTC001 NULL pointer passed to idirect");
 
   checkpointlimit = anodeend-firstnode + lastcte-firstcte + 1;
/*
   Allocate the available space into hash heads and entries.
   Calculate the number of hash heads to use.
*/
#define AVCHAINLENGTH 3        /* Desired average chain length */
   numhashheads = dirsize /
          ((sizeof(DIRENTRY*) + AVCHAINLENGTH*sizeof(DIRENTRY)) * 4);
                  /* The "4" above is for the four directories.*/
   for (i=1; (1<<i) <= numhashheads; i++) {;}
   numhashheads = 1 << (i-1);       /* Round down to a power of 2 */
   directoryhash = numhashheads - 1;  /* Set hashing function */
 
/* Directory allocation. See <kernel-logic,core-directory> */
 
   workingdirectory = (DIRECTORY *)dirptr;
   dirptr = (char*)initdir(dirptr, workingdirectory);
   unmigrateddirectory = (DIRECTORY *)dirptr;
   dirptr = (char*)initdir(dirptr, unmigrateddirectory);
   dataforapdirectory = (DIRECTORY *)dirptr;
   dirptr = (char*)initdir(dirptr, dataforapdirectory);
   journaldirectory = (DIRECTORY *)dirptr;
   dirptr = (char*)initdir(dirptr, journaldirectory);
 
/* Initialize the pool of available DIRENTRYs */
 
   dummyentry.next = &dummyentry;  /* Chain dummy entry to itself */
 
   dirfreehead = &dummyentry;
   dirblocksavailable = 0;
   for (; dirptr < dirstart+dirsize-sizeof(DIRENTRY); ) {
      DIRENTRY *de = (DIRENTRY *)dirptr;
      dirptr += sizeof(DIRENTRY);
      de->next = dirfreehead;
      dirfreehead = de;
      dirblocksavailable++;
   }
   totaldirblocks = dirblocksavailable;
 
   if ( (currentlimit = totaldirblocks/2 - checkpointlimit - 10) <= 0)
      crash("GDIRECTC002 Too few directory entries");
} /* End idirect */
 

/*********************************************************************
idibde - Build a directory entry (from data saved by checkpoint )
 
  Input -
     p             - Pointer to a page of disk directory information
     checkpointtod - The time stamp for this checkpoint
 
  Output -
     0 - Directory entry timestamp mismatch
     1 - Directory entry processed
*********************************************************************/
int idibde(PAGE p, uint64 checkpointtod)
{
   PDD *pdd = (PDD *)p;
   PDDE *pdde = pdd->entries;
   PDDE *pddeend = pdde + DISKDIRENTRYCOUNT;
   DIRENTRY *de;
   uint32 sba;
   static const char dde_zeroes[sizeof(PDDE)] = {0};
 
   //if (llicmp(&checkpointtod, &pdd->checkpointtod))
   if(checkpointtod != pdd->checkpointtod) {
      consprint("GDIRECTC035 Directory block rejected with wrong time stamp\n");
      return 0;
   }

   for (; pdde < pddeend; pdde++) {
      if (!Memcmp(pdde, dde_zeroes, sizeof(PDDE)))
         return 1;
      /*
         Acquire a directory block without checking against the
         checkpoint threshold.
      */
      if (--dirblocksavailable < 0)
         crash("GDIRECTC003 Out of directory blocks during restart");
      de = dirfreehead;             /* Get next block */
      if (&dummyentry == de)        /* Are we out of blocks */
         crash("GDIRECTC004 Out of directory blocks during restart");
      dirfreehead = de->next;       /* Unchain block */
      Memcpy(de->pcfa.cda, pdde->cda, sizeof(CDA));
      de->pcfa.flags = pdde->flags;
      de->pcfa.flags &= DIRENTGRATIS | DIRENTPROCESSINNODE | /* keep */
                        DIRENTVIRTUALZERO | DIRENTCHECKREAD; /* these */
      Memcpy(&de->pcfa.allocationid,
             &pdde->allocationid,
             sizeof pdde->allocationid);
 
      {  register DIRENTRY **chainhead =
            &(workingdirectory->chains[cdahash(de->pcfa.cda)
                                       & directoryhash]);
         de->next = *chainhead;
         *chainhead = de;
      }
      if (de->pcfa.cda[0] &0x80) {      /* Entry is for a node */
         workingdirectory->nodeentries++;
      } else {                            /* Entry is for a page */
         workingdirectory->pageentries++;
      }
 
      /* Fill in first disk location */
 
      Memcpy(&sba, pdde->first, sizeof pdde->first);
      if (sba) de->first.swaploc = grtsbasl(sba);
      else de->first.cte = 0;
 
      /* Fill in second disk location */
 
      Memcpy(&sba, pdde->second, sizeof pdde->first);
      if (sba) de->second.swaploc = grtsbasl(sba);
      else de->second.cte = 0;
 
      if (!de->first.cte) {           /* First address missing */
         de->first.swaploc = de->second.swaploc; /* Move 2nd to 1st */
         de->second.cte = 0;
      }
 
      /* If no copies and this is a real page, then crash */
 
      if (!de->first.cte && !(de->pcfa.flags & DIRENTVIRTUALZERO))
         crash("GDIRECT005 Neither copy mounted during restart");
   }
   return 1;
} /* End idibde */
 
 
/*********************************************************************
idiirap  - Initialize (sequental) read of active processes
 
  Input - None
 
  Output - None
 
  Notes:
         This routine is called by GRESTART to prepare for finding
         the active processes and getting them re-started.
*********************************************************************/
void idiirap(void)
{
   nexthashchaintoreturn = &unmigrateddirectory->chains[0];
   nextdirenttoreturn = unmigrateddirectory->chains[0];
   lasthashchaintoreturn =
                 &unmigrateddirectory->chains[numhashheads];
} /* End idiirap */
 
 
/*********************************************************************
idinap  - Read next active process (for GRESTART)
 
  Input - None
 
  Output -
     cda   - Pointer to cda of node with active process (N.B. high bit
             is on) or NULL if no more active processes
*********************************************************************/
uchar *idinap(void)
{
   for (;;) {
      DIRENTRY *de = nextdirenttoreturn;
 
      if (de == &dummyentry) {
         for (nexthashchaintoreturn++; /* Find non-null hash chain */
              nexthashchaintoreturn < lasthashchaintoreturn &&
                        &dummyentry == (de = *nexthashchaintoreturn);
              nexthashchaintoreturn++) {
         }
      }
      if (&dummyentry == de) {   /* End of directory */
   return NULL;
      }
      nextdirenttoreturn = de->next;
      if (de->pcfa.flags & DIRENTPROCESSINNODE)
   return de->pcfa.cda;
   }
} /* End idinap */
 
 
/*********************************************************************
gdiddp - Return number of directory pages needed for in-core directory
 
  Input - None
 
  Output -
     The number of pages needed on disk for maximum size directory
*********************************************************************/
uint32 gdiddp(void)
{
   if (!totaldirblocks)
      crash("GDIRECTC006 gdiddp called before initialization");
   return (totaldirblocks / DISKDIRENTRYCOUNT) + 1;
} /* End gdiddp */
 
 
/*********************************************************************
finddirectoryentry - Look for a CDA in a directory
 
  Input -
     cda   - Pointer to the CDA desired
     dir   - Pointer to the hash chain array for the directory
 
  Output -
     The directory entry for the cda or NULL
*********************************************************************/
static DIRENTRY *finddirectoryentry(const CDA cda, const DIRECTORY *dir)
{
   DIRENTRY *de;
 
   Memcpy(dummyentry.pcfa.cda, cda, sizeof(CDA));
   for (de = dir->chains[cdahash(cda) & directoryhash];
        cdacmp(de->pcfa.cda, cda);
        de = de->next) {;}
   if (&dummyentry == de) return NULL;
   return de;
} /* End finddirectoryentry */
 
 
/*********************************************************************
readdirentfirst - Get the swaploc associated with the first location
 
  Input -
     de    - Pointer to the DIRENTRY to use
 
  Output -
     The swaploc from the directory->first or 0,0
*********************************************************************/
static RANGELOC readdirentfirst(DIRENTRY *de)
{
   if (de->pcfa.flags & DIRENTCTEFIRST)
      crash("GDIRECTC007 Directory entry points to a CTE");
   if (de->pcfa.flags & DIRENTDEVREQFIRST)
      return de->first.devreq->swaploc;
   return de->first.swaploc;
} /* End readdirentfirst */
 
 
/*********************************************************************
readdirentsecond - Get the swaploc associated with the second location
 
  Input -
     de    - Pointer to the DIRENTRY to use
 
  Output -
     The swaploc from the directory->second or 0,0
*********************************************************************/
static RANGELOC readdirentsecond(DIRENTRY *de)
{
   if (de->pcfa.flags & DIRENTDEVREQSECOND)
      return de->second.devreq->swaploc;
   return de->second.swaploc;
} /* End readdirentsecond */
 
 
/*********************************************************************
gdiqds - Query directory space
 
  Input - None
 
  Output -
     0    - Directory space is not low
     !0   - Directory space is low
*********************************************************************/
int gdiqds(void)
{
   return gdirectflags & MIGRATIONHIPRIORITY;
} /* End gdiqds */
 
 
/*********************************************************************
setmigrationpriority - Set priority of migration I/O high or low
 
  Input - None
 
  Output -
     Calls GMIIMPR of GMIDMPR as needed
*********************************************************************/
static void setmigrationpriority(void)
{
   LLI comp1, comp2;     /* For double precision comparisons */
   uint32 lefttodo = 2 * (unmigrateddirectory->pageentries +
                          unmigrateddirectory->nodeentries) +
                     dataforapdirectory->pageentries;
   if (!lefttodo && gdirectflags & MIGRATIONHIPRIORITY) {
      gdirectflags &= ~MIGRATIONHIPRIORITY;
      gmidmpr();     /* None left & was high priority - Decr priority */
      return;
   }
   llitimes(safelimit-lefttodo, currentlimit, &comp2);
   llitimes(workingdirectory->pageentries +
                     workingdirectory->nodeentries,
            safelimit, &comp1);
   if (llicmp(&comp1, &comp2) > 0) {    /* Migration is behind */
      if (!(gdirectflags & MIGRATIONHIPRIORITY)) {
         gdirectflags |= MIGRATIONHIPRIORITY;
         gmiimpr();            /* Increment hi priority reasons */
      }
   } else {
      if (gdirectflags & MIGRATIONHIPRIORITY) {
         gdirectflags &= ~MIGRATIONHIPRIORITY;
         gmidmpr();            /* Decrement hi priority reasons */
      }
   }
} /* End setmigrationpriority */
 
 
/*********************************************************************
setbothmigrationpriority - Set priority of migration for both reasons
 
  Input - None
 
  Output - None
*********************************************************************/
static void setbothmigrationpriority(void)
{
   gswckmp();               /* Check priority due to swap space */
   setmigrationpriority();  /* Check priority due to directory space */
} /* End readdirentfirst */
 
 
/*********************************************************************
decrementnodecount - Decrement useful node count in core table entry
 
  Input -
     cte  - Pointer to the core table entry
 
  Output - None
*********************************************************************/
static void decrementnodecount(CTE *cte)
{
   if (!(cte->use.pot.nodepotnodecounter--))
      crash("GDIRECTC008 Node pot node counter < 0");
   if (!cte->use.pot.nodepotnodecounter &&  /* Count is zero */
       !(cte->extensionflags & ctoncleanlist) && /* Not on cln lst */
       !cte->iocount) {                /* Not in transit */
      if (cte->flags & ctkernelreadonly) gckdecpc(cte); /* reset */
      gspmpfa(cte);
   }
} /* End decrementnodecount */
 
 
/*********************************************************************
insertentryintodataforap - Put page direntry in CDA order on correct
                           dataforap directory hash chain
 
  Input -
     de   - Pointer to the directory entry
 
  Output -
     Page count for dataforap directory incremented
*********************************************************************/
static void insertentryintodataforap(DIRENTRY *de)
{
   DIRENTRY **prev;        /* Pointer to chain pointer */
 
   Memset(dummyentry.pcfa.cda, 0xff, sizeof(CDA)); /* make srch end */
   for (prev = &(dataforapdirectory->chains[cdahash(de->pcfa.cda)
                                            & directoryhash]);
        cdacmp((*prev)->pcfa.cda, de->pcfa.cda) < 0;
        prev = &(*prev)->next) {;}
   de->next = *prev;
   *prev = de;
   dataforapdirectory->pageentries++;    /* Added a page entry */
} /* End insertentryintodataforap */
 
 
/*********************************************************************
cancelio - Cancel any I/O in progress for this DIRENTRY
 
  Input -
     de   - Pointer to the directory entry
 
  Output -
     DIRENTDEVREQFIRST and DIRENTDEVREQSECOND flags are zero
*********************************************************************/
static void cancelio(DIRENTRY *de)
{
   if (de->pcfa.flags & DIRENTDEVREQFIRST) {
      DEVREQ *drq = de->first.devreq;
      de->first.swaploc = drq->swaploc;
      de->pcfa.flags &= ~DIRENTDEVREQFIRST;
      drq->flags &= ~DEVREQSWAPAREA;
      gddabtdr(drq);
   }
   if (de->pcfa.flags & DIRENTDEVREQSECOND) {
      DEVREQ *drq = de->second.devreq;
      de->second.swaploc = drq->swaploc;
      de->pcfa.flags &= ~DIRENTDEVREQSECOND;
      drq->flags &= ~DEVREQSWAPAREA;
      gddabtdr(drq);
   }
} /* End cancelio */
 
 
/*********************************************************************
freedirectoryentry - Put a directory entry on the free list
 
  Input -
     de   - Pointer to the directory entry
 
  Output - None
*********************************************************************/
static void freedirectoryentry(DIRENTRY *de)
{
   if (de->pcfa.flags & (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND)) {
      cancelio(de);
   }
   de->next = dirfreehead;
   dirfreehead = de;
   dirblocksavailable++;
} /* End freedirectoryentry */
 
 
/*********************************************************************
acquiredirectoryentry - Get a directory entry from the free list
 
  Input -
     cda  - CDA to be placed in new entry
     dir  - Directory in which it is to be placed
 
  Output -
     de   - Pointer to the directory entry
*********************************************************************/
static DIRENTRY *acquiredirectoryentry(CDA cda, DIRECTORY *dir)
{
   DIRENTRY *de;
 
   if (--dirblocksavailable < 0)
      crash("GDIRECTC009 Ran out of directory blocks");
/*
        The test below (against CURRENTLIMIT) is the primary test for
          checkpoint needed.
        I'm not sure the test here is ever needed.
*/
   if (dirblocksavailable <= checkpointlimit)
      gcktkckp(TKCKPDIRECTORYIES);
   de = dirfreehead;             /* Get next block */
   if (&dummyentry == de)        /* Are we out of blocks */
      crash("GDIRECTC010 Ran out of directory blocks");
   dirfreehead = de->next;       /* Unchain block */
   de->pcfa.flags = 0;           /* Initialize part of direntry */
   Memcpy(de->pcfa.cda, cda, sizeof(CDA));
   {  register DIRENTRY **chainhead =
         &(dir->chains[cdahash(cda) & directoryhash]);
      de->next = *chainhead;
      *chainhead = de;
   }
   if (cda[0] &0x80) {      /* Entry is for a node */
      dir->nodeentries++;
   } else {                   /* Entry is for a page */
      dir->pageentries++;
   }
   if (dir->pageentries + dir->nodeentries >= currentlimit)
      gcktkckp(TKCKPDIRECTORYIES);
   setmigrationpriority();
   return de;
} /* End acquiredirectoryentry */
 
 
/*********************************************************************
unchaindirectoryentry - Unchain an entry from a directory
 
  Input -
     cda  - Pointer to the CDA to remove
     dir  - Pointer to the directory
 
  Output -
     Address of directory entry or NULL
*********************************************************************/
static DIRENTRY *unchaindirectoryentry(CDA cda, DIRECTORY *dir)
{
   DIRENTRY **prev;        /* Pointer to chain pointer */
   DIRENTRY *rde;          /* Pointer to the one to return */
 
   Memcpy(dummyentry.pcfa.cda, cda, sizeof(CDA)); /* srch termination */
   for (prev = &(dir->chains[cdahash(cda) & directoryhash]);
        cdacmp((*prev)->pcfa.cda, cda);
        prev = &(*prev)->next) {;}
   if (&dummyentry == *prev) return NULL;
   rde = *prev;
   *prev = rde->next;
   if (&rde->next == entryofmigratememory) { /* Our cursor is here */
      entryofmigratememory = prev;
   }
   if (cda[0] & 0x80)     /* Entry is for a node */
      dir->nodeentries--;      /* Deleted a node entry */
   else dir->pageentries--;    /* Deleted a page entry */
   return rde;
} /* End unchaindirectoryentry */
 
 
/*********************************************************************
gdimgrst - Get the current state of migration
 
  Input - None
 
  Output -
     struct gdimgrstRet containing initial worst case task size and
                                   current task size
*********************************************************************/
struct gdimgrstRet gdimgrst(void)
{
   struct gdimgrstRet ret;
   ret.initial = safelimit;
   ret.current = 2 * (unmigrateddirectory->pageentries +
                      unmigrateddirectory->nodeentries) +
                 dataforapdirectory->pageentries;
   return ret;
} /* End gdimgrst */
 
 
/*********************************************************************
gdifapm - Fix allocation pot migration (after pack dismount)
 
  Input -
     cdas   - struct GdiFAPMParm with low and high CDA of dismounted
              range
 
  Output - None
*********************************************************************/
void gdifapm(struct GdiGAPMParm cdas)
{
   DIRENTRY **hashchain;
   DIRENTRY *de;
 
   for (hashchain = journaldirectory->chains;
        hashchain < journaldirectory->chains + numhashheads;
        hashchain++) {
      DIRENTRY **prev = hashchain;
      for (de = *prev; &dummyentry != de; de = *prev){
         if   (cdacmp(de->pcfa.cda, cdas.rangelow) >= 0 &&
               cdacmp(de->pcfa.cda, cdas.rangehigh) <= 0) {
            *prev = de->next;    /* Unchain entry */
            journaldirectory->pageentries--; /* Fix count */
            insertentryintodataforap(de);
         } else prev = &de->next;
      }
   }
   setbothmigrationpriority();
} /* End gdifapm */
 
 
/*********************************************************************
gdiciicd - Is CDA in the working directory
 
  Input -
     cda    - CDA to check
 
  Output -
     0      - CDA is not in working directory
     !0     - CDA is in working directory
*********************************************************************/
int gdiciicd(CDA cda)
{
   if (finddirectoryentry(cda, workingdirectory)) return 1;
   return 0;
} /* End gdiciicd */
 
 
/*********************************************************************
gdiesibd - Are there entries still in the unmigrated directory
 
  Input - None
 
  Output -
     0      - No more entries in the unmigrated director
     !0     - There are still entries in the unmigrated director
*********************************************************************/
int gdiesibd(void)
{
   if   (unmigrateddirectory->pageentries +
         unmigrateddirectory->nodeentries) return 1;
   return 0;
} /* End gdiesibd */
 
 
/*********************************************************************
gdiredrq - Clear linkage between DEVREQ and DIRENTRY
 
  Input -
     drq    - Pointer to the devreq to check
 
  Output - None
*********************************************************************/
void gdiredrq(DEVREQ *drq)
{
   DIRENTRY *de;
 
   if (!(drq->flags & DEVREQSWAPAREA))
      crash("GDIRECTC012 gdiredrq called with non-swap area devreq");
   de = drq->request->direntry;
   if (de->pcfa.flags & DIRENTDEVREQFIRST && drq == de->first.devreq) {
       /* DEVREQ is for the first swap area */
      de->first.swaploc = drq->swaploc;
      de->pcfa.flags &= ~DIRENTDEVREQFIRST;
      drq->flags &= ~DEVREQSWAPAREA;
      return;
   }
   if (de->pcfa.flags & DIRENTDEVREQSECOND && drq == de->second.devreq) {
       /* DEVREQ is for the second swap area */
      de->second.swaploc = drq->swaploc;
      de->pcfa.flags &= ~DIRENTDEVREQSECOND;
      drq->flags &= ~DEVREQSWAPAREA;
      return;
   }
   crash("GDIRECTC013 gdiredrq called for non-devreq dir entry");
} /* End gdiredrq */
 
 
/*********************************************************************
gdirembk - Remove from the unmigrated directory
 
  Input -
     cda    - The CDA to remove
     innext - non-zero if CDA will be included in next checkpoint
 
  Output - None
*********************************************************************/
void gdirembk(CDA cda, bool innext)
{
   DIRENTRY *de = unchaindirectoryentry(cda, unmigrateddirectory);
 
   if (de) {                     /* If entry was in directory */
      if (cda[0] & 0x80           /* If entry is for a node */
          || innext) {            /* or will be in next checkpoint */
         DIRECTORY *dir = journaldirectory;
         register DIRENTRY **chainhead =
            &(dir->chains[cdahash(cda) & directoryhash]);
 
         de->next = *chainhead;
         *chainhead = de;
         dir->nodeentries++;
      } else insertentryintodataforap(de);
      setmigrationpriority();
   }
} /* End gdirembk */
 
 
/*********************************************************************
gdiswap - Swap directories at the end of a checkpoint
 
  Input - None
 
  Output - None
*********************************************************************/
void gdiswap(void)
{
   DIRENTRY **hashchain;
   DIRENTRY *de;
   DIRECTORY *temp;      /* For swaping directories */
 
   if   (unmigrateddirectory->pageentries |
         unmigrateddirectory->nodeentries |
         dataforapdirectory->pageentries |
         dataforapdirectory->nodeentries)
      crash("GDIRECTC014 gdiswap called with unmigrated entries");
   if   (journaldirectory->pageentries + journaldirectory->nodeentries +
         workingdirectory->pageentries + workingdirectory->nodeentries +
         dirblocksavailable != totaldirblocks)
      crash("GDIRECTC015 journal+working+available blocks != total");
   gswreset();              /* Reset the swaparea allocations */
 
   /* Clear the journal directory */
 
   journaldirectory->pageentries = 0;
   journaldirectory->nodeentries = 0;
   for (hashchain = journaldirectory->chains;
        hashchain < journaldirectory->chains + numhashheads;
        hashchain++) {
      for (de = *hashchain; &dummyentry != de; de = *hashchain){
         *hashchain = de->next;  /* Un-chain the entry */
         freedirectoryentry(de);
      }
   }
   hashlistofmigratememory = -1;   /* Reread in migrate2 */
   entryofmigratememory = &dummyentry.next;
   temp = workingdirectory;        /* Swap working and unmigrated */
   workingdirectory = unmigrateddirectory;
   unmigrateddirectory = temp;
    /* Calc safelimit, a crude measure of the size of migration */
   safelimit = 2 * (unmigrateddirectory->pageentries +
                    unmigrateddirectory->nodeentries);
   setmigrationpriority();
} /* End gdiswap */
 
 
/*********************************************************************
gdintsdr - Initialize sequental directory read
 
  Input - None
 
  Output -
     gdindp is set to start building diskdirectories from the start
            of the directory
*********************************************************************/
void gdintsdr(void)
{
   nexthashchaintoreturn = workingdirectory->chains;
   nextdirenttoreturn = *nexthashchaintoreturn;
   lasthashchaintoreturn = nexthashchaintoreturn + numhashheads;
} /* End gdintsdr */
 
 
/*********************************************************************
gdindp - Next sequental directory entry
 
  Input -
     p             - A page to load with disk directory entries
     checkpointtod - The time stamp for this checkpoint
 
  Output -
     0     - End of directory - No data in page
     !0    - Page formated with disk directory entries
*********************************************************************/
int gdindp(PAGE p, uint64 checkpointtod)
{
   PDD *pdd = (PDD *)p;
   PDDE *pdde = pdd->entries;
   PDDE *pddeend = pdde + DISKDIRENTRYCOUNT;
   DIRENTRY *de = nextdirenttoreturn;
   RANGELOC sl;
   uint32 sba;
 
   pdd->checkpointtod = checkpointtod;

   for (; pdde < pddeend; pdde++) {
      if (de == &dummyentry) {
         for (nexthashchaintoreturn++; /* Find non-null hash chain */
              nexthashchaintoreturn < lasthashchaintoreturn &&
                        &dummyentry == (de = *nexthashchaintoreturn);
              nexthashchaintoreturn++) {
         }
      }
      if (&dummyentry == de) {   /* End of directory */
         if (pdd->entries == pdde)
   break; /* didn't build any */
         memzero(pdde,     /* clear the rest of the page */
                 (pddeend- pdde) * sizeof(PDDE));
   break;
      }
      Memcpy(pdde->cda, de->pcfa.cda, sizeof(CDA));
      pdde->flags = de->pcfa.flags;
      pdde->flags &= DIRENTGRATIS | DIRENTPROCESSINNODE |  /* keep */
                     DIRENTVIRTUALZERO | DIRENTCHECKREAD;  /* these */
      Memcpy(&pdde->allocationid,
             &de->pcfa.allocationid,
             sizeof pdde->allocationid);
 
    /* Fill in first disk location */
      sl = readdirentfirst(de);
      if (sl.range | sl.offset) {          /* If not 0,0 */
         sba = grtslsba(sl);               /* to swap block address */
         Memcpy(pdde->first, &sba, sizeof pdde->first);
      } else memzero(pdde->first, sizeof pdde->first);
    /* Same for second */
      sl = readdirentsecond(de);
      if (sl.range | sl.offset) {          /* If not 0,0 */
         sba = grtslsba(sl);               /* to swap block address */
         Memcpy(pdde->second, &sba, sizeof pdde->second);
      } else memzero(pdde->second, sizeof pdde->second);
 
      if (de->pcfa.flags & DIRENTPROCESSINNODE) { /* This has process */
         CDA pcda;
         NODE *nf;
 
         Memcpy(pcda, pdde->cda, sizeof(CDA));
         pcda[0] &= 0x7f;             /* Turn off node flag */
         nf = srchnode(pcda);
         if (!nf) crash("GDIRECTC016 Node w/process not in storage");
         nf->flags |= NFDIRTY;     /* Mark dirty for next checkpoint */
      }
      de = de->next;
   }
   nextdirenttoreturn = de;
   return (pdde - pdd->entries) * sizeof *pdde;
} /* End gdindp */
 
 
/*********************************************************************
gdiverrq - Return the version id of a page after a read
 
  Input -
     drq   - Pointer to the devreq which just completed
 
  Output -
     gdiverrq_current, gdiverrq_backup or gdiverrq_neither
 
  Note:
     We could check if the page is the next backup, but this situation
     is so unlikely it doesn't seem worth the overhead.
*********************************************************************/
int gdiverrq(DEVREQ *drq)
{
   REQUEST *req = drq->request;
   CTE *cte = srchpage(req->pcfa.cda);
   DIRENTRY *de;
 
   if (!cte) {               /* Page is not in memory */
      de = finddirectoryentry(req->pcfa.cda, workingdirectory);
      if (de) {         /* Has an entry in the working directory */
         if (drq->flags & DEVREQSWAPAREA) {  /* from swap area */
            if (de == req->direntry) return gdiverrq_current;
         } else {
             /* maybe back up version from home */
            if (finddirectoryentry(req->pcfa.cda, unmigrateddirectory))
               return gdiverrq_neither;   /* Isn't backup version */
            if (finddirectoryentry(req->pcfa.cda, dataforapdirectory))
               return gdiverrq_neither;   /* Prob not backup version */
            if (finddirectoryentry(req->pcfa.cda, journaldirectory))
               return gdiverrq_neither;   /* Prob not backup version */
         }
      } else {          /* Not in working directory */
          /* if from a swaparea --> backup swaparea --> current */
         if (drq->flags & DEVREQSWAPAREA) return gdiverrq_current;
         if (finddirectoryentry(req->pcfa.cda, unmigrateddirectory))
            return gdiverrq_neither;
         return gdiverrq_current;
      }
   } else {
     /* Current version is in memory, so the page that just came in */
     /* can't be the current version. Maybe it is the backup version */
      if (drq->flags & DEVREQSWAPAREA) {  /* from swap area */
 
        /* Then it is the backup version unless in working dir */
         de = finddirectoryentry(req->pcfa.cda, workingdirectory);
         if (de && de == req->direntry) return gdiverrq_neither;
 
      } else {                            /* Not from swap area,
             maybe back up version from home */
         if (finddirectoryentry(req->pcfa.cda, unmigrateddirectory))
            return gdiverrq_neither;   /* Isn't backup version */
         if (finddirectoryentry(req->pcfa.cda, dataforapdirectory))
            return gdiverrq_neither;   /* Prob not backup version */
         if (finddirectoryentry(req->pcfa.cda, journaldirectory))
            return gdiverrq_neither;   /* Prob not backup version */
      }
   }
 
    /* It came from the backup swap area */
   if (srchbvop(req->pcfa.cda)) return gdiverrq_neither;
   return gdiverrq_backup;
} /* End gdiverrq */
 
 
/*********************************************************************
gdiclear - Remove a CDA from the working directory
 
  Input -
     cda   - Pointer to the CDA to remove
 
  Output - None
*********************************************************************/
void gdiclear(CDA cda)
{
   DIRENTRY *de = unchaindirectoryentry(cda, workingdirectory);
 
   if (de) {
      freedirectoryentry(de);
      setbothmigrationpriority();
   }
} /* End gdiclear */
 
 
/*********************************************************************
gdireset - Remove a CDA from the working directory
 
  Input -
     cda     - Pointer to the CDA to reset
     cte     - Pointer to the CTE of the frame holding the nodepot
     process - Zero if no process in the node
 
  Output - None
*********************************************************************/
void gdireset(CDA cda, CTE *cte, int process)
{
   DIRENTRY *de;
 
   cte->use.pot.nodepotnodecounter++;
   de = finddirectoryentry(cda, workingdirectory);
   if (de) {                /* Entry in working directory for cda */
      if (de->pcfa.flags & (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND))
         cancelio(de);      /* If I/O busy, cancel it */
      if (de->pcfa.flags & DIRENTCTEFIRST) /* If it points to a CTE */
         decrementnodecount(de->first.cte);
   } else {                 /* Create a new directory entry */
      de = acquiredirectoryentry(cda, workingdirectory);
      if (!(de->pcfa.cda[0] & 0x80))
         crash("GDIRECTC017 gdireset must be called with node cdas");
      /* DIRENTRY Flags and allocationid are zero for nodes */
   }
   de->first.cte = cte;        /* Set first location */
   de->pcfa.flags = DIRENTCTEFIRST;
   de->second.cte = 0;         /* No second location yet */
   if (process) de->pcfa.flags |= DIRENTPROCESSINNODE;
} /* End gdireset */
 
 
/*********************************************************************
gdinperr - Remove old swaplocs for nodes from working directory when
           there has been an I/O error writing that swap node pot.
 
  Input -
     cte     - Pointer to the CTE of the frame holding the nodepot
 
  Output - None
 
  Notes:
     Uses the IOSYSWINDOW
*********************************************************************/
void gdinperr(CTE *cte)
{
   if (NodePotFrame == cte->ctefmt) {      /* If frame holds a pot */
      struct NodePot *np;
      int i;
 
      np = (struct NodePot*)map_window(IOSYSWINDOW, cte, MAP_WINDOW_RO);
      for (i=0; i<NPNODECOUNT; i++) {  /* for all nodes in pot */
         DIRENTRY *de = finddirectoryentry(np->disknodes[i].cda,
                                           workingdirectory);
         if (de) {                   /* We have working entry for cda */
            if   (de->first.cte == cte &&  /* It's for this pot */
                  de->pcfa.flags & DIRENTCTEFIRST) {
               de->second.cte = NULL;    /* Reset swaploc to NULL */
            }
         }
      }
   }
} /* End gdinperr */
 
 
/*********************************************************************
gdiset - Include new swap area location in working directory entry
 
  Input -
     cte     - Pointer to the CTE of the frame holding the nodepot
     swaploc - The RANGELOC to be added to the directory entry
 
  Output - None
 
  Notes:
     May use IOSYSWINDOW
*********************************************************************/
void gdiset(CTE *cte, RANGELOC swaploc)
{
   if (NodePotFrame == cte->ctefmt) {      /* If frame holds a pot */
      struct NodePot *np;
      int i;
 
      cte->use.pot.potaddress = swaploc; /* Mark the node pot */
      np = (struct NodePot*)map_window(IOSYSWINDOW, cte, MAP_WINDOW_RO);
      for (i=0; i<NPNODECOUNT; i++) {  /* for all nodes in pot */
         DIRENTRY *de = finddirectoryentry(np->disknodes[i].cda,
                                           workingdirectory);
         if (de) {                     /* working entry for node */
            if (de->pcfa.flags &       /* Is I/O active? */
                  (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND)) {
               cancelio(de);           /* Yes - abort it */
            }
            if   (de->pcfa.flags & DIRENTCTEFIRST &&
                  de->first.cte == cte) { /* Direntry is for our cte */
               if (de->second.cte) {      /* swaploc is in second */
                  de->first.swaploc = swaploc;    /* Mark first */
                  de->pcfa.flags &= ~DIRENTCTEFIRST;
               } else de->second.swaploc = swaploc; /* Mark second */
            }
         }
      }
   } else {                   /* cte should be for a page */
      DIRENTRY *de;
 
      if (PageFrame != cte->ctefmt)        /* Frame doesn't hold page */
         crash("GDIRECTC020 gdiset called with non-pot, non-page cte");
      de = finddirectoryentry(cte->use.page.cda, workingdirectory);
      if (de) {                  /* working entry for this page */
         if (de->pcfa.flags & (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND))
            crash("GDIRECTC021 gdiset called with I/O on page cte");
         de->second.swaploc = swaploc;
      } else {                   /* Need to create an entry */
         de = acquiredirectoryentry(cte->use.page.cda,
                                    workingdirectory);
         de->pcfa.flags = cte->flags & (DIRENTGRATIS | DIRENTCHECKREAD);
#if DIRENTGRATIS != ctgratis || DIRENTCHECKREAD != ctcheckread
#error DIRENTGRATIS != ctgratis || DIRENTCHECKREAD != ctcheckread
#endif
         de->pcfa.allocationid = cte->use.page.allocationid;
         Memcpy(de->pcfa.cda, cte->use.page.cda, sizeof(CDA));
         de->first.swaploc = swaploc;
         de->second.cte = NULL;
      }
   }
} /* End gdiset */
 
 
/*********************************************************************
gdisetvz - Include new virtual zero page in working directory
 
  Input -
     pcfa    - Pcfa for page, flags define ADATAGRATIS + ADATACHECKREAD
 
  Output - None
*********************************************************************/
void gdisetvz(PCFA *pcfa)
{
   DIRENTRY *de = finddirectoryentry(pcfa->cda, workingdirectory);
 
   if (de) {
      if (de->pcfa.flags & (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND)) {
         cancelio(de);
      }
   } else de = acquiredirectoryentry(pcfa->cda, workingdirectory);
   de->first.cte = 0;
   de->second.cte = 0;
   de->pcfa.flags = (pcfa->flags & (DIRENTGRATIS | DIRENTCHECKREAD)) |
                    DIRENTVIRTUALZERO;
#if DIRENTGRATIS != adatagratis || DIRENTCHECKREAD != adatacheckread
#error DIRENTGRATIS != adatagratis || DIRENTCHECKREAD != adatacheckread
#endif
   de->pcfa.allocationid = pcfa->allocationid;
} /* End gdisetvz */
 
 
/*********************************************************************
unbuildinputrequest - Release locks, devreqs and request if gotten
 
  Input -
     req     - Pointer to the request or NULL
     de      - Pointer to the directory entry
 
  Output - None
*********************************************************************/
static void unbuildinputrequest(REQUEST *req, DIRENTRY *de)
{
   if (req) {
      DEVREQ *drq = req->devreqs;
      if (drq) {               /* At most 1 devreq (2 is enough) */
         if (de->second.devreq != drq)
            crash("GDIRECTC022 request->devreq not in DIRENTRY");
         de->second.swaploc = drq->swaploc;
         de->pcfa.flags &= ~DIRENTDEVREQSECOND;
         getredrq(drq);
      }
      getrereq(req);
   }
   if (de->pcfa.cda[0] &0x80) {    /* Node are locked by swaploc */
      getunlok(pothash(de->first.swaploc));
   }
} /* End unbuildinputrequest */
 
 
/*********************************************************************
swapranges - If necessary build a request to read a page/node that is
             in the directory.
 
  Input -
     de      - Pointer to the directory entry
     actor   - Domain to queue
 
  Output - The following codes are returned:
        io_notmounted     0
        io_notreadable    1
        io_potincore      2     ioret is pointer to CTE for pot
        io_pagezero       3     ioret is *PCFA for virtual zero page
        io_cdalocked      5     CDA may already be in transit
        io_noioreqblocks  6
        io_built          8     Request built, ioret is *request
*********************************************************************/
static struct CodeIOret swapranges(DIRENTRY *de, NODE *actor)
{
   struct CodeIOret ret;   /* Our returned structure */
   REQUEST *req;           /* Only used if building a request */
   int secondresult;       /* return code from grtrsldl(de->second...*/
   struct CodeGRTRet cdl;  /* The value from grtsldl */
 
   if (de->pcfa.flags & (DIRENTDEVREQFIRST | DIRENTDEVREQSECOND)) {
      /* Already have I/O going, queue the actor */
      if (de->pcfa.cda[0] & 0x80) {    /* Entry for a node */
         if (de->pcfa.flags & DIRENTDEVREQFIRST)
            getenqio(pothash(de->first.devreq->swaploc), actor);
         else getenqio(pothash(de->first.swaploc), actor);
      } else {
         getenqio(cdahash(de->pcfa.cda), actor);
      }
      ret.code = io_cdalocked;
      return ret;
   }
   if (de->pcfa.flags & DIRENTCTEFIRST) {    /* First loc is a cte */
      ret.ioret.cte = de->first.cte;
      ret.code = io_potincore;
      return ret;
   }
   if (!de->first.cte) {       /* First is NULL, must be virtual zero */
      if (de->pcfa.flags & DIRENTVIRTUALZERO) {
         ret.ioret.pcfa = &de->pcfa;
         ret.code = io_pagezero;
         return ret;
      }
      crash("GDIRECTC023 DIRENTRY.first NULL and not virtual zero");
   }
 
   /* de->first is a swaploc */
 
   if (de->pcfa.cda[0] & 0x80) {    /* Node CDA */
      CTE *cte = getfpic(de->first.swaploc);
      if (cte) {                      /* We've found it */
         ret.ioret.cte = cte;
         ret.code = io_potincore;
         return ret;
      }
 
      /* Must read the disk - Get lock and request */
 
      if (getlock(pothash(de->first.swaploc), actor)) {
         req = acquirerequest();     /* We have the CDA lock */
         if (!req) {
            unbuildinputrequest(req, de);
            ret.code = io_noioreqblocks;
            return ret;
         }
         req->potaddress = de->first.swaploc;
         req->pcfa.flags = REQPOT;
      } else {
         ret.code = io_cdalocked;
         return ret;
      }
   } else {                           /* Page CDA */
 
      /* Must read the disk - Get lock and request */
 
      req = acquirerequest();      /* Get a request */
      if (!req) {
         unbuildinputrequest(req, de);
         ret.code = io_noioreqblocks;
         return ret;
      }
      req->pcfa = de->pcfa;
      req->pcfa.flags &= (REQCHECKREAD | REQGRATIS);
   }
#if DIRENTGRATIS != REQGRATIS || DIRENTCHECKREAD != REQCHECKREAD
#error DIRENTGRATIS != REQGRATIS || DIRENTCHECKREAD != REQCHECKREAD
#endif
 
   /* Finsh building the request to bring in the page or pot */
 
   req->direntry = de;
   if (de->second.cte) {     /* Second is not NULL */
      cdl = grtrsldl(de->second.swaploc);
      if (grt_mustread == (secondresult = cdl.code)) { /* Got devloc */
         DEVREQ *drq = acquiredevreq(req);
         if (!drq) {
            unbuildinputrequest(req, de);
            ret.code = io_noioreqblocks;
            return ret;
         }
         drq->device = cdl.ioret.readinfo.device;
         drq->offset = cdl.ioret.readinfo.offset;
         de->pcfa.flags |= DIRENTDEVREQSECOND;
         drq->flags |= DEVREQSWAPAREA;
         drq->swaploc = de->second.swaploc;
         de->second.devreq = drq;
         md_dskdevreqaddr(drq);
      }
   } else secondresult = grt_notreadable; /* No address-->can't read */
   cdl = grtrsldl(de->first.swaploc);
   if (grt_mustread != cdl.code) {    /* Problem with first location */
      if   (grt_notreadable == cdl.code &&
            grt_notreadable == secondresult) { /* Neither readable */
         unbuildinputrequest(req, de);
         ret.code = io_notreadable;
         return ret;
      } else {                         /* one not mounted */
         if (grt_mustread != secondresult) { /* 2nd not ok */
            unbuildinputrequest(req, de);
            ret.code = io_notmounted;
            return ret;
         }
      }
   } else {                           /* First location must read */
      DEVREQ *drq = acquiredevreq(req);
      if (!drq) {
         unbuildinputrequest(req, de);
         ret.code = io_noioreqblocks;
         return ret;
      }
      drq->device = cdl.ioret.readinfo.device;
      drq->offset = cdl.ioret.readinfo.offset;
      de->pcfa.flags |= DIRENTDEVREQFIRST;
      drq->flags |= DEVREQSWAPAREA;
      drq->swaploc = de->first.swaploc;
      de->first.devreq = drq;
      md_dskdevreqaddr(drq);
   }
   ret.ioret.request = req;
   ret.code = io_built;
   return ret;
} /* End swapranges */
 
 
/*********************************************************************
gdilook - Look for page/node in directory and if necessary build
          a request to read it.
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
     actor   - Domain to queue
 
  Output - The following codes are returned:
        io_notmounted     0
        io_notreadable    1
        io_potincore      2     ioret is pointer to CTE for pot
        io_pagezero       3     ioret is *PCFA for virtual zero page
        io_cdalocked      5     CDA may already be in transit
        io_noioreqblocks  6
        io_notindirectory 7     CDA not in requested directory(s)
        io_built          8     Request built, ioret is *request
*********************************************************************/
struct CodeIOret gdilook(const CDA cda, NODE *actor)
{
   DIRENTRY *de;
 
   if ( (de = finddirectoryentry(cda, workingdirectory)) ||
        (de = finddirectoryentry(cda, unmigrateddirectory)) ||
        (de = finddirectoryentry(cda, dataforapdirectory)) ||
        (de = finddirectoryentry(cda, journaldirectory)) ) {
      /* There is a directory entry for the item */
      return swapranges(de, actor);
   } else {
      struct CodeIOret ret;
      ret.code = io_notindirectory;
      return ret;
   }
} /* End gdilook */
 
 
/*********************************************************************
gdiblook - Look for page/node in the unmigrated directory and if
           necessary build a request to read it.
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
     actor   - Domain to queue
 
  Output - The following codes are returned:
        io_notmounted     0
        io_notreadable    1
        io_potincore      2     ioret is pointer to CTE for pot
        io_pagezero       3     ioret is *PCFA for virtual zero page
        io_cdalocked      5     CDA may already be in transit
        io_noioreqblocks  6
        io_notindirectory 7     CDA not in requested directory(s)
        io_built          8     Request built, ioret is *request
*********************************************************************/
struct CodeIOret gdiblook(CDA cda, NODE *actor)
{
   DIRENTRY *de;
 
   if ( (de = finddirectoryentry(cda, unmigrateddirectory)) ) {
      /* There is a directory entry for the item */
      return swapranges(de, actor);
   } else {
      struct CodeIOret ret;
      ret.code = io_notindirectory;
      return ret;
   }
} /* End gdiblook */
 
 
/*********************************************************************
gdilbv - Look up backup version of page/node and if necessary, build
          a request to read it.
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
     actor   - Domain to queue
 
  Output - The following codes are returned:
        io_notmounted     0
        io_notreadable    1
        io_potincore      2     ioret is pointer to CTE for pot
        io_pagezero       3     ioret is *PCFA for virtual zero page
        io_cdalocked      5     CDA may already be in transit
        io_noioreqblocks  6
        io_notindirectory 7     CDA not in requested directory(s)
        io_built          8     Request built, ioret is *request
*********************************************************************/
struct CodeIOret gdilbv(CDA cda, NODE *actor)
{
   DIRENTRY *de;
 
   if ( (de = finddirectoryentry(cda, unmigrateddirectory)) ||
        (de = finddirectoryentry(cda, dataforapdirectory)) ||
        (de = finddirectoryentry(cda, journaldirectory)) ) {
      /* There is a directory entry for the item */
      return swapranges(de, actor);
   } else {
      struct CodeIOret ret;
      ret.code = io_notindirectory;
      return ret;
   }
} /* End gdilbv */
 
 
/*********************************************************************
gdiladbv - Return allocation data for backup version of CDA
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
 
  Output - Pointer to the PCFA for the CDA or NULL
*********************************************************************/
PCFA *gdiladbv(CDA cda)
{
   DIRENTRY *de;
 
   if ( (de = finddirectoryentry(cda, unmigrateddirectory)) ||
        (de = finddirectoryentry(cda, dataforapdirectory)) ||
        (de = finddirectoryentry(cda, journaldirectory)) ) {
      /* There is a directory entry for the item */
      return &de->pcfa;
   } else return NULL;
} /* End gdiladbv */
 
 
/*********************************************************************
gdiladnb - Return allocation data for next backup version of CDA
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
 
  Output - Pointer to the PCFA for the CDA or NULL
*********************************************************************/
PCFA *gdiladnb(CDA cda)
{
   DIRENTRY *de;
 
   if ( (de = finddirectoryentry(cda, workingdirectory)) ) {
      /* There is a directory entry for the item */
      return &de->pcfa;
   } else return NULL;
} /* End gdiladnb */
 
 
/*********************************************************************
gdilkunm - Return swaploc from unmigrated directory
 
  Input -
     cda     - CDA for the page or node (defined by high bit)
               Must not be for a virtual zero page
 
  Output - first swaploc from the unmigrated directory or (0,0)
 
  Note: The assembler version included: after finding good swaploc
             TM CDA,X'80'
             IF NZ           A NODE CDA
               LR R2,R0      Pass potaddress
               L R15,=V(GETFPIC)  SEE IF POT IS IN MEMORY
               BALR R14,R15
                 IF 1111     IT IS IN MEMORY
                   LM R3,R15,GDIRECTSAVE+12
                   BR R14    RETURN CORETBEN
                 ENDIF ,
             JOIN ,
        This logic must now be performed by the caller
*********************************************************************/
RANGELOC gdilkunm(CDA cda)
{
   DIRENTRY *de;
   RANGELOC rl;
 
   if ( (de = finddirectoryentry(cda, unmigrateddirectory)) ) {
      /* There is a directory entry for the item */
      rl = readdirentfirst(de);
      if (!rl.range && !rl.offset)
         crash("GDIRECTC024 gdilkunm called for virtual zero page");
      return rl;
   }
   rl.range = 0;
   rl.offset = 0;
   return rl;
} /* End gdilkunm */
 
 
/*********************************************************************
migr2nextnonvzentry - Get next non-virtual zero directory entry for
           the MIGRATE(Migrate_ReadDirectory) ... operation
 
  Input - None
 
  Output -
     Pointer to the directory entry of NULL
*********************************************************************/
static DIRENTRY *migr2nextnonvzentry(void)
{
   DIRENTRY *de;
   for (;;) {
      de = *entryofmigratememory;  /* Check next entry */
      if (&dummyentry != de) {
         if   ( !(de->pcfa.cda[0] & 0x80) &&
                    de->pcfa.flags & DIRENTVIRTUALZERO) {
            /* Virtual zero pages only need allocation pot update */
            *entryofmigratememory = de->next;
            /* entryofmigratememory still points to prev */
            unmigrateddirectory->pageentries--;
            insertentryintodataforap(de);
         } else {                  /* Real pages and nodes */
            entryofmigratememory = &de->next;
            return de;
         }
      } else {
         hashlistofmigratememory++;
         if (hashlistofmigratememory >= numhashheads) {
            return NULL;
         }
         entryofmigratememory =
              &unmigrateddirectory->chains[hashlistofmigratememory];
      }
   }
} /* End migr2nextnonvzentry */
 
 
/*********************************************************************
gdimigr2 - Return next segment of unmigrated directory, used for the
           MIGRATE(Migrate_ReadDirectory) ... operation
 
  Input -
     p       - Pointer to a page size buffer for the data
 
  Output - Length of data stored in bytes, 0 means end of directory
*********************************************************************/
int gdimigr2(uchar *p)
{
   struct Migrate_DirectoryEntry *cur =
                      (struct Migrate_DirectoryEntry*)p;
   struct Migrate_DirectoryEntry *last =
                         cur + MIGRATE_DirectoryEntriesPerPage;
   DIRENTRY *de;
 
   for (;cur < last; cur++) {      /* Fill in entries in list */
      RANGELOC sl;
 
      de = migr2nextnonvzentry();  /* Check next entry */
      if (!de)
   break;                          /* End of directory */
      sl = readdirentfirst(de);    /* Pass first location */
      if (sl.range == 0)
         crash("GDIRECTC025 Virtual zero direntry not flagged");
      cur->first = grtslemi(sl);  /* Set first address */
       /* Same for second */
      if (de->pcfa.flags & DIRENTDEVREQSECOND)
         sl = de->second.devreq->swaploc;
      else sl = de->second.swaploc;
      if (sl.range != 0)
         cur->second = grtslemi(sl);  /* Set second address */
      else {
         cur->second.device = USHRT_MAX;
         cur->second.offset = USHRT_MAX;
      }
       /* Fill in CDA */
      Memcpy(cur->cda, de->pcfa.cda, sizeof(CDA));
   }
   if ((char *)cur - (char *)p) return (char *)cur - (char *)p;
   hashlistofmigratememory = -1;     /* Set up to read it again */
   entryofmigratememory = &dummyentry.next;
   return 0;
} /* End gdimigr2 */
 
 
/*********************************************************************
gdicoboc - Clean out a block of CDAs from the dataforap directory
           They are placed in the journal directory or limboed to
           the working or tobemigrated directories.
 
  Input -
     lowcda  - Lowest cda to process
     highcda - Highest+1 cda to process
     allopot - Point to CTE for allocation pot, NULL to put the cdas
               processed in limbo
 
  Output -
     returns 1 if any allocation pot entry updated;
*********************************************************************/
int gdicoboc(CDA lowcda, CDA highcda, CTE *allopot)
{
   DIRENTRY **chp = dataforapdirectory->chains;
   DIRENTRY **end = dataforapdirectory->chains + numhashheads;
   int updated = 0;
   uint32 first;                   /* Low 32 bits of lowcda */
 
   first = b2long(lowcda+2, 4);
   for (; chp < end; chp++) {
      DIRENTRY **prev = chp;
      DIRENTRY *de;
      for (de = *prev; &dummyentry != de; de = *prev) {
         if (cdacmp(de->pcfa.cda, highcda) >= 0)
      break;  /* This directory is in cda order, done with this chain */
         if (cdacmp(de->pcfa.cda, lowcda) >= 0) {
            /* CDA is within callers desired range */
            DIRECTORY *todir;      /* New directory for entry */
 
            *prev = de->next;      /* Remove from the chain */
            dataforapdirectory->pageentries--;  /* Reduce count */
            if (!allopot) {        /* Caller wants it in limbo */
               if (de->pcfa.cda[0] & 0x80)
                  crash("GDIRECTC026 Node cda in dataforap directory");
               if (de->pcfa.flags & DIRENTVIRTUALZERO)
                  todir = workingdirectory;
               else todir = unmigrateddirectory;
            } else {               /* Caller wants allo pot update */
               uint32 index;
 
               index = b2long(de->pcfa.cda+2, 4);
               index -= first;
               if (grtmap(&de->pcfa, allopot, index)) {
                  updated = 1;
                  de->pcfa.flags |= DIRENTALLOCATIONPOTMIGRATED;
               }
               todir = journaldirectory;
            }
            todir->pageentries++;   /* Increment count in new dir */
            {  register DIRENTRY **chainhead =
                  &(todir->chains[cdahash(de->pcfa.cda) & directoryhash]);
               de->next = *chainhead;
               *chainhead = de;
            }
         } else {                /* de is below our range */
            prev = &de->next;
         }
      }
   }
   setmigrationpriority();
   return updated;
} /* End gdicoboc */
 
 
/*********************************************************************
gdifncda - Find next cda in dataforap directory
 
  Input -
     mincda  - Lowest cda to find
 
  Output -
     Pointer to lowest cda >= mincda, or NULL if there is none
*********************************************************************/
uchar *gdifncda(const uchar *mincda)
{
   uint32 inputhash;              /* for directory hash */
   int i;                         /* Hash chain index */
   DIRENTRY *best = &dummyentry;  /* Start with best == dummy entry */
 
   Memcpy(dummyentry.pcfa.cda, mincda, sizeof(CDA));
   Memcpy(&inputhash, mincda, sizeof inputhash);
   i = (inputhash &= directoryhash);    /* 1st chain is hash of mincda */
   do {
      DIRENTRY *de = dataforapdirectory->chains[i];
      for (;
           cdacmp(de->pcfa.cda, mincda) < 0;
           de = de->next)         /* N.B. dummyentry cda == mincda */
         {;}                      /* Run chain until cda >= mincda */
      if (&dummyentry != de) {    /* We found one */
         if (!cdacmp(de->pcfa.cda, mincda)) {
            best = de;            /* Equal cda is best we can do */
   break;
         }
         if (&dummyentry == best) best = de; /* First found */
         else {                   /* See if we have a better cda */
            if (cdacmp(best->pcfa.cda, de->pcfa.cda) > 0)
               best = de;         /* New cda is < best (so far) cda */
         }
      }
      i = (i+1)&directoryhash;   /* Circularly increment index */
   } while (i != inputhash);
   if (&dummyentry == best) return 0;  /* None found */
   return best->pcfa.cda;
} /* End gdifncda */
 
 
/*********************************************************************
gdimigr3 - Initialize read of unmigrated directory
 
  Input - None
 
  Output - Internal cursors are initialized
*********************************************************************/
void gdimigr3(void)
{
   hashlistofmigr34cursor = 0;   /* start at first hash chain */
   entryofmigr34cursor = unmigrateddirectory->chains[0];
} /* End gdimigr3 */
 
 
/*********************************************************************
gdimigr4 - Read next unmigrated directory segment
 
  Input -
     p      - Pointer to a page to return the data in
 
  Output - Length of data in bytes, 0 means end of directory
           -1 means migration is urgent, no data returned
*********************************************************************/
int gdimigr4(uchar *p)
{
   char *cur = (char *)p;
   char *last = cur + (4096/sizeof(CDA));
 
   if (iosystemflags & MIGRATIONURGENT) {
      return -1;
   }
   for (;;) {                     /* Go through all the hash heads */
      if (&dummyentry != entryofmigr34cursor) { /* We have an entry */
         Memcpy(cur, entryofmigr34cursor->pcfa.cda, sizeof(CDA));
         entryofmigr34cursor = entryofmigr34cursor->next;
         if ( (cur += sizeof(CDA)) >= last )
            return cur - (char *)p;
      } else {                    /* End of hash chain */
         if (hashlistofmigr34cursor++ >= numhashheads)
            return cur - (char *)p;
         entryofmigr34cursor =
                unmigrateddirectory->chains[hashlistofmigr34cursor];
      }
   }
} /* End gdimigr4 */
/*
         POP USING
         EJECT
*.
* GDILUFJ - LOOKUP FOR JOURNAL WRITE
*
* INPUT -
*        R1  - ADDRESS OF CDA OF PAGE TO LOOK UP
*              PAGE MUST NOT BE VIRTUAL ZERO
*        R4  - ADDRESS OF REQUEST BLOCK
*        R14 - RETURN ADDRESS
*        R15 - ENTRY POINT ADDRESS
*
* OUTPUT -
*        If page is in the backup swap area, DEVREQs to write to the
*        backup location(s) are built and chained to the REQUEST.
*        R14+0  - A swap location is not mounted.
*              Some DEVREQs may have been built.
*        R14+4  - Couldn't get a DEVREQ. Some DEVREQs may be built.
*        R14+8  - Any necessary swap area DEVREQs were built.
*.
         SPACE 2
         PUSH USING
         ENTRY GDILUFJ
GDILUFJ  GMODSTRT GDIRECTSAVE
         L   3,UNMIGRATEDDIRECTORY  LOOK UP IN THE UNMIGRATED DIRECTORY
         BAL 10,FINDDIRECTORYENTRY   ...
             B   GDILUFJRETURNDIRECTORYENTRY     FOUND - GO
         L   3,DATAFORAPDIRECTORY  NO  -  TRY THE DATAFORAP DIRECTORY
         BAL 10,FINDDIRECTORYENTRY   ...
             B GDILUFJRETURNDIRECTORYENTRY       FOUND
         L   3,JOURNALDIRECTORY  NOT FOUND - CHECK JOURNALDIRECTORY
         BAL 10,FINDDIRECTORYENTRY
         IF  1111            found in backup swap area
GDILUFJRETURNDIRECTORYENTRY  DS  0H
             LR R3,R4        MOVE DIRENTRY POINTER
             USING DIRENTRY,R3
             TM  DIRENTFLAGS,DIRENTVIRTUALZERO    VIRTUAL ZERO?
             CRASH NZ 27     SHOULD HAVE CHECKED THIS ALREADY
             TM DIRENTFLAGS,DIRENTDEVREQFIRST+DIRENTDEVREQSECOND
             IF NZ           THERE IS I/O GOING ON
               BAL R10,CANCELIO   Journaling is more important
             JOIN ,
             ICM 0,15,DIRENTFIRST    LOAD FIRST SWAP RANGE
             CRASH NL 28             CRASH IF A CORETBEN
             L R4,GDIRECTSAVE+4*R4     RESTORE ADDR OF REQUEST
             USING REQUEST,R4
             ST R3,REQDIRENTRY   Save pointer
* Check first swaploc.           Used to call GRTSTODL in assembler
             L  15,=V(GRTSL2DL)   Convert first swaploc to devloc
             BALR R14,R15         Get R0=Offset, R1=Device
               IF 1111       Not mounted
GDILUFJNOTMOUNTED DS 0H
                 LM R0,R15,GDIRECTSAVE
                 BR R14      Return - not mounted
               ENDIF ,
             L   15,=V(GDDACDRQ) GET A DEVREQ
             BALR 14,15          ...
             IF L            Couldn't get one
GDILUFJNOBLOCKS DS 0H
               LM R0,R15,GDIRECTSAVE
               B 4(R14)      Return - no blocks available
             ENDIF ,
             USING DEVREQ,R5
             LA  15,1            INCRMENT REQCOMPLETIONCOUNT
             AH  15,REQCOMPLETIONCOUNT   ...
             STH 15,REQCOMPLETIONCOUNT   ...
             OI DIRENTFLAGS,DIRENTDEVREQFIRST
             OI DEVREQFLAGS,DEVREQSWAPAREA
             MVC DEVREQSWAPLOC,DIRENTFIRST    SAVE THIS HERE
             ST R5,DIRENTFIRST
* Check second swaploc.
             ICM R0,15,DIRENTSECOND   AND SECOND      In assembler
             IF NZ                   IF THERE IS ONE  called GRTSTODL
               L  15,=V(GRTSL2DL)   Convert second swaploc to devloc
               BALR R14,R15         Get R0=Offset, R1=Device
                 B GDILUFJNOTMOUNTED   Not mounted
               L   15,=V(GDDACDRQ) GET A DEVREQ
               BALR 14,15          ...
               BL GDILUFJNOBLOCKS Couldn't get one
               USING DEVREQ,R5
               LA  15,1            INCRMENT REQCOMPLETIONCOUNT
               AH  15,REQCOMPLETIONCOUNT   ...
               STH 15,REQCOMPLETIONCOUNT   ...
               OI DIRENTFLAGS,DIRENTDEVREQSECOND
               OI DEVREQFLAGS,DEVREQSWAPAREA
               MVC DEVREQSWAPLOC,DIRENTSECOND    SAVE THIS HERE
               ST R5,DIRENTSECOND
             JOIN ,
         JOIN ,
*        Here if not found in backup directory.
*        No swap area DEVREQs need be built.
         LM R0,R15,GDIRECTSAVE
         B 8(R14)            Return - any needed DEVREQs were built
         POP USING
*/

