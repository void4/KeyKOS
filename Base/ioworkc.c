/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* IOWORKC - Global variables for the disk I/O system */
 
#include "sysdefs.h"
#include "keyh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
 
 
CTE buppage1, buppage2;    /* Chain heads for current and next backup */
                           /* pages. Only page.leftchain and */
                           /* page.rightchain are used in these heads */
 
CTE *allocationpotchainhead = NULL;
 
uint64 todthismigration = 0;     /* Time of most recent migration */
 
char iosystemflags = MIGRATIONINPROGRESS;
bool nodecleanneeded = FALSE;
 
uint32 outstandingio[OUTSTANDINGIOSIZE>>5] = {0};
 
 
/*
   If continuecheckpoint is not NULL, then a checkpoint is in progress
   and calling the procedure assigned to continuecheckpoint will
   attempt to make further progress with the checkpoint.
*/
void (*continuecheckpoint)(void) = NULL;
 
 
/* Statistical counters */
 
#undef defctr   /* to eliminate compiler warning */
#undef defctra
#undef deftmr
#define defctr(c)    uint32 c = 0;
#define defctra(c,n) uint32 c[n] = {0};
#define deftmr(t)    LLI t = {0,0};
#include "counterh.h"

