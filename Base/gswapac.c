/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GSWAPAC - Swap area allocation module - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "kerinith.h"
#include "sysparms.h"
#include "keyh.h"
#include "wsh.h"
#include "spaceh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "dskiomdh.h"
#include "disknodh.h"
#include "gbadpagh.h"
#include "gcl2gswh.h"
#include "gmigrath.h"
#include "gswapah.h"
#include "gsw2gdih.h"
#include "gsw2gckh.h"
#include "grangeth.h"
#include "gdi2gswh.h"
#include "gckpth.h"
#include "devmdh.h"
#include "rngtblh.h"
#include "gsw2grth.h"
#include "sysgenh.h"
#include "memutil.h"
 
 
/* Local static variables */
 
static uint32 checkpointlimit;    /* Need ckpt when availible < this */
 
static uint32 startingspaceavailable; /* Space available at end of */
                                      /* last checkpoint */
static uint32 availablepool0 = 0; /* Space available in pool 0 */
static uint32 availablepool1 = 0; /* Space available in pool 1 */
 
static char migrationhipriority = 0; /* !=0 when prty for swap space */
 
 
/*********************************************************************
iswapa - Swap area allocation initialization
 
  Input - None
 
  Output -
     The data structures in gswapac have been initialized
*********************************************************************/
void iswapa(void)
{
/*
  Initilaize checkpointlimit using the following formula:
         (NUMBER PAGES + (NUMBER NODES / NPNODECOUNT) + 1) +
         NUMBER OF DISK DIRECTORY PAGES + 2 + CHECKPOINTLIMITFUDGEFACTOR
*/
   checkpointlimit =
         (lastcte - firstcte) +    /* Number of pages */
         (((anodeend - firstnode) / NPNODECOUNT) + 1) +
         gdiddp() + 2 + CHECKPOINTLIMITFUDGEFACTOR;
 
   /* Zero all the swap area allocations */
   memzero(&swapaloc, sizeof(uint32) * MAXSWAPRANGES);
 
   currentswaparea = RANGETABLESWAPAREA1;
#if RANGETABLESWAPAREA1 != SWAPAREA1
#error RANGETABLESWAPAREA1 must equal SWAPAREA1
#endif
} /* End iswapa */
 
 
/*********************************************************************
findcurrentswaprange - Find rangetable pointer
 
  Input -
     dev   - Pointer to device
 
  Output -
     Pointer to the range table entry for current swap range on device
*********************************************************************/
static RANGETABLE *findcurrentswaprange(struct Device *dev)
{
   RANGETABLE *rt = swapranges + (MAXSWAPRANGES - 1);
 
   for (; rt >= swapranges; rt--) {    /* Look at all swap ranges */
      if (rt->type == currentswaparea) {  /* If in current area */
         RANGELIST *rl = rt->devlist;
         for (; rl; rl = rl->next) {      /* Check the devices */
            if (rl->device == dev)
   return rt;
         }
      }
   }
   crash("GSWAPAC001 Current swap range not found on device");
} /* End findcurrentswaprange */
 
 
/*********************************************************************
checkpointneeded - Check if a checkpoint is really needed
 
  Input - None
 
  Output -
     Number of pages in smallest pool
*********************************************************************/
static uint32 checkpointneeded(void)
{
   uint32 pool0 = 0;          /* Space assigned to pool 0 */
   uint32 pool1 = 0;          /* Space assigned to pool 1 */
   RANGETABLE *rt = swapranges;
   uint32 *allocated = swapaloc;
 
   for (; rt < swapranges + MAXSWAPRANGES; (rt++, allocated++)) {
      if (rt->type == currentswaparea) {
         uint32 available;
 
         available = rt->dac.sba.last - rt->dac.sba.first - *allocated;
         if (pool0 > pool1) {
            pool1 += available;
            rt->flags |= RANGETABLEPOOL1;
         } else {
            pool0 += available;
            rt->flags &= ~RANGETABLEPOOL1;
         }
      }
   }
   if (!multiswapdevices) pool1 = (pool0 /= 2);
   availablepool0 = pool0;
   availablepool1 = pool1;
   if (pool0 < pool1) return pool0;
   else return pool1;
} /* End checkpointneeded */
 
void freebackupchain(void)
{
   {  register CTE *head, *c;
      if (SWAPAREA1 == currentswaparea) head = &buppage1;
      else head = &buppage2;    /* Get correct chain head */
      for (c = (CTE *)head->use.page.rightchain;
           head != c;
           c = (CTE *)head->use.page.rightchain) {
         gspdetpg(c);
         gspmpfa(c);
      }
   }
}
 
