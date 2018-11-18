/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* chec88kc.c - machine-dependent memory map check for
        Motorola 88000 */
/* The header for this file is checmdh.h */

#include "kktypes.h"
#include "sysdefs.h"
#include "keyh.h"
#include "kermap.h"
#include "memomdh.h"
#include "sparc_mem.h"
#include "sparc_map.h"
#include "wsh.h"
#include "checkh.h"
#include "addrcteh.h"
#include "checmdh.h"
#include "splh.h"
#include "psr.h"
#include "sparc_check.h"
#include "memutil.h"

/* Fix up check so that it doesn't window over user page at bottom
   of stem for peace of mind. ..... */

#if LATER
CTE phoney; /* To fool check_caches */
static CTE * addr2ctx(unsigned long p)
{
  if(p<first_user_page)
    if(p<0x10000) return &phoney;
    else if(p<0x7a000) crash("Bad cache tag");
         else return &phoney;
  else if(p<0x41000000u) 
    if(p<endmemory) { /* a page frame */
       CTE *cte = addr2cte(p);
       return cte;
    }
    else crash("Bad cache tab");
  else return &phoney;
return &phoney; // To quiet the compiler who fears "crash" might return.
}
#endif

void check_caches(void)
#if LATER
/* Check veracity of cachedr and cachedw */
/* Not to be called from interrupt level, because it writes
   to CMMUs[i].SAR  */
{  int i, j, k, cssp, s;
   phoney.cachedr = 0xc0;
   phoney.cachedw = 0x40;
   for(i=6; i<8; i++) /*i ranges over the set of CMMUs. */
      for(j=0; j<4096; j+=16) { /* j ranges over cache sets. */
         s = splhi();  /* Minimize disturbance to the cache. */
         CMMUs[i].SAR = (void *)j;
         cssp = CMMUs[i].CSSP;
         for(k = 0; k<4; k++) /* k ranges over the address tags */
            switch (cssp>>(12+2*k)&3) {
               unsigned long pageaddr;
               CTE *cte;
             case 0: case 2: /* valid unmodified line */
               pageaddr = CMMUs[i].CTP[k];
               cte = addr2ctx(pageaddr);
               if(!(cte->cachedr>>i&1))
                  crash("Unregistered Cache Line");
               break;
             case 1: /* Valid modified line */
               pageaddr = CMMUs[i].CTP[k];
               cte = addr2ctx(pageaddr);
               if(!(cte->cachedw>>i&1)) {
                  char str[80];
                  sprintf(str,"CHEC88K863 Unregistered modified"
                    "cache line at %x, cte=%x",pageaddr+j, cte);
                  crash(str);
               }
               break;
             default: ;
            }
         splx(s);
      }
}
#else
{return;}
#endif

void check_seg_map(
   long unsigned m,  /* a UAPR value */
   char cid)
{
   return /* until we find fast safe way to run. */;
}

static void check_domains_memories(struct DIB *dib, MI *mementry,
                            struct Key *key, char wrtok)
{return; /* ... */}

void check_prepasdomain_md(
   struct DIB *dib)
/* Machine-dependent check for prepared domain */
{
   NODE *np = dib->rootnode;
   extern struct DIB *cpufpowner;

/*
   Check the keys which are never involved in a domain root
*/
   if((involvedr + involvedw)
      & (np->keys[0].type | np->domkeeper.type | np->keys[10].type | np->keys[11].type))
          crash("CHEC88KC011 Key in domain root involved in error");
 
/*
   Check the keys which are always involved in a domain root
*/
   {void cik(struct Key * k, char c){if((k->type & (involvedr + involvedw)) != c)
            crash("CHEC88KC012 Some domain root key not involved");}
      char const H = (involvedr + involvedw);
      cik(&np->domhookkey, H);
      cik(&np->domtrapcode, H);
      cik(&np->keys[12], H);
      cik(&np->keys[13], H); // This collides with the hook key. Bug. Too big to fix just now!!
      cik(&np->domfpstatekey, involvedw);}
    
   if ((np->domprio.type & (involvedr + involvedw))
                   != involvedr+involvedw
        || (np->domkeyskey.type & involvedw) != involvedw
        || (np->domstatekey.type & involvedw) != involvedw)
      crash("CHEC88KC012 Some domain root key not involved");
  
/*
   Check the must be zero and one bits in the Sparc psr
*/
   if (dib->psr & 0x000fef60   /* 0==reserved, EC, PIL, PS and ET */
               || (dib->psr | 0xffffff7f) != 0xffffffff) /* 1==S */
      crash("CHEC88KC013 Incorrect bit in status register");
   if(dib->psr & PSR_EF && cpufpowner != dib) crash("disowned floating state");

/* Check the statenode chain */
   {
      int i;                      /* Statenode counter */
      int ssamount = 2+11+11+64;  /* State store in 1st node */
      NODE *sn = (NODE*)np->domstatekey.nontypedata.ik.item.pk.subject;

      if (dib->statestore != sn)
         crash("Sparc_Check014 dib->statestore doesn't point to statenode");
      for (i=0; i<15; i++) {      /* Calculate amount of statestore */
         if (sn->keys[0].type != nodekey+prepared+involvedw) break;
         ssamount += 11*15;
         sn = (NODE*)sn->keys[0].nontypedata.ik.item.pk.subject;
      }
      if (i>14)                   /* Too much statestore */
         crash("Sparc_Check015 Too many statenodes in the chain");
      if (ssamount/64 != dib->backmax)
         crash("Sparc_Check016 dib->backmax doesn't match state storage");
   }

/* Check the memory tree trunks of the domain. */

   if (dib->map != NULL_MAP) /* Domain has data memory */
      check_domains_memories(dib, &dib->map, &np->dommemroot, 0); 

   check_mem_props_md(np);

}  /* End check_prepasdomain_md */


void check_prepasstate_md(
   NODE *np)
/* Machine-dependent check for prepared statenode */
{
   struct Key *k;     /* All keys but key 0 must be involved data keys */
   for (k = &np->keys[1]; k <= &np->keys[15]; k++) {
      if (k->type != datakey + involvedr + involvedw)
         crash("CHECK095 Non-involved data key in state node");
   }
   if (np->keys[0].type != nodekey+involvedw+prepared
          && ( ((np->keys[0].type & keytypemask) != datakey) 
                || ( (np->keys[0].type & involvedw) == 0)
/* Floating point state node do use key0 for data
                || Memcmp(np->keys[0].nontypedata.dk11.databody11,
                          "\0\0\0\0\0\0\0\0\0\0\0", 11) */ ) )
      crash("Sparc_Check017 Invalid key in slot zero of a statenode");
}


void count_ctes(CTE * head,
   enum FrameType t,
   int const how_many,
   const char * who)
{
   int c=0; while(head){++c; if(head->ctefmt != t) crash("wrong cte format!");
     head = head->hashnext;}
   if(c != how_many) crash(who);
}
void check_memory_map(void)
{return; /* ... */}

void scan_mapping_tables_for_page(CTE *cte)
{return; /* ... */}
