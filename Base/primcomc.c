/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "enexmdh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "cpumemh.h"
#include "prepkeyh.h"
#include "domainh.h"
#include "domamdh.h"
#include "gateh.h"
#include "geteh.h"
#include "queuesh.h"
#include "memoryh.h"
#include "locksh.h"
#include "kschedh.h"
#include "unprndh.h"
#include "diskkeyh.h"
#include "key2dskh.h"
#include "primcomh.h"
#include "locore.h"
#include "memutil.h"

static long badeb = -1;
void priminit(){union{struct entryblock eb; long i;} u;
  u.i=0;
  u.eb.reserved1 = -1;
  u.eb.reserved2 = -1;
  u.eb.reserved3 = -1;
  badeb = u.i;}
 
void pad_move_arg(     /* Move argument, pad with zeroes */
   register void *to,           /* To address for move */
   register int len)            /* Length of move */
/*
      N.B. This routine updates cpuargaddr and cpuarglength. In this
           sense it moves rather than copies the string
*/
{
   int ml = len;                /* The length to move */
   register unsigned long arglength = cpuarglength;
   register char *argaddr = cpuargaddr;
 
   if (arglength < ml) ml = arglength;
   if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
      Memcpy(to, argaddr, ml);
   } else if ( 0 == movba2va(to, argaddr, ml) ) {
      crash("PRIMCOM011 - Overlap with stack?");
   }
   cpuargaddr = argaddr + ml;
   cpuarglength = arglength - ml;
   if (len-ml) memzero((char *)to+ml, len-ml);
}
 
 
int clean(               /* Prepare to store into a node slot */
   register struct Key *key)    /* The slot to store into */
/* Output - */
/*   clean_dont 1 - Don't store in slot - has hook */
/*   clean_ok   0 - OK to store into the slot.
                    Caller must mark the node dirty. */
{
   if (key->type & (prepared|involvedw)) {
      if (key->type & involvedw) {
         if (key->type == pihk) return clean_dont;
         crash("PRIMCOM001 Involvedw, non-hook in slot being cleaned");
      }
      /* Key is prepared, un-link it */
      ((NODE *)((NODE *)key)->rightchain)->leftchain =
               (union Item *)((NODE *)key)->leftchain;
      ((NODE *)((NODE *)key)->leftchain)->rightchain =
               (union Item *)((NODE *)key)->rightchain;
   }
   return clean_ok;
}
 
 
int puninv(              /* Prepare to fetch or store a node slot */
   register struct Key *key)     /* Pointer to the slot */
 
/* Output - */
/*   puninv_cant 1 - Some preplocked node (other than node)
                     prevented unpreparing node */
/*   puninv_ok   0 - Slot is uninvolved or a hook */
{
   if (key->type & (involvedr|involvedw)) {
      if (key->type == pihk) return puninv_ok;
      if (superzap(key) != unprnode_unprepared) return puninv_cant;
   }
   return puninv_ok;
}
 
 
void zapresumes(dib)         /* Zap all resume keys to a domain */
/* Input - */
register struct DIB *dib;       /* DIB of domain whose resumes to zap */
/*
   N.B. Does not zap any "resume keys" held by the kernel
*/
{
   register union Item *key;
   register struct Key *key2;
 
   for (key = dib->lastinvolved->item.rightchain;
        key != (union Item *)dib->rootnode
                             && key->key.type == resumekey+prepared;
        ) {
      key2 = &key->key;
      key = key->item.rightchain;
      *key2 = dk0;
   }
   /* Re-chain the back chain */
   dib->lastinvolved->item.rightchain = key;
   key->item.leftchain = dib->lastinvolved;
 
   /* All prepared resumes are zapped. Now zap unprepared resumes */
   if (dib->rootnode->flags & NFCALLIDUSED) { /* If unpreped resumes */
      unsigned long id = dib->rootnode->callid;
      if ( (id++) < dib->rootnode->callid)
                crash("PRIMCOM002 Call id overflow");
      dib->rootnode->callid = id;
      dib->rootnode->flags &= ~NFCALLIDUSED;
      dib->rootnode->flags |= NFDIRTY;
   }
}
 
 
void makeready(dib)           /* Make a busy domain ready */
/* Input - */
register struct DIB *dib;       /* DIB of domain to make ready */
{
   dib->readiness &= ~BUSY;
   if ( !(dib->rootnode->flags & NFREJECT) ) return;
   rundom( (NODE *) (
       (char *)dib->rootnode->rightchain -
          ((char*)(&dib->rootnode->domhookkey)-(char*)dib->rootnode) ));
   if ( !(dib->rootnode->flags & NFREJECT) ) return;
/* Put dib-dom on the worry queue */
   enqueuedom(dib->rootnode,&worryqueue);
   dib->rootnode->domhookkey.databyte = 0;
   startworrier();
}
 
 
static NODE *cpurekeysaddr; /* Address of corelocked keysnode */

