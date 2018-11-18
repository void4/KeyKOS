#include "kertaskh.h"
extern struct KernelTask
      *kernel_task_queue_head;  /* list terminated by NULL */
      /* This "queue" isn't FIFO. */
 
void enqueue_kernel_task(
   struct KernelTask *);
