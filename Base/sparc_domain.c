/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* This version for the Sparc */


#include "string.h"
#include "sysdefs.h"
#include "lli.h"
#include "keyh.h"
#include "cpujumph.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "locksh.h"
#include "memomdh.h"
#include "meterh.h"
#include "cpumemh.h"
#include "domamdh.h"
#include "kermap.h"
#include "domainh.h"
#include "dependh.h"
#include "gateh.h"
#include "psr.h"
#include "locore.h"
#include "sparc_cons.h"
#include "memutil.h"

static void clean_windows(struct DIB * dm){};

/* For the Sparc back_up_jumper kludge */
extern ulong_t cpu_int_pc;
extern ulong_t cpu_int_npc;


/* Bits in domain level psr that make domain malformed */
#define BADPSRBITS 0xff0fef7f

void back_up_jumper(void)
 /* Backs up PC to re-do the jump for cpudibp domain */
{
   if (cpubackupamount) {
      cpubackupamount = 0;
      cpudibp->pc = cpu_int_pc;
      cpudibp->npc = cpu_int_npc;
   }
}

void set_inst_pointer(
   struct DIB *dib,
   unsigned long ip)
{
   dib->pc = ip;
   dib->npc = dib->pc + 4;
}

 
void set_trapcode(
   register struct DIB *dib,   /* DIB to trap */
   unsigned short code)        /* trap code for the domain */
   /*   cpudibp - pointer to the dib to trap */
{
   dib->readiness |= TRAPPED;
   dib->Trapcode = code;
}


void clear_trapcode(
   struct DIB *dib)
{
   dib->Trapcode = 0;
   dib->readiness &= ~TRAPPED;
}


int trapcode_nonzero(
   struct DIB *dib)
/* Returns 1 if trapcode is nonzero, otherwise 0 */
{
   return (dib->Trapcode) != 0;
}


void deliver_to_regs(  /* Delivers len bytes from cpuargaddr to registers */
   struct DIB *dib,
   int len)
{
   unsigned int offset;

   clean_windows(dib);  /* Ensure registers are stored in the DIB */

   if (cpuargaddr != cpuargpage) { 
      if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
         Memcpy(cpuargpage, cpuargaddr, len);
      } else if ( 0 == movba2va(cpuargpage, cpuargaddr, len) ) {
         crash("SPARC_DOMAIN001 - Overlap with cpuargpage?");
      }
      cpuargaddr = cpuargpage;
   }
   offset = cpuparmaddr - (char*)dib->regs;
   if (offset < 64) {
      int len1 = (offset+len>64 ? 64-offset : len);
      Memcpy(cpuparmaddr, cpuargaddr, len1);
      len -= len1;
      if (len) {
         Memcpy((char*)(dib->backset+dib->backalloc), cpuargaddr+len1, len);
      }
   } else {
      offset = cpuargaddr - (char*)(dib->backset+dib->backalloc);
      if (offset>64) crash("SPARCDOM001 Invalid register string pointer");
      Memcpy((char*)(dib->backset+dib->backalloc)+offset, cpuargaddr, len);
   }
}


/* Get string in registers */
char *get_register_string(  /* Returns a pointer to the string (or copy of) */
   struct DIB *dib,
   unsigned long origin,
   unsigned long length)
/* Returns pointer to area in kernel address space,
   or NULL if requested area does not fit in the registers */
{
   if (   origin > 128  /* ensure no overflow on add */
       || length > 128
       || (origin + length) > 128 ) {
      return NULL;
   }
   clean_windows(dib);  /* Ensure registers are stored in the DIB */

   if (origin>=64) return (char *)(dib->backset+dib->backalloc) + origin-64;
   if (origin<64 && (origin+length)<64) return (char *)dib->regs + origin;
   /* Must copy the string to cpuargpage */
   Memcpy(cpuargpage, (char*)dib->regs+origin, 64-origin);
   Memcpy(cpuargpage+64-origin, (char*)(dib->backset+dib->backalloc),
          length+origin-64);
   return cpuargpage;
}


/* Check string in registers */
char *check_register_string(  /* Returns 0 iff string is invalid */
   struct DIB const *dib,
   unsigned long const origin,
   unsigned long const length)
{
  if (   origin > 128  /* ensure no overflow on add */
       || length > 128
       || (origin + length) > 128 ) {
      return 0;
   }
   if (origin<64) return (char*)dib->regs + origin;
   return (char*)(dib->backset+dib->backalloc) + origin-64;
}


