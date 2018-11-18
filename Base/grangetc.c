/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GRANGETC - Range table module - KeyTech Disk I/O */
 
// #include <string.h>
#include <limits.h>
#include <stdio.h>
#include "lli.h"
#include "sysdefs.h"
#include "kerinith.h"
#include "sysparms.h"
#include "migrate.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "spaceh.h"
#include "ioworkh.h"
#include "getih.h"
#include "geteh.h"
#include "disknodh.h"
#include "pdrh.h"
#include "devmdh.h"
#include "gbadpagh.h"
#include "gswapah.h"
#include "rngtblh.h"
#include "gsw2grth.h"
#include "gdi2grth.h"
#include "grangeth.h"
#include "grt2gclh.h"
#include "grt2geth.h"
#include "grt2gdih.h"
#include "grt2geth.h"
#include "grt2gmih.h"
#include "grt2greh.h"
#include "grt2grsh.h"
#include "grt2guph.h"
#include "alocpoth.h"
#include "memomdh.h"
#include "sysgenh.h"
#include "mi2mdioh.h"
#include "queuesh.h"
#include "consmdh.h"
#include "timemdh.h"
#include "memutil.h"

 
/* In converting the 370 Assembler version of this module to C, note */
/* was taken of a long desired modfication to allow binary search */
/* lookup of the rangetable. The design approach is to define two */
/* parallel tables (one for swap ranges and one for user ranges), */
/* consisting of (at a minimum) the first CDA or SBA in the range, */
/* and a pointer to the corresponding range table entry. These tables */
/* table would be kept in CDA/SBA sort order and searched with a */
/* binary search. The places in the code needing modification for */
/* this change have been marked with: */
         /* $$$$$ Modify here for binary search lookup $$$$$*/
 
 
 
/* Local static constants */
 
static const CDA cdazero = {0, 0, 0, 0, 0, 0};
static const uint64 timezero = 0;
static const LLI llione = {0, 1};
 
 
/* Local static variables */
 
static PCFA localpcfa;          /* For returning allocation data */
 
static enum {
   NEXTNONE,              /* Signal end of file */
   NEXTISNEXTDEVLOC,      /* Data is a new devloc to return */
   NEXTISNEXTWRITELOC     /* Similar to above for write */
} nexttype;        /* Type of data saved for grtnext */
static RANGELIST *nextrangelist;    /* Next rangelist to return */
static uint32 nextoffset;    /* Offset in range to use */
                             /* Following for NEXTISNEXTWRITELOC */
static
RANGETABLE *nextrangetable;     /* Range table entry being used */
 
static
DEVICE *npdrdevicepointer;    /* Next PDR device for grtnpdr */
 
static
RANGETABLE *tapeckptcursor = swapranges; /* For grtcdap0 & grtcdap1 */
 
 
/*********************************************************************
iranget - Initialize the range table and range list
 
  Input - 
     rangelists - Pointer to the area to allocate rangelists in
     number     - Number of rangelists to allocate
 
  Output - None
*********************************************************************/
void iranget(char *rangelists, int number)
{
   int i;
   RANGELIST *rl = (RANGELIST *)rangelists;
 
      /* Initialize the range table */
 
   memzero(userranges, sizeof(RANGETABLE) * MAXUSERRANGES);

   memzero(swapranges, sizeof(RANGETABLE) * MAXSWAPRANGES);
 
      /* Initialize linked list of available RANGELIST entries */
 
   rangelistbase = NULL;
   for (i=0; i < number; i++) {
      rl->next = rangelistbase;
      rangelistbase = rl;
      rl++;
   }
   
   npdrdevicepointer = ldevlast;
}  /* End iranget */
 
 
/*********************************************************************
clearallocationpotsandmigrationinfo - Remove all allocation pots for
                the range from storage and ensure any pages being
                migrated are correctly put in limbo when a range is
                dismounted
 
  Input -
     rt       - Pointer to the range table entry being dismounted
 
  Output - None
*********************************************************************/
static void clearallocationpotsandmigrationinfo(RANGETABLE *rt)
{
   int rangeindex = rt - rangetable;
   struct GdiGAPMParm parm;
   CTE **prev = &allocationpotchainhead;
   CTE *cte;
   LLI temp, temp2;
 
   for (cte = *prev; cte; cte = *prev) {
      if (rangeindex == cte->use.pot.potaddress.range) { /* Our range */
         if (iosystemflags & MIGRATIONINPROGRESS) {      /* Migrating */
            b2lli(rt->dac.cda.first, sizeof(CDA), &temp);
            temp2.hi = 0;             /* Get low cda described by pot */
            temp2.low += (cte->use.pot.potaddress.offset /
                             (APOTDATACOUNT-1)) * APOTDATACOUNT;
            lliadd(&temp, &temp2);
            lli2b(&temp, parm.rangelow, sizeof(CDA));
 
            temp2.low = APOTDATACOUNT-1;          /* and get high cda */
            lliadd(&temp, &temp2);
            lli2b(&temp, parm.rangehigh, sizeof(CDA));
            gdifapm(parm);
         }
         *prev = cte->hashnext;         /* Remove CTE from hash chain */
         gspmpfa(cte);                  /* And free the page */
      } else prev = &cte->hashnext;
   }
} /* End clearallocationpotsandmigrationinfo */
 
 
/*********************************************************************
grtsl2dl - Return device and offset of a swap area page/pot
 
  Input -
     sl       - Rangeloc of the swap area page/node pot
 
  Output -
     Returns grt_notmounted or grt_mustread
*********************************************************************/
struct CodeGRTRet grtsl2dl(RANGELOC sl)
{
   struct CodeGRTRet ret;
   RANGETABLE *rt = rangetable + sl.range;
 
   if (rt->devlist) {
      ret.ioret.readinfo.device = rt->devlist->device;
      ret.ioret.readinfo.offset = rt->devlist->firstpage + sl.offset;
      ret.ioret.readinfo.id.potaddress = sl;
      ret.code = grt_mustread;
   } else ret.code = grt_notmounted;
   return ret;
} /* End grtsl2dl */
 
 
/*********************************************************************
grtrsldl - Return device and offset of a not known bad swap area
           page/pot
 
  Input -
     sl       - Rangeloc of the swap area page/node pot
 
  Output -
     Returns grt_notmounted, grt_notreadable, or grt_mustread
*********************************************************************/
struct CodeGRTRet grtrsldl(RANGELOC sl)
{
   struct CodeGRTRet ret;
 
   ret = grtsl2dl(sl);
   if (grt_mustread == ret.code) {
      if (gbadread(ret.ioret.readinfo.device,
          ret.ioret.readinfo.offset)) {
         ret.ioret.rangeloc = sl;
         ret.code = grt_notreadable;
      }
   }
   return ret;
} /* End grtrsldl */
 
 
/*********************************************************************
grtfdsba - Find devloc for Swap Block Address  (formally GRTFDFCC)
 
  Input -
     sba      - Swap Block Address to convert
 
  Output -
     Returns grt_notmounted or grt_mustread.
     N.B. ret.ioret.readinfo.id.potaddress is NOT set up since
          current callers only use it for accessing disk directory
          blocks and the checkpoint header.
*********************************************************************/
struct CodeGRTRet grtfdsba(uint32 sba)
{
   struct CodeGRTRet ret;
   RANGETABLE *srt = rangetable - 1;
 
