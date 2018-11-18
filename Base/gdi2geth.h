/* Definitions of routines in GDIRECTC called only by GETC */
 
extern struct CodeIOret gdilook(const CDA cda, NODE *actor);
extern struct CodeIOret gdiblook(CDA cda, NODE *actor);
extern struct CodeIOret gdilbv(CDA cda, NODE *actor);
/*
   Returns only :
        io_notmounted     0
        io_notreadable    1
        io_potincore      2  -  ioret is pointer to CTE for pot
        io_pagezero       3  -  ioret is *PCFA for virtual zero page
        io_cdalocked      5  -  CDA may already be in transit
        io_noioreqblocks  6
        io_notindirectory 7  -  CDA not in requested directory(s)
        io_built          8  -  Request built, ioret is *request
*/
 
extern int gdiverrq(DEVREQ *devreq);
#define gdiverrq_current 0
#define gdiverrq_backup 1
#define gdiverrq_neither 2
 
extern PCFA *gdiladbv(CDA cda);  /* May return NULL */
