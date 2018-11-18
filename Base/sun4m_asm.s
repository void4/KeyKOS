/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "asm_linkage.h"
#include "trap.h"
#include "stack.h"
#include "psr.h"
#include "misc.h"
#include "reg.h"
#include "intreg.h"
#include "assym.h"
#if defined(viking)
#include "supersparc.h"
#endif


#define IR_MASK_OFFSET 0x4
#define IR_CLEAR_OFFSET 0x8
/*
 * Turn on or off bits in the system interrupt mask register.
 * no need to lock out interrupts, since set/clr softint is atomic.
 * === sipr as specified in 15dec89 sun4m arch spec, sec 5.7.3.2
 *
 * set_intmask(bit, which)
 *      int bit;                bit mask in interrupt mask
 *      int which;              0 = clear_mask (enable), 1 = set_mask (disable)
 */
        ENTRY_NP(set_intmask)
        mov     %psr, %g2
        or      %g2, PSR_PIL, %g1       ! spl hi to protect intreg update
        mov     %g1, %psr
        nop;nop;nop
        tst     %o1
        set     v_sipr_addr, %o5
        ld      [%o5], %o5
        set     IR_CLEAR_OFFSET, %o3
        bnz,a   1f
        add     %o3, 4, %o3             ! bump to "set mask"
1:
        st      %o0, [%o5 + %o3]        ! set/clear interrupt
        ld      [%o5 + IR_MASK_OFFSET], %o1     ! make sure mask bit set
	mov	%g2, %psr
	nop
	retl
	nop
        SET_SIZE(set_intmask)

        ENTRY_NP(set_itr)
	sethi	%hi(v_sipr_addr), %o1
	ld	[%o1 + %lo(v_sipr_addr)], %o1
	retl
	st	%o0, [%o1 + IR_SET_ITR]
        SET_SIZE(set_itr)


#if defined(viking)
/* Code for keeping track of cycle and instruction counts on the
 * Viking.
 */

#define SSPM_COUNTER_IPL 9

#define INST_COUNT_ADDR(addr) \
	set	cpu_inst_count, addr
#define CYCLE_COUNT_ADDR(addr) \
	set	cpu_cycle_count, addr

/*
 * void sspm_counter_init_asm(sspm_cpu_count *cpu_count)
 *
 * Initialize the per cpu instruction and cycle counters.
 *
 * When running the counters impose a small, but non-zero, run time overhead -
 * somewhere between 1 and 3 fairly fast interrupts per millisecond.  Each
 * interrupt takes around 35 cycles plus cache misses.  Thus the total overhead
 * is perhaps 0.2%.
 */
	ENTRY(sspm_counter_init_asm)
	!
	! Set up all the necessary hardware registers.
	!
	lda	[%g0]ASI_MCTRC, %o0	! freeze the counters
	set	MCTRC_ICNTEN | MCTRC_CCNTEN, %o1
	bclr	%o1, %o0
	sta	%o0, [%g0]ASI_MCTRC

	sta	%g0, [%g0]ASI_MCTRV	 ! clear the counters

	lda	[%g0]ASI_MCTRS, %o0	 ! clear the counter interrupt status
	set	MCTRS_ZICIS | MCTRS_ZCCIS, %o1
	bclr	%o1, %o0
	sta	%o0, [%g0]ASI_MCTRS

	lda	[%g0]ASI_MBAR, %o0	 ! enable counter interrupts
	set	MBAR_BCIPL_MASK, %o1
	bclr	%o1, %o0
	set	MBAR_BCIPL(SSPM_COUNTER_IPL) | MBAR_IEN_ZIC | MBAR_IEN_ZCC, %o1
	bset	%o1, %o0
	sta	%o0, [%g0]ASI_MBAR

	lda	[%g0]ASI_MCTRC, %o0	 ! unfreeze the counters
	set	MCTRC_ICNTEN | MCTRC_CCNTEN, %o1
	bset	%o1, %o0
	sta	%o0, [%g0]ASI_MCTRC

	retl
	nop
	SET_SIZE(sspm_counter_init_asm)

/*
 * void sspm_counter_fini_asm(sspm_cpu_count *cpu_count)
 *
 * Deinitialize the per cpu instruction and cycle counters.
 */
	ENTRY(sspm_counter_fini_asm)
	!
	! Turn the counters off.
	!
	lda	[%g0]ASI_MCTRC, %o0
	set	MCTRC_ICNTEN | MCTRC_CCNTEN, %o1
	bclr	%o1, %o0
	sta	%o0, [%g0]ASI_MCTRC

	retl
	nop
	SET_SIZE(sspm_counter_fini_asm)