void uncorelockreturnee (void)
/*
   Input -
      cpujenode - Address of the root node, corelocked.
      cpurekeysaddr - Address of the keys node, corelocked.
      statestore is locked.
   Output -
      All of the domain components are un-corelocked
*/
{
   coreunlock_node(cpujenode);
   coreunlock_node(cpurekeysaddr);
   coreunlock_statestore();
}
 
 
void abandonreturnee(void)       /* Returnee gone - unlock nodes */
{
   uncorelockreturnee();
   unsetupdestpage();
}
 
 
void unsetupreturnee(void)       /* Undo ensurereturnee */
{
   if (cpujenode) abandonreturnee();
}
 
 
void zapprocess(rn)     /* Remove process from node and zap "resume" */
   register NODE *rn;      /* The root node of the domain */
/*
   Other Input -
      cpujenode - Pointer to returnee's root node or NULL
 
   Output - None
      cpudibp is invalid (i.e. the cpu is not executing any domain).
      Leaves an involved dk(1) or a worry hook in slot 13.
*/
{
   if (rn == cpujenode) {   /* Zap the kernel's resume key */
      abandonreturnee();
      cpujenode = NULL;
   }
   if (rn->domhookkey.type != pihk)
      return;               /* Domain is not hooked */
   if (rn->domhookkey.databyte == worry_hook)
      return;               /* Worry hook does not represent process */
   zaphook(rn);
   rn->flags |= NFDIRTY;    /* We changed this node */
} /* End zapprocess */
 
 
int getreturnee()             /* After dry run, zap resumes etc. */
/*
   Input -
      Nothing is preplocked
      Returnee has been setup (e.g. by ensurereturnee)
   Output -
      The returnee domain components have been uncorelocked.
      1 - There is no returnee, or the returnee is ready or malformed
          and the dest page has been un-set-up.
      0 - cpudibp is returnee dib, cpudibp->rootnode is preplocked
          cpuentryblock has entryblock
          cpuinvokedkeytype and cpuinvokeddatabyte are set
          The destination page is set up.
*/
{
   register struct DIB *dib;
   register NODE *jenode = cpujenode;
 
   if (jenode == NULL) return 1;
   if (preplock_node(jenode,lockedby_getreturnee))
        crash("PRIMCOM003 getreturnee can't preplock returnee");
   cpuactor = jenode;
   if (jenode->prepcode != prepasdomain) {
      if (jenode->prepcode != unpreparednode) {
         if (unprnode(jenode) != unprnode_unprepared)
               crash("PRIMCOM004 getreturnee can't unprepare returnee");
         switch (prepdom(jenode)) {
          case prepdom_wait:
            crash("PRIMCOM005 getreturnee nodes not in memory");
          case prepdom_overlap:
            crash("PRIMCOM006 getreturnee components preplocked");
          case prepdom_malformed:
             /* Something we did made the returnee malformed */
            abandonreturnee();
            unpreplock_node(jenode);
            return 1;
          case prepdom_prepared:
            ;
         }
      }
   }
   dib = jenode->pf.dib;
   if (!(dib->readiness & BUSY)) {
      abandonreturnee();
      unpreplock_node(jenode);
      return 1;
   }
   {union{struct entryblock eb; long i;} b;
      b.eb = cpuentryblock;
      if(badeb & b.i){
        set_trapcode(dib, 0x308);
        *(uint32 *)&cpuentryblock = 0;
   }}
   startdom(dib);         /* make returnee the running domain */
   uncorelockreturnee();
   zapresumes(dib);
   return 0;
}
 
 
void handlejumper()           /* End of dry run - release jumper */
/*
   Input -
      cpuexitblock.jumptype - has type of jump
      cpudibp - Points to jumper, cpudibp->rootnode is prepasdomain
         preplocked and busy.
 
   Output -
      The jumper is unpreplocked. The only LOGICAL state change this
      routine makes is to make the jumper ready (in case of a return).
*/
{  switch (cpuexitblock.jumptype) {
    case jump_implicit:
    case jump_call:
      break;
    case jump_return:
      makeready(cpudibp);
      break;
    case jump_fork:
      rundom(cpudibp->rootnode);
      break;
   }
   putawaydomain();
} /* End handlejumper */
 
 
struct Key *hook_look(key)    /* Make hook key look like dk0 or dk1 */
/* Input - */
struct Key *key;
{
if (key->databyte == 0)
   return &dk0;
else return &dk1;
}
 
 
struct Key *look(key)        /* Look at a key */
   register struct Key *key;    /* The key to look at */
