/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "primmeth.h"
#include "keyh.h"
#include "wsh.h"
#include "timemdh.h"
#include "prepkeyh.h"
#include "domainh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "unprndh.h"
#include "locksh.h"
#include "queuesh.h"
#include "kertaskh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "kschedh.h"
#include "kschmdh.h"
#include "domamdh.h"
#include "meterh.h"
#include "ioworkh.h"
#include "splh.h"
#include "cyclecounter.h"
#include "realkermap.h"     /* For lowcoreflags.counters */

#define SCHEDULER 2

#define min(x,y) ((x)<(y) ? (x) : (y))
 
#define log_kprio_units 4
   /* This defines the units of values in kprio.
      One kprio unit is (1 << log_kprio_units) process timer units.
      (The process timer unit is 1/16 microsecond.)
      If (log_kprio_units == 4), kprio units are one microsecond
         and an unsigned long (32 bits or more) can hold a value
         of over one hour.
    */
 
#define log_time_constant 30
   /* (1 << log_time_constant) is the number of process timer units
         between halving of the priorities.
      (The process timer unit is 1/16 microsecond.)
      If (log_time_constant == 30) then the priorities are halved
         every 67 seconds.
      The halving is (conceptually) done when the system timer is
         a multiple of the time constant.
      log_time_constant must be less than (log_kprio_units + 31)
         to prevent overflow of kprio values.
      log_time_constant must also be at least 24 for the
         scaling in startdom to work.
    */
 
#define kpriotime(np) (*(unsigned long *)          \
   &(np->domprio.nontypedata.dk11.databody11[3]))
#define kprio(np)     (*(unsigned long *)          \
   &(np->domprio.nontypedata.dk11.databody11[7]))


void nowaitstateprocess(void)
{
}


static void handle_process_timer_wakeup(
   struct KernelTask *kt);
struct KernelTask processtimerkt
   = {NULL, &handle_process_timer_wakeup};
bool processtimerktactive = FALSE;
/* Except while clock interrupts are disabled,
   if (processtimerktactive)
      then (processtimerkt is on the kernel task queue
            or this module is processing a wakeup)
      else processtimerkt is not on the kernel task queue. */
 
struct DIB idledib;
static uint64 kclock;
unsigned long slicecache;  /* in units of 1/16 microsecond */
unsigned long cpuslicestart;  /* equals slicecache when
                   we started the domain in cpudibp */
 
static NODE *hooktonode(
   union Item *ip)
/* Returns ptr to the node containing the hook at *ip */
{
   register NODE *np;
      /* Hook key is always in same slot, subtract gives node header */
             /* Initial value of np below doesn't matter. */
   return((NODE *) ((char *)ip-((char *)&np->domhookkey-(char *)np)));
}
 
static unsigned long getprio(
   register NODE *np)
/* Get priority of a node */
{
   if (np->prepcode == prepasdomain) return (kprio(np));
   if (np->domprio.type != datakey)
      return(0);   /* malformed or strangely involved */
   else return(kprio(np));
}
 
static unsigned long updprio(
   register NODE *np)  /* ptr to domain root */
/* update priority of non-running domain. Returns priority. */
/* kclock is the current time to use. */
{
   register long elapsed;
 
   if (np->prepcode != prepasdomain
       && np->domprio.type != datakey)
      return(0);
   /* If it is prepared as a domain, domprio is ok (despite possibly
      being involved).
      domprio is also ok if it is an uninvolved data key. */
   elapsed = (uint32)(kclock >> (log_time_constant+8))
             - (kpriotime(np) >> (log_time_constant-24));
      /* number of halvings we missed */
      /* The 24 above is to scale from system_timer.hi units
         to process timer units. */
      /* The shifts are done separately on kclock.hi and kpriotime
         so the halvings are (logically) done on exact multiples
         of the time_constant. */
   if (elapsed == 0) return(kprio(np)); /* no time has elapsed, so
                                the priority is unchanged */
   kpriotime(np) = kclock >> 32; /* remember the corresponding time */
   if (elapsed < 0) /* his old kpriotime was in the future! */
      return(kprio(np));
   return(kprio(np) >>= elapsed);   /* decay the priority */
}
 
void loadpt()
/* Set process timer for this domain (cpudibp)
                     based on this time slice (slicecache) */
{
#if SCHEDULER != 2
   register unsigned long t;
   t = min(slicecache,cpudibp->cpucache);
   slicecache -= t;   /* remaining slice */
   cpudibp->cpucache -= t;  /* remaining cache */
   set_process_timer(t);
#endif
}
 
