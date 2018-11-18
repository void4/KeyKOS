/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "booleanh.h"
#include "keyh.h"
 
/*
   Usage:
     While examining slots to resolve an address call depend_build_entry
     to note the slot addresses. When finished call depend_chain_entries
     to add the slot addresses noted with depend_build_entry to the
     depend relation OR throw them away by calling
     depend_dispose_entries.
 
*/
 
struct DepEnt {
   struct DepEnt *link;
   union {                /* Field use depends on chain entry is on */
      struct Key *key;      /* If entry is on the being_built chain */
      struct {              /* If entry is on a hash chain */
         long entry_locator;  /* locator of the map entry
                                which depends on the key
                                (machine-dependent) */
         unsigned short contents_hash; /* Hash of the value in entry */
      } hce;
   } data;
};

extern char *adepspac, *aenddep;
void depend_build_entry(struct Key *slot);
       /* Build a new entry for slot and put it on being_built chain */
extern bool depend_chain_entries(
   long locator,
   unsigned short hash);
 
void depend_dispose_entries(void);
    /* Throw away the being_built chain */

void visit_depends(
   struct Key * const slot,
   void (*entry_proc)(uint32 locator, unsigned short hash)
                    );  /* Procedure to call for each entry */
 /* Visit all map entries that depend on a slot. */
 
void slotzap(struct Key *slot);
                /* Clean out map entries that depend on a slot */
extern int depend_check_entries(
            /* Check for an existing entry in the depend hash chains */
   struct Key *slot,
   long locator,
   unsigned short hash);
