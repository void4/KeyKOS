#ifndef _H_lli
#define _H_lli
/*
  Proprietary Material of Key Logic  COPYRIGHT (c) 1990 Key Logic
*/
 
/* Declarations for 64-bit unsigned integers */
 
typedef struct {unsigned long hi, low;} LLI;
 
extern void lliadd (
   LLI *a, const LLI *b);       /* adds b to a */
extern void llisub (
   LLI *a, const LLI *b);       /* subtracts b from a */
extern void llilsl (
   LLI *a, unsigned int b);     /* logical shift a left b bits */
extern void llilsr (
   LLI *a, unsigned int b);     /* logical shift a right b bits */
extern void llitimes (
   unsigned long a, unsigned long b, LLI *c);
                                /* stores a times b in c */
extern int llidiv (
   LLI const * dividend, unsigned long divisor,
   unsigned long *quotient, unsigned long *remainder);
                /* division. Returns 0 if OK, 1 if overflow or
                        division by zero. */
extern int llicmp (
   const LLI *a, const LLI *b);
                /* returns -1 if a<b, 0 if a==b, +1 if a>b */
void b2lli(
   const void *str, unsigned int len, LLI *a);
                                /* convert char array to lli */
void *lli2b(
   const LLI *a, void *str, unsigned int len);
                                /* convert lli to char array */
#endif
