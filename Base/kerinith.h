/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Headers for the initialization routines in the KeyTech Kernel */

#include "kktypes.h"

extern void init(void);      /* Overall kernel initialization */
extern void wss(void);       /* Kernel constant initialization */
extern void depends(void);
extern void kscheds(void);
extern void jconinit(void);  /* Initialize the conblock (in cons88kc.c) */
extern void memorys(void);
extern void iet(void);
extern void iranget(char *rangelists, int number);
extern void icleanl(void);
extern void iswapa(void);
extern void imigrate(char *data, uint32 len);
#define MIGRATEWORKSIZE (sizeof(RANGELOC) * (pagesize/6)   \
              + sizeof(CTE*) * (pagesize/6) + (pagesize/6))

extern void itime(void);
extern void memorys(void);  /* Initialization for memory. */
extern void spaces(void);
extern void init_copywin(void);

extern void grestart(void (*proc)(void));
