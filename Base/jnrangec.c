/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "lli.h"
#include "keyh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "geteh.h"
#include "getih.h"
#include "locksh.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "primcomh.h"
#include "queuesh.h"
#include "kschedh.h"
#include "domamdh.h"
#include "spaceh.h"
#include "disknodh.h"
#include "jnrangeh.h"
#include "kernkeyh.h"
#include "diskless.h"
#include "getmntfh.h"
#include "jnodeh.h"
#include "scafoldh.h"
#include "memutil.h"

LLI offset;
 
 
int checkcdainrange(
   unsigned char *cda,         /* The CDA to check */
   struct Key *rk)             /* The range key used */
/*
   Returns 0 if CDA is not in the range of the range key
     In this case, abandonj or simplest will have been called.
   Otherwise returns 1 and offset has the offset in the range.
*/
{
   LLI temp;
   b2lli(cda, sizeof (CDA), &offset); /* get the cda */
   b2lli(rk->nontypedata.rangekey.rangecda, 6, &temp);
/*** Memcmp didn't work for the following comparison, because
     it compared signed bytes!! */
   if (llicmp(&offset, &temp) < 0) {
      /* the cda is before the beginning of the range */
      simplest(-1);
      return 0;
   }
   llisub(&offset, &temp); /* calc offset in range */
   b2lli(rk->nontypedata.rangekey.rangesize,
         sizeof rk->nontypedata.rangekey.rangesize,
         &temp);
   if (offset.hi > 0xff /* because offset is limited to 4 bytes by specs */
       || llicmp(&offset, &temp) >= 0) {
      simplest(-1);
      return 0;
   }
   return 1;
}
 
 
unsigned char *validaterelativecda( /* Validate parameter for oc=0 */
   struct Key *rk)                      /* The range key */
/*
   Returns NULL if byte string CDA is not in the range of the range key
     In this case, simplest will have been called.
   Otherwise returns a pointer to the CDA.
*/
{
   LLI temp;
   offset.hi = 0;
   pad_move_arg((char *)&offset.low, 4);
   temp.hi = 0;
   Memcpy((char*)&temp.hi+3, rk->nontypedata.rangekey.rangesize, 5);
   if (llicmp(&offset, &temp) >= 0) {
      simplest(-1);
      return NULL;
   }
   Memcpy((char*)&temp.hi+2, rk->nontypedata.rangekey.rangecda,6);
   lliadd(&offset, &temp);
   return (unsigned char *)&offset.hi + 2;
}
  
unsigned char *validaterelativecda6( /* Validate parameter for oc=0, */
                                     /* 6 byte cda */
   struct Key *rk)                      /* The range key */
/*
   Returns NULL if byte string CDA is not in the range of the range key
     In this case, simplest will have been called.
   Otherwise returns a pointer to the CDA.
*/
{
   LLI temp;
   offset.hi = 0;
   pad_move_arg((char *)&offset.hi+2, 6);
   temp.hi = 0;
   Memcpy((char*)&temp.hi+3, rk->nontypedata.rangekey.rangesize, 5);
   if (llicmp(&offset, &temp) >= 0) {
      simplest(-1);
      return NULL;
   }
   Memcpy((char*)&temp.hi+2, rk->nontypedata.rangekey.rangecda,6);
   lliadd(&offset, &temp);
   return (unsigned char *)&offset.hi + 2;
}
 
 
static NODE *validatenodekey( /* Initial processing for oc 1-4 */
   struct Key *key)                  /* The input range key */