int node_overlaps_statestore(NODE const *np)
/* Returns 1 if the node is (part of) the statestore of cpudibp,
   0 otherwise */
{
   NODE *n = cpudibp->statestore;
   
   for (;;) {
      if (n == np) return 1;
      if (datakey+involvedw == n->keys[0].type) return 0;
      n = (NODE*)n->keys[0].nontypedata.ik.item.pk.subject;
   }
}

 
int format_control(
   struct DIB * dib /* Pointer to dib of domain */,
   unsigned char * const buffer /* Place for output */)
/*
   Output -
      Returns number of bytes of data in the control information.
*/
{
   unsigned char *c = buffer;       /* Set initial char pointer */

   if (dib->psr & PSR_EF) clean_fp(dib);

   Memcpy(c, (char*)&dib->pc, 8);   /* pc, npc */
   c += 8;

   *(long*)c = (dib->psr & (PSR_ICC | PSR_EF)) | 
                      (dib->permits&GATEJUMPSPERMITTED ? PSR_S : 0);
   c += 4;
   
   memzero2(c);                     /* Leading zeroes on trapcode */ 
   c += 2;
   
   Memcpy(c, (char*)&dib->Trapcode, 2); /* trapcode */
   c += 2;
    
   Memcpy(c, (char*)dib->trapcodeextension, 8); /* trapcodeextension */
   c += 8;
 
   Memcpy(c, (char*)&dib->fsr, 12);  /* fsr + first queued fp */
   c += 12;

   return c - buffer;
} /* End format_control */


static NODE *cpurestateaddr;  /* Addr dom state page/node */

void coreunlock_statestore(void)
/* unlocks the statestore locked by corelock_statestore */
{
   NODE *n = cpurestateaddr;

   for (;;) {
      coreunlock_node(n);
   if (datakey+involvedw == n->keys[0].type) break;
      n = (NODE*)n->keys[0].nontypedata.ik.item.pk.subject;
   }
}

void corelock_statestore(
   struct DIB *dib)
{
   NODE *n = cpurestateaddr = dib->statestore;
   
   for (;;) {
      corelock_node(1, n);
   if (datakey+involvedw == n->keys[0].type) break;
      n = (NODE*)n->keys[0].nontypedata.ik.item.pk.subject;
   }
}


static NODE *unprepare_statenode(NODE *n)
{
   NODE *nn = (NODE*)n->keys[0].nontypedata.ik.item.pk.subject;

   uninvolve(&n->keys[0]);
   {int j=16; while(--j) n->keys[j].type = datakey;}
   n->prepcode = unpreparednode;
   return nn;
}
 

