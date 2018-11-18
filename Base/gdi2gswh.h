/* Definitions of routines in GDIRECTC called only by GSWAPAC */
 
extern uint32 gdiddp(void);
 
struct gdimgrstRet {
   uint32 initial;       /* The initial (worst case) migration state */
   uint32 current;       /* The current state */
};
extern struct gdimgrstRet gdimgrst(void);