/*
  Definition: A hook looks like a data key. Others look like themselves.
 
   Output -
      key is uninvolvedr. It is suitable for reading only.
      Tries to prepare the key without doing I/O.
*/
{
   if (key->type == pihk) return hook_look(key);
   if (key->type & prepared) return key;
   tryprep(key);
   return key;
}
 
 
struct Key *readkey(key)    /* Prepare to fetch from a slot of a node */
   register struct Key *key;   /* Slot to fetch from */
/*
   Output -
      Unprepared node if necessary
      NULL - Some preplocked node (other than node) prevented
             unpreparing node
      Otherwise pointer to the key or a copy, not involvedr
*/
{
   if (key->type & involvedr) {
      if (key->type == pihk) return hook_look(key);
      if (superzap(key) != unprnode_unprepared) return NULL;
      else return key;
   }
   if (key->type & prepared) return key;
   tryprep(key);
   return key;
}
 
 
struct Key *ld1()               /* Get jumper's first key */
{
   register struct exitblock exitblock = cpuexitblock;
 
   if (exitblock.keymask & 8)
      return look(&cpudibp->keysnode->keys[exitblock.key1] );
   return &dk0;
}
 
 
struct Key *ld2()               /* Get jumper's second key */
{
   register struct exitblock exitblock = cpuexitblock;
 
   if (exitblock.keymask & 4)
      return look(&cpudibp->keysnode->keys[exitblock.key2] );
   return &dk0;
}
 
 
struct Key *ld3()               /* Get jumper's third key */
{
   register struct exitblock exitblock = cpuexitblock;
 
   if (exitblock.keymask & 2) {
      if (exitblock.jumptype == jump_implicit) {
         if (CPUP3_UNLOCKED == cpup3switch)
            crash("PRIMCOM007 Implicit jump and key3 not locked");
         coreunlock_node(cpup3node);
         cpup3switch = CPUP3_UNLOCKED;
         return &cpup3key;    /* Key is half prepared */
      }
      return look(&cpudibp->keysnode->keys[exitblock.key3] );
   }
   return &dk0;
}
 
 
struct Key *ld4()               /* Get jumper's fourth key */
{
   register struct exitblock exitblock = cpuexitblock;
 
