/* Routines in GETC used only by GMIGRATC */
 
extern struct CodeIOret getreqhp(CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern struct CodeIOret getreqnm(CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern struct CodeIOret getreqpm(CDA cda, int type,
                                void (*endingproc)(struct Request *req),
                                NODE *actor);
 
extern void getsucnp(CTE *cte, REQUEST *req);
 
extern int getendedcleanup(REQUEST *req);
 
extern int setupversion(CTE *cte, REQUEST *req);  /* 0 - pg discarded */
