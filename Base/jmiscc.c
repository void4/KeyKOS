/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "string.h"
#include <ctype.h>
#include "sysdefs.h"
#include "lli.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "locksh.h"
#include "primcomh.h"
#include "domamdh.h"
#include "timeh.h"
#include "timemdh.h"
#include "diskkeyh.h"
#include "key2dskh.h"
#include "kernkeyh.h"
#include "ioworkh.h"
#include "jresynch.h"
#include "memomdh.h"      /* For find_program_name */
#include "meterh.h"
#include "sparc_mem.h"
#include "memutil.h"

#if 0
#include "memo88kh.h"     /* for the dat2inst routine */
#endif

extern unsigned long idlrenc;
extern struct DIB idledib;

static void identify_keeper(
   unsigned char keytype,    /* Valid type for "key" */
   unsigned char keepertype) /* Valid type for keeper key */
{
   struct Key *dk, *brand, *brander, *kk;
   NODE *dknode, *kknode;
   int slot;
 
   if (!(kk = prep_passed_key1())) {
      abandonj();
      return;
   }
   if (kk->type != keytype + prepared ||
       kk->databyte & (nocall|0x0f)) {
      simplest(-1);            /* Just return rc = -1 */
      return;
   }
   kknode = (NODE*)kk->nontypedata.ik.item.pk.subject;
   if (keytype == meterkey) {
      slot = 2;
   } else {                        /* Must be a segment key */
      dk = readkey(kknode->keys+15);  /* Get format key */
      if (!dk || (dk->type & ~involvedw) != datakey) {
         simplest(-1);            /* Just return rc = -1 */
         return;
      }
      slot = (dk->nontypedata.dk7.databody[5] >> 4) & 0x0f;
   }
   dk = readkey(kknode->keys + slot);  /* Get keeper key */
   if (!dk) {
      simplest(-1);            /* Just return rc = -1 */
      return;
   }
   corelock_node(7, kknode);      /* So key is locked */
   dk = prx(dk);
   if (!dk) {
      coreunlock_node(kknode);
      abandonj();
      return;
   }
   if ((dk->type & ~involvedw) != keepertype + prepared ||
       (keepertype == resumekey && dk->databyte == restartresume) ) {
      coreunlock_node(kknode);
      simplest(-1);            /* Just return rc = -1 */
      return;
   }
   dknode = (NODE*)dk->nontypedata.ik.item.pk.subject;
   if (!(brand = readkey(dknode->keys+0))) {
      coreunlock_node(kknode);
      simplest(-1);            /* Just return rc = -1 */
      return;
   }
   corelock_node(8, dknode);
   if (!prx(brand)) {
      coreunlock_node(dknode);
      coreunlock_node(kknode);
      abandonj();
      return;
   }
   if (!(brander = prep_passed_key2())) {
      coreunlock_node(dknode);
      coreunlock_node(kknode);
      abandonj();
      return;
   }
   coreunlock_node(dknode);
   coreunlock_node(kknode);
   if (compare_keys(brander, brand)) {
      simplest(-1);
      return;
   }
   cpup1key.type = nodekey + prepared;
   cpup1key.databyte = kk->databyte;
   cpup1key.nontypedata.ik.item.pk.subject =
             kk->nontypedata.ik.item.pk.subject;
   cpuarglength = 0;
   cpuordercode = slot*256 + dk->databyte;
   jsimple(8);  /* first key */
   return;
}
 
 
static void identify_key(
   unsigned char keytype)    /* Valid type for "key" */
{
   struct Key *dk, *brand, *brander;
   NODE *dknode;
 
   if (!(dk = prep_passed_key1())) {
      abandonj();
      return;
   }
   if (dk->type != keytype + prepared) {
      simplest(-1);
      return;
   }
   dknode = (NODE*)dk->nontypedata.ik.item.pk.subject;
   if ((keytype == resumekey && dk->databyte == restartresume) ||
       !(brand = readkey(dknode->keys+0))) {
      simplest(-1);            /* Just return rc = -1 */
      return;
   }
   corelock_node(9, dknode);
   if (!prx(brand)) {
      coreunlock_node(dknode);
      abandonj();
      return;
   }
   if (!(brander = prep_passed_key2())) {
      coreunlock_node(dknode);
      abandonj();
      return;
   }
   coreunlock_node(dknode);
   if (compare_keys(brander, brand)) {
      simplest(-1);
      return;
   }
   cpup1key.type = nodekey + prepared;
   cpup1key.databyte = 0;
   cpup1key.nontypedata.ik.item.pk.subject =
             dk->nontypedata.ik.item.pk.subject;
   cpuarglength = 0;
   cpuordercode = dk->databyte;
   jsimple(8);  /* first key */
   return;
}
 
 
static void prepdiscrim(int i)
/* do discrim function on a key that must be prepared. */
{
   struct Key *k1;
   if (!(k1 = prep_passed_key1())) {
      abandonj();
      return;
   }
   if (k1->type == datakey) simplest(1);
   else simplest(i);
   return;
}
 