   /* $$$$$ Modify here for binary search lookup $$$$$*/
   for ( ; srt >= swapranges; srt--) {
      if (sba >= srt->dac.sba.first && sba <= srt->dac.sba.last) {
         /* sba is in this range */
         if (srt->devlist) {
            ret.ioret.readinfo.offset = srt->devlist->firstpage +
                            sba - srt->dac.sba.first;
            ret.ioret.readinfo.device = srt->devlist->device;
            ret.code = grt_mustread;    /* devloc returned */
            return ret;
         } else {
            ret.code = grt_notmounted;  /* Range not mounted */
            return ret;
         }
      }
   }
   ret.code = grt_notmounted;       /* Range not found */
   return ret;
} /* End grtfdsba */
 
 
/*********************************************************************
findswaparearange - Get rangetable entry for a checkpoint cda
 
  Input -
     sba      - The Swap Block Address to look up
 
  Output -
     Returns a pointer to the range table entry or NULL
*********************************************************************/
static RANGETABLE *findswaparearange(uint32 sba)
{
   RANGETABLE *srt = rangetable - 1;
 
   /* $$$$$ Modify here for binary search lookup $$$$$*/
   for ( ; srt >= swapranges; srt--) {
      if (sba >= srt->dac.sba.first  && sba <= srt->dac.sba.last) {
         return srt;
      }
   }
   return NULL;
} /* End findswaparearange */
 
 
/*********************************************************************
finduserrangetable - Get rangetable entry for a cda
 
  Input -
     cda      - Pointer to the checkpoint area cda
 
  Output -
     Returns a pointer to the range table entry or NULL
*********************************************************************/
static RANGETABLE *finduserrangetable(const CDA cda)
{
   RANGETABLE *rt = userranges;
 
   /* $$$$$ Modify here for binary search lookup $$$$$*/
   for ( ; rt < userranges + MAXUSERRANGES; rt++) {
      if (cdacmp(rt->dac.cda.first, cda) <= 0 &&
             cdacmp(rt->dac.cda.last, cda) >= 0) {
         return rt;
      }
   }
   return NULL;
} /* End finduserrangetable */
 
 
/*********************************************************************
freerangelist - Return a rangelist entry to the free pool
 
  Input -
     rl       - Pointer to the rangelist entry
 
  Output - None
*********************************************************************/
static void freerangelist(RANGELIST *rl)
{
   rl->next = rangelistbase;
   rangelistbase = rl;
   rangelistinuse--;
} /* End freerangelist */
 
 
/*********************************************************************
checkmigrationtod - See if range time stamp is obsolete
 
  Input -
     pdrd     - Pointer to the Range Descriptor from the PDR
     rt       - Pointer to the rangelist entry
 
  Output -
     Returns: one if range is obsolete because of tods, zero otherwise
*********************************************************************/
static int checkmigrationtod(PDRD *pdrd, RANGETABLE *rt)
{
   RANGELIST *rl;
   uint64 pdrtime;
   int comp;
 
   //b2lli((uchar*)pdrd->migrationtod, sizeof pdrd->migrationtod, &pdrtime);
   pdrtime = *pdrd->migrationtod;
   //comp = llicmp(&pdrtime, &rt->migrationtod);
   if (pdrtime == rt->migrationtod) return 0;        /* TODs are the same */
{char str[80];  /*...*/
 sprintf(str,"obs %d cda=%lx %lx\n",comp,
         b2long(pdrd->first,2), b2long(pdrd->first+2,4));
 consprint(str);}
   if (pdrtime < rt->migrationtod) return 1;     /* This range is obsolete */
      /* Ranges we already have are obsolete */
   rt->migrationtod = pdrtime; /* Set new time stamp */
   for (rl = rt->devlist; rl; rl = rl->next) {
      rl->flags |= RANGELISTOBSOLETE;
      rl->flags &= ~(RANGELISTUPDATEPDR
                     | RANGELISTRESYNCINPROGRESS
                     | RANGELISTWAITFORCHECKPOINT);
      obsoleteranges++;              /* Count obsolete instances */
   }
   enqmvcpu(&resyncqueue);  /* Start resyncing */
   return 0;                /* Input range is the current one */
} /* End finduserrangetable */
 
 
/*********************************************************************
countmissinginstances - Counts the number of missing range instances
 
  Input - None
 
  Output -
     missinginstances in iowork is updated
*********************************************************************/
static void countmissinginstances(void)
{
   RANGETABLE *rt = userranges;
 
   missinginstances = 0;
   for ( ; rt < userranges + MAXUSERRANGES; rt++) {
      int i = rt->nplex;
      RANGELIST *rl = rt->devlist;
 
      for ( ; rl; rl = rl->next) i--;
      if (i > 0) missinginstances += i;
   }
} /* End countmissinginstances */
 
 
struct RangeTableAndLoc {      /* Value returned by calcrangeloc */
   RANGETABLE *rt;
   RANGELOC rl;
};
 
/*********************************************************************
calcrangeloc - Calculate a RANGELOC from a cda
 
  Input -
     cda     - Pointer to the cda to convert
 
  Output -
     Returns ret.rt = NULL if range for cda not mounted.
*********************************************************************/
static struct RangeTableAndLoc calcrangeloc(const CDA cda)
{
   struct RangeTableAndLoc ret;
 
   ret.rt = finduserrangetable(cda);
   if (ret.rt) {              /* There is one */
      uint32 offset;
 
      ret.rl.range = ret.rt - rangetable;
      offset = b2long(cda+2, 4);
      offset -= b2long(ret.rt->dac.cda.first+2, 4);
        /* An offset in range is always less than 2**32 so the above */
        /* subtract works. (In fact it is less than 2**16.) */
      if (cda[0] & 0x80) {       /* CDA is a node cda */
         offset /= NPNODECOUNT;       /* Get pot offset in range */
      } else {                     /* CDA is a page cda */
         offset = offset/APOTDATACOUNT + offset + 1;
      }
      ret.rl.offset = offset;
   }
   return ret;
} /* End calcrangeloc */
 
 
/*********************************************************************
readablerangeloc - Check if a rangeloc is readable/mounted
 
  Input -
     offset  - Offset in range from the rangeloc
     rt      - Pointer to the range table entry for the range
 
  Output -
     grt_notmounted
     grt_notreadable
     grt_mustread
 
     Sets nexttype, nextoffset, nextrangelist for grtnext
*********************************************************************/
static struct CodeGRTRet readablerangeloc(uint32 offset, RANGETABLE *rt)
{
   struct CodeGRTRet ret;
   int examined = 0;             /* Number of range lists examined */
   RANGELIST *rl = rt->devlist;
 
