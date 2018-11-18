/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* ktqm88kc.c - The kernel task queue manager for Motorola 88100 */
/* The header for this file is ktqmgrh.h */

#include "sysdefs.h"
#include "ktqmgrh.h"
#include "splh.h"
 
struct KernelTask
      *kernel_task_queue_head = NULL;
 
unsigned short ektsavedsr;
 
void enqueue_kernel_task(
   struct KernelTask *ktp)
/* Enqueue a kernel task */
/* Preserves the interrupt enable level. */
{
   unsigned int level = splhi();   /* lock out interrupts */
   ktp->next = kernel_task_queue_head;
   kernel_task_queue_head = ktp;
   splx(level);   /* Restore interrupt enable level. */
}

void do_a_kernel_task(void)
/* Do a kernel task. There must be at least one. */
{
   struct KernelTask *ktp;
   int s;

   s = splhi();   /* lock out interrupts */
   ktp = kernel_task_queue_head;       /* get it */
   kernel_task_queue_head = ktp->next; /* unchain it */
   splx(s);  /* allow interrupts */
   (*ktp->kernel_task_function)(ktp);  /* call it */
}
