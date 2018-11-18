/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*   MODULE     eresyncc.c
/*   TITLE      EXTERNAL resync'er
/***********************************************************************
/*                                                                     *
/*   This routine obtains the CDAs of ranges to be resynchronized
       and resynchronizes them one at a time.
/*                                                                     *
/***********************************************************************
/*                                                                     *
*/
 
#include <keykos.h>
#include <string.h>
#include "lli.h"
#include "kktypes.h"
#include "cvt.h"
 
KEY TOOL = 0;    /* Resync tool */
#define Resynctool_Wait 0
#define Resynctool_ResyncPage 1
#define Resynctool_ResyncNode 2
#define Resynctool_End 3

typedef unsigned char CDA[6];
struct {
   CDA firstcda;
   CDA lastcda;
} cdas;
 
 int bootwomb=1;
 int stacksiz=4096;

JUMPBUF;
 
const char title [] = "ERESYNCC";
static const LLI llione = {0,1};
 
factory ()
{
   for (;;) {
      unsigned long type;
      LLI firstlli, lastlli, thislli;

      /* At this point no range is being resynced. */
      KC (TOOL, Resynctool_Wait) RCTO (type) STRUCTTO(cdas);
      if (type > 1) crash("Wait hi rc");
      b2lli(cdas.firstcda, 6, &firstlli);
      b2lli(cdas.lastcda, 6, &lastlli);
      if (llicmp(&firstlli, &lastlli) > 0) crash("first > last");
      /* Loop over cdas in the range. */
      for (thislli = firstlli;
           llicmp(&thislli, &lastlli) <= 0;
           lliadd(&thislli, &llione) ) {
         CDA thiscda;
         unsigned long rc;
         lli2b(&thislli, thiscda, 6);
         KC (TOOL, Resynctool_ResyncPage + type)
            STRUCTFROM(thiscda) RCTO(rc);
         if (rc) break;  /* Whoops, quit and retry. */
      }
      KC (TOOL, Resynctool_End) RCTO(type); /* ignore return code */
   }                       /* end forever loop */
}
 
