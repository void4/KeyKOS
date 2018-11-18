/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include "string.h"
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "primcomh.h"
#include "geteh.h"
#include "kermap.h" /* for lowcorearea */
// #include "promif.h"
 
 
int scsaddp(
   csid chargesetid,
   CTE *cte)
{
   crash("SCSADDP entered");
}
void scsuninv(
   struct Key *key)
{
   crash("SCSUNINV entered");
}
void scsinvky(
   struct Key *key)
{
   crash("SCSINVKY entered");
}
void scsunlk(
   struct Key *key)
{
   crash("SCSUNLK entered");
}
int scsgetid(
   struct Key *key)
{
   crash("SCSGETID entered");
}
 
 
static void retktplus2()
{
   cpuarglength = 0;
   cpuordercode = KT+2;
   jsimple(0);  /* no keys */
}
void jchargeset(
   struct Key *key)
{retktplus2();}
 
 
void startworrier()
{
   return; /* What me worry? */
}

int cdacmp(const uchar *cda1, const uchar *cda2)
{  int i;
   for (i=0; i<sizeof(CDA); i++) {
      if (cda1[i] != cda2[i]) {
         if (cda1[i] < cda2[i]) return -1;
         else return 1;
      }
   }
   return 0;
}


/* Tracing stuff. */

#define ntraces 120
#define tsize 90
char tracetable[ntraces][tsize+1]; /* a circular buffer of strings */
int tcursor = 0; /* points to oldest entry */

void logstr(char *p)
/* Log a string for debugging. */
{

 if (!(lowcoreflags.logbuffered)) printf(p);
 else {
   while (*p) {
      /* overlay the oldest entry */
      Strncpy(tracetable[tcursor], p, tsize);
      if (Strlen(p) > tsize) {
         p += tsize;
      } else { /* copied last piece */
         p += Strlen(p);
      }
      tracetable[tcursor][tsize] = '\0';
      if (++tcursor >= ntraces) tcursor = 0;
   }
 }
}

void printlog()
{
   int i = tcursor;
   printf("\n");
   do {
      printf(tracetable[i]);
      if (++i >= ntraces) i = 0;
   } while (i != tcursor);
   printf("\n");
}

void devicehd(CTE *p)
/* Halt DMA I/O to a page. */
{
/* We never do DMA I/O. */
}
bool emptstal(struct Key *key)
{  return FALSE; }
bool unhook(NODE *n)
{  return FALSE; }

