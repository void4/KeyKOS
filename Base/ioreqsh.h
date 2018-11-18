/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef IOREQ_H
#define IOREQ_H

 
#include "kktypes.h"
#include "keyh.h" /* for RANGELOC */

typedef uint32 TIME;          /* Units for timing I/O operations */
 
struct DevReq {
   struct DevReq *devreq;     /* Next devreq for this request or NULL */
   struct Request *request;   /* Pointer to the request */
   struct Device *device;     /* Pointer to the device for the I/O */
   uint32 offset;             /* To remember address after the I/O */
                              /* addressondevice in assembler version */
                              /* Offset of written directory after a */
                              /* directory write. */
                              /* Offset to write to on a write or */
                              /* migratewrite */
   struct DevReq *nextio;     /* Next devreq on device queue */
   struct DevReq *previo;     /* Previous devreq on device queue */
   RANGELOC swaploc;          /* Swap location of DIRENTRY that gave */
                              /*   rise to this DEVREQ. */
            /* If devreq->request->type == REQDIRECTORYWRITE,
                the swaploc written is returned here. */
   char status;               /* Current status of I/O as follows: */
#define DEVREQOFFQUEUE    0   /* Devreq not yet queued or tried */
#define DEVREQPENDING     1   /* Devreq on DEVICE I/O queue */
#define DEVREQSELECTED    2   /* Devreq off device IO queue */
                              /*   - can be building CCWs or running */
#define DEVREQCOMPLETE    3   /* Devreq complete, no error */
#define DEVREQPERMERROR   4   /* Devreq had permanent error */
#define DEVREQNODEVICE    5   /* DEVICE for devreq not available */
#define DEVREQABORTED     6   /* Devreq was aborted */
#define DEVREQABORTOREND  7   /* Selected, must complete or abort */
 
   char flags;                /* Flags as follows: */
#define DEVREQSECONDTRY 0x80  /* First read check pattern test failed */
#define DEVREQSWAPAREA  0x40  /* direntry and rangeloc fields are used */
};
typedef struct DevReq DEVREQ;
 
 
struct Request {
   struct Request *next;            /* Next request on list */
   struct DevReq *devreqs;          /* DevReq list for this request */
   void (*doneproc)(struct Request *req);  /* Procedure to call when done */
   uint32 doneparm;                 /* Parameter for doneproc */
   CTE *pagecte;                    /* CTE for frame for request, */
                                    /*   NULL if to be allocated */
   struct DirEntry *direntry;       /* Pointer to DIRENTRY that gave */
                                    /*   rise to some of the DevReqs */
                                    /*   linked from this request */
   RANGELOC potaddress;             /* Pot address for CTE iff pot */
   TIME enqtime;                    /* Time enqueued for calculating */
                                    /*   page service times */
   uint16 completioncount;          /* Number of DevReqs left to run */

   char type;                       /* Type of request as follows: */
#define REQNORMALPAGEREAD     0     /* Read of a page or pot */
#define REQDIRECTORYWRITE     1     /* Write to checkpoint directory */
#define REQCHECKPOINTHDRWRITE 2     /* Write to checkpoint header */
#define REQMIGRATEREAD        3     /* Read for migration */
#define REQMIGRATEWRITE       4     /* Write for migration */
#define REQJOURNALIZEWRITE    5     /* Write from journal key call */
#define REQRESYNCREAD         6     /* Read to re-sync obsolete range */
#define REQHOMENODEPOTWRITE   7     /* Write DevReqs one at a time */

   unsigned onqueue :1;             /* 1 iff request is on one of the
                                       three work queues */

   PCFA pcfa;                       /* cda, flags, and allocationid */
 
   /* Notes on the fields in pcfa: */
   /*   pcfa.cda is CDA of page or 0x800000000000 for a node pot or */
   /*   0x000000000000 for an allocation pot. The used flags are: */
#define REQPAGEALLOCATED 0x40       /* pagecte was allocated by gccwc.c */
#define REQPOT           0x20       /* Request is for an allocation */
                                    /*   pot or for a node pot */
#define REQALLOCATIONPOT 0x10       /*   It is for an allocation pot */
#define REQHOME          0x08       /* Devreqs are for home area */
#define REQCHECKREAD     0x04       /* Check block written correctly */
                                    /*   after reading it */
/*      REQVIRTUALZERO   0x02       Reserved. */
#define REQGRATIS        0x01       /* Page is in gratis status */
};
typedef struct Request REQUEST;
 
 
/* The following routines are in GETC */
 
extern REQUEST *acquirerequest(void);       /* Get a request block */
 
extern DEVREQ *acquiredevreq(REQUEST *req); /* Add a devreq */
 
extern void getredrq(DEVREQ *drq);          /* Free devreq */
 
extern void getrereq(REQUEST *req);         /* Free request */
#endif /* IOREQ_H */
