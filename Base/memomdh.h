/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define log_context_cnt 8
#define CtxCnt (1<<log_context_cnt)
#define kernCtx (CtxCnt-1)
#define RgnTabCnt CtxCnt /* until we have many contexts per region table */
#define RgnCnt 256
#define SegTabCnt 1024
#define SegCnt 64
#define PagTabCnt 2048
#define BytTabCnt 1 /* useless unless UniAge */
#define kMapCnt 220 /* how may kernel seg & pag tables */
#ifndef _ASM /* To placate assembler */
#include "memoryh.h"
#define UniAge 0
/* UniAge must be a compile time constant. It is 1 if the table
ageing logic is extended to cover pages and 0 if Bill's
older page aging logic is in effect. There are no Xvalid page
table entries unless UniAge. I plan for 0 to
be debugged first and 1 only if there is promise of improved
performance. */

extern ME CtxTabs[1][CtxCnt];

void markpagedirty(CTE *p); /* mark page dirty */
extern unsigned int forsakensegtabcount,
                    forsakenpagtabcount;
void reclaim_forsaken_segment_tables(void);
void reclaim_forsaken_page_tables(void);
void steal_table(CTE *);
void xvalidate_table(CTE *);
void md_unprseg(NODE *node);
void prepare_segment_node(
   NODE *np);  /* node to prepare. Must be unprepared. */
extern void zap_dib_map(
   struct DIB *dib);
extern void zap_depend_entry(
   long locator,
   unsigned short hash);
void checkSegMapFrame(CTE *);
void checkPagMapFrame(CTE *);

/* Fixed allocation of window numbers: */
#define arg_win 0  /* two consecutive windows */
#define parm_win 2 /* two consecutive windows */
#define segtab_win 4
/* 5 unused. */
#define clear_win 6
#define check_win 7
#define pagtab_win 8
#define IOSYSWINDOW 9
#define CKPMIGWINDOW 10
#define CLEANWINDOW1 11
#define KERNELWINDOW 12
#define space_win 13
#define QUICKWINDOW 14  /* Any procedure call may invalidate
                           this window. */
#define first_physdev_win 15 /* There are MAXPHYSDEVS of these */
#define first_reqsense_win (first_physdev_win + MAXPHYSDEVS)
/* window "first_reqsense_win+MAXPHYSDEVS(19)" to 20 is not used now */
#define ESPREG_WINDOW 19 /* Windows for esp reg address */
#define ESPDMA_WINDOW 20 /* Windows for esp DMA reg address  */
#define COPYWINDOW 22	/* Windows for kernel data copy key */
#define COPYWINDOW_MAX 44 /* number of data copy window */
#define TOTAL_MAPWIN_SIZE COPYWINDOW+COPYWINDOW_MAX /* total size */
/* All the above must be less than number_of_windows (in kermap.h) */
/* NOTE: When changing number of window, update number_of_windows
      in kermap.h */

unsigned char *map_any_window(      /* Map bus address for kernel */
          int window,             /* Window number to use */
          unsigned long busaddr,  /* bus address of page to map */
          int rw);                /* Access needed - 1=rw, 0=ro */
uchar * map_uncached_window(int, uint32, int);
   /*Like map_any_window but uncached and 36 bit shifted address */
   /* Returns a pointer to the kernel virtual address of the window */
unsigned char *map_map_window(     /* Map bus address of map frame for kernel */
          int window,             /* Window number to use */
          CTE *cte,               /* CTE of page to map */
          int rw);                /* Access needed - 1=rw, 0=ro */
   /* Returns a pointer to the kernel virtual address of the window */
unsigned char *map_window(         /* Map bus address of non-map for kernel */
          int window,             /* Window number to use */
          CTE *cte,               /* CTE of page to map */
          int rw);                /* Access needed - 1=rw, 0=ro */
   /* Returns a pointer to the kernel virtual address of the window */

#define MAP_WINDOW_RO 0
#define MAP_WINDOW_RW 1

#define NULL_MAP 0 /* Index into super array of maps */

extern void set_memory_management(void);
           /* Sets hardware memory management to map cpudibp's addrs */

CTE *map_parm_page(  /* Find parm page in a virtual address space */
   unsigned long addr,
   struct DIB *dib); /* DIB for domain whose page is to be found */
CTE *map_arg_page(  /* Find arg page in a virtual address space */
   unsigned long addr);
void kernMap(void);

/* The following are only used in a kludge in gatec.c */
extern CTE * thepagecte; /* value returned from resolve_address */

void mark_page_unreferenced(CTE *cte);
const char *find_program_name(void);

extern int PurgeCount;
void PTLB(void);
#endif
