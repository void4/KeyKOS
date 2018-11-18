/* Routines in GDIRECTC called only by GCKPTC */
 
extern void gdintsdr(void);     /* Start building disk directories */
 
extern int gdindp(PAGE p, uint64 checkpointtod); /* 0-->dir end, p is empty */
