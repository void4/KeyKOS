
/* Queues defined in the kernel */
/* N.B These queues are allocated space in QUEUESC */
 
#ifndef QUEUE_H
#define QUEUE_H

#include "keyh.h"

extern struct QueueHead cpuqueue;
extern struct QueueHead frozencpuqueue;
extern struct QueueHead migratewaitqueue;
extern struct QueueHead migratetransitcountzeroqueue;
extern struct QueueHead junkqueue;
extern struct QueueHead worryqueue;
extern struct QueueHead kernelreadonlyqueue;
extern struct QueueHead rangeunavailablequeue;
extern struct QueueHead noiorequestblocksqueue;
extern struct QueueHead nonodesqueue;
extern struct QueueHead nopagesqueue;
extern struct QueueHead resyncqueue;
#define NUMBERIOQUEUES 32 /* must be a power of two, for IOQUEUESMASK */
extern struct QueueHead ioqueues[NUMBERIOQUEUES];
#define IOQUEUESMASK (32-1)
 
 
/* Routines to queue domains */
 
extern void enqueuedom(NODE *rootnode, struct QueueHead *q);
/*
   THIS MAY NOT BE USED TO ENQUEUE ON ANOTHER DOMAIN!
   Do not use to enqueue on the CPUQUE - use RUNDOM for that.
   Domain's NFDOMHOOKKEY must have a hook or be unprepared.
 
   Output - Domain is inserted at tail of queue.
         DATABYTE of the hook is set to 1.  If queue is "worryqueue",
         caller must change the hook key data byte to 0.
*/
 
/* Move all domains on queue to cpu queue */
void enqmvcpu(struct QueueHead *queue);

#endif /* QUEUE_H */
