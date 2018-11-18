/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykosasm.h"
#include "trap.h"
#include "stack.h"

! KEY SB=4;
! KEY DOMKEY=3;
! KEY TEMP=13;
! KEY ROOT=14;
! KEY TEMP1=15;

#define LSEGFORMAT 15
#define LSEGDATA 14
#define LSEGBUILD 13
#define LSEGSAVE1 10
#define LSEGSAVE2 11

#define Domain_GetMemory 3
#define Domain_ReplaceMemory 74
#define Domain_Swap 32
#define Domain_Get 0
#define Domain_ClearSPARCOldWindows 212

#define Node_Fetch 0
#define Node_Swap 16
#define Node_MakeNodeKey 35
#define Node_WriteData 45	
#define Node_DataByte 40

#define SB_CreatePage 16
#define SB_CreateNode 0

        .seg ".text"

        .type _start, #function
        .global _start
        .type sbrk, #function
        .global sbrk
        .type _sbrk, #function
        .global _sbrk
        .type _cerror, #function
        .global _cerror
        .type ___errno, #function
        .global ___errno
        .type errno, #object
        .global errno
        .type crash, #function
        .global crash
        .type _mutex_lock, #function
        .global _mutex_lock
        .type _mutex_unlock, #function
        .global _mutex_unlock

        .type exit, #function
        .global exit

        .ascii "FACTORY "
        .word   factory
        .word   title

_start:   /* oc in %i3 and ordinal in %i4 */

        !"        KALL (DOMKEY,Domain_ClearSPARCOldWindows);"
OC(Domain_ClearSPARCOldWindows)
XB(0x00300000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

        or    %g0, %g0, %i6 /* clear frame pointer */

        or    %i3, %g0, %o0
        or    %i4, %g0, %o1 /* becomes i0,i1 after factory() does save */
        call   factory
        nop

/* return means exit */

        set 0, %o0
exit:
        or  %o0, 0, %i2
        !"        KALL (DOMKEY,Domain_GetMemory) KEYSTO(ROOT);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

exitloop:
        !"        KALL (ROOT,Node_DataByte) RCTO(rc);"
OC(Node_DataByte)
XB(0x00E00000)
RC(rc)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)

        tst %o0
        bneg crash
        andcc %o0, 0x0f, %o0
        bz    mighthaveroot
        nop
moreexitloop:
        !"        KALL (ROOT,Node_Fetch+0) KEYSTO(ROOT);"
OC(Node_Fetch+0)
XB(0x00E00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

        ba   exitloop
        nop

mighthaveroot:
        !"        KALL   (ROOT,Node_Fetch+LSEGFORMAT) KEYSTO(TEMP);"
OC(Node_Fetch+LSEGFORMAT)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (TEMP,1) CHARTO(datakey,16) RCTO(rc);"
OC(1)
XB(0x00D00000)
RC(rc)
RS2(datakey,16)
NB(0x0B000000)
cjcc(0x00080000,&_jumpbuf)

        sethi  %hi(datakey+15), %l5
        ldub   [%l5 + %lo(datakey+15)], %l0
        cmp    %l0, 0xA5
        bne    moreexitloop               /* probably debugging red node, loop some more */
        nop

haveroot:
        !"        KALL (ROOT,Node_Fetch+LSEGBUILD) KEYSTO(TEMP);"
OC(Node_Fetch+LSEGBUILD)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

        set  8, %i0  /* destruction */
        !"        KALL (DOMKEY,Domain_ReplaceMemory) CHARFROM(buildstart,4) KEYSFROM(TEMP);"
OC(Domain_ReplaceMemory)
PS2(buildstart,4)
XB(0x8430D000)
NB(0x00000000)
cjcc(0x08000000,&_jumpbuf)


        ta     ST_KEYERROR

___errno:
        sethi  %hi(errno), %o0
        retl
        or     %o0, %lo(errno), %o0

_cerror:
        sethi  %hi(errno), %g1
        st     %o0, [%g1 + %lo(errno)]
        retl
        mov    -1, %o0

_mutex_lock:
_mutex_unlock:
        retl
        set 0, %o0

_sbrk:
sbrk:

/*******************************************************************************/
/*          This works ONLY if requests are for page sized increments          */
/*     Memory expands up to 0x00900000 (the stack segment)                     */
/*     Assumes initial brk address is on a page boundary and page is missing   */
/*******************************************************************************/

        save   %sp, -SA(MINFRAME+128), %sp

        !"        KALL   (DOMKEY,Domain_Swap+11) KEYSFROM(ROOT);"
OC(Domain_Swap+11)
XB(0x8030E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (DOMKEY,Domain_GetMemory) KEYSTO(ROOT);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)


/*   Someone may have expanded the memory tree.  We assume that no matter       */
/*   how large the address space is, the original lsf is down there somewhere   */
/*   defining the lowest 1 meg of memory.  The KeyNIX keeper runs in an lss 7   */
/*   node to get 256 megabyte windows but the first 256 meg slot is only 16 meg */
/*   and is the original lsf (one node down) */

getrootloop:
        !"        KALL   (ROOT,Node_DataByte) RCTO(rc);"
OC(Node_DataByte)
XB(0x00E00000)
RC(rc)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)

        tst    %o0
        bneg   cant
        andcc  %o0, 0x0f, %o0               /* isolate databyte */
        bz     mighthavelsf                      /* got it */
        nop