   for ( ; rl; rl = rl->next) {
      examined++;                /* Looked at one more */
      if (!(rl->flags & RANGELISTOBSOLETE)) {
         ret.ioret.readinfo.device = rl->device;
         ret.ioret.readinfo.offset = offset + rl->firstpage;
         if (!gbadread(ret.ioret.readinfo.device,
                       ret.ioret.readinfo.offset)) {
               /* Disk location is readable */
            nexttype = NEXTISNEXTDEVLOC;
            nextrangelist = rl->next;
            nextrangetable = rt;
            nextoffset = offset;
            ret.code = grt_mustread;
            return ret;
         }
      }
   }
   if (examined == rt->nplex) {
      ret.ioret.rangeloc.range = rt - rangetable;
      ret.ioret.rangeloc.offset = offset;
      ret.code = grt_notreadable;
   }
   else ret.code = grt_notmounted;
   return ret;
} /* End readablerangeloc */
 
 
/*********************************************************************
checkuserrange -  Check range descriptor & rangetable compatibility
 
  Input -
     pdrd    - Pointer to the Pack Discriptor Range Descriptor
     rt      - Pointer to the range table entry for a swap range
 
  Output - See codes below
*********************************************************************/
#define checkrange_identical 0
#define checkrange_reject    1
#define checkrange_nooverlap 2
static int checkuserrange(PDRD *pdrd, RANGETABLE *rt)
{
   if (cdacmp(rt->dac.cda.first, pdrd->first) == 0
         && cdacmp(rt->dac.cda.last, pdrd->last) == 0
         && pdrd->type == rt->type)
      return checkrange_identical;
   /* first and last sbas or types not identical */
 
   if ((pdrd->first[0] ^ rt->dac.cda.first[0]) & 0x80)
      return checkrange_nooverlap;
   if (cdacmp(pdrd->first, rt->dac.cda.first) <= 0
       && cdacmp(pdrd->last, rt->dac.cda.first) <= 0)
      return checkrange_nooverlap;
   if (cdacmp(pdrd->first, rt->dac.cda.last) >= 0)
      return checkrange_nooverlap;
   return checkrange_reject;
} /* End checkuserrange */
 
 
/*********************************************************************
checkswaprange - Check range descriptor & rangetable for compatibility
 
  Input -
     pdrd    - Pointer to the Pack Discriptor Range Descriptor
     rt      - Pointer to the range table entry for a swap range
     first   - The sba.first from the PDRD
     last    - The sba.last from the PDRD
 
  Output - See codes from checkuserrange
*********************************************************************/
static int checkswaprange(PDRD *pdrd, RANGETABLE *rt,
                          uint32 first, uint32 last)
{
   if (rt->dac.sba.first == first && rt->dac.sba.last == last
         && pdrd->type == rt->type)
      return checkrange_identical;
   /* first and last sbas or types not identical */
   if (first <= rt->dac.sba.first
       && last <= rt->dac.sba.first) return checkrange_nooverlap;
   if (first >= rt->dac.sba.last) return checkrange_nooverlap;
   return checkrange_reject;
} /* End checkswaprange */
 
 
/*********************************************************************
grtfadfp - Find allocation id and flags for a page
 
  Input -
     rl      - The rangeloc of the page
     cda     - A pointer the the cda of the page
 
  Output -
     io_readpot with rangeloc of pot or
     io_allocationdata with *PCFA for cda
 
  Notes:
     1. Changes the I/O window
*********************************************************************/
struct CodeIOret grtfadfp(RANGELOC rl, const CDA cda)
{
   struct CodeIOret ret;
   RANGELOC aploc;
   CTE *cte;
 
   aploc = rl;
   aploc.offset -= rl.offset % (APOTDATACOUNT+1);
 
   /* Search for the allocation pot in main storage */
 
   for (cte = allocationpotchainhead; cte; cte = cte->hashnext) {
      if (cte->use.pot.potaddress.range == aploc.range &&
            cte->use.pot.potaddress.offset == aploc.offset) {
         struct AlocPot *pot = (struct AlocPot *)
                           map_window(QUICKWINDOW, cte, MAP_WINDOW_RO);
         int index = rl.offset - aploc.offset - 1;
 
         localpcfa.flags = pot->entry[index].flags;
         Memcpy(localpcfa.cda, cda, sizeof(CDA));
         Memcpy(&localpcfa.allocationid,
                pot->entry[index].allocationid,
                sizeof localpcfa.allocationid);
         ret.ioret.pcfa = &localpcfa;
         ret.code = io_allocationdata;
         return ret;
      }
   }
   ret.ioret.rangeloc = aploc;
   ret.code = io_readpot;
   return ret;
} /* End grtfadfp */
 
 
/*********************************************************************
grtmap - Migrate allocation pot
 
  Input -
     pcfa    - The cda, flags, & allocation id to be place in the pot
     allopot - Pointer to the CTE of the allocation pot to update
     index   - Index of the entry to update in the allocation pot
 
  Output -
     return 1 if the pot has been changed, otherwise zero
 
  Notes:
     1. Changes the I/O window
*********************************************************************/
int grtmap(PCFA *pcfa, CTE *allopot, uint32 index)
{
   struct AlocPot *pot;
   int flagssave = allopot->flags;   /* Save current ctchanged bit */
   ALOCDATA *ad;
 
   if (index >= APOTDATACOUNT)
       crash("GRANGETC030 Bad allocation pot index passed to grtmap");
   pot =
     (struct AlocPot *)map_window(QUICKWINDOW, allopot, MAP_WINDOW_RW);
   allopot->flags = flagssave;       /* Restore after map_window */
   ad = pot->entry + index;
   if   (Memcmp(ad->allocationid, &pcfa->allocationid,
                sizeof ad->allocationid)
         || (ad->flags ^ pcfa->flags) & ~ADATAINTEGRITY) {
      Memcpy(ad->allocationid, &pcfa->allocationid,
                sizeof ad->allocationid);
      ad->flags = pcfa->flags;
      allopot->flags |= ctchanged;
      allopot->extensionflags |= ctkernellock;
      return 1;
   }
   return 0;
} /* End grtmap */
 
 
/*********************************************************************
grtintap - Update the integrity data in an allocation pot
 
  Input -
     allopot - Pointer to the CTE of the allocation pot to update
 
  Output - None
 
  Notes:
     1. Changes the I/O window
*********************************************************************/
void grtintap(CTE *allopot)
{
   struct AlocPot *pot =
     (struct AlocPot *)map_window(QUICKWINDOW, allopot, MAP_WINDOW_RW);
   uchar *cb = &pot->checkbyte;
 
   pot->entry[0].flags += ADATAINTEGRITYONE;
   pot->checkbyte = (pot->entry[0].flags & ADATAINTEGRITY)
                     | (~(*(cb-1)) & (UCHAR_MAX - ADATAINTEGRITY));
} /* End grtintap */
 
 
/*********************************************************************
grthomep - Find first home devloc for a page
 
  Input -
     cda     - A pointer the the cda of the page
 
  Output -
     grt_notmounted
     grt_notreadable
     grt_readallopot with rangeloc of pot or
     grt_mustread with first device, offset, and *PCFA for cda
 
  Notes:
     1. Changes the I/O window
*********************************************************************/
struct CodeGRTRet grthomep(const CDA cda)
{
   struct CodeGRTRet ret;
   struct CodeIOret pd;
   struct RangeTableAndLoc r;
 