   if (exitblock.jumptype == jump_call) {
      cpup4key.type = resumekey + prepared;
      cpup4key.databyte = returnresume;
      cpup4key.nontypedata.ik.item.pk.subject =
                     (union Item *)cpudibp->rootnode;
      return &cpup4key;
   }
   if (exitblock.jumptype == jump_implicit) {
      cpup4key.type = resumekey + prepared;
      if (cpuexitblock.keymask & 1)
           cpup4key.databyte = faultresume;
      else cpup4key.databyte = restartresume;
      cpup4key.nontypedata.ik.item.pk.subject =
                     (union Item *)cpudibp->rootnode;
      return &cpup4key;
   }
   if (exitblock.keymask & 1)
      return look(&cpudibp->keysnode->keys[exitblock.key4] );
   return &dk0;
}
 
 
struct Key *prx(struct Key *key)  /* Return prepared key */
                 /* Must be uninvolvedr or a hook */
                 /* Must be in a tied down node */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
*/
{
   if (key->type & prepared) {
      if (key->type & involvedr) {
         if (key->type != pihk)
            crash("PRIMCOM008 prx for non-hook involvedr");
         return hook_look(key);
      }
      return key;
   }
    if (prepkey(key) == prepkey_wait) return NULL;
   return key;
}
 
 
struct Key *prep_passed_key1()  /* Returns prepared first passed key */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
 
      N.B. May return a halfprepared key, key will not be prepared if
           it's type can't be prepared.
*/
{
   register struct exitblock exitblock = cpuexitblock;

   if (exitblock.keymask & 8) {
      return prx(&cpudibp->keysnode->keys[exitblock.key1]);
   }
   return &dk0;
}
 
 
struct Key *prep_passed_key2()  /* Returns prepared second passed key */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
 
      N.B. May return a halfprepared key, key will not be prepared if
           it's type can't be prepared.
*/
{
   register struct exitblock exitblock = cpuexitblock;

   if (exitblock.keymask & 4) {
      return prx(&cpudibp->keysnode->keys[exitblock.key2]);
   }
   return &dk0;
}
 
struct Key *prep_passed_key3()  /* Returns prepared third passed key */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
 
      N.B. May return a halfprepared key, key will not be prepared if
           it's type can't be prepared.
*/
{
   register struct exitblock exitblock = cpuexitblock;

   if (exitblock.keymask & 2) {
      if (exitblock.jumptype == jump_implicit) {
         if (CPUP3_UNLOCKED == cpup3switch)
            crash("PRIMCOM009 Implicit jump and key3 not locked");
         coreunlock_node(cpup3node);
         cpup3switch = CPUP3_UNLOCKED;
         return &cpup3key;    /* Key is half prepared */
      }
      return prx(&cpudibp->keysnode->keys[exitblock.key3]);
   }
   return &dk0;
}
 
 
struct Key *prep_passed_key4()  /* Returns prepared fourth passed key */
/*
   Input -
     cpudibp - Pointer to the jumper's DIB
     cpuexitblock - The jumper's exit block
   Output -
     *Key - If the key is prepared or is a non-prepareable type
     NULL - The object designated is not in memory, cpudibp queued
*/
{
   register struct exitblock exitblock = cpuexitblock;

   if (exitblock.keymask & 1) { /* Jumper is passing a 4th key */
      return prx(&cpudibp->keysnode->keys[exitblock.key4]);
   }
   else return &dk0;               /* No key passed */
} /* End prep_passed_key_4 */
 
 
int compare_keys(key1, key2)       /* Compare two keys */
   struct Key *key1, *key2;           /* The two keys */
/*
   N.B. Both keys have been checked for obsoleteness (DK0)
 
Returns 0 if equal, 1 if not equal
*/
{
   DISKKEY dk1, dk2;
 
   key2dsk(key1,&dk1);
   key2dsk(key2,&dk2);
   return Memcmp(&dk1, &dk2, sizeof dk1)!=0;
}
 
