
#include "sysdefs.h"
#include "keyh.h"
 
 
/* CPU dependent fields for memory management */
 
CTE *cpumempg[4] = {NULL, NULL, NULL, NULL};
                         /* page locked into window or NULL */
