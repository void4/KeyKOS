/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef KSCHED_H
#define KSCHED_H

#include "keyh.h"
#include "kertaskh.h"
#include "booleanh.h"

void nowaitstateprocess(void);
extern struct KernelTask processtimerkt;
extern bool processtimerktactive;
extern struct DIB idledib;
extern void ksstall(NODE *,NODE *);
extern void loadpt(void);
extern void startdom(struct DIB *);
extern void uncachecpuallocation(void);
void stopdisp(void);
void runmigr(void);
void slowmigr(void);
extern void putawaydomain(void);
void rundom(NODE *);
void rundomifok(NODE *);
extern void select_domain_to_run(void);
void startworrier(void);
extern void slowstart(void);      /* Handle bits on in cpudibp->readiness */
#endif /* KSCHED_H */