void unpr_dom_md(NODE *rn)
{
   struct DIB *dib = rn->pf.dib;
   NODE *sn;
   char *c;
   int backwindowcount;

   clean_windows(dib);   /* Get domain's registers into dib */
   
   if (dib->permits & FPPERMITTED) {

          /* Copy the fsr, the deferred queue, and the floating */
          /* registers to the floating point state node */
      if(rn->domfpstatekey.type != 0xa3) Panic();
      rn->domfpstatekey.type = prepared | nodekey;
      sn = (NODE*)rn->domfpstatekey.nontypedata.ik.item.pk.subject;
      c = (char *)&dib->fsr;

      if (dib->psr & PSR_EF) clean_fp(dib);

      {int j; for(j=0; j<14; ++j) {
        Memcpy(sn->keys[j].nontypedata.dk11.databody11, c, 11);
        c += 11;}}
      Memcpy(sn->keys[14].nontypedata.dk11.databody11, c, 10);
      unprepare_statenode(sn);
      uninvolve(&rn->domfpstatekey);
   } else {if (rn->domfpstatekey.type != involvedw) Panic();
           else rn->domfpstatekey.type = datakey;}
 
/*
 Move the registers and state from the dib to the state node
*/
   sn = (NODE*)rn->domstatekey.nontypedata.ik.item.pk.subject;
   c = (char *)dib->regs;
 
     /* Copy over the registers */
 
   { int j; for(j=1; j<6; ++j) {
     Memcpy(sn->keys[j].nontypedata.dk11.databody11, c, 11);
     c += 11;}}
   
   Memcpy(sn->keys[6].nontypedata.dk11.databody11, c, 9);
   c = (char*)&dib->backset[dib->backalloc];
   Memcpy(sn->keys[6].nontypedata.dk11.databody11+9, c, 2);
   c += 2;
   {int j; for(j=7; j<12;++j){
      Memcpy(sn->keys[j].nontypedata.dk11.databody11, c, 11);
      c += 11;}}
   Memcpy(sn->keys[12].nontypedata.dk11.databody11, c, 7);
 
  
     /* Copy pc, npc, and psr to state node */
 
   dib->psr &= ~BADPSRBITS;    /* Convert psr to user style */
   Memcpy(sn->keys[12].nontypedata.dk11.databody11 + 7,
            (char *)&dib->pc, 4);
   Memcpy(sn->keys[13].nontypedata.dk11.databody11,
            (char *)&dib->npc, 8);
   if (dib->permits & GATEJUMPSPERMITTED) {
      sn->keys[13].nontypedata.dk11.databody11[7] |= 0x80;
   }

     /* Set the number of back windows from DIB to statenode */

   backwindowcount = (dib->backalloc - dib->backdiboldest) & 31;
   *(sn->keys[13].nontypedata.dk11.databody11 + 8) = backwindowcount;

   /* Set up pointers for copying the back windows */
   {
      unsigned char *t = sn->keys[13].nontypedata.dk11.databody11+9;
      int s = 11-9;     /* amount left in key */
      int k = 13;       /* slot number of key */

       /* Copy the back windows to state node */

      char *c = (char*)&dib->backset[dib->backdiboldest];
      int n = backwindowcount * sizeof(backwindow);

      for (; n; ) {
         int l = (s<n ? s : n);

         if (c+l > (char*)&dib->backset[32]) { /* Wrap around */
            int fl = (char*)&dib->backset[32] - c;
            Memcpy(t, c, fl);
            c = (char*)dib->backset;
            Memcpy(t+fl, c, l-fl);
            c += l-fl;
         } else {
            Memcpy(t, c, l);
            c += l;
         }
         if ( (n -= l) == 0) {
            s -= l;
            t += l;
      break;
         }
         if (16 == ++k) {
            /* Get next node in chain */
            sn = unprepare_statenode(sn);
            k = 1;
         }
         s = 11;
         t = sn->keys[k].nontypedata.dk11.databody11;
      }
      /*  Clear rest of node(s) to DK(0) */

      if (s) Memset(t, 0, s);
      for (;;) {
         if (16 == ++k) {
            /* Get next node in chain */
            sn = unprepare_statenode(sn);
            k = 1; 
         }
      if (NULL == sn) break;
         t = sn->keys[k].nontypedata.dk11.databody11;
         Memset(t, 0, 11);
      }
   }

     /* Copy the trapcode and trapecode extension to domain root */

   Memcpy(rn->domtrapcode.nontypedata.dk11.databody11+1,
          (char*)&dib->Trapcode, 10);
   rn->domtrapcode.type = datakey;  /* Uninvolve the key */
 
     /* Uninvolve the memory key */
 
   if (rn->dommemroot.type & involvedw) /* If memory involved */
      uninvolve(&rn->dommemroot);

}  /* End unpr_dom_md */


unprnode_ret superzap_dom_md(
   NODE *hn,      /* A node prepared as a domain */
   int slot)      /* slot number in the above node containing an involved key */
/* Uninvolves the slot in the node. */
{
   switch (slot) {
    case 1:                /* Domain's meter */
      retcache(hn->pf.dib);
      break;
    case 3:                /* Domain's memory tree root */
      slotzap(&hn->keys[slot]);
      zap_dib_map(hn->pf.dib);
      break;
    case  5:               /* Trap code etc. */
    case  9:               /* FP state node */
    case 12:               /* Domain's priority */
    case 13:               /* Domain's hook */
    case 14:               /* Domain's keys key */
    case 15:               /* Domain's state key */
      if (unprnode(hn) == unprnode_cant) return unprnode_cant;
      break;
    default: crash("UNPRND007 Superzap for dom slot never involved");
   }
   uninvolve(&hn->keys[slot]);
   return unprnode_unprepared;
}


static void undo_involve_key(  /* Undoes involving annex key, runs the
                                  chain uninvolving the node keys in slot
                                  zero and unpreplocking the nodes.  
                                  Uninvolves the data key in slot zero
                                  of the last node */
   struct Key *key)            /* Pointer to the annex key to uninvolved */
{
   while (key->type == nodekey+prepared+involvedw) {
      NODE *next = (NODE *)key->nontypedata.ik.item.pk.subject;

      uninvolve(key);          /* Maintain backchain order */
      key = &next->keys[0];
      unpreplock_node(next);
   }
   if (key->type & nodekey)
        uninvolve(key);
   else
        key->type = datakey;
}


