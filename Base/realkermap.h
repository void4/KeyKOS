/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keyh.h" /* define pagesize */

#if 0
#define KER_INT_ST_MASK0 ((volatile unsigned long *) \
    window_address(1+number_of_windows))
    /* 1+ above to leave an unused page to help catch addressing errors */
#define KER_OBIO_CLOCK0  ((volatile unsigned long *) \
    ((char *)KER_INT_ST_MASK0 +pagesize) )
#define KER_NVRAM_ADDR ((volatile unsigned char *)(KER_OBIO_CLOCK0)+pagesize)
#define KER_SPC_ADDR ((volatile struct SPC *)(KER_NVRAM_ADDR+0x2000))
#define KER_OBIO_SIO_ADDR ((volatile unsigned char *) \
    ((char *)KER_SPC_ADDR+pagesize))
#define KER_PAGE_TABLE ((volatile unsigned long *)0x7d000)
#define window_page_table (KER_PAGE_TABLE+_wpn)

#define CMMUs ((volatile struct cmmu_regs *)0xfff00000)
/* For example, CMMUs[i].SAR is the SAR of the  CMMU with ID i */
#endif

/* These constants are known in src/Mapfile */
#define traptable 0xf8000000
/* I think that this is current with the following e-mail:
From dcc@oes.amdahl.com Fri Mar 10 13:41 PST 1995
Date: Fri, 10 Mar 1995 13:41:07 +0800
*/

#define vWindows ((uchar *)0xf8004000)
#define window_address(window_number) \
   ((unsigned char *)(vWindows+(window_number*pagesize)))

/* The following stuff is for debugging. */
/* N.B. Assembler code uses absolute bit numbers to access gatelogenable */
/* and counters.  Don't try to move them */
struct logenableflags {
   unsigned gatelogenable :1;	/* See Note Above */
   unsigned counters:1;		/* See Note - enable instru/cycle counters */
   unsigned iologenable :1;
   unsigned getlogenable :1;
   unsigned pixelfetchdisable :1;
   unsigned prtckpt :1;
   unsigned timeless :1;	/* don't do scheduling */
   unsigned noclock :1;		/* no take clock interrupts */
   unsigned logbuffered :1;
   unsigned trustmem:1;         /* suppress memory map integrity checking */
   unsigned bootcpu:5;		/* used to redirect interrupts */
};
extern struct logenableflags lowcoreflags;

