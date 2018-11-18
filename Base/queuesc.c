/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "kschedh.h"
#include "queuesh.h" 
#include "diskless.h"
#include "kermap.h" /* for lowcorearea */
 
/* Wait queues, additional queues in the device blocks */
 
#define dqh(q) struct QueueHead q = {(union Item*)&q,(union Item*)&q}
 
dqh(cpuqueue);
dqh(frozencpuqueue);
dqh(migratewaitqueue);
dqh(migratetransitcountzeroqueue);
dqh(junkqueue);
dqh(worryqueue);
dqh(kernelreadonlyqueue);
dqh(rangeunavailablequeue);
dqh(noiorequestblocksqueue);
dqh(nonodesqueue);
dqh(nopagesqueue);
dqh(resyncqueue);

struct QueueHead ioqueues[32]; /* Initialized in inits.c */
/* See queue definifions also in:
 timec.c (bwaitqueue) and cons88kc.c (romconsole.readcaller & writecaller)
  and readcaller & writcaller in each of structures line and mousekbd in
  uart88kc.c */
 
void enqueuedom(           /* Put a domain on a queue */
/*
   THIS MAY NOT BE USED TO ENQUEUE ON ANOTHER DOMAIN!
   Do not use to enqueue on the CPUQUE - use RUNDOM for that.
   Domain's NFDOMHOOKKEY must have a hook or be unprepared.
 
   Input -
*/
register NODE *rn,            /* Domain root node to enqueue */
register struct QueueHead *q) /* Pointer to the queuehead to enq on */
/*
   Output - Domain is inserted at tail of queue.
         DATABYTE of the hook is set to 1.  If queue is "worryqueue",
         the caller must change the hook key data byte to 0.
*/
{
#if defined(diskless_kernel)
    if(q == &kernelreadonlyqueue 
       || q == &nonodesqueue || q == &nopagesqueue) { 
    crash("Out of real RAM");}
#endif
 
/* Remove domain from any queue it is on. */
 
   if (rn->domhookkey.type == pihk) zaphook(rn);
 
/* Fix hooked */
 
   if (rn->prepcode == prepasdomain)
       rn->pf.dib->readiness |= HOOKED;
 
 
/* Build new Hook key in "domhookkey" */
 
   rn->domhookkey.nontypedata.ik.item.pk.leftchain = q->tail;
   ((NODE *)q->tail)->rightchain = (union Item *)&rn->domhookkey;
   rn->domhookkey.nontypedata.ik.item.pk.rightchain = (union Item *)q;
   q->tail = (union Item *)&rn->domhookkey;
   rn->domhookkey.nontypedata.ik.item.pk.subject = (union Item *)q;
   rn->domhookkey.databyte = 1;
   rn->domhookkey.type = pihk;
} /* End enqueuedom */
 
 
void enqmvcpu(q)      /* Move all domains on wait queue to cpu queue */
/* Input - */
register struct QueueHead *q;   /* The wait queue to serve */
/*
   Output - All domains on the queue are moved to the cpu or frozen cpu
            queue. (Depending on whether dispatching domains is
            inhibited.)
*/
{
   struct Key *k;
   NODE *n;
   for (k = (struct Key *)q->head;
        k != (struct Key *)q;
        k = (struct Key *)q->head
        ) {
 
      /* Hook key is always in same slot, subtract gives node header */
      n = (NODE *)((char *)k-((char *)&n->domhookkey-(char *)n));
 
      rundomifok(n);
   }
} /* End enqmvcpu */