   r = calcrangeloc(cda);
   if (r.rt) {                /* Found a range table entry */
      pd = grtfadfp(r.rl, cda);
      if (io_allocationdata == pd.code) {
         ret = readablerangeloc(r.rl.offset, r.rt);
         ret.ioret.readinfo.id.pcfa = pd.ioret.pcfa;
      } else {
         ret.ioret.rangeloc = pd.ioret.rangeloc;
         ret.code = grt_readallopot;
      }
      return ret;
   }
   ret.code = grt_notmounted;
   return ret;
} /* End grthomep */
 
 
/*********************************************************************
grthomen - Find first home devloc for a node
 
  Input -
     cda     - A pointer the the cda of the node
 
  Output -
     grt_notmounted
     grt_notreadable
     grt_potincore with pointer to the CTE of the node pot frame
     grt_mustread with first device, offset, and *PCFA for cda
 
  Notes:
     1. Changes the I/O window
*********************************************************************/
struct CodeGRTRet grthomen(CDA cda)
{
   struct CodeGRTRet ret;
   struct RangeTableAndLoc r;
 
   r = calcrangeloc(cda);
   if (r.rt) {                /* Found a range table entry */
      CTE *cte = getfpic(r.rl);
      if (cte) {
         ret.ioret.cte = cte;
         ret.code = grt_potincore;
         return ret;
      }
      ret.ioret.rangeloc = r.rl;
      ret = readablerangeloc(r.rl.offset, r.rt);
      ret.ioret.readinfo.id.potaddress = r.rl;
      return ret;
   }
   ret.code = grt_notmounted;
   return ret;
} /* End grthomen */
 
 
/*********************************************************************
grtnext - Return the next devloc for a home I/O operation
 
  Input - None
 
  Output -
     returns next device and offset on device.
     ret.device is NULL when there are no more devices.
          If ret.device is NULL and operation is a home write then
               ret.offset = 0 if the PDRs are OK
               ret.offset != 0 If the PDRs must be updated
*********************************************************************/
struct GRTReadInfo grtnext(void)
{
   struct GRTReadInfo ret;
 