#if SCHEDULER != 2
static void sidle(
/* start to run a domain or idle job */
   register unsigned long slice,
   register struct DIB *dibp)
{
   cpudibp = dibp;
   cpuslicestart = slicecache = slice;
   loadpt();
   start_process_timer();
}
#endif
 
void startdom(dibp)
register struct DIB *dibp;
{
#define log_maxslice 20
   /* (1L << log_maxslice) is maximum time slice, in kprio units.
      log_maxslice + log_kprio_units must be less than 32,
      to avoid overflow.
    */
#define minslice 4096
   /* Roughly, the minimum time slice we give to any deserving domain.
      In kprio units.
    */
   register unsigned long thisprio;
#if SCHEDULER != 2
   register unsigned long slice;        /* in kprio units */
#endif
 
   kclock = read_system_timer();
   thisprio = updprio(dibp->rootnode);
#if SCHEDULER != 2
 
   /* Calculate a time slice for him. */
   if (iosystemflags & DISPATCHINGDOMAINSINHIBITED) slice = 0;
   else {
      register union Item *ip;
 
      /* Find domain at head of CPU queue */
      ip = cpuqueue.head;
      if (ip == (union Item *)&cpuqueue) /* the CPU queue is empty */
         slice = 1L << log_maxslice;
      else {
         register unsigned long otherprio;
         /* Get that domain's priority */
         otherprio = getprio(hooktonode(ip)) + minslice;
         otherprio += otherprio >> 1; /* increase by half
                                         to avoid thrashing */
         if (otherprio < thisprio) slice = 0;
         else {
            slice = otherprio - thisprio;
            slice = min(slice, 1L << log_maxslice);
         }
      }
   }
   sidle(slice << log_kprio_units, dibp);
#else
   {
      register union Item *ip = cpuqueue.head;
      if (ip != (union Item *)&cpuqueue  /* the CPU queue is empty */
           && getprio(hooktonode(ip)) < thisprio) {
         dibp->readiness |= LOWPRIORITY;
      }
   }
   cpudibp = dibp;
   start_process_timer();
#endif
   set_memory_management(); /* Set up hardware */
   md_startdom();           /* machine-dependent setup */
}
 
void uncachecpuallocation()
/* The remaining cpu allocation (in the process timer)
      is restored to the cpucache and slicecache.
   On exit, the process timer is no longer meaningful.
 */
{
#if SCHEDULER != 2
   unsigned long process_timer;
   cpudibp->cpucache += (process_timer = read_process_timer());
            /* restore cpu allocation to cpucache */
   slicecache += process_timer;
#endif
}
 
static void zapslice(void)
/* Zeroes the slice for the domain in cpudibp */
{
#if SCHEDULER != 2
   uncachecpuallocation();
   set_process_timer(0); /* this also effectively stops the timer */
   cpuslicestart -= slicecache;
   slicecache = 0;
#else
   cpudibp->readiness |= LOWPRIORITY;
#endif
}

void stopdisp(void)
/* Stop dispatching all domains. */
/* Called at checkpoint to prevent domains from modifying anything. */
{
   iosystemflags |= DISPATCHINGDOMAINSINHIBITED;
   enqmvcpu(&cpuqueue); /* This will move cpu queue to frozen */
   if (cpudibp) zapslice(); /* Stop the running domain. */
   /* Other domains will be caught at startdom. */
}

static uint64 outagestart;

void runmigr(void)
/* Stops the prime meter, so that no domains run other than
   the external migrator, which has its own meter. */
/* The cda of the prime meter node must be 2. */
{
   NODE *primemeter;
   outagestart = read_system_timer(); /* record start of outage */
   primemeter = srchnode((unsigned char *)"\0\0\0\0\0\2");
   if (primemeter == NULL) /* should be locked in memory */
      crash("KSCHEDxyz prime meter not in memory");
   if (unprnode(primemeter) != unprnode_unprepared)
      crash("KSCHEDxya can't unprepare prime meter");
   if (primemeter->keys[14].type & prepared)
      crash("KSCHEDxyb garbage in slot 14");
   /* Save CPU counter (slot 3) in slot 14, then zero slot 3. */
   primemeter->keys[14] = primemeter->keys[3];
   primemeter->keys[3] = dk0;
}

