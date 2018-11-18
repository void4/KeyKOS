/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "memomdh.h"
#include "dependh.h"
#include "kerinith.h"
 
/*
   DEPEND - REMEMBER THAT A MEMORY MAP ENTRY DEPENDS ON A KEY SLOT
 
            Depend is only used to provide the back link from keys to
            the hardware memory map. Depend is only used to determine
            which entries to destroy when a key slot is changed. The
            contract for depend allows depend to destroy extra entries.
            All map entries that are built for a set of key slots are
            described on a hash chain which is hashed by the key slot
            address.  Whenever a key slot is changed necessitating
            the destruction of entries, all entries that MIGHT have been
            built based on the key slot are destroyed.  The entries that
            a particular key produced may have been destroyed for some
            other reason and the new entry may actually depend on key
            slot Y at the time that Depend is called to destroy
            tables produced by key slot X.  The extra destructions
            only cause extra work in re-building memory map entries.
 
*/
 
 
struct DepEnt *being_built = NULL;
static bool being_built_damaged = FALSE; /* Tells whether the being_built
          chain has been invalidated by a slotzap. */
long depcursor = 0;
struct DepEnt *free_depend_head = NULL;
 
static struct DepEnt **depend_chains;
    /* origin of an array of pointers to DepEnts. */
static unsigned int num_depend_heads; /* size of the array */
 
char *adepspac, *aenddep; /* start and end of our space */
 
 
static long depend_hash(  /* Return hash of slot address */
struct Key *slot)
{
   return (slot - (struct Key *)firstnode) % num_depend_heads;
} /* End depend_hash */
 
 
static void zap_chain(  /* Zap mapping entries for a hash chain */
   long index)        /* Index of chain to zap */
{
   struct DepEnt *de;
   struct DepEnt **ch = &depend_chains[index];
 
   for (de = *ch; de; de = *ch) {
      zap_depend_entry(de->data.hce.entry_locator,
                       de->data.hce.contents_hash);
      *ch = de->link;
      de->link = free_depend_head;
      free_depend_head = de;
   }
} /* End zap_chain */
 
 
void depend_build_entry(slot)
       /* Build a new entry for slot and put it on being_built chain */
/* Input - */
struct Key *slot;
{
   struct DepEnt *de;
 
   /* See if address of key slot is outside of Node Space */
   if ((NODE *)slot < firstnode || (NODE *)slot >= anodeend)
      crash("DEPEND001 Key address not in node space");
 
   for (;;) {            /* Allocate a depend entry */
      de = free_depend_head;
      if (de != NULL) {
         free_depend_head = de->link;
   break;
      }
 
      /* Zap some non-empty chain to make room. */
      zap_chain(depcursor);
      depcursor++;
      if (depcursor == num_depend_heads) depcursor = 0;
   }
/*
   Add depend block onto the being_built chain.
*/
   de->link = being_built;
   being_built = de;
   de->data.key = slot;
} /* End depend_build_entry */
 
 
void depend_dispose_entries()  /* Throw away the being_built chain */
{
   struct DepEnt *de;
 
   for (de = being_built; de; de = being_built) {
      being_built = de->link;
      de->link = free_depend_head;
      free_depend_head = de;
   }
   being_built_damaged = FALSE;
} /* End depend_dispose_entries */
 
 
bool depend_chain_entries(
   long locator,
   unsigned short hash)
/* Chain entries on being_built chain onto hash chains */
/* Returns TRUE if ok,
   FALSE if a call to slotzap has invalidated the depend entries. */
{
   struct DepEnt *de;
   int dh;
 
   if (being_built_damaged) {
      depend_dispose_entries();
      return FALSE;
   }
   for (de = being_built; de; de = being_built) {
      being_built = de->link;
      dh = depend_hash(de->data.key);
      de->link = depend_chains[dh];
      depend_chains[dh] = de;
      de->data.hce.entry_locator = locator;
      de->data.hce.contents_hash = hash;
   }
   return TRUE;
} /* End depend_chain_entries */
 
 
void visit_depends(
   struct Key * const slot,
   void (*entry_proc)(uint32 locator, unsigned short hash)
                    ) /* Procedure to call for each entry */
 /* Visit all map entries that depend on a slot. */
{
   struct DepEnt *de;

   /* See if address of key slot is outside of Node Space */
   if ((NODE *)slot < firstnode || (NODE *)slot >= anodeend)
      crash("DEPEND123 Key address not in node space");
 
   for (de = depend_chains[depend_hash(slot)]; de; de = de->link) {
      entry_proc(de->data.hce.entry_locator,
                 de->data.hce.contents_hash);
   }
}
 
void slotzap(struct Key * const slot)
 /* Clean out map entries that depend on a slot */
{
   struct DepEnt *de;

   /* See if address of key slot is outside of Node Space */
   if ((NODE *)slot < firstnode || (NODE *)slot >= anodeend)
      crash("DEPEND002 Key address not in node space");
 
   zap_chain(depend_hash(slot));
   for (de = being_built; de; de = de->link) {
      if (de->data.key == slot) {
         /* Oops. We are uninvolving a slot on which the table entry
            being built depends. Make a note not to proceed with
            building the entry. */
         being_built_damaged = TRUE;
   break;
      }
   }
} /* End slotzap */
 
static long DepEnt_count = 0; 
 
int depend_check_entries(
            /* Check for an existing entry in the depend hash chains */
   struct Key *slot,
   long locator,
   unsigned short hash)
/*
   Output -
      Returns 1 if entry is not in chain, 0 if it is
*/
{
   struct DepEnt *de;
   long count = DepEnt_count; 
   for (de = depend_chains[ depend_hash(slot) ];
        de;
        de = de->link) {
      if (de->data.hce.entry_locator == locator
          && de->data.hce.contents_hash == hash)
         return 0;
      if(!--count) crash("Loop in Depend entries.");
   }
   return 1;
} /* End depend_check_entries */
 

void depends(void)
/* One-time initialization. */
{
   int i;
   struct DepEnt *p;
 
   num_depend_heads = (aenddep-adepspac)/sizeof(struct DepEnt) / 8;
           /* calc number of hash chain heads */
   depend_chains = (struct DepEnt **)adepspac;
   for (i=0; i<num_depend_heads; i++) depend_chains[i] = NULL;
   for (p = (struct DepEnt *)(depend_chains + num_depend_heads);
        (char *)p <= (aenddep-sizeof(struct DepEnt)); p++) {
      p->link = free_depend_head;
      free_depend_head = p; ++DepEnt_count;
   }
} /* End depends */
