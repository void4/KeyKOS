/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "geteh.h"  /* get codepcfa */
#define setupdestpageforcall() setupdestpage(cpudibp)
 
void priminit(void);
 
struct Key *readkey(        /* Prepare to fetch from a slot of a node */
   struct Key *key);           /* Slot to fetch from */
/*
   Output -
      Unprepared node if necessary
      NULL - Some preplocked node (other than node) prevented
             unpreparing node
      Otherwise pointer to the key or a copy, not involvedr
*/
 
 
struct Key *prx(           /* Return prepared key */
   struct Key *key);         /* Must be uninvolvedr or a hook */
                             /* Must be in a tied down node */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
*/
 
 
extern int prepare_domain(
   NODE *rn);             /* Root node of domain to be prepared */
/*
   Prepares the node as a domain.
 
   Output -
      Return codes:
         prepdom_prepared  - Domain prepared
         prepdom_overlap   - The jumper is trapped
         prepdom_wait      - Missing node or page
         prepdom_malformed - Domain is malformed
*/
 
 
extern bool dry_run_prepare_domain(
   NODE *rn,            /* Root node of domain to be prepared */
   long int str);         /* Nonzero if a string could be returned */
/*
   Corelocks node, ensures returnee, and prepares node as a domain.
 
   Output -
      Returns TRUE if domain is prepared, rn is still corelocked.
      Returns FALSE if domain couldn't be prepared.
*/
 
 
extern void pad_move_arg(           /* Move argument, pad with zeroes */
   void *to,                          /* To address for move */
   int len                            /* Length of move */
   );
/*
      N.B. This routine updates cpuargaddr and cpuarglength. In this
           sense it moves rather than copies the string
*/
 
 
extern struct Key *ld1(void);    /* Get jumper's first key */
extern struct Key *ld2(void);    /* Get jumper's second key */
extern struct Key *ld3(void);    /* Get jumper's third key */
extern struct Key *ld4(void);    /* Get jumper's fourth key */
 
 
extern struct Key *prep_passed_key1(void); /* First passed key  */
extern struct Key *prep_passed_key2(void); /* Second passed key */
extern struct Key *prep_passed_key3(void); /* Third passed key  */
extern struct Key *prep_passed_key4(void); /* Fourth passed key */
/*
   Returns:
      *key if key OK, NULL if cpudibp (actor) has been queued for I/O
 
      N.B. May return a halfprepared key, key will not be prepared if
           it's type can't be prepared.
*/
 
 
extern int clean(            /* Prepare to store into a node slot */
   struct Key *key);              /* The slot to store into */
/* Output - */
#define clean_dont 1            /* Don't store in slot - has hook */
#define clean_ok 0              /* OK to store into the slot */
 
 
extern int puninv(           /* Prepare to fetch or store a node slot */
   struct Key *key);             /* Pointer to the slot */
 
/* Output - */
#define puninv_cant 1
/* Some preplocked node (other than node) prevented unpreparing node */
#define puninv_ok 0            /* Slot is uninvolved or a hook */
 
 
#define unsetupdestpage() release_parm_pages()


extern int setupdestpage(
   struct DIB *jedib);     /* The jumpee's dib */
/* Maps the destination page */
/* Input -  */
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
 
 
extern void zapresumes(
   struct DIB *dib);
/* Zap all resume keys to a domain */
 
 
void jsimplecall(void);   /* Fast path for call invocations of keys */
/*
   N.B. If the caller returns a string then setupdestpageforcall()
        must have already been called.  Otherwise cpuarglength must
        be equal to zero.
 
   Input -
     cpuordercode - Set to return code
     cpuarglength - set to length of returned string
       cpuargaddr - set if cpuarglength != 0
       cpuexitblock.argtype - set to arg_regs if cpuarglength != 0
     cpuexitblock.keymask - set for keys being returned
       cpup1key ... cpup4key - Set according to keymask
     cpuexitblock.jumptype - Has original jump type
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run
*/
 
 
extern void jsimple(int);    /* Simple return from primary key */
/* int keymask;  the keymask used to determine what keys are passed in message */
/*
   Input -
     cpuordercode - Set to return code
     cpuarglength - set to length of returned string
       cpuargaddr - set if cpuarglength != 0
       cpuexitblock.argtype - set to arg_regs if cpuarglength != 0
     cpuexitblock.keymask - set for keys being returned
       cpup1key ... cpup4key - Set according to keymask
     cpuexitblock.jumptype - Has original jump type
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run if any
*/
 
 
extern void simplest(
   long rc);    /* the return code to pass */
/* Return rc from restartable key calls */
/*
   Input -
     rc is the return code to pass
     cpuexitblock.jumptype - Has original jump type
     cpudibp - Jumper's dib. cpudibp->rootnode is preplocked
   Output - following set up:
     cpudibp - pointer to the dib to run if any
*/
 
 
extern struct Key *hook_look(
   struct Key *k);
/* Make hook key look like dk0 */
 
 
struct Key *look(            /* Look at a key */
   struct Key *key);            /* The key to look at */
/*
  Definition: A hook looks like a data key. Others look like themselves.
 
   Output -
      key is uninvolvedr. It is suitable for reading only.
      Tries to prepare the key without doing I/O.
*/
 
 
extern void handlejumper(void);   /* End of dry run - release jumper */
 
extern void unsetupreturnee(void);    /* Undo ensurereturnee */
 
extern int ensurereturnee(long retstring);/* Ensure presence of returnee */
#define ensurereturnee_wait 0
#define ensurereturnee_overlap 1
#define ensurereturnee_setup 2
 
 
extern int getreturnee(void);     /* After dry run, zap resumes etc. */
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
 
 
extern void makeready(               /* Make a busy domain ready */
   struct DIB *dib);            /* DIB of domain to make ready */
 
 
extern void zapprocess(  /* Remove process from node and zap "resume" */
   NODE *rn);               /* The root node of the domain */
/*
   Other Input -
      cpujenode - Pointer to returnee's root node or NULL
 
   Output - None
      cpudibp is invalid (i.e. the cpu is not executing any domain).
      Leaves an involved dk(1) or a worry hook in slot 13.
*/
 
 
int compare_keys(
   struct Key *key1,                  /* The two keys */
   struct Key *key2);
/*
   N.B. Both keys have been checked for obsoleteness (DK0)
 
Returns 0 if equal, 1 if not equal
*/
 
extern struct codepcfa validatepagekey(struct Key *pk1);
/* Validate that an unprepared page key is current */
/* Caller must have called tryprep() for the key. */
/* Values returned in codepcfa.code: */
#define vpk_wait     0   /* abandonj() has been called */
#define vpk_ioerror  1
#define vpk_obsolete 2   /* key is now dk0 */
#define vpk_current  3   /* key is valid, codepcfa.pcfa is set */