morerootloop:
        !"        KALL   (ROOT,Node_Fetch+0) KEYSTO(ROOT);"
OC(Node_Fetch+0)
XB(0x00E00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

        ba     getrootloop
        nop

mighthavelsf:
        !"        KALL   (ROOT,Node_Fetch+LSEGFORMAT) KEYSTO(TEMP);"
OC(Node_Fetch+LSEGFORMAT)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (TEMP,1) CHARTO(datakey,16) RCTO(rc);"
OC(1)
XB(0x00D00000)
RC(rc)
RS2(datakey,16)
NB(0x0B000000)
cjcc(0x00080000,&_jumpbuf)

        sethi  %hi(datakey+15), %l5
        ldub   [%l5 + %lo(datakey+15)], %l0
        cmp    %l0, 0xA5
        bne    morerootloop               /* probably debugging red node, loop some more */
        nop

havelsf:                                    /* found the lsf */
        !"        KALL   (ROOT,Node_Swap+LSEGSAVE1) KEYSFROM(TEMP);"
OC(Node_Swap+LSEGSAVE1)
XB(0x80E0D000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (ROOT,Node_Swap+LSEGSAVE2) KEYSFROM(TEMP1); /* save some keys */"
OC(Node_Swap+LSEGSAVE2)
XB(0x80E0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* save some keys */

        !"        KALL   (ROOT,Node_Fetch+LSEGDATA) KEYSTO(TEMP);"
OC(Node_Fetch+LSEGDATA)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (TEMP,1) CHARTO(datakey,16) RCTO(rc);     /* get current brk address */"
OC(1)
XB(0x00D00000)
RC(rc)
RS2(datakey,16)
NB(0x0B000000)
cjcc(0x00080000,&_jumpbuf)
     /* get current brk address */
        sethi   %hi(datakey+12), %l5
        ld     [%l5 + %lo(datakey+12)], %l0
        mov    %l0, %l6                                  /* for return of current */

        cmp    %l0, -1
        be     nospace
        nop

        tst    %i0
        bnz    expand
        nop
          or   %l6, %g0, %i0                 /* current value is return value */
          bz   sbrkreturn                    /* and new value for that matter */
          nop
expand:
        add    %i0, 4095, %i0
        srl    %i0, 12, %i0                  /* # pages to expand */

/* %i0 has the number of pages to expand                                */
/* %l0 has the current brk address (address of first non existant page) */

addloop:
        srl    %l0, 20, %l1
        and    %l1, 0xf, %l1                 /* megabyte slot # */
        srl    %l0, 16, %l2
        and    %l2, 0xf, %l2                 /* 64K slot # */
        srl    %l0, 12, %l3
        and    %l3, 0xf, %l3                 /* page slot # */

/* %l1 %l2 %l3 have slot numbers of page to allocate */

        cmp    %l1, 9                        /* leave stack alone */
        bge    nospace
        nop
getmeg:                               /* get lss 4 node from root node */
        add    %l1, Node_Fetch, %o0   /* OC(%l1+ Node_Fetch) */
        !"        KALL  (ROOT, %o0) KEYSTO(TEMP);"
OCR(%o0)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)


get64:
        add    %l2, Node_Fetch, %o0   /* OC(%l2+Node_Fetch) */
        !"        KALL (TEMP, %o0) KEYSTO(TEMP1) RCTO(zzz);"
OCR(%o0)
XB(0x00D00000)
RC(zzz)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)

        tst %o0
        bz getpage
        nop             /* oops there is no lss 4 node here */
          !"          KALL (SB,SB_CreateNode) KEYSTO(TEMP) RCTO(rc);   /* must get 1 meg node */"
OC(SB_CreateNode)
XB(0x00400000)
RC(rc)
NB(0x0880D000)
cjcc(0x00000000,&_jumpbuf)
   /* must get 1 meg node */
          tst %o0
          bnz nospace
          nop
          !"          KALL (TEMP,Node_MakeNodeKey) CHARFROM(db4,1) KEYSTO(TEMP);"
OC(Node_MakeNodeKey)
PS2(db4,1)
XB(0x04D00000)
NB(0x0080D000)
cjcc(0x08000000,&_jumpbuf)

          add  %l1, Node_Swap, %o0   /* OC(%l2+Node_Swap) */
          !"          KALL (ROOT, %o0) KEYSFROM(TEMP);"
OCR(%o0)
XB(0x80E0D000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

          ba   get64                         /* repeat get of 64K node	*/
          nop
getpage:
        !"        KALL   (TEMP1,0) RCTO(rc);           /* need to test 64K node */"
OC(0)
XB(0x00F00000)
RC(rc)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)
           /* need to test 64K node */
        tst %o0
        bz  nodeok
        nop
          !"          KALL (SB,SB_CreateNode) KEYSTO(TEMP1) RCTO(rc);  /* must get 64K node */"
OC(SB_CreateNode)
XB(0x00400000)
RC(rc)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)
  /* must get 64K node */
          tst  %o0
          bnz  nospace
          nop
          !"          KALL (TEMP1,Node_MakeNodeKey) CHARFROM(db3,1) KEYSTO(TEMP1);"