static int next_statenode(struct Key *key, /* Key that points to node */
                          NODE **sn) {     /* Returned pointer to node */
/* Returns: */
/*   prepdom_prepared  {0}  There is no next node and sn has been set to NULL
                               or there is a next node which sn now points to. */
/*   prepdom_overlap   {1}  The domain overlaps with a
                               preplocked node */
/*   prepdom_wait      {2}  An object must be fetched, actor queued */
/*   prepdom_malformed {3}  The domain is malformed */

   NODE *n;

   if (key->type != nodekey+prepared &&
           key->type != nodekey) {
      if (key->type != datakey) return prepdom_malformed;
      *sn = NULL;
      return prepdom_prepared;
   }
   switch (involven(key,unpreparednode)) {
    case involven_ioerror:    crash("PREPDOM003 statenode I/O error");
    case involven_wait:       return prepdom_wait;
    case involven_obsolete:
      key->type = datakey+involvedw; 
      *sn = NULL; 
      return prepdom_prepared; 
    case involven_preplocked: return prepdom_overlap;
    case involven_ok:         /* Key has been involved */
      *sn = (NODE *)key->nontypedata.ik.item.pk.subject;
   }
   n = *sn;

/*
  Ensure all state keys in new state node are data keys
*/
   {int j = 16; while(--j) if (n->keys[j].type) return prepdom_malformed;}
   return prepdom_prepared;
}


int prepdom_md(
/* Do machine-dependent part of domain preparation.
   Sets GATEJUMPSPERMITTED.
 */
   NODE *dr,          /* Root node to be prepared */
   struct DIB *dib)   /* Dib being built */
/* Returns: */
/*   prepdom_prepared  {0}  The machine-dependent part of the dib
                               has been prepared */
/*   prepdom_overlap   {1}  The domain overlaps with a
                               preplocked node */
