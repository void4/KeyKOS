/* Routines in GDIRECTC used by more than one module */
#include "ioreqsh.h"
 
extern void gdiredrq(DEVREQ *devreq);
 
extern PCFA *gdiladnb(CDA cda);  /* May return NULL */
 
extern void gdiswap(void);
 
extern void gdisetvz(PCFA *pcfa);
 
extern void gdiclear(CDA cda);
 
extern int gdiciicd(CDA cda);   /* 0 if not in working directory */
 
extern void gdirembk(CDA cda, bool innext);
