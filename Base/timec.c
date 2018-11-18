/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* timec.c - The time manager. */

#include "sysdefs.h"
#include "keyh.h"
#include "splh.h"
#include "types.h"
#include "cvt.h"
#include "timeh.h"
#include "timemdh.h"
#include "ktqmgrh.h"
#include "primcomh.h"
#include "cpujumph.h"
#include "queuesh.h"
#include <limits.h>
#include "gateh.h"
#include "kerinith.h"

extern struct TQE dummytqe;
struct {
   struct TQE *tail, *head;
} tqehead = {&dummytqe, &dummytqe};
/* The following is a dummy TQE, used to give a value to
   kwakeuptime when we are waiting for nothing. */
struct TQE dummytqe =
   {(struct TQE *)&tqehead, (struct TQE *)&tqehead,
    -1LL};

/* The queue heads for domains waiting on bwait keys. */
struct QueueHead bwaitqueue[nbwaitkeys];

/* The TQE's for the bwait keys, one per key. */
struct bwaitTQE {
/* Note: the first part of this must match struct TQE exactly. */
   struct TQE *prev, *next;
   uint64 wakeuptime;
   void (*wakeuproutine)( /* function to call on wakeup */
      struct TQE *);
   int bwaitnumber;
} bwaittqe[nbwaitkeys];

static void handle_wakeuptime(
   struct KernelTask *ktp);
/* The kernel task for the time manager. */
struct KernelTask timekt = {NULL, &handle_wakeuptime};

bool timektactive = FALSE;
/* Except while clock interrupts are disabled,
   if (timektactive)
      then (timekt is on the kernel task queue
            or this module is processing a wakeup)
      else timekt is not on the kernel task queue. */
uint64 kwakeuptime = -1LL;

static void dequeuetqe(
   struct TQE *tqe)
/* Dequeue a TQE. */
{
   struct TQE *p = tqe->next,
              *q = tqe->prev;
   if (tqe == tqehead.head) { /* was first in queue */
      kwakeuptime = p->wakeuptime;
         /* This never requires checkwakeup, because we are
            increasing kwakeuptime. */
   }
   q->next = p;  /* unchain it */
   p->prev = q;
   tqe->next = tqe->prev = tqe;
}

static void enqueuetqe(
   struct TQE *tqe)
/* Enqueue a TQE.
   When read_system_time() >= tqe->wakeuptime,
        tqe->wakeuproutine will be called. */
{
   struct TQE *p,*q;
   int s;
   dequeuetqe(tqe); /* first dequeue it if it is on a queue */
   for (p = tqehead.head;
        p->wakeuptime < tqe->wakeuptime;
        p = p->next) {}
   /* Now p points to the first tqe later than ours. */
   q = p->prev;
   tqe->next = p;  /* chain in place */
   tqe->prev = q;
   q->next = p->prev = tqe;
   if (tqehead.head != tqe) return;
   /* We added to the beginning of the chain. */
   s = splhi();
   kwakeuptime = tqe->wakeuptime;
   checkwakeup();
   splx(s);
}

/* Handle invocation of a bwait key. */
void jbwait(
   struct Key *key) /* the bwait key */
{
   int bwaitindex = b2int(key->nontypedata.dk7.databody+5, 2);
          /* get which bwait object from key */
   uint64 bwaittod;
   // unsigned char bwaittodstr[8];
   struct bwaitTQE *thistqe;

   thistqe = &bwaittqe[bwaitindex];
   switch (cpuordercode) {

    case 0: /* wait for timer */
      if (thistqe->next != (struct TQE *)thistqe) {
         /* already on the queue */
         simplest(1);
         return;
      }
      bwaittod = read_system_timer();
      // if (llicmp(&thistqe->wakeuptime, &bwaittod) <= 0)
      if(thistqe->wakeuptime <= bwaittod) {
         /* time expired already */
         simplest(0);
         return;
      }
      /* must wait for the timeout */
      enqueuetqe((struct TQE *)thistqe);
      enqueuedom(cpuactor, &bwaitqueue[bwaitindex]);
      abandonj();
      return;

    case 1: /* set waituptime */
      //pad_move_arg(bwaittodstr, 8);
      //b2lli(bwaittodstr, 8, &bwaittod);
      pad_move_arg((char*)&bwaittod, 8);
      switch (ensurereturnee(0)) {
       case ensurereturnee_wait:
         abandonj();
         return;
       case ensurereturnee_overlap:
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      handlejumper();
      if (getreturnee()) return; /* no returnee */
      thistqe->wakeuptime = bwaittod;
      if (thistqe->next != (struct TQE *)thistqe) {
         /* if it is on the queue */
         enqueuetqe((struct TQE *)thistqe); /* move to new queue location */
      }
      cpuordercode = 0; /* return code */
      cpuarglength = 0;
      cpuexitblock.keymask = 0;
      return_message();
      return;

    case KT:
      simplest(KT+2);
      return;
    default:
      simplest(KT+2);
      return;
   } /* end of switch cpuordercode */
}

static void wakeupbwaiter(
   struct bwaitTQE *tqe)
{
   enqmvcpu(&bwaitqueue[tqe->bwaitnumber]);
}

static void handle_wakeuptime(
   struct KernelTask *ktp) /* will be &timekt */
{
	int s;
   uint64 now = read_system_timer();
   while (tqehead.head != NULL
          && tqehead.head->wakeuptime <= now) {
      /* This TQE has woken up. */
      struct TQE *tqe = tqehead.head;
      dequeuetqe(tqe);
      (*tqe->wakeuproutine)(tqe);
   }
   s = splhi();
   timektactive = FALSE; /* we are done */
   checkwakeup();
   splx(s);
} 

void itime(void)
/* Initialization. */
{
   int i;
   for (i=0; i<nbwaitkeys; i++) {
      struct bwaitTQE *tqe = &bwaittqe[i];
      struct QueueHead *qh = &bwaitqueue[i];
      tqe->bwaitnumber = i;
      tqe->wakeuproutine = (void (*)(struct TQE *))&wakeupbwaiter;
      tqe->next = tqe->prev = (struct TQE *)tqe; /* empty chain */
      qh->head = qh->tail = (union Item *)qh;
   }
}

