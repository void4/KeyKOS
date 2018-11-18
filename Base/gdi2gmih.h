/* Definitions of routines in GDIRECTC called only by GMIGRATC */
 
extern int gdiesibd(void);      /* 0 --> no more unmigrated entries */
 
uchar *gdifncda(const uchar *mincda); /* Next cda in dataforap or NULL */
 
extern int gdicoboc(CDA lowcda, CDA highcda, CTE *allopot);
 
extern RANGELOC gdilkunm(CDA cda);