void slowmigr(void)
/* Restores the prime meter cpu count, so domains can run. */
{
   NODE *primemeter;
   uint64 outageend;
   primemeter = srchnode((unsigned char *)"\0\0\0\0\0\2");
   if (primemeter == NULL) /* should be locked in memory */
      crash("KSCHEDxyz prime meter not in memory");
   if (unprnode(primemeter) != unprnode_unprepared)
      crash("KSCHEDxya can't unprepare prime meter");
   if (primemeter->keys[14].type & prepared)
      crash("KSCHEDxyb garbage in slot 14");
   primemeter->keys[3] = primemeter->keys[14];
   enqmvcpu(&junkqueue); /* restart domains that trapped */
   /* Compute and save migration outage duration. */
   outageend = read_system_timer(); /* record end of outage */
   // llisub(&outageend, &outagestart); 
   outageend -= outagestart;  /* calc length of outage */
   // lliadd(&cumulativemigrationoutage, &outageend);
   *(uint64*)&cumulativemigrationoutage += outageend;
   // if (llicmp(&maxmigrationoutage, &outageend) < 0)
   if (*(uint64*)&maxmigrationoutage < outageend)
       *(uint64*)&maxmigrationoutage = outageend;
}
 
void putawaydomain()
/* Makes the domain in cpudibp no longer primed to run. */
{
   register NODE *np = {cpudibp->rootnode};
#if defined(viking)
   long long cycleend;
   long long instend;
#endif

#if SCHEDULER != 2
   uncachecpuallocation();
   kprio(np) += (cpuslicestart - slicecache);
        /* add in the amount of time the domain ran */
   md_putawaydomain();   /* machine-dependent teardown */
#endif
#if defined(viking)
   if (lowcoreflags.counters) {
      cycleend = get_cycle_count();
      instend = get_inst_count();
      cpudibp->ker_cycles += (cycleend - cpu_cycle_start);
      cpudibp->ker_instructions += (instend - cpu_inst_start);
      cpu_cycle_start = cycleend;
      cpu_inst_start = instend;
   }
#endif
   unpreplock_node(np);
   cpudibp = NULL;       /* no domain running now */
   cpuactor = NULL;
}
 
void rundom(register NODE *rootnode)
{
   register unsigned long thisprio;
   register union Item *ip, *nextitem, *leftitem;
 
   if (rootnode < firstnode || rootnode >= anodeend)
      crash("rundom of non-node");
   if (rootnode->domhookkey.type == pihk)
         zaphook(rootnode);
   if (rootnode->prepcode == prepasdomain)
         rootnode->pf.dib->readiness |= HOOKED;
   kclock = read_system_timer();
   thisprio = updprio(rootnode);
 
   /* Give him a hook key to the cpu queue */
   rootnode->domhookkey.type = pihk;
   rootnode->domhookkey.databyte = 1;
   rootnode->domhookkey.nontypedata.ik.item.pk.subject =
         (union Item *)&cpuqueue;
   /* Find his proper place in the cpu queue */
   for (ip=(union Item *)&cpuqueue; ; ip=nextitem) {
      /* Every domain at or to the left of this item in the CPU queue
         has better priority than this domain. */
      NODE *np;
 
      nextitem = ip->key.nontypedata.ik.item.pk.rightchain;
      if (nextitem == (union Item *)&cpuqueue)
            break;  /* reached end of queue */
      np = hooktonode(nextitem);
      if (thisprio <= getprio(np) /* it might go here */
          && thisprio <= updprio(np)  /* check that his priority is
                                         accurate */
         ) break;  /* it does go here */
   }
   /* This domain goes in front of (to left of) item at nextitem. */
   leftitem = (union Item *)
              (nextitem->key.nontypedata.ik.item.pk.leftchain);
   if (leftitem == (union Item *)&cpuqueue
                              /* we are inserting at head of queue */
       && cpudibp != NULL     /* and a domain is running */
      ) zapslice();           /* we may have higher priority */
   /* chain this hook into the queue */
   rootnode->domhookkey.nontypedata.ik.item.pk.leftchain = leftitem;
   rootnode->domhookkey.nontypedata.ik.item.pk.rightchain = nextitem;
   leftitem->key.nontypedata.ik.item.pk.rightchain =
      nextitem->key.nontypedata.ik.item.pk.leftchain =
         (union Item *)&(rootnode->domhookkey);
}

void rundomifok(
   NODE *rootnode)
{
   if (iosystemflags & DISPATCHINGDOMAINSINHIBITED)
      enqueuedom(rootnode, &frozencpuqueue);
   else rundom(rootnode);
}
 