   switch (nexttype) {
    case NEXTNONE:
      ret.device = NULL;
      ret.offset = 0;
      return ret;
    case NEXTISNEXTDEVLOC:
      for (;;) {
         RANGELIST *rl = nextrangelist;
         if (!rl) {                   /* End of the range list */
            nexttype = NEXTNONE;
            ret.device = NULL;
            ret.offset = 0;
            return ret;
         }
         ret.device = rl->device;
         ret.offset = rl->firstpage + nextoffset;
         nextrangelist = rl->next;
         if (!(rl->flags & RANGELISTOBSOLETE)
             && !gbadread(ret.device, ret.offset))
            return ret;
      }
    case NEXTISNEXTWRITELOC:
      {
         RANGELIST *rl = nextrangelist;
         if (!rl) {                   /* End of the range list */
            RANGETABLE *rt = nextrangetable;
 
            ret.offset = 0; /* Assume that PDRs are OK */
            if (rt->migrationtod == timezero ||
                    rt->flags & RANGETABLEOBSOLETEHAVETHISTOD) {
               for (rl = rt->devlist; rl; rl = rl->next) {
                  if (!(rl->flags & RANGELISTOBSOLETE)) {
                     rl->flags |= RANGELISTUPDATEPDR;
                     rt->flags |= RANGETABLEUPDATEPDR;
                     rt->flags &= ~RANGETABLEOBSOLETEHAVETHISTOD;
                  }
               }
               ret.offset = 1; /* Must update PDRs */
            }
            nexttype = NEXTNONE;
            ret.device = NULL;
            /* offset set above */
            return ret;
         }
         ret.device = rl->device;
         ret.offset = rl->firstpage + nextoffset;
         nextrangelist = rl->next;
         return ret;
      }
    default: crash("vnoeww"); // don't know whether this path is valid.
   }
} /* End grtnext */
 
 
/*********************************************************************
grtnplex - Return whether thare is more than 1 home copy of CDA
 
  Input -
     cda     - Pointer to the CDA to test
 
  Output -
     returns: zero if only one copy, non-zero if more than one.
*********************************************************************/
int grtnplex(CDA cda)
{
   RANGETABLE *rt = finduserrangetable(cda);
 
   if (rt && rt->nplex > 1) return 1;
   return 0;
} /* End grtnplex */
 
 
/*********************************************************************
grtadd - Add a range instance to the range table
 
  Input -
     pdrd    - Pointer to the range descriptor
     dev     - Pointer to the device
 
  Output -
     returns: zero if range instance could not be added, otherwise one.
*********************************************************************/
int grtadd(PDRD *pdrd, DEVICE *dev)
{
   int obsolete = 0;
   RANGETABLE *rtent = NULL;
   RANGELIST *rl = rangelistbase;
   uint16 nplex;          /* local version of nplex from pdrd */
 
   if (pdrd->offset[0] != 0 || pdrd->offset[1] != 0) return 0;
   if (!rl) return 0;
   rangelistbase = rl->next;
   if (rangelistmaxused < ++rangelistinuse)
      rangelistmaxused = rangelistinuse;
#if PDRDSWAPAREA1 <= PDRDCHECKPOINTHEADER ||         \
                      PDRDSWAPAREA2 <= PDRDCHECKPOINTAREA
#error PDRD swap area codes must be greater than checkpoint code
#endif
   if (pdrd->type >= PDRDCHECKPOINTHEADER) {
      /* Range is a swap range */
      uint32 first, last;
      RANGETABLE *rt;
 
      Memcpy(&first, pdrd->first+2, sizeof first);
      Memcpy(&last, pdrd->last+2, sizeof last);
      if (pdrd->first[0] != 0 || pdrd->first[1] != 0
            || pdrd->last[0] != 0 || pdrd->last[1] != 0
              /* either first or last sba address > 4 bytes */
            || first > last || last - first > USHRT_MAX) {
              /* First sba greater than last sba or too many frames */
         freerangelist(rl);      /* Reject pack */
         return 0;
      }
      for (rt = rangetable - 1; rt >= swapranges; rt--) {
         if (!rt->dac.sba.first) {
            rtent = rt;
      break;
         } else {
            switch (checkswaprange(pdrd, rt, first, last)) {
             case checkrange_identical:
               rtent = rt;
             break;
             case checkrange_reject:
               freerangelist(rl);      /* Reject pack */
               return 0;
             case checkrange_nooverlap:
             break;
            }
            if (rtent)
      break;
         }
      }
   } else {                  /* Range is a user range */
      LLI first, last;
      uint32 framecount;     /* number of disk frames in range */
      RANGETABLE *rt;
 
      b2lli(pdrd->first, sizeof(CDA), &first);
      b2lli(pdrd->last, sizeof(CDA), &last);
      llisub(&last, &first);
      if (last.hi) {
         freerangelist(rl);      /* Reject pack */
         return 0;
      }
      if (pdrd->first[0] & 0x80) {     /* Range is a node range */
         framecount = (last.low + NPNODECOUNT-1) / NPNODECOUNT;
      } else {
         framecount = (last.low+APOTDATACOUNT)/APOTDATACOUNT+last.low;
      }
      if (framecount > USHRT_MAX) {
         freerangelist(rl);      /* Reject pack */
         return 0;
      }
      for (rt = rangetable+MAXUSERRANGES-1; rt >= rangetable; rt--) {
         if (!rt->devlist) {
            rtent = rt;
         } else {
            switch (checkuserrange(pdrd, rt)) {
             case checkrange_identical:
               rtent = rt;
               if (rt->flags & RANGETABLEUPDATEPDR) obsolete = 1;
               else obsolete |= checkmigrationtod(pdrd, rtent);
             break;
             case checkrange_reject:
               freerangelist(rl);      /* Reject pack */
               return 0;
             case checkrange_nooverlap:
      continue;
            }
            if (rtent)
      break;
         }
      }
   }
   if (!rtent) {            /* No range table or place for one */
      freerangelist(rl);       /* Reject pack */
      return 0;
   }
   if (rtent >= rangetable
            ? !cdacmp(rtent->dac.cda.first, cdazero)
            : !rtent->devlist) {   /* Format new range table entry */
       /* $$$$$ Modify here for binary search lookup $$$$$*/
      if (rtent >= rangetable) {
         Memcpy(rtent->dac.cda.first, pdrd->first, sizeof(CDA));
         Memcpy(rtent->dac.cda.last, pdrd->last, sizeof(CDA));
      } else {
         Memcpy(&rtent->dac.sba.first, pdrd->first+2, sizeof(uint32));
         Memcpy(&rtent->dac.sba.last, pdrd->last+2, sizeof(uint32));
      }
      Memcpy(&rtent->migrationtod, pdrd->migrationtod,
             sizeof rtent->migrationtod);
      rtent->devlist = NULL;
      Memcpy(&rtent->nplex, pdrd->nplex, sizeof rtent->nplex);
      rtent->type = pdrd->type;
      rtent->flags = 0;
   }
   Memcpy(&nplex, &pdrd->nplex, sizeof nplex);
   if (rtent->nplex < (sint16)nplex) {
      rtent->nplex = nplex;   /* Remember highest nplex */
   }
   Memcpy(&rl->firstpage, pdrd->offset+2, sizeof rl->firstpage);
   rl->device = dev;           /* set device */
   if (obsolete) rl->flags = RANGELISTOBSOLETE;
   else rl->flags = 0;
   rl->next = rtent->devlist;  /* Add rangelist to rangetable chain */
   rtent->devlist = rl;
   rtent->flags |= RANGETABLEOBSOLETEHAVETHISTOD; /* force PDR update */
   if (RANGETABLESWAPAREA1 == rtent->type
          || RANGETABLESWAPAREA2 == rtent->type) {
      gswinss(rtent);
   }
   if (rl->flags & RANGELISTOBSOLETE) {
      obsoleteranges++;           /* Increment counter */
      enqmvcpu(&resyncqueue);  /* Start resyncing */
   }
        /* Calc high water mark for ranges */
   if (rtent < rangetable) {   /* Swap range */
      if (rangetable-rtent > maxswapranges)
         maxswapranges = rangetable-rtent;
   } else {
      if (rtent-rangetable > maxuserranges)
         maxuserranges = rtent-rangetable;
   }
   countmissinginstances();
   return 1;
} /* End grtadd */
 
 
/*********************************************************************
grtslsba - Return the Swap Block Address for a swaploc
 
  Input -
     swaploc - The RANGELOC in the swap area
 
  Output -
     Returns: the Swap Block Address for the range
*********************************************************************/
uint32 grtslsba(RANGELOC swaploc)
{
   return rangetable[swaploc.range].dac.sba.first + swaploc.offset;
} /* End grtslsba */
 
 
/*********************************************************************
grtchdrl - Return the checkpoint header location
 
  Input -
     id      - 1=Primary header, 2=secondary header, others invalid
 
  Output -
     Returns grt_notmounted or grt_mustread.
     N.B. ret.ioret.readinfo.id.potaddress is NOT set up since
          current callers only use it for accessing disk directory
          blocks and the checkpoint header.
*********************************************************************/
struct CodeGRTRet grtchdrl(int id)
{
   RANGETABLE *rt = rangetable;
   struct CodeGRTRet ret;
 
