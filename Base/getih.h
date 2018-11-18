/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef GETIH_H
#define GETIH_H

#include "kktypes.h"
#include "cvt.h"
#include "ioreqsh.h"

#define NUMBEROFREQUESTS 30   /* 200 on the 370 */

union IOret {              /* Return value, depends on code below: */
   CTE *cte;
   PCFA *pcfa;
   struct Request *request;
   RANGELOC rangeloc;
};
 
struct CodeIOret {
   union IOret ioret;      /* Returned value, depends on code below: */
   int code;               /* Status code for request as follows: */
#define io_notmounted     0
#define io_notreadable    1
#define io_potincore      2  /* ioret is pointer to CTE for pot */
#define io_pagezero       3  /* ioret is *PCFA for virtual zero page */
#define io_started        4
#define io_cdalocked      5  /* CDA may already be in transit */
#define io_noioreqblocks  6
#define io_notindirectory 7  /* CDA not in requested directory(s) */
#define io_built          8  /* Request built, ioret is *request */
#define io_allocationdata 9  /* ioret is *PCFA with allocation data */
#define io_readpot       10  /* ioret is RANGELOC of pot to read */
};
 
#define cdahash(cda) b2long((cda)+2, 4)
#define pothash(rl) ((rl).range<<16 | (rl).offset)
 
extern void getenqio(uint32 id, NODE *actor);  /* Add to I/O queue */
 
extern CTE *getfpic(RANGELOC rl);              /* Find pot in core */
 
extern int getlock(uint32 id, NODE *actor);    /* Get CDA lock */
 
extern void getunlok(uint32 id);               /* Release CDE lock */
 
extern void checkforcleanstart(void);          /* Run kernel agenda */
 
extern struct Request *acquirerequest(void);
 
struct DevReq *acquiredevreq(struct Request *req);
/* Conversion notes:
   Callers must set the drq->device and drq->offset fields and
   then call md_dskdevreqaddr(drq); */
/*   The drq->offset was called addressondevice in assembler */
 
extern void getended(struct Request *req);
 
extern void setupvirtualzeropage(PCFA *pcfa, CTE *cte);
 
extern struct CodeIOret getreqn(CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern struct CodeIOret getreqp(const CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern struct CodeIOret getreqap(RANGELOC rl, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern struct CodeIOret getreqba(CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern NODE *movenodetoframe(const uchar* cda, CTE *cte); /* NULL==>no frame */
 
void getreqpcommon(
   struct CodeIOret *ret,
   const CDA cda,
   int type,
   void (*endingproc)(REQUEST *req),
   NODE *actor,
   CTE *cte); /* CTE to read into, or NULL to allocate one */

#endif /* GETIH_H */
