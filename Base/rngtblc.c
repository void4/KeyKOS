#include "kktypes.h"
#include "lli.h"
#include "sysdefs.h"
#include "sysparms.h"
#include "keyh.h"
#include "rngtblh.h"
 
uint32 swapaloc[MAXSWAPRANGES];
char swapused[MAXSWAPRANGES];
 
RANGETABLE swapranges[MAXSWAPRANGES + MAXUSERRANGES];
RANGETABLE *userranges = &swapranges[MAXSWAPRANGES];
 
RANGELIST *rangelistbase = NULL;  /* Available rangelists */