   for ( ; rt < rangetable + MAXUSERRANGES; rt++) {
      RANGELIST *rl = rt->devlist;
      for ( ; rl; rl = rl->next) {
         if (rl->flags & RANGELISTWAITFORCHECKPOINT) {
            /* Can now update the PDR after a resync */
            rt->flags |= RANGETABLEUPDATEPDR;
            rl->flags |= RANGELISTUPDATEPDR;
            rl->flags &= ~RANGELISTWAITFORCHECKPOINT;
         }
      }
      if (rt->flags & RANGETABLEUPDATEPDR) {
         /* Mark range lists whose PDR should be updated */
         RANGELIST *rl = rt->devlist;
         for ( ; rl; rl = rl->next) {
            if (!(rl->flags & RANGELISTOBSOLETE)) {
               rl->flags |= RANGELISTUPDATEPDR;
            }
         }
      }
   }
   if (1 == id) {
      ret = grtfdsba(PRIMARYCHECKPOINTHEADERLOCATION);
      return ret;
   }
   if (2 == id) {
      ret = grtfdsba(SECONDARYCHECKPOINTHEADERLOCATION);
      return ret;
   }
   crash("GRANGETC001 grtchdrl called with invalid ckpt header id");
} /* End grtchdrl */
 
 
/*********************************************************************
grtnpud - Return next PDR to update
 
  Input - None
 
  Output -
     Returns: the next device whose PDR should be updated or NULL
*********************************************************************/
DEVICE *grtnpud(void)
{
   register DEVICE *dev = npdrdevicepointer;
   RANGETABLE *rt;
 
   for (; dev < ldevlast; dev++) {
      for (rt = rangetable; rt < rangetable + MAXUSERRANGES; rt++) {
         if (rt->flags & RANGETABLEUPDATEPDR) {  /* Try this one */
            RANGELIST *rl = rt->devlist;
            for ( ; rl; rl = rl->next) {
               if (rl->flags & RANGELISTUPDATEPDR
                      && rl->device == dev) {
                  npdrdevicepointer = dev + 1;   /* For next entry */
                  return dev;
               }
            }
         }
      }
   }
   npdrdevicepointer = dev;   /* Shortcut subsiquent calls */
   return NULL;
} /* End grtnpud */
 
 
/*********************************************************************
grtfpud - Return first device whose PDR should be updated
 
  Input - None
 
  Output -
     Returns: the next device whose PDR should be updated or NULL
*********************************************************************/
DEVICE *grtfpud(void)
{
   todthismigration = read_system_timer();
   npdrdevicepointer = ldev1st;   /* Start with first */
   return grtnpud();
} /* End grtfpud */
 
 
/*********************************************************************
grtclear - Clear a device from the range table
 
  Input -
     dev     - Pointer to the DEVICE to clear
 
  Output - None
*********************************************************************/
void grtclear(DEVICE *dev)
{
   RANGETABLE *rt = swapranges;
 
   for ( ; rt < rangetable + MAXUSERRANGES; rt++) {
      RANGELIST *rl = rt->devlist;
 
      if (rl) {                     /* Range has devices */
         if (rl->device == dev) {    /* Entry for our device */
            if (rl->flags & (RANGELISTOBSOLETE | RANGELISTUPGRADE)) {
               if (obsoleteranges > 0) obsoleteranges--;
            }
            rt->devlist = rl->next;   /* dequeue and free rangeloc */
            freerangelist(rl);
            if (!rt->devlist) {       /* No devices left */
               if (rt >= rangetable) {  /* In user portion of table */
                  /* $$$$$ Modify here for binary search lookup $$$$$*/
                  clearallocationpotsandmigrationinfo(rt);
                  memzero(rt, sizeof(RANGETABLE));
               } else {
                  gswdess(rt);        /* Decrement swap space */
               }                      /* Leave entry for re-mount */
            } else rt->flags |= RANGETABLEOBSOLETEHAVETHISTOD;
         } else {                   /* First rangelist not for dev */
            RANGELIST *prev = rl;
 
            for ( rl = prev->next; rl; rl = prev->next) { /* Try rest */
               if (rl->device == dev) {       /* For our device */
                  prev->next = rl->next; /* dequeue and free rangeloc */
                  freerangelist(rl);
                  rt->flags |= RANGETABLEOBSOLETEHAVETHISTOD;
               } else prev = rl;
            }
         }
      }
   }
   gbaddmnt(dev);
   countmissinginstances();
} /* End grtclear */
 
 
/*********************************************************************
grtcrl - Return the RANGELOC for a CDA
 
  Input - None
 
  Output -
     Returns (-1, USHRT_MAX) if not mounted
*********************************************************************/
RANGELOC grtcrl(CDA cda)
{
   struct RangeTableAndLoc rti;
 
   rti = calcrangeloc(cda);
   if (!rti.rt) {
      rti.rl.range = -1;
      rti.rl.offset = USHRT_MAX;
   }
   return rti.rl;
} /* End grtcrl */
 
 
/*********************************************************************
grthomwl - Return the first home write location and set up grtnext
 
  Input -
     cte      - Pointer to the coretable entry for the page
 
  Output -
     grt_notmounted
     grt_mustread with first device, offset, and rangeloc for cda
*********************************************************************/
struct CodeGRTRet grthomwl(CTE *cte)
{
   struct CodeGRTRet ret;
   RANGETABLE *rt;
   RANGELIST *rl;
   uchar *cda = NULL;
 
   switch (cte->ctefmt) {     /* Switch on frame usage */
    case AlocPotFrame:
      rt = rangetable + cte->use.pot.potaddress.range;
      ret.ioret.readinfo.id.potaddress = cte->use.pot.potaddress;
      break;
    case NodePotFrame:
      {  struct NodePot *np = (struct NodePot *)
                           map_window(IOSYSWINDOW, cte, MAP_WINDOW_RO);
         cda = np->disknodes[0].cda;
      }
      break;
    case PageFrame:
      cda = cte->use.page.cda;
      break;
    default:
      crash("GRANGETC002 grthomwl called with non-page/pot cte");
   }  /* End switch on frame usage */

