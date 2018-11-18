/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "addrcteh.h"
 
CTE *intrn_addr2cte(   /* Convert bus address to core table entry */
   unsigned long busaddr)
{
#if LATER
 CTE * j;
 busaddr &= -pagesize;

/* This is very crude but I want it right soon!  */
  for(j=apageend; j<lastcte; j++) if(busaddr == j->busaddress) return j;
  crash("addr2cte unknown busaddr");
#else
  crash("intrn_addr2cte(): not inplemented, should not be called");
#endif
  return 0;
}
