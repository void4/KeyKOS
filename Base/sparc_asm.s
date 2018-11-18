/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "asm_linkage.h"
#include "trap.h"
#include "stack.h"
#include "psr.h"
#include "misc.h"
#include "reg.h"

#define PSR_PIL_BIT 8

        ENTRY(splhi)
	b	splr
	mov	(0xf << PSR_PIL_BIT), %o0
        SET_SIZE(splhi)

        ENTRY(splx)
	rd      %psr, %o4               ! get old PSR
	and     %o0, PSR_PIL, %o2       ! psr delay - mask off argument
	andn    %o4, PSR_PIL, %o3       ! psr delay - clear PIL from old PSR
	nop
	wr      %o3, %o2, %psr
	nop                             ! psr delay
	retl                            ! psr delay
	mov     %o4, %o0                ! psr delay - return old PSR
        SET_SIZE(splx)

	! splr is like splx but will only raise the priority
        ENTRY(splr)
	rd      %psr, %o4               ! get old PSR
	and     %o0, PSR_PIL, %o2       ! mask off argument
	and	%o4, PSR_PIL, %o5

	! if old PIL is greater than new PIL, use it instead
	cmp	%o5, %o2
	bg,a	1f
	mov	%o5, %o2
1:
	andn    %o4, PSR_PIL, %o3       ! clear PIL from old PSR
	wr      %o3, %o2, %psr
	nop                             ! psr delay
	retl                            ! psr delay
	mov     %o4, %o0                ! psr delay - return old PSR
        SET_SIZE(splr)

        ENTRY(splclock)
	b	splr
	mov	(10 << PSR_PIL_BIT), %o0
        SET_SIZE(splclock)

        ENTRY(splclockon)
	b	splx
	mov	(9 << PSR_PIL_BIT), %o0
        SET_SIZE(splclockon)

        ENTRY(scsi_intr_on)
	b	splx
	mov	(3 << PSR_PIL_BIT), %o0
        SET_SIZE(scsi_intr_on)

        ENTRY(spltty)
	b	splr
	mov	(12 << PSR_PIL_BIT), %o0
        SET_SIZE(spltty)

	ENTRY(idlefunction)
	b	idlefunction
	nop
	SET_SIZE(idlefunction)


#include "memomdh.h"
.common CtxTabs,CtxCnt*4,4096
.common RgnTabs,RgnTabCnt*256*4,4096
.common SegTabs,SegTabCnt*64*4,4096
.common PagTabs,PagTabCnt*64*4,4096
.common CpuArgPage,4096,4096
.common kRgnT,1024,4096
.common kMapT,kMapCnt*256,4096
#define sizeofMapHeader 32
.global HeaderZero,RgnHeaders,SegHeaders,PagHeaders
.reserve HeaderZero,(1+RgnTabCnt+SegTabCnt+PagTabCnt)*sizeofMapHeader,".bss",16
RgnHeaders = HeaderZero +         1*sizeofMapHeader
SegHeaders = RgnHeaders + RgnTabCnt*sizeofMapHeader
PagHeaders = SegHeaders + SegTabCnt*sizeofMapHeader
! The four lines above cause the four arrays of MapHeaders to appear
! also as one array in which HeaderZero is element zero.

.global	sta03,sta04,lda03,lda04,lda06,lda20,sta20,cpuargpage
sta03: retl
   sta %o1,[%o0]3
sta04: retl
   sta %o1,[%o0]4
sta20: retl
   sta %o1,[%o0]0x20
lda03: retl
  lda [%o0]3,%o0
lda04: retl
  lda [%o0]4,%o0
lda06: retl
  lda [%o0]6,%o0
lda20: retl
  lda [%o0]0x20, %o0
     .data
cpuargpage:
.word CpuArgPage