   if (cda) {                  /* Need to look up the cda */
      struct RangeTableAndLoc loc;
 
      loc = calcrangeloc(cda);
      rt = loc.rt;
      ret.ioret.readinfo.id.potaddress = loc.rl;
   }
   if (!rt || !rt->devlist) {
      ret.code = grt_notmounted;
      return ret;
   }
   rl = rt->devlist;
   nexttype = NEXTISNEXTWRITELOC;
   nextrangelist = rl->next;
   nextrangetable = rt;
   nextoffset = ret.ioret.readinfo.id.potaddress.offset;
   ret.ioret.readinfo.device = rl->device;
   ret.ioret.readinfo.offset = rl->firstpage + nextoffset;
   ret.code = grt_mustread;
   return ret;
} /* End grthomwl */
 
 
/*********************************************************************
grtcdap0 - Initialize read of mounted ranges
 
  Input - None
 
  Output -
     Cursor for grtcdap1 has been initialzed
*********************************************************************/
void grtcdap0(void)
{
   tapeckptcursor = swapranges;
} /* End grtcdap0 */
 
 
/*********************************************************************
grtcdap1 - Extract data from next mounted ranges
 
  Input - None
 
  Output -
     Length of data returned, zero means mo more ranges
     Data has been placed in cpuargpage
*********************************************************************/
int grtcdap1(void)
{
   RANGELIST *rl;
   char *p;              /* Cursor for building output */
   RANGETABLE *rt = tapeckptcursor;
 
   for ( ; rt < rangetable + MAXUSERRANGES; rt++) {
      tapeckptcursor = rt + 1;
      cpuargpage[0] = rt->type;
      if (rt < rangetable) {      /* Its a swap range */
         memzero(cpuargpage+1, 2 * sizeof(CDA));
         Memcpy(cpuargpage+3, &rt->dac.sba.first, sizeof(uint32));
         Memcpy(cpuargpage+9, &rt->dac.sba.last, sizeof(uint32));
      } else {                    /* Page or node range */
         Memcpy(cpuargpage+1, &rt->dac.cda.first, sizeof(CDA));
         Memcpy(cpuargpage+7, &rt->dac.cda.last, sizeof(CDA));
         if (cpuargpage[1] & 0x80) {  /* Range is a node range */
            cpuargpage[1] &= ~0x80;     /* Turn off node bit */
            cpuargpage[7] &= ~0x80;     /* Turn off node bit */
            cpuargpage[0] = 4;        /* Flag type as node range */
         }
      }
      Memcpy(cpuargpage+13, &rt->nplex, sizeof rt->nplex);
      p = cpuargpage+15;
      for (rl = rt->devlist; rl; rl = rl->next) {  /* for all devices */
         memzero(p+8, 2);             /* Clear top of 6 byte offset */
         Memcpy(p+10, &rl->firstpage, 4);
         if (rl->device->flags & DEVMOUNTED) {
            Memcpy(p, rl->device->packid, 8);  /* Copy pack ID */
            Memcpy(p+24, rl->device->physdev->type, 7);
                                               /* Copy device type */
            p += 21;                           /* advance cursor */
            if (p-cpuargpage > pagesize-21)
      return p-cpuargpage;
         }
      }
      return p-cpuargpage;
   }
   return 0;
} /* End grtcdap1 */
 
 
/*********************************************************************
grtmor - Return mounted obsolete range information
 
  Input - None
 
  Output -
     Pointer the the first & last cdas in the range {2(char[6])}
     Returns NULL if no obsolete ranges are mounted
*********************************************************************/
uchar *grtmor(void)
{
   int found = 0;         /* Found an obsolete range */
   RANGETABLE *rt = rangetable;
 
   for ( ; rt < rangetable+MAXUSERRANGES; rt++) {
      RANGELIST *rl = rt->devlist;
      for ( ; rl; rl = rl->next) {
         if (rl->flags & (RANGELISTOBSOLETE | RANGELISTUPGRADE)
            /* Range instance obsolete or needing upgrade */
            && !(rl->flags & (RANGELISTUPDATEPDR
                              | RANGELISTRESYNCINPROGRESS
                              | RANGELISTWAITFORCHECKPOINT))) {
               /* And not already being done */
            rl->flags |= RANGELISTRESYNCINPROGRESS; /* Do this one */
            found = 1;      /* Indicate we've found a range */
         }
      }
      if (found) return rt->dac.cda.first; /* both first and last */
   }
   return NULL;
} /* End grtmor */
 
 
/*********************************************************************
grtbrd - Set flags when Basic Rsync of a range is Done
 
  Input -
     cda      Pointer to a CDA in the just copied range
 
  Output - None
*********************************************************************/
void grtbrd(CDA cda)
{
   RANGETABLE *rt = finduserrangetable(cda);
   if (rt) {                  /* Range is still here */
      RANGELIST *rl = rt->devlist;
 
      for ( ; rl; rl = rl->next) {
         if (rl->flags & RANGELISTRESYNCINPROGRESS) {
            /* Range instance being resynced */
            rl->flags &= ~RANGELISTRESYNCINPROGRESS; /* Done */
            rl->flags |= RANGELISTWAITFORCHECKPOINT; /* Do next stage */
         }
      }
   }
} /* End grtbrd */
 
 
/*********************************************************************
grtrsyab - Abort resync because of some error (reset rangetable flags)
 
  Input -
     cda      Pointer to a CDA in the just copied range
 
  Output - None
*********************************************************************/
void grtrsyab(CDA cda)
{
   RANGETABLE *rt = finduserrangetable(cda);
   if (rt) {                  /* Range is still here */
      RANGELIST *rl = rt->devlist;
 
      for ( ; rl; rl = rl->next) {
         if (rl->flags & RANGELISTRESYNCINPROGRESS) {
            /* Range instance being resynced */
            rl->flags &= ~RANGELISTRESYNCINPROGRESS; /* Not any more */
         }
      }
   }
} /* End grtrsyab */
 
 
/*********************************************************************
grtslemi - Swaploc to Migrate_DeviceOffset  (formally GRTSLEDI)
 
  Input -
     swaploc  - RANGELOC of the page/node in the swap area
 
  Output -
     The device and offset for the external migrator
*********************************************************************/
struct Migrate_DeviceOffset grtslemi(RANGELOC swaploc)
{
   struct Migrate_DeviceOffset ret;
   RANGETABLE *rt = rangetable + swaploc.range;
   RANGELIST *rl = rt->devlist;
 
