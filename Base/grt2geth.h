/* Definitions of routines in GRANGETC called only by GETC */
 
extern struct CodeIOret grtfadfp(RANGELOC rl, const CDA cda);
  /* Returns io_readpot with rangeloc of pot or */
  /*  io_allocationdata with *PCFA for cda */
 
  /* grtrrr returns grt_notmounted, grt_notreadable, and grt_mustread */
extern struct CodeGRTRet grtrrr(RANGELOC rl);
