#ifndef _H_kermap
#define _H_kermap
#define MAXPHYSDEVS 2 /* Max number of physical devices (each has a window) */
#if 0
#define number_of_windows 15+MAXPHYSDEVS
/*** Note: We(sparc) now have two set of kernel mapping windows:
           The first set is the same as before, the 2nd set is used by
	   kernel data copy key */
/*** Note: when changing the above, you must also change KER_INT_ST_MASK0
   in vectors.s */
/* the window numbers are declared in memomdh.h */

#define cpuDataCMMUID 6  /* The ID for the data CMMU for the (only) processor */
/* The instruction CMMU ID is cpuDataCMMUID +1  */
#endif

        /* Bits in the page and segment table entries */

#define PTE_WT 0x200           /* Write through,         */
#define PTE_SP 0x100           /* Supervisor protection, */
#define PTE_G  0x080           /* Global,                */
#define PTE_CI 0x040           /* Cache Inhibit,         */
#define PTE_M  0x010           /* Modified (not seg tbl) */
#define PTE_U  0x008           /* Used (not seg tbl)     */
#define PTE_WP 0x004           /* Write Protect,         */
#define PTE_V  0x001           /* Valid,                 */

struct cmmu_regs {
   unsigned long IDR,
                 SCR,  /* 0x004 System Command Register */
                 SSR;  /* 0x008 System Status Register */
   void *        SAR;  /* 0x00c System Address Register */
   unsigned long fill1[0x3d],
                 SCTR, /* 0x104 System ConTrol Register */
                 PFSR, /* 0x108 P bus Fault Status Register */
                 PFAR, /* 0x10c P bus Fault Address Register */
                 fill2[0x3c],
                 SAPR, /* 0x200 Supervisor Area Pointer Register */
                 UAPR, /* 0x204 User Area Pointer Register */
                 fill3[0x7e],
                 fill4[0x100], /* BATC write ports are in here */
                 CDP[4], /* 0x800 Cache Data Ports */
                 fill5[12],
                 CTP[4], /* 0x840 Cache Tag Ports */
                 fill6[12],
                 CSSP, /* 0x880 Cache Set Status Port */
                 fill7[0x1DF];
};

#include "realkermap.h"
/* On 88100, include realkermap.h */
/* To test, include mackermap.h */

#endif

