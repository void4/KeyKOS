/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* llic.c - long long int routines written in C */
/* The header for this file is lli.h */
#include <string.h>
#include "lli.h"

 
/* Functions to support 64-bit unsigned integers */
 
void lliadd (
   LLI *a, const LLI *b)             /* adds b to a */
{
   unsigned long t = a->low + b->low;
   a->hi += b->hi;
   if (t < a->low || t < b->low) a->hi++;
   a->low = t;
}
void llisub (
   LLI *a, const LLI *b)             /* subtracts b from a */
{
   LLI c;
   LLI llione = {0,1};
   /* c = -b; */
   c.hi = b->hi ^ 0xffffffff;
   c.low = b->low ^ 0xffffffff;
   lliadd(&c, &llione);
   lliadd(a, &c);
}
void llilsl (
   LLI *a, unsigned int b)     /* logical shift a left b bits */
{
   for (; b>0; b--) {
      a->hi <<= 1;
      if (a->low > 0x7fffffff) a->hi += 1;
      a->low <<= 1;
   }
}

void llilsr (
   LLI *a, unsigned int b)     /* logical shift a right b bits */
{
   for (; b>0; b--) {
      a->low >>= 1;
      if (a->hi & 1) a->low += 0x80000000;
      a->hi >>= 1;
   }
}

void llitimes (
   unsigned long a, unsigned long b, LLI *c)
                                /* stores a times b in c */
{
   long long la,lb; 
 
   union { 
     LLI lc; 
     long long llc; 
   } un; 
 
   la=a; 
   lb=b;
   un.llc=la*lb; 
   *c=un.lc;
 
   return; 
}

int llidiv (
   LLI const *dividend, unsigned long divisor,
   unsigned long *quotient, unsigned long *remainder)
                /* division. Returns 0 if OK, 1 if overflow or
                        division by zero. */
{
   return 1; /****/
}

int llicmp (
   const LLI *a, const LLI *b)
                /* returns -1 if a<b, 0 if a==b, +1 if a>b */
{
   if (a->hi > b->hi) return 1;
   if (a->hi < b->hi) return -1;
   if (a->low > b->low) return 1;
   if (a->low < b->low) return -1;
   return 0;
}

void b2lli(
   const void *str, unsigned int len, LLI *a)
                                /* convert char array to lli */
{
   char *p = (char *)a + sizeof(LLI) - len;
   memset(a, 0, sizeof(LLI));
   memcpy(p, str, len);
}

void *lli2b(
   const LLI *a, void *str, unsigned int len)
                                /* convert lli to char array */
{
   char *p = (char *)a + sizeof(LLI) - len;
   memcpy(str, p, len);
   return str;
}