/*********************************************************************
setcurrentswapareahereflags - Flag devices in current swap area
 
  Input - None
 
  Output - None
*********************************************************************/
static void setcurrentswapareahereflags(void)
{
   multiswapdevices = FALSE;
   {  register struct Device *dev;
      for (dev = ldev1st; dev < ldevlast; dev++) { /* for all disks */
         dev->flags &= ~DEVCURRENTSWAPAREAHERE;      /* Reset flags */
      }
   }
   {  RANGETABLE *rt = swapranges;
      struct Device *swapdev = NULL;
      for (; rt < swapranges + MAXSWAPRANGES; rt++) {
         if (rt->type == currentswaparea && rt->devlist) {
            rt->devlist->device->flags |= DEVCURRENTSWAPAREAHERE;
            if (swapdev && swapdev != rt->devlist->device)
               multiswapdevices = TRUE;
            swapdev = rt->devlist->device;
         }
      }
   }
} /* End setcurrentswapareahereflags */

 
/*********************************************************************
gswckmp - Check migration priority change needed due to swap space
          availability
 
  Input -
     dev   - Pointer to device
 
  Output -
     Calls gmiimpr or gmidmpr if necessary
 
  Notes -
     if (io left / total io needed) > (space left / starting space)
     then migration needs to be at high priority. Otherwise it needs
     to be at low priority. This calculation is performed as
     (starting space * io left) > (space left * total io needed).
     Note that the measures of total I/O needed and I/O left are crude.
*********************************************************************/
void gswckmp(void)
{
   struct gdimgrstRet migrstate;
   long int spacetockpt;
   LLI sltimesiio, sstimesiol;
 
   migrstate = gdimgrst();
   if   (availablepool0 < availablepool1)
      spacetockpt = availablepool0;
   else spacetockpt = availablepool1;
   if (spacetockpt < 0) spacetockpt = 0;
   llitimes(startingspaceavailable, migrstate.current, &sstimesiol);
   llitimes(spacetockpt, migrstate.initial, &sltimesiio);
   if (llicmp(&sstimesiol, &sltimesiio) > 0) {
      if (!migrationhipriority) {
         gmiimpr();               /* Increment reasons for hi prty */
         migrationhipriority = 1;
      }
   } else {
      if (migrationhipriority) {
         gmidmpr();               /* Increment reasons for hi prty */
         migrationhipriority = 0;
      }
   }
} /* End gswckmp */
 
 
/*********************************************************************
gswnext - Return next swap area slot
 
  Input -
     dev    - Pointer to the device on which to allocate the swap page
 
  Output -
     Returns grt_mustread if space available or
             grt_notmounted if there is no space available
*********************************************************************/
struct CodeGRTRet gswnext(struct Device *dev)
{
   for (;;) {                /* Search till we find a good one */
      RANGETABLE *rt = findcurrentswaprange(dev);
      int index = rt - swapranges; /* Allocation index */
      RANGELOC swaploc;
      struct CodeGRTRet ret;
 
      if (rt->dac.sba.first + swapaloc[index] > rt->dac.sba.last) {
         ret.code = grt_notmounted;
   return ret;
      }
      if (rt->flags & RANGETABLEPOOL1) {
         if (--availablepool1 <= checkpointlimit)
            gcktkckp(TKCKPSWAPSPACE);
      } else {
         if (--availablepool0 <= checkpointlimit)
            gcktkckp(TKCKPSWAPSPACE);
      }
      swaploc.range = index - MAXSWAPRANGES;  /* Rng tbl index (<0) */
      swaploc.offset = (swapaloc[index]++);
      ret = grtsl2dl(swaploc);
      if (grt_notmounted == ret.code)
         crash("GSWAPAC002 Not mounted from grtsl2dl, internal error");
      if (!gbadread(ret.ioret.readinfo.device,
                    ret.ioret.readinfo.offset)) {  /* Not known bad */
         /* ret.code already grt_mustread, device+offset set up */
         gswckmp();        /* Check migration priority */
   return ret;
      }
   }
} /* End gswnext */
 
 
/*********************************************************************
gswppod - Return probable page on device that will be allocated for
          a swap area write. Used in arm optimization.
 
  Input -
     dev    - Pointer to the device on which to allocate the swap page
 
  Output -
     Returns grt_mustread if space available or
             grt_notmounted if there is no space available
*********************************************************************/
uint32 gswppod(struct Device *dev)
{
   RANGETABLE *rt = findcurrentswaprange(dev);
   int index = rt - swapranges; /* Allocation index */
 
   return rt->devlist->firstpage + swapaloc[index];
} /* End gswppod */
 
 
/*********************************************************************
gswnbest - Return next best swap device
 
  Input -
     maxstate - Highest DEVENQSTATE to consider for use
 
  Output -
     Returns pointer to a device or NULL
 
  Notes:
     gswfbest initializes this routine to return the "best" swap
     device. Subsiquent calls return the "next best" until there
     are no eligble devices left.  The number of times any particular 
     device is returned depends on the value of multiswapdevices. If
     it is TRUE a device is returned a maximum of one time.  If it is 
     FALSE a device is returned a maximum of two times.
 
     Device returned must have dev->enqstate <= maxstate, and enough
     available space in the current swap range to be useful for a
     swap operation.  Of the devices elegible on those criteria, the
     one with the least arm motion needed is returned. If there are
     two with equal arm motion, the one with the most available swap
     space is returned.
*********************************************************************/
struct Device *gswnbest(int maxstate)
{
   RANGETABLE *best = NULL;
   long int bestdist = LONG_MAX;
   uint32 bestspace = 0;
   RANGETABLE *rt = swapranges;
   char *currentused = swapused;
   char maxreturns = (multiswapdevices ? 1 : 2);
 