int setupdestpage(jedib)  /* Maps the destination page */
/* Input -  */
register struct DIB *jedib;      /* The jumpee's DIB */
/*
     jedib->rootnode must be preplocked
     cpudibp holds dib of the actor
   Output -
   cpuentryblock is set
     0 - dest page is set up. Meaning:
        If the parameter specification or entry block is invalid
        then the jedib domain will be marked traped and the entry
        block set to not accept the string or keys and string.
        Otherwise cpuparmaddr and cpuparmlength have been set
        If the parameter string is in memory, user window 2
        (and possibly 3) will be set up and the page(s) locked
     1 - page not available, actor enqueued.
   Calls map_user_page
*/
{
   register struct entryblock entryblock;
   get_entry_block(jedib);     /* get the jumpee's entry block */
   entryblock = cpuentryblock; 
   {union{struct entryblock eb; long i;} b;
      b.eb = entryblock;
      if(badeb & b.i){set_trapcode(jedib, 0x308);
        *(uint32 *)&cpuentryblock = 0;
        return 0;
   }}
 
   /* Get the parameter (destination) string pages */
 
   if (entryblock.str) {        /* string wanted */
      unsigned long parmlength;
      unsigned long a = get_parm_pointer(jedib);
      cpuparmlength = parmlength = get_parm_length(jedib);
      if (parmlength == 0) cpuentryblock.str = 0;
      else if (entryblock.regsparm) {   /* in registers */
         cpuparmaddr = check_register_string(jedib, a, parmlength);
         if (NULL == cpuparmaddr) {
            set_trapcode(jedib, 0x310);
            cpuentryblock.str = 0;    /* Pretend no string */
         }
      }
      else {           /* The parameter string is in memory */
         if (0 == (cpuparmaddr = map_parm_string(a, jedib, parmlength))) {
            if (trapcode_nonzero(jedib)) {
               cpuentryblock.str = 0;    /* If trap Pretend no string */
               return 0;
            }
            return 1;  /* Page not available, actor queued */
         }
      }
   }
   return 0;
} /* End setupdestpage */
 
 
int ensuredestpage(     /* Ensure that dest page is set up */
   register struct DIB *dib)      /* The dib of the jumpee */
/*
     dib->rootnode is corelocked, may be preplocked.
     dib->statestore is corelocked
     dib->keysnode is corelocked
     If parameter page is not in memory, the actor node must be
        tied down
   Output -
     0 - returnee not in memory, jumper enqueued.
            dib->rootnode is uncorelocked,
            dib->statestore is uncorelocked
            dib->keysnode is uncorelocked
     2 - Returnee is set up.
        Preplocks and corelocks unchanged.
        If (cpujenode != NULL) THEN
            cpuinvokeddatabyte has type of resume key to returnee
            cpuinvokedkeytype is resumekey+prepared
        cpuentryblock has been set to the entry block.
        The (parameter) destination page is set up.
*/
{
   if (preplock_node(dib->rootnode,lockedby_ensuredestpage)) {
      /* Already preplocked, jumper must == jumpee */
      if (setupdestpage(dib)) {   /* Actor enqueued */
         uncorelockreturnee();
         return 0;
      }
   }
   else {
      if (setupdestpage(dib)) {   /* Actor enqueued */
         uncorelockreturnee();
         unpreplock_node(dib->rootnode);
         return 0;
      }
      unpreplock_node(dib->rootnode);
   }
   return 2;
} /* End ensuredestpage */
 
 
static int prepreturnee(   /* Prepare node as returnee */
   register NODE *re)      /* Pointer to the node */