OC(Node_MakeNodeKey)
PS2(db3,1)
XB(0x04F00000)
NB(0x0080F000)
cjcc(0x08000000,&_jumpbuf)

          add   %l2, Node_Swap, %o0
          !"          KALL (TEMP, %o0) KEYSFROM(TEMP1);"
OCR(%o0)
XB(0x80D0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

nodeok:
        !"        KALL (SB,SB_CreatePage) KEYSTO(TEMP); /* page in TEMP */"
OC(SB_CreatePage)
XB(0x00400000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)
 /* page in TEMP */
        add  %l3, Node_Swap, %o0
        !"        KALL (TEMP1, %o0) KEYSFROM(TEMP);"
OCR(%o0)
XB(0x80F0D000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)


        add   %l0, 4095, %l0
        inc   %l0                            /* new brk address */
        deccc %i0                            /* number done */
        bnz   addloop
        nop

sbrkreturn:   /* l0 has the new value (or current value if no increment ) */

        st    %l0, [%l5 + %lo(datakey+12)]

        !"        KALL   (ROOT,Node_WriteData) CHARFROM(nodedata,24);  /* update highest address */"
OC(Node_WriteData)
PS2(nodedata,24)
XB(0x04E00000)
NB(0x00000000)
cjcc(0x08000000,&_jumpbuf)
  /* update highest address */
        !"        KALL   (ROOT,Node_Fetch+LSEGSAVE1) KEYSTO(TEMP);"
OC(Node_Fetch+LSEGSAVE1)
XB(0x00E00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL   (ROOT,Node_Fetch+LSEGSAVE2) KEYSTO(TEMP1);"
OC(Node_Fetch+LSEGSAVE2)
XB(0x00E00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

shortret:
        !"        KALL   (DOMKEY,Domain_Get+11) KEYSTO(ROOT);       /* restore saved keys */"
OC(Domain_Get+11)
XB(0x00300000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)
       /* restore saved keys */

        or     %l6, %g0, %i0      /* always return old value */
        ret
        restore

nospace:
        sethi   %hi(errno), %l4
        set    12, %l3                       /* ENOMEM */
        st     %l3, [%l4 + %lo(errno)]
        set    -1, %l6
        set    -1, %l0                       /* flag for future */
        ba     sbrkreturn
        nop
cant:
        sethi   %hi(errno), %l4
        set    12, %l3                       /* ENOMEM */
        st     %l3, [%l4 + %lo(errno)]
        set    -1, %l6
        ba     shortret
        nop

crash:
        ta      ST_KEYERROR

	nop

        .type  setjmp, #function
        .global setjmp

setjmp:
        clr     [%o0]
        st      %sp, [%o0 + 4]
        add     %o7, 8, %o1
        st      %o1, [%o0 + 8]
        st      %fp, [%o0 + 12]
        st      %i7, [%o0 + 16]

        std     %i0, [%o0 + 24]
        std     %i2, [%o0 + 32]
        std     %i4, [%o0 + 40]
        std     %i6, [%o0 + 48]

        std     %l0, [%o0 + 56]
        std     %l2, [%o0 + 64]
        std     %l4, [%o0 + 72]
        std     %l6, [%o0 + 80]

        retl
        mov     %g0, %o0

        .type longjmp, #function
        .global longjmp
longjmp:
        ta      3

        ldd     [%o0 + 24], %i0
        ldd     [%o0 + 32], %i2
        ldd     [%o0 + 40], %i4
        ldd     [%o0 + 48], %i6

        ldd     [%o0 + 56], %l0
        ldd     [%o0 + 64], %l2
        ldd     [%o0 + 72], %l4
        ldd     [%o0 + 80], %l6

        ld      [%o0 + 4], %sp
        ld      [%o0 + 8], %o7
        ld      [%o0 + 12], %fp
        ld      [%o0 + 16], %i7

        tst     %o1
        bne     1f
        sub     %o7, 8, %o7
        mov     1, %o1
1:      retl
        mov     %o1, %o0


        .seg ".data"

db4:    .byte 4
db3:    .byte 3
       .align 4
nodedata:
       .word LSEGDATA
       .word LSEGDATA
datakey:
       .word 0,0,0,0
buildstart:
       .word  0xAC    /* start address for builder/destroyer */
errno:
       .word 0