/*
   Returns NULL if key is not valid
     In this case, abandonj or simplest will have been called.
   Otherwise returns 1 and offset has the offset in the range.
*/
{
   register struct Key *pk1 = prep_passed_key1();
   register NODE *node;
 
   if (pk1 == NULL) {
      abandonj();
      return NULL;
   }
   if (pk1->type != nodekey + prepared) {
      simplest(-1);
      return NULL;
   }
   node = (NODE *)pk1->nontypedata.ik.item.pk.subject;
   if (checkcdainrange(node->cda, key) == 0) return NULL;
   return node;
}
 
 
static void sever_node(         /* Sever a node */
   register NODE *n)                 /* The node to sever */
{
   if (n->flags & EXTERNALQUEUE)
      crash("JNRANGE003 Node found with external queue");
 
   for (;;) {    /* Uninvolve all keys to this node */
      struct Key *k = (struct Key *)n->rightchain;
      if ((NODE *)k == n || !(k->type & involvedr+involvedw))
   break;
      if (k->type == pihk) rundom(keytonode(k));
      else {            /* Key is not a hook */
         if (puninv(k) == puninv_cant) crash("Can't uninvolve key");
      }
   }
   zapprocess(n);
 
   if (n->prepcode == unpreparednode)
                  n->domhookkey.type &= ~(involvedr+involvedw);
 
   for (;;) {    /* Zap prepared keys (all are uninvolved) */
      struct Key *k = (struct Key *)n->rightchain;
      if ((NODE *)k == n)
   break;
      n->rightchain = (union Item*)k->nontypedata.ik.item.pk.rightchain;
      *k = dk0;
   }
   n->leftchain = (union Item *)n;  /* End zap prepared keys */
 
   /* All prepared keys zapped, now zap unprepared non-resume keys */
 
   if (n->flags & NFALLOCATIONIDUSED) {
      n->allocationid += 1;
      if (!n->allocationid)
         crash("JNRANGE001 Node allocationid wrapped");
      n->flags &= ~NFALLOCATIONIDUSED;
   }
 
   /* All prepared keys are zapped. Now zap unprepared resumes */
 
   if (n->flags & NFCALLIDUSED) { /* If unprepared resumes */
      unsigned long id = n->callid;
      if ( (id++) < n->callid)
                crash("JNRANGE002 Call id overflow");
      n->callid = id;
      n->flags &= ~NFCALLIDUSED;
      n->flags |= NFDIRTY;
   }
 
   cpup1key.type = nodekey+prepared;
   cpup1key.databyte = 0;
   cpup1key.nontypedata.ik.item.pk.subject = (union Item *)n;
   cpuexitblock.keymask = 8;
}
 
 
static void make_gratis(        /* Make a node gratis */
   register NODE *n)                 /* The node */
{
   n->flags |= NFGRATIS;
   cpuexitblock.keymask = 0;
}
 
 
static void make_nongratis(     /* Make a node not gratis */
   register NODE *n)                 /* The node */
{
   n->flags &= ~NFGRATIS;
   cpuexitblock.keymask = 0;
}
 
 
static void alter_node_state( /* General node state change */
   struct Key *key,                       /* The range key */
   NODE *n,             /* the node */
   void (*rtn)(NODE *))                   /* The change state routine */
                     /* N.B. rtn must set up returned keys */
{
 
   if (!n) return;
   /* We once had this test here to see if we could use a fast path.
      There was a bug in which sever_node called zapprocess which
      examined cpujenode before it was set.
   if (cpuexitblock.jumptype != jump_call
       || n == cpudibp->rootnode
       || n == cpudibp->keysnode
       || node_overlaps_statestore(n) ) {  */    /* Do it the slow way */

      corelock_node(14, n);                     /* Core lock the node */
      switch (ensurereturnee(0)) {
       case ensurereturnee_wait:
         coreunlock_node(n);
         abandonj();
         return;
       case ensurereturnee_overlap:
         coreunlock_node(n);
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      handlejumper();
      n->flags |= NFDIRTY;
      (*rtn)(n);                       /* Change the node's state */
      coreunlock_node(n);
      cpuordercode = 0;
      cpuarglength = 0;
      if (! getreturnee()) return_message();
      return;
}
 
 
#if diskless_kernel
/* Here is a DiskNode that serves as a model for nodes we create
   in the diskless system. */
struct DiskNode newnode = {{0}}; /* initialize to zero */
#endif

void jnrange(          /* Node range key */
   struct Key *key)           /* The range key invoked */
{
   register NODE *n;
   unsigned char *cda;
   struct Key *fkey;
 
   switch (cpuordercode) {
    case 0:              /* Create node key */
    case 5:              /* Create node key - no wait */
    case 9:              /* Create node key and clear, no wait */
      if (9 == cpuordercode) cda = validaterelativecda6(key);
      else cda = validaterelativecda(key);

      if (cda == NULL) return;
      n = srchnode(cda);

      if (n == NULL) {
#if diskless_kernel
         {LLI i; b2lli(cda, 6, &i);
          if(i.hi == 0 && i.low<4096)
             crash("Probably an undefined primordial node key.");}
         Memcpy(newnode.cda, cda, 6);
         n = getmntf(&newnode);
         if(n==NULL) crash("Really out of nodes!");
 
#else /* not diskless_kernel */

         switch (getnode(cda)) {
          case get_ioerror:
            simplest(3);
            return;
          case get_wait:
            if (cpuordercode != 0 &&
                cpudibp->rootnode->domhookkey.nontypedata.ik.item.
                    pk.subject == (union Item *)&rangeunavailablequeue){
logstr("node on nonmounted range");
               if (!(cpudibp->rootnode->preplock & 0x80))
                  crash("JNRANGE478 Actor not preplocked after get");
               zaphook(cpuactor);
               simplest(2);
               return;
            }
            abandonj();
            return;
          case get_tryagain: n = srchnode(cda);
         }
#endif
      }
      cpup1key.type = nodekey+prepared;
      cpup1key.databyte = 0;
      cpup1key.nontypedata.ik.item.pk.subject = (union Item *)n;
      cpuarglength = 0;

      if (9 == cpuordercode) {    /* Create key and clear */
         if (nodeslowtest(n)) {  /* Clear the slow way */
            corelock_node(12, n);   /* Core lock the node */
            switch (ensurereturnee(0)) {
             case ensurereturnee_wait:
               coreunlock_node(n);
               abandonj();
               return;
             case ensurereturnee_overlap:
               coreunlock_node(n);
               midfault();
               return;
             case ensurereturnee_setup: break;
            }
            handlejumper();
            clearnode(n);
            coreunlock_node(n);
            cpuordercode = 0;
            cpuexitblock.keymask = 8;
            if (! getreturnee()) return_message();
            return;
 
         } /* End clear the slow way */
         else {                 /* Clear the fast way */
            clearnode(n);
         } /* End clear the fast way */
      }
      cpuordercode = 0;
      jsimple(8);   /* first key */
      return;
 
    case 1:                       /* Get CDA */
      if (!(n = validatenodekey(key))) return;
      if (n->flags & NFGRATIS) cpuordercode = 1;
      else cpuordercode = 0;
      cpuexitblock.argtype = arg_regs;
      cpuargaddr = (char *)&offset.low;
      cpuarglength = 4;
      jsimple(0);  /* no keys */
      return;
 
    case 2:                       /* Sever node */
       n = validatenodekey(key);
      alter_node_state(key, n, sever_node);
      return;
 
    case 3:                       /* Make gratis */
      n = validatenodekey(key);
      alter_node_state(key, n, make_gratis);
      return;
 
    case 4:                       /* Make non-gratis */
      n = validatenodekey(key);
      alter_node_state(key, n, make_nongratis);
      return;

    case 8:    /* sever, fork, and clear node based on cda  */

      cda = validaterelativecda6(key);
      if (cda == NULL) return;
      n = srchnode(cda);

      if (n == NULL) {
#if diskless_kernel
	simplest(0);
        return;
 
#else /* not diskless_kernel */

         switch (getnode(cda)) {
          case get_ioerror:
            simplest(3);
            return;
          case get_wait:
            if (cpuordercode != 0 &&
                cpudibp->rootnode->domhookkey.nontypedata.ik.item.
                    pk.subject == (union Item *)&rangeunavailablequeue){
               if (!(cpudibp->rootnode->preplock & 0x80))
                  crash("JNRANGE478a Actor not preplocked after get");
               zaphook(cpuactor);
               simplest(2);
               return;
            }
            abandonj();
            return;
          case get_tryagain: n = srchnode(cda);
         }
#endif
      }

/* now have node , must fork all the resume keys we find */

      for(fkey = n->keys; fkey <= n->keys+15; fkey++) {
        if ((fkey->type & keytypemask) == resumekey) {  /* could be */
           fkey = prx(fkey);
           if (!fkey) {
               abandonj();
               return;
           }
           if (fkey->type == resumekey + prepared) {  /* still is */
             /* must set up for fork KT+1 no keys no arg */
             cpuexitblock.jumptype=jump_fork;
             cpuordercode=KT+1;
             cpuexitblock.keymask=0;
             cpuexitblock.argtype=arg_none;
             jresume(fkey);
             abandonj();   /* loop till none found */
             return;
           }
        }
      }

/* this part clears the node */

      if (nodeslowtest(n)) {  /* Clear the slow way */
         corelock_node(12, n);   /* Core lock the node */
         switch (ensurereturnee(0)) {
          case ensurereturnee_wait:
            coreunlock_node(n);
            abandonj();
            return;
          case ensurereturnee_overlap:
            coreunlock_node(n);
            midfault();
            return;
          case ensurereturnee_setup: break;
         }
         handlejumper();
         clearnode(n);
         coreunlock_node(n);
 
      } /* End clear the slow way */
      else {                 /* Clear the fast way */
         clearnode(n);
      } /* End clear the fast way */

/* this part severs the node */

      alter_node_state(key, n, sever_node);
      return;
 
    default:
      if (cpuordercode == KT) simplest(771);
      else simplest(KT+2);
   }
}