/* Input - */
/*    cpuactor has actor */
#define prepreturnee_wait 0
#define prepreturnee_overlap 1
#define prepreturnee_ok 2
/*
     If an annex or the parm page might not be in memory, then
     at most cpuactor is preplocked, and if it is, it is prepasdomain.
     If cpuactor is not prepasdomain with cpudibp pointing to its
     DIB the caller is responsible for ensuring that return code 1 is
     not generated.
   cpuexitblock - jumper's exit block
      cpuexitblock.jumptype gives type of jump
   Output -
     prepreturnee_wait:
       - returnee not in memory, jumper enqueued.
     prepreturnee_overlap:
       - returnee overlaps jumper (but not completely).
        trapcode is set
     prepreturnee_overlap:
       - cpujenode is either NULL or prepared.
*/
{
if (re->prepcode != prepasdomain) {
   if (preplock_node(re,lockedby_prepreturnee))
       crash ("PRIMCOM001 - Returnee domain root already locked");
   if (re->prepcode != unpreparednode) {
      if (unprnode(re) != unprnode_unprepared) {
         set_trapcode(cpudibp, 0x620);
         unpreplock_node(re);
         return prepreturnee_overlap;
      }
   }
   switch (prepdom(re)) {
    case prepdom_wait:
      unpreplock_node(re);
      return prepreturnee_wait;
    case prepdom_overlap:
      set_trapcode(cpudibp, 0x624);
      unpreplock_node(re);
      return prepreturnee_overlap;
    case prepdom_malformed:
      unpreplock_node(re);
      cpujenode = NULL;        /* There is no returnee */
      return prepreturnee_ok;
    case prepdom_prepared:
      ;
   }
   unpreplock_node(re);
   return prepreturnee_ok;
}
cpujenode = re;
return 2;
} /* End prepreturnee */

void strangeResume(void);
 
int ensurereturnee(      /* Ensure presence of returnee */
   long retstr)             /* 0 if no string returned, else nz */
                    /* This is declared as a long so the actual string
                       length can be easily passed here. */
/*
   Input -
     cpudibp - Jumper's DIB, cpudibp->rootnode is preplocked
     cpuexitblock - jumper's exit block
        cpuexitblock.jumptype gives type of jump
   Output -
     ensurereturnee_wait - returnee not in memory, jumper enqueued.
     ensurereturnee_overlap - returnee overlaps jumper
                              (but not completely).
           trapcode is set
     ensurereturnee_setup - Returnee is set up.
        cpujenode -> root of returnee or NULL if no returnee
           If (cpujenode != NULL) THEN
              The rootnode is corelocked
              The keysnode is corelocked
                 cpurekeysaddr - Address of the keys node
              The statestore is corelocked
              cpuinvokeddatabyte has type of resume key to returnee
              cpuinvokedkeytype is resumekey+prepared
        cpuentryblock has been set to the entry block.
                      Bad entry block traps have not been recognized
        If (retstr) the (parameter) destination page is set up
*/
{
   register struct Key *key;
   register struct DIB *redib;
   register struct exitblock exitblock = cpuexitblock;
   register NODE *jenode;
 
   switch (exitblock.jumptype) {
    case jump_return:
    case jump_fork:
      key = prep_passed_key4();
      if (key == NULL) return ensurereturnee_wait;
      if (key->type != prepared+resumekey) {
         cpujenode = NULL;
         return ensurereturnee_setup;
      }
      cpuinvokeddatabyte = key->databyte;
      jenode = (NODE *)key->nontypedata.ik.item.pk.subject;
      switch (prepreturnee(jenode)) {
       case prepreturnee_wait: return ensurereturnee_wait;
       case prepreturnee_overlap: return ensurereturnee_overlap;
       case prepreturnee_ok: ;
      }
      redib = jenode->pf.dib;
      break;
    case jump_implicit:
      if (exitblock.keymask & 1)
               cpuinvokeddatabyte = faultresume;
      else     cpuinvokeddatabyte = restartresume;
      redib = cpudibp;
      jenode = cpudibp->rootnode;
      break;
    case jump_call:
      cpuinvokeddatabyte = returnresume;
      redib = cpudibp;
      jenode = cpudibp->rootnode;
      break;
    default: crash("Can't get here.");
   }
   cpujenode = jenode;
   /*
     At this point the following have been set up:
     cpujenode
     jenode
     redib
     cpuinvokeddatabyte.
   */
   cpuinvokedkeytype = resumekey+prepared; /* type is always resume */
 
   corelock_node(22, jenode);
   cpurekeysaddr = redib->keysnode;
   corelock_node(23, cpurekeysaddr);
   corelock_statestore(redib);
   if (cpuinvokeddatabyte != returnresume) {
      strangeResume();
      return ensurereturnee_setup;
   }
   if (retstr) return ensuredestpage(redib);
   else {
      get_entry_block(redib);
      return ensurereturnee_setup;
   }
} /* End ensurereturnee */
 
 
int prepare_domain(rn)
   register NODE *rn;        /* Root node of domain to be prepared */