/*   prepdom_wait      {2}  An object must be fetched, actor queued */
/*   prepdom_malformed {3}  The domain is malformed */
{
   NODE *sn;           /* The annex node */
   char *c;                         /* For moving to the dib */
   int numbackwindows;

/*
  Ensure trap code slot in domain root is data key.
*/
   if (dr->keys[5].type) {
/*    undo_involve_key(&dr->domstatekey); */
      return prepdom_malformed;
   }

   /* Get and check floating point statenode */

   switch (next_statenode(&dr->domfpstatekey, &sn)) {
    case prepdom_overlap:
      undo_involve_key(&dr->domfpstatekey);
      return prepdom_overlap;
    case prepdom_wait:
      undo_involve_key(&dr->domfpstatekey);
      return prepdom_wait;
    case prepdom_prepared:
      break;
    case prepdom_malformed:
      undo_involve_key(&dr->domfpstatekey);
      return prepdom_malformed;
   }
   dr->domfpstatekey.type |= involvedw;
   
   if (sn) {   /* Domain has a floating point state node */

          /* Copy the fsr, the deferred queue, and the floating */
          /* registers from the floating point state node */

      c = (char *)&dib->fsr;

      {int j; for(j=0; j<14; ++j) {
         Memcpy(c, sn->keys[j].nontypedata.dk11.databody11, 11);
         c += 11;}}
      Memcpy(c, sn->keys[14].nontypedata.dk11.databody11, 10); 
      dib->permits |= FPPERMITTED;
      dib->fsr &= 0xcfcfefff;   /* Zero reserved bits */
   } else {			/* No floating point */
      dib->fsr = 0;
      dib->permits &= ~FPPERMITTED;
      dib->deferred_fp[0].address = 0;
      dib->deferred_fp[0].instruction = 0;
   }
      

   /* Get and check 1st statenode */

   switch (next_statenode(&dr->domstatekey, &sn)) {
    case prepdom_overlap:
      undo_involve_key(&dr->domfpstatekey);
      undo_involve_key(&dr->domstatekey);
      return prepdom_overlap;
    case prepdom_wait:
      undo_involve_key(&dr->domfpstatekey);
      undo_involve_key(&dr->domstatekey);
      return prepdom_wait;
    case prepdom_prepared:
      if (sn) break;          /* Fall thru if domstatekey is a data key */
    case prepdom_malformed:
      undo_involve_key(&dr->domfpstatekey);
      undo_involve_key(&dr->domstatekey);
      return prepdom_malformed;
   }
   dib->statestore = sn;

/*
  Fill in the registers etc in the dib
*/
   c = (char *)dib->regs;
 
     /* Copy over the registers to the dib */
   
   {int j; for(j=1; j<6; ++j) {
      Memcpy(c,sn->keys[j].nontypedata.dk11.databody11,11);
      c += 11;}}
 
   Memcpy(c,sn->keys[6].nontypedata.dk11.databody11,9);
//   c = (char*)dib->backset;   /* Newest to backset[0] */
   c = (char*)&dib->backset[16];   /* Newest to backset[16] */
   Memcpy(c,sn->keys[6].nontypedata.dk11.databody11+9,2);
   c += 2;
   {int j; for(j=7; j<12; ++j){
      Memcpy(c,sn->keys[j].nontypedata.dk11.databody11,11);
      c += 11;}}
   Memcpy(c,sn->keys[12].nontypedata.dk11.databody11,7);
   c += 7;
  
  
     /* Copy pc, npc, and psr to the dib */

   Memcpy((char *)&dib->pc, 
          sn->keys[12].nontypedata.dk11.databody11 + 7, 4);
   Memcpy((char *)&dib->npc, 
          sn->keys[13].nontypedata.dk11.databody11, 8);
   if (dib->psr & BADPSRBITS) {  /* Check for bad bits */
/*    unpreplock_node(sn); */
      undo_involve_key(&dr->domfpstatekey);
      undo_involve_key(&dr->domstatekey);
      return prepdom_malformed;
   }  
   dib->psr = ((dib->psr & PSR_ICC) | PSR_S); /* With bits we want */

   if (sn->keys[13].nontypedata.dk11.databody11[7] & 0x80) {
      dib->permits |= GATEJUMPSPERMITTED;
   } else dib->permits &= ~GATEJUMPSPERMITTED;

     /* Set the number of back windows from DIB to statenode */

   numbackwindows = sn->keys[13].nontypedata.dk11.databody11[8];
   if (numbackwindows>31) {         /* Malformed, too many back windows */
      undo_involve_key(&dr->domfpstatekey);
      undo_involve_key(&dr->domstatekey);
      return prepdom_malformed;
   } 
//   dib->backdiboldest = (0-numbackwindows) & 31;
//   dib->backalloc = 0;              /* Most recent is in backset[0] */
   dib->backdiboldest = (16-numbackwindows) & 31;
   dib->backalloc = 16;              /* Most recent is in backset[0] */

   /* Set up pointers for copying the back windows */
   {  /* Handle domain statenode chain */
      unsigned char *f = sn->keys[13].nontypedata.dk11.databody11+9;
      int s = 88;       /* Total bytes of windows, 88 in current node */
      int maxn = 12;    /* max number statenodes after 1st */
      char *t = (char *)&dib->backset[dib->backdiboldest]; /* Copy to */

      {
         int n = numbackwindows * sizeof(backwindow);
         int k;                 /* Slot number being copied */

         if (numbackwindows) {     /* At least one to copy */
            Memcpy(t, f, 2);        /* Copy from the first statenode */
            t += 2;
            f = sn->keys[14].nontypedata.dk11.databody11;
            Memcpy(t, f, 11);
            t += 11;
            f = sn->keys[15].nontypedata.dk11.databody11; 
            Memcpy(t, f, 11); 
            t += 11;
            n -= 11+11+2;
         }
         for (; maxn; maxn--) {
            switch (next_statenode(&sn->keys[0], &sn)) {
             case prepdom_overlap:
      	       undo_involve_key(&dr->domfpstatekey);
               undo_involve_key(&dr->domstatekey);
               return prepdom_overlap;
             case prepdom_wait:
      	       undo_involve_key(&dr->domfpstatekey);
               undo_involve_key(&dr->domstatekey);
               return prepdom_wait;
             case prepdom_prepared: 
                break;
             case prepdom_malformed:
      	       undo_involve_key(&dr->domfpstatekey);
               undo_involve_key(&dr->domstatekey);
               return prepdom_malformed;
            }

         if (!sn) break;
            s += 11*15;               /* Add in another node of windows */
            if (n) for (k=1; k<16; k++) {    /* Copy out the data */
               int sz = (n<11 ? n : 11);

               f = sn->keys[k].nontypedata.dk11.databody11;
               Memcpy(t, f, sz);
               if ( (n -= sz) == 0) {  /* Copied it all */
            break;
               }
               t += sz;
            }
         }
      }  /* End copy and prepare statenodes */

      if (0 == maxn) {         /* Malformed, too many statenodes */
      	 undo_involve_key(&dr->domfpstatekey);
         undo_involve_key(&dr->domstatekey);
         return prepdom_malformed;
      }

      dib->backmax = s >> 6;  /* Max is bytes / (16*4) */
   }  /* End handle domain statenode chain */

   /* Copy trapcode and trap code extension to to the dib */

   if (Memcmp(dr->domtrapcode.nontypedata.dk11.databody11, "\0", 1)) {
       undo_involve_key(&dr->domfpstatekey);
       undo_involve_key(&dr->domstatekey);
       return prepdom_malformed;
   }   
   c = (char *)&dib->Trapcode;
   Memcpy(c, dr->domtrapcode.nontypedata.dk11.databody11+1,10);
   if(dib->Trapcode) dib->readiness |= TRAPPED;

   dib->map=NULL_MAP;
   
/* Now the preparation cannot fail. */

     /* Mark the keys as involved and the nodes as prepared */

   if (dr->domfpstatekey.type == nodekey+prepared+involvedw) {
      sn = (NODE*)dr->domfpstatekey.nontypedata.ik.item.pk.subject;

      {struct Key *kp, *kpl = &sn->keys[16];
       for(kp = &sn->keys[0]; kp < kpl; ++kp)
       kp->type = datakey+involvedw+involvedr;}

      sn->pf.dib = dib;
      sn->prepcode = prepasstate;
      sn->flags |= NFDIRTY;
      unpreplock_node(sn);
   }
   for (sn=(NODE*)dr->domstatekey.nontypedata.ik.item.pk.subject;
        sn;
        sn=(NODE*)sn->keys[0].nontypedata.ik.item.pk.subject) {
           struct Key *kp, *kpl = &sn->keys[16];
           sn->keys[0].type |= involvedw;
           for(kp = &sn->keys[1]; kp < kpl; ++kp)
              kp->type = datakey+involvedw+involvedr;
 
      sn->pf.dib = dib;
      sn->prepcode = prepasstate;
      sn->flags |= NFDIRTY;
      unpreplock_node(sn);
   }
   dr->keys[5].type = datakey+involvedw+involvedr;

#if defined(viking)
   dib->dom_instructions = 0;
   dib->dom_cycles       = 0;
   dib->ker_instructions = 0;
   dib->ker_cycles       = 0;
#endif

   return prepdom_prepared;
}


