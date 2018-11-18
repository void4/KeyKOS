#ifndef KERTASK_H
#define KERTASK_H

struct KernelTask {
   struct KernelTask *next;
   void (*kernel_task_function)(struct KernelTask *);
};

#endif /* KERTASK_H */