/*
   Prepares the node as a domain.
 
   Output -
      Return codes:
         prepdom_prepared  - Domain prepared
         prepdom_overlap   - The jumper is trapped,
               cputrapcode is set
         prepdom_wait      - Missing node or page
         prepdom_malformed - Domain is malformed
*/
{
   int rc;
 
   if (rn->prepcode != prepasdomain) {
      if (preplock_node(rn,lockedby_jdomain))
         crash("JDOMAIN010 Non-domainroot preplocked");
      if ((rn->prepcode != unpreparednode) &&
              unprnode(rn) != unprnode_unprepared) {
         cputrapcode = 0x300+36;
         unpreplock_node(rn);
         return prepdom_overlap;
      }
      if ((rc = prepdom(rn)) == prepdom_overlap)
         cputrapcode = 0x300+40;
      unpreplock_node(rn);
      return rc;
   }
   return prepdom_prepared;
}
 
 
bool dry_run_prepare_domain(
   register NODE *rn,      /* Root node of domain to be prepared */
   long str)               /* Nonzero if a string may be returned */
                    /* This is declared as a long so the actual string
                       length can be easily passed here. */
/*
   Corelocks node, ensures returnee, and prepares node as a domain.
 
   Output -
      Returns TRUE if domain is prepared, rn is still corelocked.
      Returns FALSE if domain couldn't be prepared.
*/
{
   int rc;
 
   corelock_node(24, rn);
   switch (ensurereturnee(str)) {
    case ensurereturnee_wait:
      coreunlock_node(rn);
      abandonj();
      return FALSE;
    case ensurereturnee_overlap:
      coreunlock_node(rn);
      midfault();
      return FALSE;
    case ensurereturnee_setup: break;
   }
   rc = prepare_domain(rn);
   if (rc == prepdom_prepared) return TRUE;
   coreunlock_node(rn);
   if (rc == prepdom_malformed) {
      /* The domain rn is malformed. Give return code 2. */
      handlejumper();
      cpuexitblock.keymask = 0;  /* No returned keys */
      cpuarglength = 0;          /* No returned string */
      cpuordercode = 2;          /* Return code 2 */
      if (! getreturnee()) return_message();
      return FALSE;
   }
   unsetupreturnee();
   if (rc == prepdom_wait) abandonj();
   else midfault();
   return FALSE;
} /* End dry_run_prepare_domain */
 
 
static void check_new_entry_block(void)
/* Check entry block for validity */
/*
   N.B. rn must be prepared as domain. It is assumed that the caller
        will not return a string, as the new values for the parameter
        string are not checked.
*/
{
   union{struct entryblock eb; long i;} b;
   b.eb = cpuentryblock;
   if(badeb & b.i){
      set_trapcode(cpudibp, 0x308);
      *(uint32 *)&cpuentryblock = 0;}
}
 
 
void jsimplecall()    /* Fast path for call invocations of keys */
/*
   N.B. If the caller returns a string then setupdestpageforcall()
        must have already been called.  Otherwise cpuarglength must
        be equal to zero.
 
   Input -
       dry run has been ended.
     cpuordercode - Set to return code
     cpuarglength - set to length of returned string
       cpuargaddr - set if cpuarglength != 0
       cpuexitblock.argtype - set to arg_regs if cpuarglength != 0
     cpuexitblock.keymask - set for keys being returned
       cpup1key ... cpup4key - Set according to keymask
     cpuexitblock.jumptype - Has original jump type (i.e. jump_call)
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run
*/
{
   cpuinvokeddatabyte = returnresume;
   cpuinvokedkeytype = resumekey+prepared;
   get_entry_block(cpudibp);
   check_new_entry_block();
   return_message();    /* Will release_arg_page & p2node */
   return;
}
 
 
void jsimple (keymask)       /* Standard return from restartable key calls */
/*
   Input -
     cpuordercode - Set to return code
     cpuarglength - set to length of returned string
       cpuargaddr - set if cpuarglength != 0
       cpuexitblock.argtype - set to arg_regs if cpuarglength != 0
     cpuexitblock.keymask  - Has original keymask
     cpuexitblock.jumptype - Has original jump type
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run
*/
   int keymask;   /* the key mask for the exit block when actually delivering mesg */
 /*      cpup1key ... cpup4key - Set according to keymask */
{
   if (cpuexitblock.jumptype == jump_call) {   /* Use fast method */
      cpuinvokeddatabyte = returnresume;
      cpuinvokedkeytype = resumekey+prepared;
      get_entry_block(cpudibp);
      if (cpuarglength) {
         if (setupdestpage(cpudibp)) {
            abandonj();
            return;
         }
      }
   /* End dry run */
      check_new_entry_block();
      cpuexitblock.keymask=keymask;
      return_message();    /* Will release_arg_page & p2node */
      return;
   }
   else {                                      /* Use the slow way */
      switch (ensurereturnee(cpuarglength)) {
       case ensurereturnee_wait:  {
          abandonj();
          return;
       }
       case ensurereturnee_overlap: {
          midfault();
          return;
       }
       case ensurereturnee_setup: handlejumper();
      }
   /* End dry run */
      cpuexitblock.keymask=keymask;
      if (! getreturnee()) return_message();
      return;
   }
} /* End jsimple */
 
 
void simplest(       /* Return rc from restartable key calls */
     long rc)        /* The return code to pass */