void ksstall(jumper,jumpee)
register NODE *jumper, *jumpee;
/* Check the priority of the jumper against that of the jumpee
  If the jumper's priority is larger (worse) than the jumpee,
    then the jumper will run after the jumpee, and all is OK
  If the jumper has a better (lower) priority than the jumpee,
    adjust (worsen) its priority to be that of the jumpee
    and if the jumper is on the CPU queue, adjust its position.
 */
{
   register unsigned long jeprio;
 
   jeprio = updprio(jumpee);
   if (jeprio <= kprio(jumper)) return;
   kprio(jumpee) = kprio(jumper);
   if (jumpee->domhookkey.type == pihk
       && jumpee->domhookkey.nontypedata.ik.item.pk.subject
          == (union Item *)&cpuqueue ) /* jumpee is on the cpu queue */
             rundom(jumpee);  /* Move to correct location in the queue
                                 based on new priority */
}

void select_domain_to_run(void)
/* Sets cpudibp to best domain to run */
{
   register union Item *ip;
   register NODE *np;
 
   for (;;) {   /* loop looking for a domain we can prepare */
      if ((ip = cpuqueue.head) == (union Item *)&cpuqueue
          /* no domains to run */
          && ((*waitstateprocess)(),
              (ip = cpuqueue.head) == (union Item *)&cpuqueue ) ) {
                  /* still no domains to run */
#if SCHEDULER != 2
                  sidle(1L<<log_maxslice, &idledib);
#else
                  cpudibp = &idledib;
                  start_process_timer();
#endif
                  return;
              }
      np = hooktonode(ip);
      if (preplock_node(np,lockedby_selectdomain))
         crash("KSCHED001 CPU queue domain preplocked 3579");
      cpuactor = np;
      if (np->prepcode == prepasdomain)
   break; /* a good domain */
      if (np->prepcode != unpreparednode)
         if (unprnode(np) != unprnode_unprepared)
            crash("KSCHED002 Can't unprepare CPU queue node 3580");
      switch (prepdom(np)) {
         case prepdom_overlap:
            crash("KSCHED003 Prepdom_overlap on CPU queue domain 3581");
         case prepdom_malformed:
/*......**/ crash("malformed domain\n");
            enqueuedom(np,&junkqueue);
            /* fall into next case */
         case prepdom_wait:
            /* couldn't prepare it */
         case prepdom_prepared:
            break;
      }
      if (np->prepcode == prepasdomain)
   break;       /* prepared it */
      unpreplock_node(np);     /* couldn't prepare it, it has
                        been removed from the cpu queue */
   }
   /* Domain at *np is now prepared as domain and preplocked */
   zaphook(np); /* remove from cpu queue so we can compute a correct slice */
   startdom(np->pf.dib);
   return;
}

/* handle_process_timer_wakeup is called every clock tick when SCHEDULER==2.
   Otherwise it is only called when the simulated process timer runs out.
*/
static void handle_process_timer_wakeup(
   struct KernelTask *kt)
{
   int s;
#if SCHEDULER != 2

   if (cpudibp == NULL) select_domain_to_run();
   else if (read_process_timer() == 0) { /* if not a false alarm */
      /* There are two cases:
         (1) time slice exhausted
         (2) meter cache exhausted
       */
      if (slicecache == 0) {
         /* time slice exhausted */
         if (cpudibp != &idledib) {
            NODE *rn = cpudibp->rootnode;
            putawaydomain(); /* this sets cpudibp to NULL */
            rundomifok(rn);
         }
         select_domain_to_run();
      } else { /* meter cache exhausted */
         refill_cpucache();
      }
   } /* else a false alarm */
#else
   union Item *ip;
   extern struct DIB *tickdib;

   kclock = read_system_timer();

   for (ip = cpuqueue.head;	/* Update cpuqueue domain's priorities */
        ip != (union Item *)&cpuqueue; 
        ip = ip->key.nontypedata.ik.item.pk.rightchain) {
      updprio(hooktonode(ip));
   }

   if (tickdib) {
      /* add all the time to this domain */
      if ((tickdib->cpucache <= 160000)) {
         tickdib->cpucache = 0;
         tickdib->readiness |= ZEROCACHE;
      }
      else  tickdib->cpucache -= 160000;
      if (tickdib != &idledib && tickdib->rootnode->prepcode == prepasdomain) {
         NODE *rn = tickdib->rootnode;
         kprio(rn) += 160000;
         tickdib->readiness |= LOWPRIORITY;
      }
   }
#endif
   s = splhi();
   processtimerktactive = FALSE; /* we are done */
#if SCHEDULER != 2
   checkptwakeup();
#endif
   splx(s);
}