   if (rl && rl->device->flags & DEVMOUNTED) {
      ret.device = rl->device - ldev1st;  /* Device table index */
      ret.offset = rl->firstpage + swaploc.offset;
   } else {                 /* no device for swaploc */
      ret.device = USHRT_MAX;
      ret.offset = USHRT_MAX;
   }
   return ret;
} /* End grtslemi */
 
 
/*********************************************************************
grtuppdr - Update the PDR for current ranges
 
  Input -
     dev      - Pointer to the device
     pdr      - Pointer to the PDR from that device
 
  Output -
     The input PDR has been updated in place for the changed ranges
*********************************************************************/
void grtuppdr(DEVICE *dev, PDR *pdr)
{
   PDRD *pdrd = pdr->ranges;
   int count;
 
   for (count = pdr->pd.rangecount; count; count--) {
                                          /* All the ranges in the PDR */
      if (pdrd->type < PDRDCHECKPOINTHEADER) { /* User range */
         RANGETABLE *rt = finduserrangetable(pdrd->first);
 
         if (rt && rt->flags & RANGETABLEUPDATEPDR) { /* this one */
            int resetflag = 1;
            RANGELIST *rl = rt->devlist;
 
            for ( ; rl; rl = rl->next) {
               if (rl->device == dev) {   /* For this device */
                  if (rl->flags & RANGELISTUPDATEPDR) {
                     if (rl->flags & (RANGELISTOBSOLETE
                                      | RANGELISTUPGRADE)) {
                        /* Range has been resynced */
                        obsoleteranges--;
                        resyncscomplete++;
                     }
                     rl->flags &= ~(RANGELISTUPDATEPDR   /* Reset */
                                    | RANGELISTOBSOLETE  /* flags */
                                    | RANGELISTUPGRADE);
                     Memcpy(pdrd->migrationtod, &todthismigration,
                            sizeof pdrd->migrationtod);
                     rt->migrationtod = todthismigration;
                  }
               } else {
                  if (rl->flags & RANGELISTUPDATEPDR) resetflag = 0;
               }
            }
            if (resetflag) rt->flags &= ~RANGETABLEUPDATEPDR;
         }
      }
      pdrd++;            /* Look at next PDR range entry */
   }
} /* End grtuppdr */
 
 
/*********************************************************************
grtsbasl - Swap Block Address to SwapLoc  (formally GRTCCTSL)
 
  Input -
     sba      - The swap block address
 
  Output -
     the swap RANGELOC of the block or zero if it is not mounted
*********************************************************************/
RANGELOC grtsbasl(uint32 sba)
{
   RANGELOC rl;
   RANGETABLE *rt = findswaparearange(sba);
 
   if (rt) {               /* Found it */
      rl.range = rt - rangetable;
      rl.offset = sba - rt->dac.sba.first;
   } else {
      rl.range = 0;
      rl.offset = 0;
   }
   return rl;
} /* End grtsbasl */
 
 
/*********************************************************************
grtsyncd - Synchronize directories (after restart)
 
  Input -
     sba1     - The first Swap Block Address,
                     or zero if not available.
     sba2     - The second Swap Block Address or zero
 
  Output -
     If at least one of the sbas is mounted, the directory
     it represents is set as the current directory.
     (Later gdiswap will be called to change it to backup.)
     Otherwise a crash is issued.
*********************************************************************/
void grtsyncd(uint32 sba1, uint32 sba2)
{
   RANGETABLE *rt = findswaparearange(sba1);
 
   if (!rt) rt= findswaparearange(sba2);
   if (!rt) crash("GRANGETC003 Swap area range missing at boot time");
   gswscsaa(rt->type);
} /* End grtsyncd */
 
 
/*********************************************************************
grtnmp - Locate next mounted page
 
  Input -
     cda     - The page CDA to start looking for
 
  Output -
     The lowest mounted CDA >= the input CDA
     or 7fffffffffff if there is none
*********************************************************************/
static const CDA high = {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uchar *grtnmp(const CDA cda)
{
         /* $$$$$ Modify here for binary search lookup $$$$$ */
   RANGETABLE *rt = rangetable;
   const uchar *best = high;
 
   if (cda[0] & 0x80) crash("GRT396 grtnmp node cda");
   for ( ; rt < rangetable+MAXUSERRANGES; rt++) {
      if (!(rt->dac.cda.first[0] & 0x80) /* if page range */
          && cdacmp(rt->dac.cda.first, cda) >= 0
          && cdacmp(rt->dac.cda.first, best) < 0) {
         best = rt->dac.cda.first;
      }
   }
   return best;
} /* End grtnmp */
 
 
/*********************************************************************
grtrlr - Answer if a rangeloc is readable
 
  Input -
     rangeloc - The RANGELOC to test
 
  Output -
     1  - Rangeloc is readable, 0 - It is not readable
*********************************************************************/
int grtrlr(RANGELOC rangeloc)
{
   RANGETABLE *rt = rangetable + rangeloc.range;
   RANGELIST *rl = rt->devlist;
 
   for ( ; rl; rl = rl->next) {
      if (!(rl->flags & RANGELISTOBSOLETE)
          && !gbadread(rl->device, rl->firstpage + rangeloc.offset)) {
         return 1;
      }
   }
   return 0;
} /* End grtrlr */
 
 
/*********************************************************************
grtrrr - Return the next readable rangeloc
 
  Input -
     rangeloc - The RANGELOC to test
 
  Output -
     grtrrr returns grt_notmounted, grt_notreadable, and grt_mustread
     
     N.B. ret.ioret.readinfo.id.potaddress is not set for grt_mustread,
          the caller should use the rangeloc passed as input
*********************************************************************/
struct CodeGRTRet grtrrr(RANGELOC rl)
{
   RANGETABLE *rt = rangetable + rl.range;
 
   return readablerangeloc(rl.offset, rt);
} /* End grtrrr */
 
 
/*********************************************************************
grtapi - Return allocation pot information
 
  Input -
     cda      - The CDA to find allocation pot information for
 
  Output -
     If the range is mounted, then:
        lowcda, hicda are the Low & High cdas covered by pot
        rangeloc is the rangeloc of the allocation pot
        resync is 1 if the range is being resynced, otherwise zero
     If the range is not mounted, them:
        rangeloc.range is -1
        lowcda is set to the input cda
        hicda is set to the next mounted page cda
*********************************************************************/
struct GrtAPI_Ret grtapi(const CDA cda)
{
   struct GrtAPI_Ret ret;
   struct RangeTableAndLoc rtal;
 
   rtal = calcrangeloc(cda);
   if (rtal.rt) {              /* Range is mounted */
      uint32 cdaoffset = rtal.rl.offset % (APOTDATACOUNT+1);
      RANGELIST *rl;
      char test = 0;
      LLI tempcda, op2;
 
      if (!cdaoffset) crash ("GRANGETC001 calcrangeloc bug detected");
      ret.rangeloc.range = rtal.rl.range;
      ret.rangeloc.offset = rtal.rl.offset - cdaoffset;
 
      b2lli(cda, sizeof(CDA), &tempcda);
      op2.hi = 0;          /* Calc difference from cda to 1st in pot */
      op2.low = cdaoffset - 1;
      llisub(&tempcda, &op2);     /* Calc 1st cda in pot */
      lli2b(&tempcda, ret.lowcda, sizeof(CDA));
      op2.low = APOTDATACOUNT;    /* Calc 1st cda in next pot */
      lliadd(&tempcda, &op2);
      lli2b(&tempcda, ret.highcda, sizeof(CDA));
      if (cdacmp(ret.highcda, rtal.rt->dac.cda.last) > 0) {
                  /* Stay in the range */
         b2lli(rtal.rt->dac.cda.last, sizeof(CDA), &tempcda);
         lliadd(&tempcda, &llione);
         lli2b(&tempcda, ret.highcda, sizeof(CDA));
      }
 
      for (rl = rtal.rt->devlist; rl; rl = rl->next) {
         test = test | rl->flags;
      }
      ret.resync = test & (RANGELISTUPDATEPDR
                           | RANGELISTRESYNCINPROGRESS
                           | RANGELISTWAITFORCHECKPOINT);
      return ret;
   }
   ret.rangeloc.range = -1;
   Memcpy(ret.lowcda, cda, sizeof(CDA));
   Memcpy(ret.highcda, grtnmp(cda), sizeof(CDA));
   return ret;
} /* End grtapi */
 
 
/*********************************************************************
grtisipl - Find if CDA is part of a mounted IPL range
 
  Input -
     cda      - The CDA to look up
 
  Output -
     0 - CDA not part of an IPL range, !0 cda is part of an IPL range
*********************************************************************/
int grtisipl(CDA cda)
{
   RANGETABLE *rt = finduserrangetable(cda);
 
   if (!rt) return 0;
   return rt->type == RANGETABLEIPL;
} /* End grtisipl */
 
 
/*********************************************************************
grtmro - Mark range obsolete (when a write to it fails)
 
  Input -
     dev      - The device on which the write failed
     cte      - Core Table Entry for the frame being written
 
  Output - None
*********************************************************************/
void grtmro(DEVICE *dev, CTE *cte)
{
   RANGETABLE *rt;
   RANGELIST *rl;
 
   switch (cte->ctefmt) {     /* Switch on frame usage */
    case NodePotFrame:
    case AlocPotFrame:
      rt = rangetable + cte->use.pot.potaddress.range;
      if (cdacmp(rt->dac.cda.first, cdazero) == 0
          && cdacmp(rt->dac.cda.last, cdazero) == 0)
         return;
      break;
    case PageFrame:
      rt = finduserrangetable(cte->use.page.cda);
      if (!rt) return;
      break;
    default:
      crash("GRANGETC004 Non pot or page cte passed to grtmor");
   }  /* End switch on frame usage */

   /* rt has range table for the input cte */
   for (rl = rt->devlist; rl; rl = rl->next) {
      if (rl->device == dev) {       /* This one for our device */
         if (!(rl->flags & RANGELISTOBSOLETE)) { /* ! obsolete */
            obsoleteranges++;            /* Count it */
            rl->flags |= RANGELISTOBSOLETE;      /* Mark it */
            rl->flags &= ~(RANGELISTUPDATEPDR    /* reset these */
                           | RANGELISTRESYNCINPROGRESS
                           | RANGELISTWAITFORCHECKPOINT);
         }
      } else {                          /* Not our device */
         if (!(rl->flags & RANGELISTOBSOLETE)) { /* Not obsolete */
            rl->flags |= RANGELISTUPDATEPDR;    /* Force PDR update */
            rt->flags |= RANGETABLEUPDATEPDR;
         }
      }
   }
} /* End grtmro */