/* Stuff for peek key order code 2. */
static unsigned int numctrs; /* number of counters so far */
static unsigned int numtmrs; /* number of timers so far */
static char *passctr(
   char *op,
   unsigned long c,
   const char *name)
{
   *(unsigned long *)op = c;  /* pass counter value */
   Strncpy(op+4, name, 28);
   numctrs++;
   return op+32;
}
static char *passtmr(
   char *op,
   LLI *t,
   const char *name)
{
   lli2b(t, op, 8);  /* pass timer value */
   Strncpy(op+8, name, 28);
   numtmrs++;
   return op+40;
}

extern int * xcvZ, * xCvZ, * XCvZ, * xCVZ;
int * bllist[] = {(int*)&xcvZ, (int*)&xCvZ, (int*)&XCvZ, (int*)&xCVZ};

void jmisc(struct Key *key)    /* Handle jumps to misckeys */
{
   switch (key->nontypedata.dk11.databody11[0]) {
    case  returnermisckey:
      switch (cpuexitblock.jumptype) {
       case jump_implicit:
         crash("PRIMCOM011 Implicit jump to returner");

       case jump_call:
         cpup1key = *ld1();
         cpup2key = *ld2();
         cpup3key = *ld3();  /* May return pointer to cpup3key */
         jsimple(0xe);   /* return 3 keys */
         return;

       case jump_fork:
       case jump_return:
         {  struct Key *rk = ld4();

            /* See if it is a resume key before going to the expense
               (namely I/O) of preparing it. */
            if ((rk->type & keytypemask) == resumekey) { /* Could be */
               rk = prx(rk);
               if (!rk) {
                  abandonj();
                  return;
               }
               if (rk->type == resumekey + prepared) {  /* It is */
                  jresume(rk);
                  return;
               }
            }
         }
         /* Not resume key - make process disappear */
         handlejumper();
         return;
      } /* end of switch on jumptype */

    case  domtoolmisckey:
      switch (cpuordercode) {
       struct Key *dk;
 
       case 0:              /* Make domain key */
         if (!(dk = prep_passed_key1())) {
            abandonj();
            return;
         }
         if (dk->type != nodekey + prepared) {
            simplest(-1);            /* Just return rc = -1 */
            return;
         }
         cpup1key.type = domainkey + prepared;
         cpup1key.databyte = 0;
         cpup1key.nontypedata.ik.item.pk.subject =
                   dk->nontypedata.ik.item.pk.subject;
         cpuarglength = 0;
         cpuordercode = 0;
         jsimple(8);  /* first key */
         return;
 
       case 1:              /* Identify Start */
         identify_key(startkey);
         return;
 
       case 2:              /* Identify Resume */
         identify_key(resumekey);
         return;
 
       case 3:              /* Identify Domain */
         identify_key(domainkey);
         return;
 
       case 5:              /* Identify Segment w/Start key keeper */
         if (!(dk = prep_passed_key1())) {
            abandonj();
            return;
         }
         if((dk->type & keytypemask) == segmentkey) identify_keeper(segmentkey, startkey);
         else  identify_keeper(frontendkey, startkey);
         return;
 
       case 6:              /* Identify Segment w/Resume key keeper */
         if (!(dk = prep_passed_key1())) {
            abandonj();
            return;
         }
         if((dk->type & keytypemask) == segmentkey) identify_keeper(segmentkey, resumekey);
         else  identify_keeper(frontendkey, resumekey);
         return;
 
       case 7:              /* Identify Segment w/Domain key keeper */
         identify_keeper(segmentkey, domainkey);
         if(cpuordercode == -1) {
            identify_keeper(frontendkey, domainkey);
         }
         return;
 
       case 9:              /* Identify Meter w/Start key keeper */
         identify_keeper(meterkey, startkey);
         return;
 
       case 10:             /* Identify Meter w/Resume key keeper */
         identify_keeper(meterkey, resumekey);
         return;
 
       case 11:             /* Identify Meter w/Domain key keeper */
         identify_keeper(meterkey, domainkey);
         return;
 
       default:
         if (cpuordercode == KT) simplest(0x109);
         else simplest(KT+2);
         return;
      }
 
    case  keybitsmisckey:
      if (cpuordercode == 0) {
         struct Key *pk1 = ld1();
 
         memzero(cpuargpage, 4);
         key2dsk(pk1, (DISKKEY*)(cpuargpage+4));
         cpuarglength = sizeof (DISKKEY) + 4;
         cpuargaddr = cpuargpage;
         cpuexitblock.argtype = arg_regs;
         cpuordercode = 0;
         jsimple(0); /* no keys */
      }
      else simplest(KT+2);
      return;
 
    case  datamisckey:
      switch (cpuordercode) {
       case 0:
         memzero(cpup1key.nontypedata.dk11.databody11, 5);
         pad_move_arg(cpup1key.nontypedata.dk11.databody11+5, 6);
         cpup1key.type = datakey;
         cpuarglength = 0;
         cpuordercode = 0;
         jsimple(8);  /* 1 key */
         return;
 
       case 1:
         pad_move_arg(cpuargpage,5);
         pad_move_arg(cpup1key.nontypedata.dk11.databody11, 11);
         cpup1key.type = datakey;
         cpuarglength = 0;
         if (Memcmp(cpuargpage,"\0\0\0\0\0", 5))
            cpuordercode = 0;
         else cpuordercode = 1;
         jsimple(8);  /* 1 key */
         return;
 
       default:
         if (cpuordercode == KT) simplest(0x309);
         else simplest(KT+2);
         return;
      }
 
    case  discrimmisckey:
      switch (cpuordercode) {
         struct Key *k1, *k2;
 
       case 0:                           /* Discriminate */
         k1 = ld1();
         switch ((k1->type) & keytypemask) {
          case hookkey:
          case datakey:
            simplest(1); return;
          case resumekey:
            prepdiscrim(2);
            return;
          case pagekey:
            if (!(k1->type & prepared)) {
               /* not prepared. Validate it. */
               switch (validatepagekey(k1).code) {
                case vpk_wait:     return;
                case vpk_ioerror:  crash("i/o error");
                case vpk_obsolete: simplest(1); return;
                case vpk_current:  break;
               }
            }
            simplest(3); return;
          case segmentkey:
          case nodekey:
          case sensekey:
            prepdiscrim(3); return;
          case meterkey:
            prepdiscrim(4); return;
          case fetchkey:
          case startkey:
          case domainkey:
          case frontendkey:
            prepdiscrim(5); return;
          default:
            simplest(5); return;
         }
 
       case 1:                           /* Check if sensory */
         if (!(k1 = prep_passed_key1())) {
            abandonj();
            return;
         }
         if (k1->type == prepared + segmentkey &&
             (k1->databyte & (readonly+nocall)) == readonly+nocall)
            simplest(0);
         else if (k1->type == prepared + pagekey &&
                  k1->databyte & readonly)
            simplest(0);
         else if (k1->type == prepared + sensekey)
            simplest(0);
         else simplest(1);
         return;
 
       case 2:                           /* Compare keys */
         if (!(k1 = prep_passed_key1())) {
            abandonj();
            return;
         }
         if (!(k2 = prep_passed_key2())) {
            abandonj();
            return;
         }
/*
  The first key may have been unprepared, but we know for sure
    whether it is DK(0).
*/
         simplest(compare_keys(k1,k2));
         return;
 
       default:
         if (cpuordercode == KT) simplest(0x409);
         else simplest(KT+2);
         return;
      }

    case  bwaitmisckey:
      jbwait(key);
      return;

    case  takeckptmisckey:
      jckfckpt();
      return;
 
    case  resynctoolmisckey:
      jresync();
      return;
 
    case  errormisckey:
      {  struct Key *domkey = prep_passed_key3();
                                  /* Third keeper parameter (domkey) */
         NODE *rn;                  /* Root node of the domain */
 
         if (domkey == NULL) return;

         {          /* Get name of caller and print console message */
            char buf[80];
            const char *name;
            char *op;

            /* The following is a kludge to try to find the name of this
               program. But, if it can't be found, don't call any keepers. */
            name = find_program_name();
            if (name==NULL) {
               abandonj();
               return;
            }
            Strncpy(buf, "JMISC001 Error key invoked by ", 80);
            op = buf + Strlen(buf);
            while(op < buf + sizeof(buf) - 1) *(op++) = *(name++);
            *op = 0;
            crash(buf);
         }

         if (domkey->type != domainkey+prepared) { /* not dom key */
            simplest(0);            /* Just return rc=0 */
            return;
         }
         rn = (NODE *)domkey->nontypedata.ik.item.pk.subject;
         if (! dry_run_prepare_domain(rn,0)) return;
         handlejumper();              /* End of dry run */
 
         clear_trapcode(rn->pf.dib);
 
         cpuordercode = 0;
         coreunlock_node(rn);
         cpuexitblock.keymask = 0;
         cpuarglength = 0;
         if (! getreturnee()) return_message();
         return;
      }
 
    case  peekmisckey:
      if (cpuordercode > 2
          && cpuordercode < sizeof(bllist)/sizeof(int*)+3) {
          int * x = bllist[cpuordercode-3];
          char * z = cpuargpage;
          while(*x){Memcpy(z, (char*)*(x+1), *x); z += *x; x += 2;}
          cpuarglength = z - cpuargpage;
          cpuargaddr = cpuargpage;
          cpuexitblock.argtype = arg_regs;
          cpuordercode = cpuarglength;
          jsimple(0);
          return;
      }
      switch (cpuordercode) {
       case 1:  /* Read counters */
        // {  LLI now = read_system_timer();
        //    lli2b(&now, cpuargpage, 8);}
        *(uint64*)cpuargpage = read_system_timer();
         {  unsigned long *lenp = (unsigned long *)(cpuargpage+8);
            unsigned long *op;
            unsigned int i;
#undef defctr
#undef defctra
#undef lastcounter
#undef deftmr
#define defctr(c) *++op = c;
#define defctra(c,n) for(i=0;i<n;i++) *++op=c[i];
#define lastcounter() *lenp = (op-lenp)*4; lenp=++op;
#define deftmr(t) *++op = t.hi; *++op = t.low;
            op = lenp;
#include "counterh.h"
            *lenp = (op-lenp)*4; /* end of timers */

            cpuarglength = (char *)++op - cpuargpage;
            cpuordercode = 0;
            cpuargaddr = cpuargpage;
            cpuexitblock.argtype = arg_regs;
            jsimple(0);  /* no keys */
            return;
         }
       case 2:  /* Read counters and names */
         //{  LLI now = read_system_timer();
         //   lli2b(&now, cpuargpage, 8);}
         *(uint64*)cpuargpage = read_system_timer();
         {
            char *op;
            unsigned int i;
static const char namei[20][5] = {
   "[0]","[1]","[2]","[3]","[4]","[5]","[6]","[7]","[8]","[9]",
   "[10]","[11]","[12]","[13]","[14]","[15]","[16]","[17]","[18]","[19]"};
#undef defctr
#undef defctra
#undef lastcounter
#undef deftmr
#define defctr(c)    op = passctr(op,c,#c);
#define defctra(c,n) op = passctr(op,c[0],#c"[0]"); \
    for(i=1;i<n;i++) op = passctr(op,c[i],namei[i]);
#define lastcounter()
#define deftmr(t)    op = passtmr(op,&t,#t);
            op = cpuargpage+16;
            numctrs = numtmrs = 0;
#include "counterh.h"
            *(unsigned long *)(cpuargpage+8) = numctrs;
            *(unsigned long *)(cpuargpage+12) = numtmrs;

            cpuarglength = op - cpuargpage;
            cpuordercode = 0;
            cpuargaddr = cpuargpage;
            cpuexitblock.argtype = arg_regs;
            jsimple(0);   /* no keys */
            return;
         }
       case KT:
         simplest(0x909);
         return;
       default: simplest(KT+2);
         return;
      }
    case  Fpeekmisckey: 
    case  Fpokemisckey:
      if(cpuordercode<3){int rv = 0;
        if (cpuexitblock.keymask & 8){
          struct Key * exee = &cpudibp->keysnode->keys[cpuexitblock.key1];
          rv = exee -> type;
          if ((exee -> type & keytypemask) == nodekey) {
            if (rv & prepared) {
              NODE * np = (NODE *)exee->nontypedata.ik.item.pk.subject;
              rv |= np -> prepcode << 8;
              if (np -> prepcode == prepasmeter) {
                rv |= np -> pf.drys <<16;
                if(key->nontypedata.dk11.databody11[0] == Fpokemisckey)
                     scavenge_meter(np, cpuordercode);
              }
            }
          }
        }
        simplest(rv);
        return;
      }
      simplest(KT+2);
      return; 
 
    case  chargesettoolmisckey:
    case  journalizekeymisckey:
    case  deviceallocationmisckey:
    case  geterrorlogmisckey:
    case  iplmisckey:
    case  measuremisckey:
    case  cdapeekmisckey:
      break;
#if !defined(diskless_kernel)
    case  migrate2misckey:
      jmigrate();
      return;
#endif
    case  kiwaitmisckey:
    case  kdiagmisckey:
    case  kerrorlogmisckey:
      break;
    case  systimermisckey:
      switch (cpuordercode) {
       case 7: {  /* return system time */
         uint64 timer;
 
         timer = read_system_timer();  /* read the timer */
         cpuexitblock.argtype = arg_regs;
         cpuargaddr = (char *)&timer;
         cpuarglength = 8;
         cpuordercode = 0;
         jsimple(0);  /* no keys */
         return;
         }
       case 8: {  /* return idle time */
         uint64 timer;
            
         timer = idlrenc * 0x7fffffff + (0x7fffffff - idledib.cpucache);
         cpuexitblock.argtype = arg_regs;
         cpuargaddr = (char *)&timer;
         cpuarglength = 8;
         cpuordercode = 0;
         jsimple(0);  /* no keys */
         return;
         }
       case KT:
         simplest(0x509);
         return;
       default: simplest(KT+2);
         return;
      }
    case  calclockmisckey:
      jcalclock();
      return;
    case dat2instmisckey:
      dat2inst(); return;
    case copymisckey:
      jcopy(0); return;
    default: ;
   }
 
   simplest(KT+2);
} /* end jmisc */