void call_domain_keeper(void)
/* cpuordercode must be set. */
{
      cpup3key.databyte = 0;
      cpup3key.type = domainkey+prepared;
      cpup3node = cpudibp->rootnode;
      cpuexitblock.keymask = 2;
      cpuexitblock.argtype = arg_regs;
      /* Set cpuargaddr & length for call to domain keeper */
      /* string is built in cpuargpage */
      {
         register unsigned char *c;

         clean_windows(cpudibp);
         c = (unsigned char *)cpuargpage;
         cpuargaddr = (char *)c;
         Memcpy(c,(char*)cpudibp->regs,16*4);
         Memcpy(c+16*4, (char*)&cpudibp->backset[cpudibp->backalloc], 16*4);
         c += 32*4;
         c += format_control(cpudibp, c);
         cpuarglength = c - (unsigned char *)cpuargaddr;
      }
      keepjump(&cpudibp->rootnode->domkeeper, faultresume);
}


void dispatch_trapped_domain(void)
/* Handle dispatch of a domain with TRAPPED bit on */
{
   if (cpudibp->Trapcode) {
      /* Invoke domain keeper */
      cpuordercode = 0x80000000
               + (cpudibp->Trapcode);
      call_domain_keeper();
   }
   else cpudibp->readiness &= ~TRAPPED; /* it was a false alarm */
}

extern void idlefunction(void);

void init_idledib_md(
   struct DIB *dib)
/* Initialize machine-dependent part of idledib */
{
   set_inst_pointer(dib, (unsigned long)idlefunction);
   dib->psr = 0x000000c0;    /* (supervisor state, interrupts disabled) */
   dib->map = kernCtx;       /* Runs in the kernel's address space */
   dib->Trapcode = 0;
}
