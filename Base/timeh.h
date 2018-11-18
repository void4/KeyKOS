/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef TIME_H
#define TIME_H

#include "keyh.h"
#include "kertaskh.h"
#include "booleanh.h"

/* timeh.h - The time manager. */

#define nbwaitkeys 4  /* number of bwait keys supported */

struct TQE {
   struct TQE *prev, *next;
  uint64 wakeuptime;
   void (*wakeuproutine)( /* function to call on wakeup */
      struct TQE *);
};
void jbwait(struct Key *);

/* Stuff used by timemdc.c */
extern bool timektactive;
extern struct KernelTask timekt;
extern uint64 kwakeuptime;

#endif /* TIME_H */
