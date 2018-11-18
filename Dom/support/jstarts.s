/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykosasm.h"
#include "trap.h"
#include "stack.h"

! KEY COMP=0;
! KEY SB=1;
! KEY CALLER=2;
! KEY PSB=4;
! KEY DOMKEY=3;
! KEY DOMCRE=6
! KEY TEMP2=13;
! KEY TEMP1=14;
! KEY TEMP=15;

#define Domain_GetMemory 3
#define Domain_SwapMemory 35
#define VCS_GetBaseSegmentKey 6
#define VCS_Freeze  17
#define DC_Destroy 1
#define DC_DestroyMe 8
#define DESTROY 0x80000004
#define Node_Fetch 0
#define FactoryB_InstallSensory 0
#define FactoryB_InstallFactory 32
#define FactoryB_InstallHole 128
#define FactoryB_MakeRequestor 66


        .seg ".text"

        .type _start, #function
        .global _start

!        .type sbrk, #function
!        .global sbrk
!        .type _sbrk, #function
!        .global _sbrk

!        .type brk, #function
!        .global brk
!        .type _brk, #function
!        .global _brk

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
        .type __freezedry, #function
        .global __freezedry
        .type cursbrk, #object
        .global cursbrk

        .ascii "FACTORY "
        .word   jstart
        .word   title

_start:   /* oc in %i3 and ordinal in %i4 */
        or    %i3, %g0, %o0
        or    %i4, %g0, %o1 /* becomes i0,i1 after factory() does save */
        sethi %hi(cursbrk), %l1
        ld    [%l1 + %lo(cursbrk)], %l2
        add   %l2, 4095, %l2
        srl   %l2, 12, %l2
        sll   %l2, 12, %l2
        st    %l2, [%l1 + %lo(cursbrk)]
        set   0x0FFFFF80,%sp
        call  jstart
        nop

/* return means exit */

        set 0, %o0
exit:
        or  %o0, %g0, %l6
        !"        KALL (DOMKEY,Domain_GetMemory) KEYSTO(TEMP);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL (TEMP,VCS_GetBaseSegmentKey) KEYSTO(TEMP);"
OC(VCS_GetBaseSegmentKey)
XB(0x00F00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL (DOMKEY,Domain_SwapMemory) KEYSFROM(TEMP) KEYSTO(TEMP);"
OC(Domain_SwapMemory)
XB(0x8030F000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL (TEMP, DESTROY) RCTO(rc);"
OC(DESTROY)
XB(0x00F00000)
RC(rc)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)


/*      KALL (DOMCRE, DC_DestroyMe) CHARFROM(R14*4,4,R) KEYSFROM(CALLER,PSB); */

        OC(DC_DestroyMe)
        XB(0xCC602400)
        NB(0x00000000)
        PS2(22*4,4)  /* %l6 */
        cjcc(0,0)

        ta ST_KEYERROR

/*
   freeze memory, install .program in factory, seal and return to caller
   factory Builder key is in K1 (TEMP1)

   caller has done a setjmp to preserve the registers and stack
*/


__freezedry:

        !"        KALL (DOMKEY,Domain_GetMemory) KEYSTO(TEMP);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

        !"        KALL (TEMP,VCS_Freeze) KEYSFROM(PSB) KEYSTO(TEMP);   /* makes my memory read only (no stack) */"
OC(VCS_Freeze)
XB(0x80F04000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)
   /* makes my memory read only (no stack) */

        !"        KALL (TEMP1,FactoryB_InstallFactory+17) KEYSFROM(TEMP) CHARFROM(restartaddr,4) RCTO(rc);"
OC(FactoryB_InstallFactory+17)
PS2(restartaddr,4)
XB(0x84E0F000)
RC(rc)
NB(0x08000000)
cjcc(0x08000000,&_jumpbuf)

        !"        KALL (TEMP1,FactoryB_MakeRequestor) KEYSTO(TEMP) RCTO(rc);"
OC(FactoryB_MakeRequestor)
XB(0x00E00000)
RC(rc)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)


        !"        LDEXBL(CALLER,0) KEYSFROM(TEMP,TEMP1);"
OC(0)
XB(0xC020FE00)

        !"        FORKJUMP();"
fj(0x00000000,&_jumpbuf)


        !"        KALL (DOMCRE,DC_Destroy) KEYSFROM(DOMKEY,PSB);"
OC(DC_Destroy)
XB(0xC0603400)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)


        ta ST_KEYERROR

/*
   Come here for begin of freezedry products
*/
_restart:
        or    %i3, %g0, %o0
        or    %i4, %g0, %o1 /* becomes i0,i1 after main() does save */

        set   altstack,%sp
        call  jstart
        nop
        set 0,%o0
        b    exit
        nop

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

!_brk:
!brk:
!        save   %sp, -SA(MINFRAME), %sp

!        or     %i0, %g0, %i2  ! new value
!        set    cursbrk, %l1
!        ld     [%l1], %i0     ! old value
!        st     %i2, [%l1]

!        set    0xFFFFF000, %l3
!        and    %i0, %l3, %i0
!        and    %i2, %l3, %i2
!        cmp    %i2, %i0
!        ble    brkret
!        or     %i2,%g0,%o0
!        set    0, %o1
!        call   memset
!        sub    %i2, %i0, %o2    ! zero all the pages, sorry
!brkret:
!        set    0,%i0
!        ret
!        restore

!_sbrk:
!sbrk:
!        save   %sp, -SA(MINFRAME), %sp

!        or     %i0, %g0, %i2
!        set    cursbrk, %l1
!        ld     [%l1], %i0
!        add    %i0, %i2, %i2
!        st     %i2, [%l1]        ! i0 has the return value

!        set    0xFFFFF000, %l3
!        and    %i0, %l3, %i0
!        and    %i2, %l3, %i2
!        cmp    %i2, %i0
!        ble    sbrkret
!        or     %i2,%g0,%o0
!        set    0, %o1
!        call   memset
!        sub    %i2, %i0, %o2    ! zero all the pages, sorry

!sbrkret:                        ! i0 still has return value
!        ret
!        restore

crash:
        ta      ST_KEYERROR

	nop

        .type  setjmp, #function
        .global setjmp

setjmp:
        ta      3                ! we may change memories (freeze) and
                                 ! need to have the stack intact
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

       .align 4

restartaddr: .word _restart
errno:
       .word 0
temp:
       .word 0
cursbrk:
       .word _end

       .skip 4000
       .align 8
altstack:
       .skip 200
