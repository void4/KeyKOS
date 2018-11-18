/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GBADPAGC - Manage unreadable DEVLOCs          - KeyTech Disk I/O */
 
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "keyh.h"
#include "devmdh.h"
#include "ioworkh.h"
#include "gbadpagh.h"
 
#define HASH(n)         ((n & 0x0000000C)>>2)
#define MAXBADDEVLOCS   100
 
typedef struct baddevloc BDL;
struct baddevloc {            /* Bad DEVLOC */
   DEVICE  *device;           /* device for one bad devloc */
   uint32   offset;           /* offset of one bad devloc  */
   BDL     *next;             /* pointer to next baddevloc */
};
 
/* Local static variables */
 
static
BDL *badpagehashhead[4] = {NULL, NULL, NULL, NULL}; /* chain heads */
static
BDL bdlpool[MAXBADDEVLOCS];               /* array of BADDEVLOCs */
static
BDL *pooladdress    = &bdlpool[0];          /* next entry to use */
static
BDL *firstbaddevloc = &bdlpool[0];                      /* first */
static
BDL *lastbaddevloc  = &bdlpool[MAXBADDEVLOCS-1];        /* last  */
static
int baddevlocdeqcount = 0;            /* count of DEVLOCs dequeued */
 

/*********************************************************************
*deqbaddevloc - remove bad DEVLOC entry from hash chain if its on one.
*
*  Input -
*         dev     - Pointer to DEVICE to test
*
*  Output -
*         0 == DEVLOC dequeued
*        !0 == DEVLOC was available
*
*********************************************************************/
static int deqbaddevloc(BDL *bdl)
{
 BDL **ip;
 for (ip = &badpagehashhead[HASH(bdl->offset)];
      (*ip)!=NULL; ip=&((*ip)->next)) {
   if (*ip == bdl) {       /* this points to our baby */
     (*ip) = bdl->next;      /* deque him */
     return(0);                 /* signal dequed */
   }
 }
 return(1);                     /* signal no deque */
} /* End deqbaddevloc */

/*********************************************************************
*gbadread - Queries if a DEVLOC is readable
*
*  Input -
*         dev     - Pointer to DEVICE to test
*         offset  - Offset on device of page to test
*
*  Output -
*         0 == page ok,  DEVLOC not in list of unreadable pages
*        !0 == page bad, DEVLOC unreadable
*********************************************************************/
int gbadread(DEVICE *dev, uint32 offset)
{
 BDL *ip;
 for (ip = badpagehashhead[HASH(offset)];
      ip != NULL; ip = ip->next) {
   if (ip->offset == offset
       && ip->device == dev)
      return(1);  /* bad page for this device */
 }
 return(0);                               /* page not on hash chain */
} /* End gbadread */
 
 
/*********************************************************************
*gbadlog - Add bad DEVLOC to head of list of unreadable pages
*
*  Input -
*         dev     - Pointer to DEVICE that contains unreadable page
*         offset  - Offset on device of the unreadable page
*
*  Output - none.
*
*********************************************************************/
void gbadlog(DEVICE *dev, uint32 offset)
{
 int n;
 if (0==deqbaddevloc(pooladdress))  /* pre-empt next slot if in use */
     baddevlocdeqcount += 1;          /* count it if it was in use  */
 n = HASH(offset);                    /* calc the chain head */
 pooladdress->next = badpagehashhead[n]; /* fill in new entry */
 badpagehashhead[n] = pooladdress;  /* put it at head of chain  */
 pooladdress->offset = offset;
 pooladdress->device = dev;
 if (pooladdress == lastbaddevloc)    /* is this the last one     */
   pooladdress = firstbaddevloc;      /* yes, reset to first one  */
 else pooladdress++;               /* no, bump pointer to next */
 return;
} /* End gbadlog */

/*********************************************************************
*gbaddmnt - Remove entries from list for dismounted device
*
*  Input -
*         dev - Pointer to the dismounted DEVICE
*
*  Output - none.
*
*********************************************************************/
void gbaddmnt(DEVICE *dev)
{
 BDL *ip;
 for (ip = firstbaddevloc; ip <= lastbaddevloc; ip++) {
   if (ip->device == dev) deqbaddevloc(ip);
 }
} /* End gbaddmnt */

/*********************************************************************
*gbadrewt - Remove entries from list for re-written page
*
*  Input -
*         dev - Pointer to the DEVICE
*         offset  - Offset on device of the re-written page
*
*  Output - none.
*
*********************************************************************/
void gbadrewt(DEVICE *dev, uint32 offset)
{
 BDL *ip;
 for (ip = firstbaddevloc; ip <= lastbaddevloc; ip++) { /* entire pool */
   if (ip->device == dev)                        /* matching device ? */
     if (ip->offset == offset) deqbaddevloc(ip); /* matching offset ? */
 }
} /* End gbadrewt */

