/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "diskkeyh.h"
#include "key2dskh.h"
#include "memutil.h"


void key2dsk(key, diskkey)   /* Convert key in nodeframe to disk form */
   struct Key *key;          /* The key in a node frame */
   DISKKEY *diskkey;         /* Space for the disk form of the key */
/*
  DISCUSSION -
    The routine is called to convert a key in a node frame into its
    disk format. The basic structure of this routine is as follows:
         if not PREPARED then
           if INVOLVED -> CRASH
           else CASE on key type
         else (prepared)
           if HOOK -> process
           else (not hook)
             if INVOLVEDR -> CRASH  (user must check before calling)
             if EXIT -> process
             else (not exit) -> process
*/
{
   if (key->type & prepared) {
      if (key->type == pihk) {   /* Make datakey from hook's databyte */
         diskkey->dkdk.keytype = datakey;
         memzero(diskkey->dkdk.databody11, 10);
         diskkey->dkdk.databody11[10] = key->databyte;
         return;
      }
      if (key->type & involvedr) crash("KEY2DSK004 Key is involvedr");
      diskkey->ik.keytype = key->type & keytypemask;
      diskkey->ik.databyte = key->databyte;
 
      if (diskkey->ik.keytype == pagekey) {
         CTE *cte = (CTE *)key->nontypedata.ik.item.pk.subject;
 
         Memcpy(diskkey->ik.cda, cte->use.page.cda, sizeof(CDA));
         Memcpy(diskkey->ik.allocationid,
                &cte->use.page.allocationid,
                sizeof diskkey->ik.allocationid);
         cte->flags |= ctallocationidused;
      } else {
         NODE *node = (NODE *)key->nontypedata.ik.item.pk.subject;
 
         Memcpy(diskkey->ik.cda,
                node->cda,
                sizeof (CDA));
         if (diskkey->ik.keytype == resumekey) {
            Memcpy(diskkey->ik.allocationid,
                   &node->callid,
                   sizeof diskkey->ik.allocationid);
            node->flags |= NFCALLIDUSED;
         } else {                  /* Not a resume key */
            Memcpy(diskkey->ik.allocationid,
                   &node->allocationid,
                   sizeof diskkey->ik.allocationid);
            node->flags |= NFALLOCATIONIDUSED;
         }
      }
      return;
   }
   if (key->type & involvedr) crash("KEY2DSK001 Key is involvedr");
   diskkey->dkdk.keytype = key->type & ~involvedw;
   switch (diskkey->dkdk.keytype) {
    case datakey:
    case misckey:
    case chargesetkey:
    case devicekey:
    case copykey:
      Memcpy(diskkey->dkdk.databody11,
             key->nontypedata.dk11.databody11,
             sizeof diskkey->dkdk.databody11);
      break;
    case nrangekey:
    case prangekey:
      Memcpy(diskkey->rangekey.cda,
             key->nontypedata.rangekey.rangecda,
             sizeof diskkey->rangekey.cda);
      Memcpy(diskkey->rangekey.rangesize,
             key->nontypedata.rangekey.rangesize,
             sizeof diskkey->rangekey.rangesize);
      break;
    case hookkey:
      crash("KEY2DSK003 Unprepared hook key found");
    case pagekey:
    case segmentkey:
    case nodekey:
    case meterkey:
    case fetchkey:
    case startkey:
    case resumekey:
    case domainkey:
    case sensekey:
    case frontendkey:
      diskkey->ik.databyte = key->databyte;
      Memcpy(diskkey->ik.cda,
             key->nontypedata.ik.item.upk.cda,
             sizeof diskkey->ik.cda);
      Memcpy(diskkey->ik.allocationid,
             &key->nontypedata.ik.item.upk.allocationid,
             sizeof diskkey->ik.allocationid);
      break;
    default: crash("KEY2DSK002 Key type not recognized");
   }
   return;
}