/*
 * sspm_breakpoint_intr.  Viking breakpoint fast trap handler.
 */
	ENTRY(sspm_breakpoint_intr)
	mov	%psr, %l0
	!-
	lda	[%g0]ASI_MCTRS, %l6	! counter overflow status in %l6
	!-
	btst	MCTRS_ZICIS, %l6	! instruction counter overflow?
	bnz	icount_overflow
	!-
	nop
	btst	MCTRS_ZCCIS, %l6	! cycle counter overflow?
	bnz	ccount_overflow
	!-
	nop

	! Keep track of how many times we are interrupted and no counter
	! interrupt is pending.  This is quite normal since there might be
	! other interrupts at the same level.  However there also appears to be
	! a bug in the cycle counter hardware that can cause this to occur,
	! hence we are interested in keeping count of how often this happens.

	set	sspm_chain_count, %l6	! address of counter
	ld	[%l6], %l7		! update count
	inc	%l7			
	st	%l7, [%l6]

	! ignore it
	mov %l0, %psr
	nop
	jmp	%l1
	rett	%l2
#if 0
	set	indirect_sys_trap, %l3	! branch to generic trap handler
	jmp	%l3
	mov	%l0, %psr		! (delay slot) restore %psr for handler
#endif

icount_overflow:
	INST_COUNT_ADDR(%l7)			! get inst counter addr
	ldd	[%l7], %l4			! get count in %l4 %l5
	!-
	set	MCTRV_ICNT_LIMIT, %l3		! insn. per overflow in %l3
	addcc	%l5, %l3, %l5			! inc. low word
	!-
	addx	%l4, 0, %l4			! inc. high word if required
	btst	MCTRS_ZCCIS, %l6		! cycle counter overflow also?
	bz	count_out
	!-
	std	%l4, [%l7]			! (delay slot) store updated
						! count
	!-

ccount_overflow:
	CYCLE_COUNT_ADDR(%l7)			! get cycle counter addr
	ldd	[%l7], %l4			! get count in %l4 %l5
	!-
	set	MCTRV_CCNT_LIMIT, %l3		! cycles per overflow in %l3
	addcc	%l5, %l3, %l5			! inc. low word
	!-
	addx	%l4, 0, %l4			! inc. high word if required
	!-
	std	%l4, [%l7]			! store updated count
	!-

count_out:
	mov	%l0, %psr			! restore psr_cc
	nop; nop
#if 0
	!-
	lda	[%g0]ASI_MCTRV, %l4		! (psr delay) read hw counter
	!-
	st	%l4, [%l7 + OLD_MCTRV]		! (psr delay) old counter value
						! used later to detect parallax
#endif
	jmp	%l1				! (psr delay) return to %pc
	!-
	rett	%l2
	!-
	SET_SIZE(sspm_breakpoint_intr)

	ENTRY(get_cycle_count)
	lda	[%g0]ASI_MCTRV, %o3		! read hardware counter ASAP
	CYCLE_COUNT_ADDR(%o0)			! address of cycle counter
	set	MCTRV_CCNT_LIMIT - 1, %o2	! mask for cycle count in %o2
	ldd	[%o0], %o0			! sw cycle count in %o0
	and	%o3, %o2, %o2			! hw cycle count in %o2
	set	MCTRV_CCNT_LIMIT, %o3		! cycle limit
	sub	%o3, %o2, %o2			! cycles spent since last intr
	addcc	%o1, %o2, %o1			! add hw and sw counts
	retl
	addx	%o0, 0, %o0
	SET_SIZE(get_cycle_count)

	ENTRY(get_inst_count)
	lda	[%g0]ASI_MCTRV, %o3		! read hardware counter ASAP
	INST_COUNT_ADDR(%o0)			! address of inst counter
	ldd	[%o0], %o0			! sw instruction count in %o0
	srl	%o3, MCTRV_ICNT_SHIFT, %o2	! hw instruction count in %o2
	set	MCTRV_ICNT_LIMIT, %o3		! instruction limit
	sub	%o3, %o2, %o2			! inst's spent since last intr
	addcc	%o1, %o2, %o1			! add hw and sw counts
	retl
	addx	%o0, 0, %o0
	SET_SIZE(get_inst_count)

	.global	sspm_chain_count
.reserve sspm_chain_count, 4, ".data", 4

#endif /* viking */