   for (; rt < swapranges + MAXSWAPRANGES; (currentused++, rt++)) {
      RANGELIST *rl = rt->devlist;
      struct Device *dev;
      if (   rl
          && rt->type == currentswaparea 
          && *currentused < maxreturns 
          && (dev = rl->device, dev->physdev->enqstate <= maxstate)) {
         /* This range may be eligble to be returned */
         int index = rt - swapranges; /* Allocation index */
         uint32 available, needed;
         long int dist;
 
         available = rt->dac.sba.last - rt->dac.sba.first
                     - swapaloc[index];
 
         if (iosystemflags & CHECKPOINTMODE) needed = 1;
         else needed = MAXPAGESTOCLEANATATIME;
         if (available >= needed) {
            /* This range is eligble to be returned */
            dist = offset2cyl(rl->firstpage + swapaloc[index]) -
                             offset2cyl(dev->physdev->lastaddress);
            if (dist < 0) dist = -dist;
            if (dist < bestdist) {     /* New best */
               best = rt;
               bestdist = dist;
               bestspace = available;
            }
            else if (dist == bestdist  &&    /* New best */
                      bestspace < available) {
               best = rt;
               bestspace = available;
            }
         }
      }
   }
   if (best) {             /* We have a device to return */
      swapused[best - swapranges] += 1;    /* Remember it */
      return best->devlist->device;
   }
   return NULL;
} /* End gswnbest */
 
 
/*********************************************************************
gswfbest - Return the best swap out device
 
  Input -
     maxstate - Highest DEVENQSTATE to consider for use
 
  Output -
     Returns pointer to a device or NULL
 
  Notes - See gswnbest for notes
*********************************************************************/
struct Device *gswfbest(int maxstate)
{
   memzero(swapused, MAXSWAPRANGES); /* zero the used array */
   return gswnbest(maxstate);
} /* End gswfbest */
 
 
/*********************************************************************
gswinss - Increment swap space available
 
  Input -
     rt    - Pointer to the range table entry for the just mounted range
 
  Output -
     Updates internal tables for the new range
*********************************************************************/
void gswinss(RANGETABLE *rt)
{
   checkpointneeded();       /* Update internal space information */
   if (rt->type == currentswaparea) {
      rt->devlist->device->flags |= DEVCURRENTSWAPAREAHERE;
   }
} /* End gswinss */
 
 
/*********************************************************************
gswdess - Increment swap space available
 
  Input -
     rt    - Pointer to the range table entry for the dismounted range
 
  Output -
     Updates internal tables for the dismounted range
*********************************************************************/
void gswdess(RANGETABLE *rt)
{
   availablepool0 = 0;        /* Set available to zero to force */
   availablepool1 = 0;        /*     recalculation */
   setcurrentswapareahereflags();
} /* End gswdess */
 
 
/*********************************************************************
gswnumsd - Return number of swap devices
 
  Input - None
 
  Output -
     Number of devices with a range of the current swap area
*********************************************************************/
int gswnumsd(void)
{
   int count = 0;
   RANGETABLE *rt = swapranges;
 
   for (; rt < swapranges + MAXSWAPRANGES; rt++) {
      if (rt->type == currentswaparea && rt->devlist) count++;
   }
   return count;
} /* End gswnumsd */
 
 
/*********************************************************************
gswreset - Change to other swap area and reset allocations
 
  Input - None
 
  Output - None
*********************************************************************/
void gswreset(void)
{
   if (RANGETABLESWAPAREA1 == currentswaparea) /* Switch areas */
      currentswaparea = RANGETABLESWAPAREA2;
   else currentswaparea = RANGETABLESWAPAREA1;
 
   memzero(swapaloc, sizeof(uint32)*MAXSWAPRANGES); /* Zero allocations */
 
   freebackupchain();
   setcurrentswapareahereflags();          /* Flag current devices */
 
   startingspaceavailable = checkpointneeded(); /* Build pools */
/* ZZZ Due to large memory (256M) and only 1G spare disk. For now, allow this
   check condition.  Just make sure we have lots of swap space.
   Configued 180M*4 swap space.
   if (checkpointlimit > startingspaceavailable)
      crash("GSWAPAC003 Checkpoint limit > available swap space");
*/
 
   gswckmp();                 /* Check migration priority */
} /* End gswreset */
 
 
/*********************************************************************
gswscsaa - Set current swap area for allocation
 
  Input -
     current - The ID of the current swap area, RANGETABLESWAPAREA1 or
               RANGETABLESWAPAREA2. Used during initialization.
 
  Output - None
*********************************************************************/
void gswscsaa(uchar current)
{
   currentswaparea = current;
   setcurrentswapareahereflags();
} /* End gswscsaa */