/*
   Input -
     cpuexitblock.jumptype - Has original jump type
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run if any
*/
{
   cpuarglength = 0;
   cpuordercode = rc;
   jsimple(0);  /* pass a key mask of zero */
   return;
} /* End simplest */
 
 
struct codepcfa validatepagekey(struct Key *pk1)
/* Validate that an unprepared page key is current */
/* Caller must have called tryprep() for the key. */
/* Values returned in codepcfa.code: */
/*    vpk_wait     0    abandonj() has been called */
/*    vpk_ioerror  1    */
/*    vpk_obsolete 2    key is now dk0 */
/*    vpk_current  3    key is valid, codepcfa.pcfa is set */
{
   struct codepcfa ga;
   ga = getalid(pk1->nontypedata.ik.item.upk.cda);
   switch (ga.code) {
    case get_wait:
      abandonj();
      ga.code = vpk_wait;
      break;
    case get_ioerror:
      ga.code = vpk_ioerror;
      break;
    case get_tryagain:
      if (pk1->nontypedata.ik.item.upk.allocationid !=
          ga.pcfa->allocationid) {  /* key is obsolete */
         *pk1 = dk0;     /* change to dk0 */
         ga.code = vpk_obsolete;
         break;
      }
        /* fall through to default */
    default:
      ga.code = vpk_current; /* and leave ga.pcfa */
   }
   return ga;
}
