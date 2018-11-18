/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ident "@(#)sysdefs.h	1.4 31 Mar 1995 08:09:06 %n%"

#ifndef SYSDEFS_H
#define SYSDEFS_H
/* Special system dependent definitions. */
 
/* This version for the Omron Luna 88K gcc compiler */
 
#define prototypes 1
 
#define KT 0x80000000u
#if !defined(NULL)
#define NULL 0
#endif
 
#include "booleanh.h"
 
extern void crash(const char *message) __attribute__ ((noreturn));
 // Parsimonious version of "crash":
extern void Panic(void);

#define memzero(s,n) Memset(s,0,n)
/* memzero2(s) is like memzero(s,2) where s is known to be even. */
#define memzero2(s) (*(short *)(s) = 0)
/* memzero4(s) is like memzero(s,4) where s is known to be
      a multiple of 4. */
#define memzero4(s) (*(long *)(s) = 0L)
/* The following is used when s and n are known to be multiples of 4. */
#define memzero4n(s,n) {int zzi=(n); char *zzp=(char *)(s); \
      for (; zzi>0; zzi-=4) {          \
         memzero4(zzp); zzp+=4; } }
#endif /* SYSDEFS_H */
