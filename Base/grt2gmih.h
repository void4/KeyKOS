/* Definitions of routines in GRANGETC called only by GMIGRATE */
 
struct GrtAPI_Ret {
   CDA lowcda, highcda;    /* Low & High cdas covered by pot */
   RANGELOC rangeloc;      /* Rangeloc of the allocation pot */
                           /* If the range is not mounted, */
                           /* rangeloc.range will be -1 and */
                           /* lowcda = input cda and */
                           /* hicda = next mounted page cda */
   int resync;             /* Range is being resynced */
};
extern struct GrtAPI_Ret grtapi(const CDA cda);
 
extern int grtrlr(RANGELOC rangeloc);  /* zero if not readable */
 
extern void grtmro(struct Device *dev, CTE *cte);
 
extern void grtintap(CTE *allopot);
