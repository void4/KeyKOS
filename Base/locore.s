/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Debugging levels:
   0 - No debugging code
   1 - Interrupts are enabled (at level 15) to enable Omak to catch traps in
       certain kernel routines
   2 - Certain kernel trap routines run extra code to ensure that they are
       running in WIM-valid windows.  This code allows Omak to "go" after a
       breakpoint in these routines.
*/
#define DEBUG 0

/* Scheduler levels:
   0 - Scheduler updates time used and priority every domain switch
   1 - Scheduler updates time used but not priority every domain switch
   2 - "Neanderthal" Scheduler - time and priority only updated during clock
       interrupt
*/
#define SCHEDULER 2

#include "asm_linkage.h"
#include "trap.h"
#include "stack.h"
#include "psr.h"
#include "fsr.h"
#include "misc.h"
#include "reg.h"
#include "intreg.h"
#include "assym.h"
#include "memomdh.h"
#include "supersparc.h"
#include "mmu.h"
#include "sparc_domdefs.h"

! The following defines should be move to a centralized location at some
! point.


/* offset of level10 limit register from v_level10clk_addr */
#define CTR_LIMIT10 0x0
#define CTR_COUNT10 0x4

! FIX with correct values
#define DMT_VALID_BIT 0x80000000

! Call with d being the register which holds the address of the domain's DIB.
!
! Saves the globals in dib->regs.

#define PUTAWAY_GLOBALS(d) \
        st      %g1, [d + 4]; 					\
        std     %g2, [d + 8]; 					\
        std     %g4, [d + 16];					\
        std     %g6, [d + 24]; 					\
        mov     %y, %g1; 					\
        st      %g1, [d];

! Call with d being the register which holds the address of the domain's DIB.
!
! Saves the domain`s outs (this window's ins) in dib->regs.
!  also saves dib->psr, dib->pc, and dib->npc
!  then saves the active windows in dib->backset.  Sets dib->backalloc and
!  dib->backdiboldest.
! Assumes dib->backmax >= nwin_minus_one.  One and only one WIM invalid
!  register.  %g1 = psr.
! Returns with the current window being WIM invalid-1 (a restore will fault).
!  %g1, %g6 unchanged, %g7 = d
! Uses local labels 1 and 2.  N.B. RETURNS IN A DIFFERENT WINDOW!

#define PUTAWAY_WINDOWS(d) \
	st	%l0, [d + DIB_PSR];				\
	st	%l1, [d + DIB_PC];				\
	st	%l2, [d + DIB_NPC];				\
	mov	d, %g7;						\
								\
	std	%i0, [d + 32];					\
	std	%i2, [d + 40];					\
	std	%i4, [d + 48];					\
	std	%i6, [d + 56];					\
								\
	/* Calculate %g2=current window mask, %g4=hi window mask */	\
	sethi	%hi(nwin_minus_one), %g4;			\
	mov	1,%g2;						\
	ld	[%g4 + %lo(nwin_minus_one)], %g4;		\
	mov	%wim, %g3;					\
	sll	%g2, %g4, %g4;					\
	sll	%g2, %g1, %g2;					\
								\
	/* Get place to save first window */			\
	add	%g7, DIB_BACKSET + 16*64, %g5; 			\
								\
	/* Registers: %g1=psr, %g2=current window mask, %g3=wim */	\
        /*    %g4=max window mask, %g5=dib->backset[n] */	\
								\
1:	cmp	%g2, %g4;	/* In highest window? */	\
	bne	2f;		/* No - Go */			\
	sll	%g2, 1, %g2;	/* Mask of previous window */	\
	set	1,%g2;	 	/* Mask of 1st window */	\
2:	btst	%g2, %g3;	/* Is previous window valid? */	\
	bnz	2f;		/* No - Done saving windows */	\
	nop;							\
	restore;		/* to previous window */	\
	SAVE_WINDOW(%g5);					\
	ba	1b;						\
	  sub	%g5, RWINSIZE, %g5; 				\
								\
	/* Set dib->backdiboldest and dib->backalloc */		\
2:	set	16, %l0;					\
	stb	%l0, [%g7 + DIB_BACKALLOC];			\
								\
	add	%g7, DIB_BACKSET-RWINSIZE, %l1; /* Calc oldest */	\
	sub	%g5, %l1, %l1;	/* %g5 is last_used-RWINSIZE */	\
	srl	%l1, 6, %l1;	/* Convert to index */		\
	stb	%l1, [%g7 + DIB_BACKDIBOLDEST];



! Call with d being the register which holds the address of the domain's DIB.
!
! Saves the globals and domain`s outs (this window's ins) in dib->regs.
!  also saves dib->psr, dib->pc, and dib->npc
!  then saves the active windows in dib->backset.  Sets dib->backalloc and
!  dib->backdiboldest.
! Assumes dib->backmax >= nwin_minus_one.  One and only one WIM invalid
!  register.  %l0 has the psr at trap.
! Returns with the current window being WIM invalid-1 (a restore will fault).
!  %g1 = psr at entry, %g7 = d
! Uses local labels 1 and 2.  N.B. RETURNS IN A DIFFERENT WINDOW!

#define SAVE_REGS(d) \
	PUTAWAY_GLOBALS(d) 					\
								\
	mov	%l0, %g1;					\
	PUTAWAY_WINDOWS(d)



! Set up kernel stack and enable for level 9 and above

#define SETUP_KERNEL_STACK \
	/* Set up kernel stack */					\
	set	kernel_stack+KERNSTKSZ-SA(MINFRAME + REGSIZE), %sp;	\
	mov	0, %fp;							\
	mov	%psr, %l0;						\
	andn	%l0, PSR_ET, %l3;	/* debug */			\
	mov	%l3, %psr;		/* debug */			\
	set	PSR_PIL+PSR_ET+PSR_EF, %l4;/* Clr PIL, disable FP+traps */\
	andn	%l0, %l4, %l3;						\
	set	0x00000800, %l4;	/* PIL=8 allows level 9 */	\
	or	%l3, %l4, %l3;						\
	mov	%l3, %psr;						\
	wr	%l3, PSR_ET, %psr;					\
	nop;nop;nop



! Call with d being the register which holds the address of the domain DIB
!  and %l0 has the psr at trap.
! Saves the domain's registers, sets up a kernel stack and enables 
!  interrupts level 10 and above.  N.B. RETURNS IN A DIFFERENT WINDOW!
! Uses local labels 1 and 2

#define SWITCH_TO_KERNEL(d)						\
	/* enable clock traps (at level 10) */				\
	SAVE_REGS(d);							\
									\
	mov	kernCtx, %l3;	/* Set the context number */		\
	mov	RMMU_CTX_REG, %l4;					\
	sta	%l3, [%l4]ASI_RMMU;					\
	SETUP_KERNEL_STACK



! Assumes dib->backmax >= nwin_minus_one.  Sets wim to one and only one WIM 
!  invalid register.  %g7 points to the domain's dib.  dib->psr has
!  interrupt level 0 and interrupts disabled, current psr has ET on
! Restores the back windows from the dib to the hardware windows.  Then
!  restores the globals and domain`s outs in dib->regs.
! Returns with the current window being the window to run the domain from.
!  %l0=the psr, %l1=PC, %l2=nPC.  psr.icc is the domain's condition code.
! Uses local labels 1 and 2

#define RESTORE_REGS							\
	mov	%psr, %l0;		/* %g3=psr window=invalid */	\
	andn	%l0, PSR_ET, %l0;	/* Turn interrupt enable off */	\
	mov	%l0, %psr;		/* Disable interrupts */	\
	set	1, %l1;			/* psr delay */			\
	sll	%l1, %l0, %g3;		/* psr delay, new wim */	\
	mov	%g0, %wim;		/* psr delay, wim=0 */		\
	ldub	[%g7 + DIB_BACKDIBOLDEST], %g5;				\
	sll	%g5, 6, %g5;		/* g5 has backdiboldest * 64 */	\
	ldub	[%g7 + DIB_BACKALLOC], %g6;				\
	sll	%g6, 6, %g6;		/* g6 has backalloc*64 */	\
	add	%g7, DIB_BACKSET, %l0;					\
	add	%l0, %g5, %g5;		/* g5 has addr of oldest */	\
	add	%l0, %g6, %g6;		/* g6 has addr of current */	\
	add	%g7, DIB_BACKSET+31*64, %g4;	/* g4 has wrap point */	\
	save;				/* To 1st valid window */	\
									\
1:	RESTORE_WINDOW(%g5);						\
	save;								\
	cmp	%g5, %g6;		/* Restored current? */		\
	be	2f;			/* Yes - Go */			\
	cmp	%g5,%g4;		/* Reached wrap point? */	\
	bne	1b;			/* No - Go */			\
	add	%g5, RWINSIZE, %g5;					\
	ba	1b;							\
	add	%g7, DIB_BACKSET, %g5;					\
									\
2:	mov	%g3, %wim;		/* Window at entry invalid */	\
	ld	[%g7 + DIB_PSR], %l0;					\
	ld	[%g7 + DIB_PC], %l1;					\
	ld	[%g7 + DIB_NPC], %l2;					\
	mov	%psr, %l3;		/* Get current cwp */		\
	ldd	[%g7 + 32], %i0;					\
	and	%l3, PSR_CWP, %l3;					\
	ldd	[%g7 + 40], %i2;					\
	andn	%l0, PSR_CWP, %l4;					\
	ldd	[%g7 + 48], %i4;					\
	or	%l3, %l4, %l0;		/* psr to load */		\
	ldd	[%g7 + 56], %i6;					\
	ld      [%g7], %g1;						\
        mov     %g1, %y;						\
	mov	%l0, %psr;		/* Set PIL=0, S=1, ET=0 */	\
        ld      [%g7 + 4], %g1;						\
        ldd     [%g7 + 8], %g2;						\
        ldd     [%g7 + 16], %g4;					\
        ldd     [%g7 + 24], %g6;


! KERNEL_CYCLES stops charging the cpudibp domain for domain cycles and
! instructions and starts charging it for kernel instructions/cycles.

! Assumes called soon after an interrupt so %l3 - %l7 are available

! Since called soon after an interrupt of a domain, we will ignore 
! pending counter interrupts, as they won't bias the numbers too much.
#if 1 /* NHxx*/
#define KERNEL_CYCLES /* NHxx */
#else /* NHxx */
#define KERNEL_CYCLES \
	/* Get the cycle and instruction counts */			\
	sethi	%hi(lowcoreflags), %l7;					\
	ldub	[%l7 + %lo(lowcoreflags)], %l7;				\
	btst	0x40, %l7;			/* counters == 1? */	\
	bz	1f;				/* No - skip */		\
	  .empty;				/* silence complaint */	\
        nop;                                                            \
	lda	[%g0]ASI_MCTRV, %l3;		/* read counter */	\
	srl	%l3, MCTRV_ICNT_SHIFT, %l7;	/* get inst count */	\
	set	cpu_cycle_count,%l4;		/* master counter */	\
	set	MCTRV_CCNT_LIMIT - 1, %l6;	/* mask for count */	\
	and	%l3, %l6, %l6;			/* hw cycle count */	\
	ldd	[%l4], %l4;			/* sw cycle count */	\
	set	MCTRV_CCNT_LIMIT, %l3;		/* cycle limit */	\
	sub	%l3, %l6, %l6;			/* cycles since intr */	\
	sub	%l3, %l7, %l7;			/* inst since intr */	\
	set	cpu_cycle_start, %l3;					\
	addcc	%l5, %l6, %l5;			/* add hw and sw */	\
	ld	[%l3 + 4], %l6;			/* Get start count */	\
	addx	%l4, 0, %l4;						\
	st	%l5, [%l3 + 4];			/* Save new value */	\
	subcc	%l5, %l6, %l5;			/* Compute diff */	\
	ld	[%l3], %l6;						\
	st	%l4, [%l3];			/* Save new value */	\
	sethi	%hi(cpudibp), %l3;					\
	ld	[%l3 + %lo(cpudibp)], %l3;	/* jumper's DIB */	\
	subx	%l4, %l6, %l4;						\
	ld	[%l3 + DIB_DOM_CYCLES + 4],%l6;	/* Add to dib ctr */	\
	addcc	%l6, %l5, %l5;						\
	ld	[%l3 + DIB_DOM_CYCLES], %l6;				\
	addx	%l6, %l4, %l4;						\
	set	cpu_inst_count, %l6;		/* Calc instrutions */	\
	std	%l4, [%l3 + DIB_DOM_CYCLES];				\
	ldd	[%l6], %l4;						\
	set	cpu_inst_start, %l6;		/* compute diff */	\
	addcc	%l5, %l7, %l5;						\
	ld	[%l6 + 4], %l7;						\
	addx	%l4, 0, %l4;			/* %l4-l5 has insts */	\
	st	%l5, [%l6 + 4];						\
	subcc	%l5, %l7, %l5;						\
	ld	[%l6], %l7;						\
	st	%l4, [%l6];						\
	subx	%l4, %l7, %l4;						\
	ldd	[%l3 + DIB_DOM_INST], %l6;	/* Add to dib value */	\
	addcc	%l7, %l5, %l5;						\
	addx	%l6, %l4, %l4;						\
	std	%l4, [%l3 + DIB_DOM_INST];				\
1:
#endif  /* NHxx */

/*
 * Opcodes for instructions in PATCH macros
 */
#define MOVPSRL0	0xa1480000
#define MOVL4		0xa8102000
#define BA		0x10800000
#define	NO_OP		0x01000000
#define	SETHI		0x27000000
#define	JMP		0x81c4e000
	.section	".data"

	.global	kernel_stack
	.global	kernel_stack_top
	.global	win_oflo_ct
	.global	win_uflo_ct
	.global	mon_clock14_vec


/*
 * The thread 0 stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important if the stack overflows. We get a
 * red zone below this stack for free when the kernel text is
 * write protected.
 */

	.align	8
kernel_stack:
	.skip	KERNSTKSZ			! kernel stack
kernel_stack_top:
kernel_savearea:
	.skip	DIB_SIZEOF

mon_clock14_vec:
	.skip 16
win_oflo_ct:	.word	0
win_uflo_ct:	.word	0

	.align	4
fprestartqueue:	.skip 4*6	! Space for 4 instructions, ret and nop

check_freq:	.word	10000
check_counter:	.word	0
cpuargcteaddr:	.word	0

ob_debug_entry:	.word	0
OBsTB: .word 0

	.align	8
cpu_tempsave:	.skip	8

	.align	8
trap_mon:
#if 0
	btst	PSR_PS, %l0		! test pS
	bz	_sys_trap		! user-mode, treat as bad trap
	nop
#endif
!	lda	[%g0]ASI_RMMU, %l5	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %l6
!	andn	%l5, %l6, %l6
!	sta	%l6, [%g0]ASI_RMMU

	mov	%l0, %psr		! restore psr
	nop				! psr delay
	b	mon_breakpoint_vec
	nop				! psr delay, too!

	.align	8			! MON_BREAKPOINT_VEC MUST BE 
					! DOUBLE ALIGNED.
mon_breakpoint_vec:
        .skip 16			! gets overlaid.

.global kernelname
kernelname:
	.asciz "kom"


!KOM Start
!
!    Trap vector tables, or scb's must be 0x1000 aligned.
/*
 * KOM Trap Vector Macros
 */
#define TRAP(H) \
	sethi %hi(H),%l3; jmp %l3+%lo(H); mov %psr,%l0; nop;
#if 0
#define SYS_TRAP(T) \
	mov %psr,%l0; sethi %hi(_sys_trap),%l3; \
	jmp %l3+%lo(_sys_trap); mov (T),%l4;
#endif

!  DOM_TRAPs trap domains.  They are fatal in the kernel
#define DOM_TRAP \
	mov %psr,%l0; sethi %hi(dom_trap),%l3; \
	jmp %l3+%lo(dom_trap); mov (.-scb)>>4,%l4;

#define TRAP_MON(T) \
    mov %psr,%l0; sethi %hi(trap_mon),%l3; jmp %l3+%lo(trap_mon); mov (T),%l4;
#define BAD_TRAP \
	sethi %hi(badTrap),%l3; jmp %l3+%lo(badTrap); \
	mov %psr,%l0; nop;
#define DOM_FREEZE sethi %hi(_fault), %l3; jmp %l3+%lo(_fault); sethi %hi(0xa0a0a0a0), %l4; nop;

/*
 * KOM Trap vector table.
 *
 * When a trap is taken, we vector to DEBUGSTART+(TT*16) and we have
 * the following state:
 *	2) traps are disabled
 *	3) the previous state of PSR_S is in PSR_PS
 *	4) the CWP has been decremented into the trap window
 *	5) the previous pc and npc is in %l1 and %l2 respectively.
 *
 * Registers:
 *	%l0 - %psr immediately after trap
 *	%l1 - trapped pc
 *	%l2 - trapped npc
 */
	.section ".trap", #alloc | #execinstr
	.align 0x1000
	.global _STart, _ConstantKernelFragment, scb
	.type _STart, #function
	.type _ConstantKernelFragment, #function
	.type scb, #function
_STart:
_ConstantKernelFragment:
scb:
	TRAP(.entry);				! 00 - reset
	TRAP(inst_mem_fault);			! 01 - instruction access
	DOM_TRAP;				! 02 - illegal instruction
	DOM_TRAP;				! 03 - privileged instruction
	TRAP(fp_disabled);			! 04 - floating point disabled
	TRAP(win_oflo);				! 05 - register window overflow
	TRAP(win_uflo);				! 06 - register window underflow
	DOM_TRAP;				! 07 - alignment fault
	TRAP(fp_exception);			! 08 - floating point exception
	TRAP(data_mem_fault);			! 09 - data access
	DOM_TRAP;				! 0A - tag_overflow
	BAD_TRAP; 				! 0B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 0C - 0F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 10 - 13
	TRAP(level4);				! 14
	BAD_TRAP;				! 15
	BAD_TRAP;				! 16
	BAD_TRAP;				! 17
	BAD_TRAP; 				! 18
#if defined(viking)
	TRAP(sspm_breakpoint_intr);		! 19
#else
	BAD_TRAP;				! 19
#endif
	TRAP(level10);				! 1A
	BAD_TRAP; 				! 1B
	BAD_TRAP; BAD_TRAP;			! 1C - 1D
	BAD_TRAP;				! 1E
	BAD_TRAP;				! 1F
!	TRAP(level15);				! 1F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 20 - 23
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 24 - 27
	BAD_TRAP;				! 28
	BAD_TRAP;				! 29
	DOM_TRAP;				! 2A - division by zero
	BAD_TRAP;				! 2B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 2C - 2F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 30 - 34
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 34 - 37
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 38 - 3B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 3C - 3F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 40 - 44
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 44 - 47
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 48 - 4B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 4C - 4F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 50 - 53
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 54 - 57
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 58 - 5B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 5C - 5F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 60 - 64
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 64 - 67
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 68 - 6B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 6C - 6F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 70 - 74
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 74 - 77
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 78 - 7B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 7C - 7F
	!
	! software traps
	!
	DOM_TRAP;				! 80 - ST_OSYSCALL
	DOM_TRAP;				! 81 - ST_BREAKPOINT
	DOM_TRAP;				! 82 - ST_DIV0
	TRAP(window_flush)			! 83 - ST_FLUSH_WINDOWS
	DOM_TRAP;				! 84 - ST_CLEAN_WINDOWS
	DOM_TRAP;				! 85 - ST_RANGE_CHECK
	DOM_TRAP;				! 86 - ST_FIX_ALIGN
	DOM_TRAP;				! 87 - ST_INT_OVERFLOW
	DOM_TRAP;				! 88 - ST_SYSCALL
	DOM_TRAP; DOM_TRAP; DOM_TRAP; 		! 89 - 8B
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! 8C - 8F
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! 90 - 93
	DOM_TRAP; DOM_TRAP; 			! 94 - 95
	TRAP(keykos_trap); DOM_TRAP; 		! 96 - 97
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! 98 - 9B
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! 9C - 9F
	TRAP(get_cc);				! A0 - ST_GETCC
	TRAP(set_cc);				! A1 - ST_SETCC
	DOM_TRAP;				! A2 - ST_GETPSR
	DOM_TRAP;				! A3 - ST_SETPSR
	DOM_TRAP;				! A4 - ST_GETHRTIME
	DOM_TRAP;				! A5 - ST_GETHRVTIME
	DOM_TRAP;				! A6 - ST_GETHRESTIME
	DOM_TRAP;				! A7
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! A8 - AB
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! AC - AF
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! B0 - B3
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! B4 - B7
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! B8 - BB
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! BC - BF
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! C0 - C3
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! C4 - C7
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! C8 - CB
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! CC - CF
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! D0 - D3
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! D4 - D7
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! D8 - DB
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! DC - DF
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! E0 - E3
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! E4 - E7
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! E8 - EB
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! EC - EF
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! F0 - F3
	DOM_TRAP; DOM_TRAP; DOM_TRAP; DOM_TRAP; ! F4 - F7
	DOM_TRAP;				! F8
	DOM_TRAP;				! F9
	DOM_TRAP; DOM_TRAP;			! FA - FB
	DOM_TRAP; DOM_TRAP; DOM_FREEZE; 		! FC - FE
	TRAP_MON(0xff);				! FF
.section ".text"
.entry:
	.global kk_entry
kk_entry:
Missive=0xf8003f00
aWindowPageTable=32
	!
	! Stash away our arguments.
	!
	mov	%o0, %g7		! save arg (romp) until bss is clear
	mov	%o2, %g5		! save bootops

	mov	0x02, %wim		! setup wim
	set	PSR_S|PSR_PIL|PSR_ET, %g1
	mov	%g1, %psr		! initialize psr: supervisor, splmax
	nop				! and leave traps enabled for monitor
	nop				! psr delay
	nop				! psr delay

    .global WindowPageTable
    sethi %hi(Missive+aWindowPageTable), %g1
    ld [%g1 + %lo(Missive+aWindowPageTable)], %g1
    sethi %hi(WindowPageTable), %g2
    st %g1, [%g2+%lo(WindowPageTable)]
    mov %tbr, %g1
    andn %g1, 0xfff, %g1
    sethi %hi(OBsTB), %g2
    st %g1, [%g2+%lo(OBsTB)]
    
	sethi	%hi(romp), %g1
	st	%g7, [%g1 + %lo(romp)]

	sethi	%hi(bootops), %g1
	st	%g5, [%g1 + %lo(bootops)]

	!
	! Patch vector 0 trap to "zero" if it happens again.
	!
	! PATCH_ST(0, 0)
	!
	! Find the the number of implemented register windows.
	! Using %g4 here is OK, as it doesn't interfere with fault info
	! stored in %g4, since this is only executed during startup.
	!
	! The last byte of every trap vector must be equal to
	! the number of windows in the implementation minus one.
	! The trap vector macros (above) depend on it!
	!
	mov	%g0, %wim		! note psr has cwp = 0

	sethi	%hi(nwin_minus_one), %g4 ! initialize pointer to nwindows - 1

	save				! decrement cwp, wraparound to NW-1
	mov	%psr, %g1
	and	%g1, PSR_CWP, %g1       ! we now have nwindows-1
	restore				! get back to orignal window
	mov	2, %wim			! reset initial wim

	st	%g1, [%g4 + %lo(nwin_minus_one)] ! initialize nwin_minus_one

	inc	%g1			! initialize the nwindows variable
	sethi	%hi(nwindows), %g4	! initialzie pointer to nwindows
	st	%g1, [%g4 + %lo(nwindows)] ! initialize nwin_minus_one

#if 0
	!
	! Now calculate winmask.  0's set for each window.
	!
	dec	%g1
	mov	-2, %g2
	sll	%g2, %g1, %g2
	sethi	%hi(winmask), %g4
	st	%g2, [%g4 + %lo(winmask)]
#endif

! traptable setup
	mov	%tbr, %g1		! save monitor's tbr
	bclr	0xfff, %g1		! remove tt

	!
	! Save monitor's level14 clock interrupt vector code.
	! and duplicate it to our area.
	!
	or	%g1, TT(T_INT_LEVEL_14), %o0
	!set	mon_clock14_vec, %o1
	set	scb, %o1
	or	%o1, TT(T_INT_LEVEL_14), %o1
	ldd	[%o0], %o2
	ldd	[%o0 + 8], %o4
	std	%o2, [%o1]
	std	%o4, [%o1 + 8]

	mov	%g1, %l4
	!
	! Save monitor's breakpoint vector code. 
	!
	or	%l4, TT(ST_MON_BREAKPOINT + T_SOFTWARE_TRAP), %o0
	set	mon_breakpoint_vec, %o1
	ldd	[%o0], %o2
	ldd	[%o0 + 8], %o4
	std	%o2, [%o1]
	std	%o4, [%o1 + 8]

	!
	! Remember the Open Boot bad trap handler
	! for redirecting traps
	! %g1 contains current trap table
	
	add	%g1, 0x81*16, %g1	! points to a bad trap handler
	sethi %hi(ob_debug_entry), %o2
	st 	%g1, [%o2 +%lo(ob_debug_entry)]

	!
	! Switch to our trap base register
	! remap traptable to write protect it
	!
#define trapHeist 0
! The above switch turns on code to study what code actually responds to traps.
! It allocates yet another trap table which becomes the real one,
! (tbr points to it) after locore code stops mucking with what it thinks
! will be the real table.
! The first trap code records the fact
! that the trap happened and logs information yet to be determined.
! Perhaps the new trap table will evolve to surplant the old.
! In that case the trapHeist switch and its logic would hopfully be
! replaced by civilized logic.
#if trapHeist
	set	TrapTableDebug, %g1		! setup debug trap handler
#else
	set	scb, %g1		! setup trap handler
#endif
	mov	%g1, %tbr

	!
	! Zero kernel stack
	!
	set	kernel_stack, %g1	! setup kernel stack pointer
	set	KERNSTKSZ, %l1
0:	subcc	%l1, 4, %l1
	bnz	0b
	clr	[%g1 + %l1]

	!
	! Setup kernel stack
	!
	set	KERNSTKSZ, %g2
	add	%g1, %g2, %sp
	sub	%sp, SA(MINFRAME + REGSIZE), %sp
	mov	0, %fp


#if 0
	!
	! It's now safe to call other routines.  Can't do so earlier
	! because %tbr need to be set up if profiling is to work.
	!
	call    module_setup		! setup correct module routines
	lda     [%g0]ASI_MOD, %o0	! find module type

	!
	! Clear status of any registers that might be latching errors.
	! Note: mmu_getsyncflt assumes that it can clobber %g1 and %g4.
	!
	call	mmu_getsyncflt
	nop
	sethi	%hi(afsrbuf), %o0
	call	mmu_getasyncflt		! clear module AFSR
	or	%o0, %lo(afsrbuf), %o0
#endif


#if defined(viking)
	! determine whether we are in mbus mode or not and
	! set mbus_mode accordingly
	!
!	lda	[%g0]ASI_MOD, %l0
!	set	CPU_VIK_MB, %l1
!	btst	%l1, %l0
!	bz	1f
!	nop
	set	1, %l1
	sethi	%hi(mbus_mode), %l0
	st	%l1, [%l0 + %lo(mbus_mode)]
1:
#endif
	set kernelname, %o0
!	call prom_init
!	nop

!	call omak_init
!	nop

!	call	map_wellknown_devices
!	nop






	!
	! Enable interrupts in the SIPR by writing all-ones to
	! the sipr mask "clear" location.
	!
        sethi	%hi(v_sipr_addr), %g1
        ld	[%g1 + %lo(v_sipr_addr)], %g1
	sub	%g0, 1, %g2		! clear all mask bits by writing ones
	st	%g2, [%g1 + IR_CLEAR_OFFSET] ! to the CLEAR location for itmr
	!
	! Now call main.  
	!
	call	Main
	nop

	SET_SIZE(_STart)
	SET_SIZE(scb)

       ENTRY(probe)
	sll	%o1, 8, %o1
	or	%o0, %o1, %o0
        retl
        lda     [%o0]ASI_FLPR, %o0               ! delay slot
        SET_SIZE(probe)

        ENTRY(flush)
	retl
	flush	%o0
        SET_SIZE(flush)

	ENTRY(clean_windows)	! All windows are in DIB in this version
	retl
	nop
	SET_SIZE(clean_windows)


/*
 * Window Overflow Trap Handler
 *
 * Current window is invalid; it was reserved for the occurrence of
 * a trap.  A save has occurred that would have made it the current
 * window, and the resulting trap brought us here.  We need to make
 * the window valid and the "next" window invalid, so that the save
 * can succeed.  To do that we must save the next window's contents
 * to the area preassigned for it on the stack and then  change the
 * value in the Window-Invalid Mask.
 *
 * Note: The word "next" is used antithetically.  More is less.
 */
	.global	win_oflo
win_oflo:
	! Count an overflow
	set	win_oflo_ct, %l7
	ld	[%l7], %l6
	add	%l6, 1, %l6
	st	%l6, [%l7]
	!
	set	nwin_minus_one, %l6
	ld	[%l6],%l6
	mov     %wim, %l3       ! Get WIM into local 3.
	!
	! Get old WIM and free %g1 for use
	!
	mov     %g1, %l7        ! Save %g1 in local 7.
        mov     %g4, %l5
	!
	! Compute new Window Invalid Mask
	!
	srl     %l3, 1, %g1		! Perform ror(WIM, 1, NW)
	sll     %l3, %l6, %l4		!  to form new

	btst	PSR_PS,%l0	! Overflow in the kernel?
	bnz	win_oflo_sup	! Yes - go handle kernel overflow
	or      %l4, %g1, %g1	!  New Window Invalid Mask.
#if DEBUG>=1
		set	0xf00, %l4
		or	%l0, %l4,%l4
		mov	%l4, %psr
! As I read the SPARC manual the instruction below turns on the ET
! bit which enables external interrupts. I can't imagine why and
! I can't imagine it being tolerable. Nor do I understand why
! they do it with this strange command sequence!
! It is possible that the ldda []0xa instructions below fail to properly fail
! with ET=0. That would disturbing and bizare.
		wr	%l4, PSR_ET, %psr
#endif
		set     RMMU_FSR_REG, %l5
		lda     [%l5]ASI_RMMU, %g0      ! clear speculative faults
		set	RMMU_CTL_REG, %l5	! Set no fault into the mmu
		lda	[%l5]ASI_RMMU, %l6
		bset	MCR_NF,%l6
		sta	%l6, [%l5]ASI_RMMU	
		save                    ! Enter the window we'll save,
		andcc %sp,7,%g0  ! test frame address for alignment
		bnz ZapHer
		mov     %g1, %wim       !  and then set the WIM
			    !  to mark it as the invalid window.
				! Save its contents on the stack.
        mov     %sp, %g4
		stda	%l0, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%l2, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%l4, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%l6, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%i0, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%i2, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%i4, [%g4]ASI_UD
		add	%g4, 8, %g4
		stda	%i6, [%g4]ASI_UD
		restore                 ! Now return to the trap window.
		bclr	MCR_NF,%l6	! Set no fault off in the mmu
		sta	%l6, [%l5]ASI_RMMU
		set	RMMU_FSR_REG, %l5	! Read the FSR
		lda	[%l5]ASI_RMMU, %l6
		andcc	%l6,SFSREG_FT,%g0	! Any faults?
#if DEBUG>=1
		bz	win_oflo_userret
#else
		bz	win_oflo_ret		! No - Go finish up
#endif
		nop
		mov	%l7, %g1	! Restore %g1
                mov     %l5, %g4
		mov     %l3, %wim       ! Restore wim

		sethi	%hi(cpu_mmu_fsr), %l7	! Save the FSR
		st	%l6, [%l7 + %lo(cpu_mmu_fsr)]

		KERNEL_CYCLES
		sethi	%hi(cpudibp), %l5
		ld	[%l5 + %lo(cpudibp)], %l5
  		SWITCH_TO_KERNEL(%l5)		! Save domain's registers

		set	RMMU_FAV_REG, %l7	! Save the fault address
		lda	[%l7]ASI_RMMU, %l7
		sethi	%hi(cpu_mmu_addr), %l6
		call	handle_data_obstacle
		  st	%l7, [%l6 + %lo(cpu_mmu_addr)]

		ba	return_from_user_exception
		nop
#if DEBUG>=1
win_oflo_userret:
 		mov	%psr, %l4
		ba	win_oflo_ret
		  wr	%l4, PSR_ET, %psr
#endif

ZapHer: restore
        mov  %l3, %wim   !  Back out changes since trap.
       	bclr MCR_NF, %l6
		sta	%l6, [%l5]ASI_RMMU
		mov %l7, %g1
		ba dom_trap
		mov 0x70, %l4  ! Report alignment error
		
	!
	! Save new window to stack, and set new WIM
	!
win_oflo_sup:	
	save;			! Enter the window we'll save,
	mov     %g1, %wim       !  and then set the WIM making it invalid
	nop; nop; nop;          !  to mark it as the invalid window.
	SAVE_WINDOW(%sp)        ! Save its contents on the stack.
	restore                 ! Now return to the trap window.
	!
	! Clean up and return from trap
	!
win_oflo_ret:	
	mov	%l0, %psr	! Restore the condition code
    ba jmp_rett     ! Proceed to next user instruction.
	mov %l7, %g1    ! Restore %g1 from local 7.

/*
 * Window Underflow Trap Handler
 *
 * A restore has occurred that would have made the invalid window
 * the current window.  An underflow trap occurred, and the implicit
 * save that occurred moved us to the "next" window.  For example,
 * if the invalid window is 4, we are now in window 2.  We need to
 * make window 5 invalid and to restore to window 4 its previous
 * contents, saved earlier in the stack by win_oflo above.
 *
 * Note: The word "previous" is used antithetically.  Less is more.
 */
	.global win_uflo
win_uflo:
	! Count an underflow
	set	win_uflo_ct, %l7
	ld	[%l7], %l6
	add	%l6, 1, %l6
	st	%l6, [%l7]
	!
	set	nwin_minus_one, %l6
	ld	[%l6],%l6
	!
	! Compute and install new Window Invalid Mask
	!
	mov     %wim, %l3		! Get WIM into local 3.
	sll     %l3, 1, %l4		! Perform rol(WIM, 1, NW)
	srl     %l3, %l6, %l5		!  to form new
	or      %l5, %l4, %l5		!  Window Invalid Mask.
	mov     %l5, %wim		! Install new WIM to mark the
	nop; nop; nop;			!  previous window as invalid.

	btst	PSR_PS,%l0	! Overflow in the kernel?
	bnz	win_uflo_sup		! Yes - go handle kernel overflow
	nop
#if DEBUG>=1
		set	0x00000f00, %l4
		or	%l0, %l4, %l4
		mov	%l4, %psr
		wr	%l4, PSR_ET, %psr
#endif
		set     RMMU_FSR_REG, %l7
		lda     [%l7]ASI_RMMU, %g0      ! clear speculative faults
		set	RMMU_CTL_REG, %l7	! Set no fault into the mmu
		lda	[%l7]ASI_RMMU, %l6
		bset	MCR_NF,%l6
		sta	%l6, [%l7]ASI_RMMU	
		restore                 ! Go two windows "backward" to the one
		restore                 ! whose contents are to be restored.
        andcc %sp,7,%g0  ! test for alignment of stack pointer
        bnz ZapHim

		add	%sp, 8, %l0
		ldda	[%l0]ASI_UD, %l2
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %l4
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %l6
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %i0
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %i2
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %i4
		add	%l0, 8, %l0
		ldda	[%l0]ASI_UD, %i6
		ldda	[%sp]ASI_UD, %l0

		save                    ! Return to our own window, where
		save                    ! locals 1 and 2 contain PC and nPC.
		bclr	MCR_NF,%l6	! Set no fault off in the mmu
		sta	%l6, [%l7]ASI_RMMU
		set	RMMU_FSR_REG, %l7	! Read the FSR
		lda	[%l7]ASI_RMMU, %l5
		btst	SFSREG_FT,%l5		! Any faults?
#if DEBUG>=1
		bz	win_uflo_userret
#else
		bz	win_uflo_ret		! No - Go finish up
#endif
		nop

		mov     %l3, %wim		! Restore the WIM

		sethi	%hi(cpu_mmu_fsr), %l7	! Save FSR for memory
		st	%l5, [%l7 + %lo(cpu_mmu_fsr)]

		KERNEL_CYCLES
		sethi	%hi(cpudibp), %l5
		ld	[%l5 + %lo(cpudibp)], %l5
#if DEBUG>=1
		mov	%psr, %l4
		wr	%l4, PSR_ET, %psr	! Disable interrupts
		nop; nop; nop
#endif
  		SWITCH_TO_KERNEL(%l5)		! Save domain's registers

		set	RMMU_FAV_REG, %l7	! Save the fault address
		lda	[%l7]ASI_RMMU, %l7
		sethi	%hi(cpu_mmu_addr), %l6
		call	handle_data_obstacle
		  st	%l7, [%l6 + %lo(cpu_mmu_addr)]
		ba	return_from_user_exception
		nop
#if DEBUG>=1
win_uflo_userret:
 		mov	%psr, %l4
		ba	win_uflo_ret
		  wr	%l4, PSR_ET, %psr
#endif

ZapHim: save; save  ! get back to where we were
        bclr MCR_NF, %l6
        sta %l6, [%l7]ASI_RMMU
        mov %l3, %wim
        ba dom_trap
        mov 0x70, %l4 ! call it an alignment problem

	!
	! Restore contents of the newly valid window
	!
win_uflo_sup:	
	restore                 ! Go two windows "backward" to the one
	restore                 ! whose contents are to be restored.
	RESTORE_WINDOW(%sp)     ! Restore the erstwhile invalid window.
	save                    ! Return to our own window, where
	save                    ! locals 1 and 2 contain PC and nPC.
	!
	!
	!
win_uflo_ret:
	mov	%l0, %psr	! Restore the condition code
	ba jmp_rett; nop  ! Proceed to user instruction after restore.
	! jmp     %l1             ! Re-execute the restore that trapped.
	! rett    %l2             ! Return from this trap handler.

/*
 * GETSIPR: read system interrupt pending register
 */
#define GETSIPR(r) \
        sethi   %hi(v_sipr_addr), r                     ;\
        ld      [r + %lo(v_sipr_addr)], r               ;\
        ld      [r], r
 
 
        ENTRY(level4)
        GETSIPR(%l4)
        set     SIR_SCSI, %l3
        btst   %l4, %l3		 	! check the SCSI mask bit in sipr
 
        bz      scsi_exit
        nop
 
        btst    PSR_PS, %l0             ! test pS
        bnz     level4_sup              ! trap from kernel
        nop
 
level4_dom:
        ! trap from user
        ! Save registers to the DIB
        sethi   %hi(cpudibp), %l5
        ld      [%l5 + %lo(cpudibp)], %l5
        SAVE_REGS(%l5)  ! N.B. Now running in a new window
 
        mov     kernCtx, %l3    ! Set the context number
        mov     RMMU_CTX_REG, %l4
        sta     %l3, [%l4]ASI_RMMU
 
        ! Set up kernel stack
        set     kernel_stack+KERNSTKSZ-SA(MINFRAME + REGSIZE), %sp
        mov     0, %fp
 
 
        ! enable traps (except interrupts), so we can call C routines
        mov     %psr, %l0
        or      %l0, PSR_PIL, %l3
        mov     %l3, %psr
        wr      %l3, PSR_ET, %psr
        nop                             ! psr delay
	KERNEL_CYCLES
 
        ! call SCSI interrupt
        call    esp_poll_loop           ! psr delay
        nop                             ! psr delay
 
        ba      gotdomain
        nop
 
level4_sup:
        ! level4 interrupt from supervisor mode
 
        set     idlefunction, %l6       ! Is the idle function is running?
        sub     %l1, %l6, %l6             ! compute pc-idlefunction
        cmp     %l6, 4
        bleu    level4_dom              ! Yes - treat as domain trap
 
        ! See if we are running in an invalid window and do overflow logic
        ! if we are
 
        mov     %wim, %l3               ! Get the wim
        mov     1, %l5                  ! Calculate current win's mask
        sll     %l5,%l0,%l5
        btst    %l5, %l3
        set     nwin_minus_one, %l6
        bz      level4_windowvalid
        ld      [%l6],%l6
                !
                ! Get old WIM and free %g1 for use
                !
                mov     %g1, %l7        ! Save %g1 in local 7.
                !
                ! Compute new Window Invalid Mask
                !
                srl     %l3, 1, %g1             ! Perform ror(WIM, 1, NW)
                sll     %l3, %l6, %l4           !  to form new
                or      %l4, %g1, %g1   !  New Window Invalid Mask.
                !
                ! Save new window to stack, and set new WIM
                !
                save;                   ! Enter the window we'll save,
                mov     %g1, %wim       ! and then set the WIM making it invalid
                nop; nop; nop;          !  to mark it as the invalid window.
                SAVE_WINDOW(%sp)        ! Save its contents on the stack.
                restore                 ! Now return to the trap window.
                mov     %l7, %g1        ! Restore %g1 from local 7.
level4_windowvalid:
        ! %l5 has current window's mask, %l6 has nwindows-1
 
        ! Check the frame pointer for reasonableness
        set     kernel_stack, %l4
        sub     %fp, %l4, %l4
        set     KERNSTKSZ, %l3
        cmp     %l4, %l3
        bgeu    _fault
        nop
 
 
        ! create new stack frame
        add     %fp, -SA(MINFRAME+8*4), %sp
 
        ! Save the globals
        st      %g1, [%fp-28]
        mov     %y, %g1
        std     %g2, [%fp-24]
        std     %g4, [%fp-16]
        std     %g6, [%fp-8]
        st      %g1, [%fp-32]
 
        ! enable traps (except interrupts), so we can call C routines
        or      %l0, PSR_PIL, %l3
        mov     %l3, %psr
        wr      %l3, PSR_ET, %psr
        nop                     ! psr delay
 
        mov     RMMU_CTX_REG, %l4
        lda     [%l4]ASI_RMMU, %l5      ! Save current context number
        mov     kernCtx, %l3            ! Set the kernel context number
        sta     %l3, [%l4]ASI_RMMU
 
        ! call SCSI interrutp routine
        call    esp_poll_loop   ! psr delay
        nop                     ! psr delay
        ! Ensure we will not rett to an invalid window
        restore
        save
 
        sta     %l5, [%l4]ASI_RMMU      ! Restore interrupted context
 
        ! Restore the globals
        ld      [%fp-32], %g1
        ldd     [%fp-24], %g2
        ldd     [%fp-16], %g4
        ldd     [%fp-8], %g6
        mov     %g1, %y
        ld      [%fp-28], %g1
 
scsi_exit:
        ! restore %psr
        mov     %l0, %psr
        nop;                   !psr delay
        ba jmp_rett; nop  ! return to user instructions 
        ! jmp     %l1
        ! rett    %l2
        SET_SIZE(level4)
 
!
!LEVEL 10 TIMER INTERRUPT
!
        ENTRY(level10)
	! read the limit register, to clear the interrupt
        sethi   %hi(v_level10clk_addr), %l3
        ld      [%l3 + %lo(v_level10clk_addr)], %l3
!   ta 0x45
	ld      [%l3 + CTR_LIMIT10], %l4
	ld      [%l3 + CTR_COUNT10], %l5

	btst	PSR_PS, %l0		! test pS
	bnz	level10_sup		! trap from kernel
	nop

level10_dom:
	! trap from user
	! Save registers to the DIB
	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
	SAVE_REGS(%l5)	! N.B. Now running in a new window

	mov	kernCtx, %l3	! Set the context number
	mov	RMMU_CTX_REG, %l4
	sta	%l3, [%l4]ASI_RMMU

	! Set up kernel stack
	set	kernel_stack+KERNSTKSZ-SA(MINFRAME + REGSIZE), %sp
	mov	0, %fp


	! enable traps (except interrupts), so we can call C routines
	mov	%psr, %l0
	set	PSR_EF, %l3		! Disable floating point in kernel
	andn	%l0, %l3, %l3
	or	%l3, PSR_PIL, %l3
	mov	%l3, %psr 
	wr	%l3, PSR_ET, %psr
	nop				! psr delay
	KERNEL_CYCLES

	! call timer routine
	call	timerinterrupt		! psr delay
	nop				! psr delay

	ba	gotdomain
	nop

	
level10_sup:
	! level10 interrupt from supervisor mode
	
	set	idlefunction, %l6	! Is the idle function is running?
	sub	%l1, %l6, %l6		  ! compute pc-idlefunction
	cmp	%l6, 4
	bleu	level10_dom		! Yes - treat as domain trap

	! See if we are running in an invalid window and do overflow logic
	! if we are

	mov	%wim, %l3		! Get the wim
	mov	1, %l5			! Calculate current win's mask
	sll	%l5,%l0,%l5
	btst	%l5, %l3
	set	nwin_minus_one, %l6
	bz	level10_windowvalid
	ld	[%l6],%l6
		!
		! Get old WIM and free %g1 for use
		!
		mov     %g1, %l7        ! Save %g1 in local 7.
		!
		! Compute new Window Invalid Mask
		!
		srl     %l3, 1, %g1		! Perform ror(WIM, 1, NW)
		sll     %l3, %l6, %l4		!  to form new
		or      %l4, %g1, %g1	!  New Window Invalid Mask.
		!
		! Save new window to stack, and set new WIM
		!
		save;			! Enter the window we'll save,
		mov     %g1, %wim       ! and then set the WIM making it invalid
		nop; nop; nop;          !  to mark it as the invalid window.
		SAVE_WINDOW(%sp)        ! Save its contents on the stack.
		restore                 ! Now return to the trap window.
		mov     %l7, %g1        ! Restore %g1 from local 7.
level10_windowvalid:
	! %l5 has current window's mask, %l6 has nwindows-1

	! Check the frame pointer for reasonableness
	set	kernel_stack, %l4
	sub	%fp, %l4, %l4
	set	KERNSTKSZ, %l3
	cmp	%l4, %l3	
	bgeu	_fault
	nop


	! create new stack frame
	add	%fp, -SA(MINFRAME+8*4), %sp

	! Save the globals
	st	%g1, [%fp-28]
	mov	%y, %g1
	std	%g2, [%fp-24]
	std	%g4, [%fp-16]
	std	%g6, [%fp-8]
	st	%g1, [%fp-32]

	! enable traps (except interrupts), so we can call C routines
	or	%l0, PSR_PIL, %l3
	mov	%l3, %psr 
	wr	%l3, PSR_ET, %psr
	nop			! psr delay

	mov	RMMU_CTX_REG, %l4
	lda	[%l4]ASI_RMMU, %l5	! Save current context number
	mov	kernCtx, %l3		! Set the kernel context number
	sta	%l3, [%l4]ASI_RMMU

	! call timer routine
	call	timerinterrupt	! psr delay
	nop			! psr delay

	sta	%l5, [%l4]ASI_RMMU	! Restore interrupted context

	! Ensure we will not rett to an invalid window
	restore
	save

	! Restore the globals
	ld	[%fp-32], %g1
	ldd	[%fp-24], %g2
	ldd	[%fp-16], %g4
	ldd	[%fp-8], %g6
	mov	%g1, %y
	ld	[%fp-28], %g1

	! restore %psr
	mov	%l0, %psr
    ba jmp_rett; nop ! resume user instructions
	! jmp	%l1
	! rett	%l2
        SET_SIZE(level10)

        ENTRY(level15)
	call prom_naked_enter
	nop
        SET_SIZE(level15)


! Handle instruction access faults
	ENTRY(inst_mem_fault)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc

	btst	PSR_PS, %l0		! Shouldn't occur to the kernel
	bnz	_fault			!  _fault can have a bad %l6

	sethi	%hi(cpu_mmu_addr), %l6	! Failing virtual address is PC
	st	%l1, [%l6 + %lo(cpu_mmu_addr)]

	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
  	SWITCH_TO_KERNEL(%l5)		! Save the trapped domain's registers
	KERNEL_CYCLES

	set	RMMU_FSR_REG, %l7	! Save the fault status register
	lda	[%l7]ASI_RMMU, %l7
	sethi	%hi(cpu_mmu_fsr), %l6
	call	handle_data_obstacle
	st	%l7, [%l6 + %lo(cpu_mmu_fsr)]

	ba	return_from_user_exception
	nop

	SET_SIZE(inst_mem_fault)


! Handle data access faults
	ENTRY(data_mem_fault)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc

	btst	PSR_PS, %l0		! Shouldn't occur to the kernel
	bnz	_fault			!  _fault can have a bad %l5

	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
  	SWITCH_TO_KERNEL(%l5)		! Save the trapped domain's registers
	KERNEL_CYCLES

	set	RMMU_FSR_REG, %l7	! Save the fault status register
	lda	[%l7]ASI_RMMU, %l7
	sethi	%hi(cpu_mmu_fsr), %l6
	st	%l7, [%l6 + %lo(cpu_mmu_fsr)]

	set	RMMU_FAV_REG, %l7	! Save the fault address register
	lda	[%l7]ASI_RMMU, %l7
	sethi	%hi(cpu_mmu_addr), %l6
	call	handle_data_obstacle
	st	%l7, [%l6 + %lo(cpu_mmu_addr)]

	ba	return_from_user_exception
	nop

	SET_SIZE(data_mem_fault)


! Handle the ST_GETCC trap
	ENTRY(get_cc)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc
! Returns integer condition code in %g1

	srl	%l0,20,%g1
	and	%g1,PSR_ICC>>20,%g1
	jmp	%l2
	rett	%l2+4
	SET_SIZE(get_cc)


! Handle the ST_SETCC trap
	ENTRY(set_cc)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc
!	%g1 - Interger condition to set

	set	PSR_ICC,%l3
	andn	%l0,%l3,%l0
	and	%g1,PSR_ICC>>20,%g1
	sll	%g1,20,%g1
	or	%g1,%l0,%l0
	mov	%l0,%psr
	nop; nop;
	jmp	%l2
	rett	%l2+4
	SET_SIZE(set_cc)


! Handle the ST_FLUSH_WINDOWS trap
	ENTRY(window_flush)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc

	/* Save some global registers in local registers */

	mov	%g7, %l7
	mov	%g6, %l6
	mov	%g5, %l5
	mov	%g4, %l4
	mov	%g3, %l3
	mov	%l0, %g7	! Put psr in G7

	/* Calculate %g6=current window mask, %g4=NW-1 */
	mov	1,%g6
	sethi	%hi(nwin_minus_one), %g4
	ld	[%g4 + %lo(nwin_minus_one)], %g4
	mov	%wim, %g3
	sll	%g6, %g7, %g6

	/* Registers: %g7=psr, %g6=current window mask */
	/*    %g4=number of windows minus one, %g3=wim */

window_flush_next:	
	srl	%g6, %g4, %g5	/* Prev win's mask = rot cwm left 1 size NW */
	tst	%g5
	bz,a	1f		/* If current the maximum, don't shift */
	sll	%g6, 1, %g5	/* Not max, shift left */
1:	mov	%g5, %g6
	btst	%g6, %g3;	/* Is previous window valid? */
	bnz	window_flush_done;	/* No - Done saving windows */
	nop;
	restore;		/* to previous window */
	btst	PSR_PS,%g7	! Overflow in the kernel?
	bnz	window_flush_sup	! Yes - go handle kernel flush
	nop
		set     RMMU_FSR_REG, %g5
		lda     [%g5]ASI_RMMU, %g0      ! clear speculative faults
		set	RMMU_CTL_REG, %g5	! Set no fault into the mmu
		lda	[%g5]ASI_RMMU, %g3
		bset	MCR_NF,%g3
		sta	%g3, [%g5]ASI_RMMU	
					! Save its contents on the stack.
		stda	%l0, [%sp]ASI_UD
		add	%sp, 8, %g3
		stda	%l2, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%l4, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%l6, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%i0, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%i2, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%i4, [%g3]ASI_UD
		add	%g3, 8, %g3
		stda	%i6, [%g3]ASI_UD

		lda	[%g5]ASI_RMMU, %g3
		bclr	MCR_NF,%g3	! Set no fault off in the mmu
		sta	%g3, [%g5]ASI_RMMU
		set	RMMU_FSR_REG, %g5	! Read the FSR
		lda	[%g5]ASI_RMMU, %g3
		btst	SFSREG_FT,%g3		! Any faults?
		bz,a	window_flush_next	! No - Go save next window
		mov	%wim, %g3	/* Get the wim for top of loop */

		mov	%g7, %psr	/* Go back to the trap window */
		nop; nop; nop;		/* psr delay */

		sethi	%hi(cpu_mmu_fsr), %g7	! Save the FSR
		st	%g3, [%g7 + %lo(cpu_mmu_fsr)]

		mov	%l7, %g7	/* Restore the saved g regs */
		mov	%l6, %g6
		mov	%l5, %g5
		mov	%l4, %g4
		mov	%l3, %g3

		KERNEL_CYCLES
		sethi	%hi(cpudibp), %l5
		ld	[%l5 + %lo(cpudibp)], %l5
  		SWITCH_TO_KERNEL(%l5)		! Save domain's registers

		set	RMMU_FAV_REG, %l7	! Save the fault address
		lda	[%l7]ASI_RMMU, %l7
		sethi	%hi(cpu_mmu_addr), %l6
		call	handle_data_obstacle
		st	%l7, [%l6 + %lo(cpu_mmu_addr)]

		ba	return_from_user_exception
		nop

window_flush_sup:	 	! flush kernel register windows.
	SAVE_WINDOW(%sp)        ! Save its contents on the stack.
	ba	window_flush_next
	mov	%wim, %g3		! Get wim for top of loop								
window_flush_done:
	mov	%g7, %psr	/* Go back to the trap window */
	nop; nop; nop;		/* psr delay */

	/* Registers: %g7=psr, %g4=number of windows minus one */

	mov	1, %g3		! Set only last of caller's windows valid
	sll	%g3, %g7, %G3	! %g3 is current window mask
	sub	%g4, 1, %g4	! Rotate CWM left 2 mod Number of windows 
	srl	%g3, %g4, %g4
	sll	%g3, 2, %g3
	wr	%g4, %g3, %wim

	mov	%l7, %g7	/* Restore the saved g regs */
	mov	%l6, %g6
	mov	%l5, %g5
	mov	%l4, %g4
	mov	%l3, %g3

    mov %l2, %l1
    ba jmp_rett; add %l2, 4, %l2
	! jmp	%l2
	! rett	%l2+4
	SET_SIZE(window_flush)



! Defines for fields of the exit block

#define EXIT_KEYMASK	0xf0000000
#define EXIT_KEYMASK8	0x80000000
#define EXIT_KEYMASK4	0x40000000
#define EXIT_KEYMASK2	0x20000000
#define EXIT_KEYMASK1	0x10000000
#define EXIT_ARGTYPE	0x0c000000
#define EXIT_ARGNONE		0x00000000
#define EXIT_ARGMEMORY		0x04000000
#define EXIT_ARGREGS		0x0c000000
#define EXIT_JUMPTYPE	0x03000000
#define EXIT_JUMPTYPE_SHIFT	24
#define EXIT_JUMPIMPLICIT	0x00000000
#define EXIT_JUMPCALL		0x01000000
#define EXIT_JUMPRETURN		0x02000000
#define EXIT_JUMPFORK		0x03000000
#define EXIT_GATE	0x00f00000
#define EXIT_ARGTYPESHIFT	26
#define EXIT_GATE_SHIFT		20
#define EXIT_KEY1_SHIFT		12
#define EXIT_KEY2_SHIFT		8
#define EXIT_KEY3_SHIFT 	4
#define EXIT_KEY4_SHIFT 	0


/* Certain bits in the copy of the entry block held in a register (%l6) are
   used for communication between different sections of the code.  These bits
   are "reserved" and illegal for a domain to set.  Currently ENTRY_NOFAULT,
   ENTRY_DOMKEEP, and ENTRY_RCZERO are used.
*/
#define ENTRY_RESERVED	0xd00f0000
#define ENTRY_NOTFAULT	0x80000000	/* EB is not for a fault resume */
#define ENTRY_DOMKEEP   0x40000000	/* Processing a trap to dom keeper */
#define ENTRY_RCZERO	0x10000000	/* Generate a return code of zero */

#define ENTRY_REGSPARM	0x20000000
#define ENTRY_RC	0x08000000
#define ENTRY_DB	0x04000000
#define ENTRY_STR	0x02000000
#define ENTRY_STRLEN	0x01000000
#define ENTRY_KEYMASK	0x00f00000
#define ENTRY_KEYMASK8	0x00800000
#define ENTRY_KEYMASK4	0x00400000
#define ENTRY_KEYMASK2	0x00200000
#define ENTRY_KEYMASK1	0x00100000
#define ENTRY_KEY1_SHIFT	12
#define ENTRY_KEY2_SHIFT	8	
#define ENTRY_KEY3_SHIFT	4
#define ENTRY_KEY4_SHIFT	0

#if KEY_SIZEOF == 16
#define KEYINDEXTOOFFSETSHIFT 4
#define KEYOFFSETMASK 0xf0
#endif


! Handle those traps that are fatal for the kernel and go to the domain's
! keeper for a domain

	ENTRY(dom_trap)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc
!	%l4 - The trap number of the trap

	btst	PSR_PS, %l0		! These shouldn't occur to the kernel
	bnz	_fault

	sethi	%hi(cpudibp), %l7
	ld	[%l7 + %lo(cpudibp)], %l7
	sth	%l4, [%l7 + DIB_TRAPCODE]	! Set the trap code
	ldub	[%l7 + DIB_READINESS], %l4	! And mark domain trapped
	bset	DIB_TRAPPED, %l4
	stb	%l4, [%l7 + DIB_READINESS]

	KERNEL_CYCLES

	sethi	%hi(cpudibp), %l7
	ld	[%l7 + %lo(cpudibp)], %l7
#if 1
  	SAVE_REGS(%l7)		! Save the trapped domain's registers
				! N.B. Trapped domain's dib is now in %g7

        ! enable traps (except interrupts), so we can call C routines
        mov     %psr, %l0
        or      %l0, PSR_PIL, %l3
        mov     %l3, %psr
        wr      %l3, PSR_ET, %psr

	ld	[%g7 + DIB_ROOTNODE], %l7	! Get the root node

	mov	kernCtx, %l3;	/* Set the kernel context number */
	mov	RMMU_CTX_REG, %l4;
	sta	%l3, [%l4]ASI_RMMU;

	sethi	%hi(lowcoreflags), %l4
	ldub	[%l4 + %lo(lowcoreflags)], %l4
	btst	0x80, %l4		! gatejumplogenable == 1?
!	bnz	dom_trap_bail		! Give up if logging jumps
        ba      dom_trap_bail

	  ldd	[%l7 + NODE_DOMKEEPER + 8], %g4	! Get keeper key
	and	%g5, 0xff, %l6			! Isolate key type
	cmp	%l6, KT_STARTKEY + KEY_TYPE_PREPARED
	bne	dom_trap_bail
	  nop

! We are invoking a start key
	ldub	[%g4 + NODE_PREPLOCK], %l3	! Preplock jumpee
	btst	0x80, %l3
	mov	LOCKEDBY_ASM_DOM_TRAP, %l3
	bnz	dom_trap_bail
	  ldub	[%g4 + NODE_PREPCODE], %l6

	stb	%l3, [%g4 + NODE_PREPLOCK]
	cmp	%l6, PREPASDOMAIN
	bne	dom_trap_bail_unpreplock
	  ld	[%g4 + NODE_DIB], %l7

	! %g4 Points to the root node of the jumpee
	! %g5 Holds the keeper key databyte and the keytype (prep+start)
	! %g7 points to the faulter's dib
	! %l7 points to the jumpee's dib

	ldub	[%l7 + DIB_READINESS], %l3
	btst	DIB_BUSY, %l3		! If jumpee is busy, get out
	bnz	dom_trap_bail_unpreplock
	  .empty

! string is (64,dibregs) (64,curbackset) (16,pc npc psr (2,0) (2,trapcode) 
! 	    (8,trapcodeextension) (4,fsr) (8,1st queued fp))

	set	64+64+16+8+4+8, %g2	! Set offered length

	ld	[%l7 + DIB_REGS + (8+2)*4], %l6	! Get entry block
	set	ENTRY_DOMKEEP, %g3	! Mark as dom keeper jump
	or	%l6, %g3, %l6

	sethi	%hi(ENTRY_STR), %l4
	btst	%l4, %l6
	bz	dom_trap_bail_unpreplock

	  ld	[%l7 + DIB_REGS + (1)*4], %l5	! max parm length
	tst	%l5				! Is parm length zero
	bz,a	dom_trap_give_strl	! Yes - done
	  clr	%g2			! String length is zero
	set	ENTRY_REGSPARM, %l3		! Register parm?
	btst	%l3, %l6
	bnz	dom_trap_bail_unpreplock
	  ld	[%l7 + DIB_REGS + (8+5)*4], %l0	! parm address
	add	%l0, %l5, %l3		! Compute end of string address
	dec	%l3
	xor	%l3, %l0, %l3		! Is it on the same page?
	andncc	%l3, 0xfff, %g0
	bnz	dom_trap_bail_unpreplock	! No - Have gate do it

	! %l0 is the jumpee's parm string address

	  btst	0x7, %l0		! Require double word alignment
	bnz	dom_trap_bail_unpreplock

	  srl	%l0, 28, %l3		! Address too big?
	cmp	%l3, 0x0f	
	be	dom_trap_bail_unpreplock	! Yes - have gate do it
	  ld	[%l7 + DIB_MAP], %l1
	tst	%l1				! is the map NULL_MAP?
	bz	dom_trap_bail_unpreplock	! Yes - Punt

	  cmp	%g2, %l5		! Bail if keeper doesn't want all of it
	bgu	dom_trap_bail_unpreplock

	! End dry run - (but hold handle jumper until common code below)

	  mov	RMMU_CTX_REG, %l2	! Switch to jumpee's map
	sta	%l1, [%l2]ASI_RMMU

	! %g2 has the length of the trap string
	! %g4 Points to the root node of the jumpee
	! %g5 Holds the keeper key databyte and the keytype (prep+start)
	! %g7 points to the faulter's dib
	! %l0 has the jumpee's address
	! %l5 has the jumpee's maximum length >= %g2
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

#if RMMU_CTL_REG != 0
  error - hardware changed
#endif
	set	RMMU_FSR_REG, %l4	! Clear any fault
	lda	[%l4]ASI_RMMU, %g0

	lda	[%g0]ASI_RMMU, %l4	! Set no fault into the RMMU_CTL_REG
	set	MCR_NF , %l2
	or	%l4, %l2, %l2
	sta	%l2, [%g0]ASI_RMMU
	
	! %l4 has the old contents of the RMMU_CTL_REG

! Pass: (64,dibregs) (64,curbackset) (16,pc npc psr (2,0) (2,trapcode) 
! 	(8,trapcodeextension) (4,fsr) (8,1st queued fp))

	ldd	[%g7 + DIB_REGS], %l2		! Pass DIB_REGS
	std	%l2, [%l0]
	ldd	[%g7 + DIB_REGS + 8], %l2
	std	%l2, [%l0 + 8]
	ldd	[%g7 + DIB_REGS + 16], %l2
	std	%l2, [%l0 + 16]
	ldd	[%g7 + DIB_REGS + 24], %l2
	std	%l2, [%l0 + 24]
	ldd	[%g7 + DIB_REGS + 32], %l2
	std	%l2, [%l0 + 32]
	ldd	[%g7 + DIB_REGS + 40], %l2
	std	%l2, [%l0 + 40]
	ldd	[%g7 + DIB_REGS + 48], %l2
	std	%l2, [%l0 + 48]
	ldd	[%g7 + DIB_REGS + 56], %l2
	std	%l2, [%l0 + 56]

	ldub	[%g7 + DIB_BACKALLOC], %g6	! Pass most recent backset
	add	%g7, DIB_BACKSET, %l2
	sll	%g6, 6, %g6			/* g6 has backalloc*64 */
	add	%l2, %g6, %g6			/* g6 has addr of current */
	ldd	[%g6], %l2
	std	%l2, [%l0 + 64]
	ldd	[%g6 + 8], %l2
	std	%l2, [%l0 + 8 + 64]
	ldd	[%g6 + 16], %l2
	std	%l2, [%l0 + 16 + 64]
	ldd	[%g6 + 24], %l2
	std	%l2, [%l0 + 24 + 64]
	ldd	[%g6 + 32], %l2
	std	%l2, [%l0 + 32 + 64]
	ldd	[%g6 + 40], %l2
	std	%l2, [%l0 + 40 + 64]
	ldd	[%g6 + 48], %l2
	std	%l2, [%l0 + 48 + 64]
	ldd	[%g6 + 56], %l2
	std	%l2, [%l0 + 56 + 64]

	ldd	[%g7 + DIB_PC], %l2		! pc and npc
	std	%l2, [%l0 + 64 + 64]
	ld	[%g7 + DIB_PSR], %l2		! psr, 0, and trapcode
	lduh	[%g7 + DIB_TRAPCODE], %l3
	std	%l2, [%l0 + 64 + 64 + 8]
	ldd	[%g7 + DIB_TRAPCODEEXTENSION], %l2
	std	%l2, [%l0 + 64 + 64 + 16]
	ld	[%g7 + DIB_FSR], %l2
	st	%l2, [%l0 + 64 + 64 + 16 + 8]
	ldd	[%g7 + DIB_DEFERRED_FP], %l2
	st	%l2, [%l0 + 64 + 64 + 16 + 8 + 4]
	st	%l2, [%l0 + 64 + 64 + 16 + 8 + 4 + 4]

	sta	%l4, [%g0]ASI_RMMU	! Restore the RMMU_CTL_REG
	set	RMMU_FSR_REG, %l4	! Read the FSR
	lda	[%l4]ASI_RMMU, %l2

	mov	RMMU_CTX_REG, %l0	! Back to kernel's context
	mov	kernCtx, %l1
	sta	%l1, [%l0]ASI_RMMU

	btst	SFSREG_FT,%l2		! Any faults?
	bnz	dom_trap_bail_unpreplock
	  .empty				! No - Return

dom_trap_give_strl:
	set	ENTRY_STRLEN, %l0
	btst	%l0, %l6
	bnz,a	.+8		! Set the string length if wanted
	   st	%g2, [%l7 + DIB_REGS + (8+4)*4]

	! End dry run - (but hold handle jumper until common code below)
	! %g5 holds (2,???) (1,databyte), (1,KT_STARTKEY+KEY_TYPE_PREPARED)
	! %l6 holds the entry block

	set	ENTRY_DB, %l3		! Pass databyte if wanted
	btst	%l3, %l6
	srl	%g5, 8, %g5
	and	%g5, 0xff, %g5
	bnz,a	.+8		! Set databyte if it is wanted
	   st	%g5, [%l7 + DIB_REGS + (8+3)*4]
	
!   We now have verified that the busyness state of the jumpee will
!   allow the jump to proceed.  The data string has been passed.

	set	EXIT_JUMPIMPLICIT, %g3

! set ordercode= 0x80000000 + trapcode
! cpup3key is prepared domain key to the domain

	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

	ba	asm_trap_common		! Join common code
	  ld	[%g7 + DIB_ROOTNODE], %g5

dom_trap_bail_unpreplock:
	mov	HILRU, %l3
	stb	%l3, [%g4 + NODE_PREPLOCK]

dom_trap_bail:
	SETUP_KERNEL_STACK
#else
	SWITCH_TO_KERNEL(%l7)
#endif
	ba	gotdomain
	  nop

	SET_SIZE(dom_trap)



asm_setupdestpageandmovestr:
! This routine sets up the jumpee's parm page and moves the string to it.

! WARNING - Highly non-standard linkage

! Input:
	! %g1 Holds the address of the "call" instruction
	! %g3 Holds the exit block
	! %g7 points to the jumper's dib
	! %l6 holds the jumpee's entry block
	! %l7 points to the jumpee's dib
	! cpuarglength holds the jumper's string length
	! if cpuarglength != 0 then
	!    cpuargcteaddr holds the address of the jumper's argpage's CTE
	!    The CTE is corelocked.
! Output:
	! %g1 Holds the address of the "call" instruction
	! %g3 Holds the exit block
	! %g4 unchanged
	! %g6 unchanged
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

	! All other local and global registers may be changed
	
! Exits:
	! Normal return is via jmp %g1 + 8
	! Error return is via goto latecallgate

! Begin setupdestpage
	set	ENTRY_RESERVED, %l4	! Reserved entry block fields
	btst	%l4, %l6		! check_new_entry_block
	bnz	latecallgate
	  sethi	%hi(cpuarglength), %g2		! Get the jumper's length
	! ... Test for ones in the reserved fields (noped in C code)
	set	ENTRY_NOTFAULT, %l4	! Mark entry block not fault resume
	or	%l4, %l6, %l6

	ld	[%g2 + %lo(cpuarglength)], %g2	! Get the jumper's length
	tst	%g2
	bz	asm_setupdestpageandmovestr_nopage 
	  sethi	%hi(cpuargcteaddr), %g5
	sethi	%hi(ENTRY_STR), %l4
	btst	%l4, %l6
	bz	asm_setupdestpageandmovestr_nostr
	  ld	[%g5 + %lo(cpuargcteaddr)], %g5
	ld	[%l7 + DIB_REGS + (1)*4], %l5	! max parm length
	tst	%l5				! Is parm length zero
	bz,a	asm_setupdestpageandmovestr_exit! Yes - done
	  clr	%g2			! Returned string length is zero
	set	ENTRY_REGSPARM, %l3		! Register parm?
	btst	%l3, %l6
	bnz	latecallgate
	  ld	[%l7 + DIB_REGS + (8+5)*4], %l0	! parm address
	add	%l0, %l5, %l3		! Compute end of string address
	dec	%l3
	xor	%l3, %l0, %l3		! Is it on the same page?
	andncc	%l3, 0xfff, %g0
	bnz	latecallgate		! No - Have gate do it
! begin map_parm_string
! begin map_parm_page
! begin resolve_address with write specified
	! %l0 is the jumpee's parm string address

	  srl	%l0, 28, %l3		! Address too big?
	cmp	%l3, 0x0f	
	be	latecallgate		! Yes - have gate do it
	  ld	[%l7 + DIB_MAP], %l1
	tst	%l1			! is the map NULL_MAP?
	bz	latecallgate		! Yes - Punt

	! End dry run - (but hold handle jumper until common code below)
	! %l6 holds the entry block

	  mov	RMMU_CTX_REG, %l2	! Switch to jumpee's map
	sta	%l1, [%l2]ASI_RMMU

! Skip getting bus address since we will use mapped nofault stores
! Skip addr2cte since we will use mapped nofault stores
! end resolve_address
! end map_parm_page
! skip corelock_page since we will use mapped nofault stores
! end map_parm_string

	! %g2 has the jumper's offered length
	! %g5 has the jumper's page CTE address
	! %l0 has the jumpee's address
	! %l5 has the jumpee's maximum length 
	! %l6 holds the entry block

	cmp	%g2, %l5		! Take min(offered, wanted)
	bleu	asm_offered_smaller
	  ld	[%g5 + CTE_BUSADDRESS], %l1	! Get the physical address
	mov	%l5, %g2		! Set length to wanted
asm_offered_smaller:
	mov	%g2, %l5		! Copy length for loop control

	! on a machine with > 4 gig of memory, we would find busaddress had
	! been shifted right 4 bits to hold the high bits of the physical
	! address.  We would then use those top 4 bits to select the correct
	! copy routine (which alternate address space for the load instructions)

	ld	[%g7 + DIB_REGS + (8+3)*4], %l2	! Get jumper's virtual addr
	andn	%l1, 0xfff, %l1		! Compute starting address
	and	%l2, 0xfff, %l2
	or	%l1, %l2, %l1		! %l1 holds the data's physical address

#if RMMU_CTL_REG != 0
  error - hardware changed
#endif
	set	RMMU_FSR_REG, %l4	! Clear any fault
	lda	[%l4]ASI_RMMU, %g0

	lda	[%g0]ASI_RMMU, %l4	! Set no fault into the RMMU_CTL_REG
!	set	MCR_NF + MCR_AC, %l2
	set	MCR_NF, %l2
	or	%l4, %l2, %l2
	sta	%l2, [%g0]ASI_RMMU
	
	or	%l1, %l0, %l2		! Select on data alignments
	and	%l2, 7, %l2
	sll	%l2, 2, %l2		! times entry size
	set	asm_move_table, %l3
	ld	[%l3 + %l2], %l2
	jmp	%l2
	  sub	%l0, %l1, %l0		! Calc difference in addresses
asm_move_table:
	.word	asm_move_double		! addresses end in 3 zeroes
	.word	asm_move_byte		! addresses end in 0 zeroes
	.word	asm_move_half		! addresses end in 1 zeroes
	.word	asm_move_byte		! addresses end in 0 zeroes
	.word	asm_move_word		! addresses end in 2 zeroes
	.word	asm_move_byte		! addresses end in 0 zeroes
	.word	asm_move_half		! addresses end in 1 zeroes
	.word	asm_move_byte		! addresses end in 0 zeroes

	! %g5 has the locked page CTE address
	! %l0 has the jumpee's address - low 32 bits of jumper`s physical addr
	! %l1 has the low 32 bits of the jumper's data's physical address
	! %l4 has the old contents of the RMMU_CTL_REG
	! %l5 has the length
asm_move_double:
	deccc	8, %l5
	bl,a	2f
	  inccc	4, %l5
1:	ldda	[%l1]ASI_PASSMEM, %l2
	deccc	8, %l5
	std	%l2, [%l1 + %l0]
	bge	1b
	  inc	8, %l1

	inc	8, %l5
asm_move_word:
	deccc	4, %l5
2:	bl,a	2f
	  inccc	2, %l5
1:	lda	[%l1]ASI_PASSMEM, %l2
	deccc	4, %l5
	st	%l2, [%l1 + %l0]
	bge	1b
	  inc	4, %l1

	inc	4, %l5
asm_move_half:
	deccc	2, %l5
2:	bl,a	2f
	  inccc	1, %l5
1:	lduha	[%l1]ASI_PASSMEM, %l2
	deccc	2, %l5
	sth	%l2, [%l1 + %l0]
	bge	1b
	  inc	2, %l1

	inc	2, %l5
asm_move_byte:
	deccc	1, %l5
2:	bl	2f
	  nop
1:	lduba	[%l1]ASI_PASSMEM, %l2
	deccc	1, %l5
	stb	%l2, [%l1 + %l0]
	bge	1b
	  inc	1, %l1

2:	sta	%l4, [%g0]ASI_RMMU	! Restore the RMMU_CTL_REG
	set	RMMU_FSR_REG, %l4	! Read the FSR
	lda	[%l4]ASI_RMMU, %l2

	mov	RMMU_CTX_REG, %l0	! Back to kernel's context
	mov	kernCtx, %l1
	sta	%l1, [%l0]ASI_RMMU

	btst	SFSREG_FT,%l2		! Any faults?
	bnz	latecallgate
	  .empty				! No - Return

asm_setupdestpageandmovestr_exit:

	set	ENTRY_STRLEN, %l0
	btst	%l0, %l6
	bnz,a	.+8		! Set the string length if wanted
	   st	%g2, [%l7 + DIB_REGS + (8+4)*4]
asm_setupdestpageandmovestr_nostr:
	ldub	[%g5 + CTE_CORELOCK], %l1	! unlock jumper's CTE
	dec	8, %l1
	bset	HILRU, %l1
	jmp	%g1 + 8			! Return
	  stb	%l1, [%g5 + CTE_CORELOCK]

asm_setupdestpageandmovestr_nopage:
	set	ENTRY_STRLEN, %l4
	btst	%l4, %l6		! Want length
	bnz,a	.+8			! Yes - store it
	  clr	[%l7 + DIB_REGS + (8+4)*4] ! Returned string length is zero
	jmp	%g1 + 8			! Return
	nop





! Handle a CALL, FORK or RETURN
! Registers:
!   %l0 - psr
!   %l1 - PC
!   %l2 - nPC

!   %i0 - ordercode
!   %i1 - exitblock
!   %i4 - argument string length

        ENTRY(keykos_trap)
	btst	PSR_PS, %l0		! test pS
	bnz	_fault			! shouldn't get this in kernel
#if DEBUG>=2
	mov	%wim, %l3		! Get the wim
	mov	1, %l5			! Calculate current win's mask
	sll	%l5,%l0,%l5
	btst	%l5, %l3
	set	nwin_minus_one, %l6
	bz	9f
	ld	[%l6],%l6
		!
		! Get old WIM and free %g1 for use
		!
		mov     %g1, %l7        ! Save %g1 in local 7.
		!
		! Compute new Window Invalid Mask
		!
		srl     %l3, 1, %g1		! Perform ror(WIM, 1, NW)
		sll     %l3, %l6, %l4		!  to form new
		or      %l4, %g1, %g1	!  New Window Invalid Mask.
	        set	RMMU_FSR_REG, %l5 
		lda     [%l5]ASI_RMMU, %g0      ! clear speculative faults
		set	RMMU_CTL_REG, %l5	! Set no fault into the mmu
		lda	[%l5]ASI_RMMU, %l6
		bset	MCR_NF,%l6
		sta	%l6, [%l5]ASI_RMMU	
		save                    ! Enter the window we'll save,
		mov     %g1, %wim       !  and then set the WIM
				        !  to mark it as the invalid window.
					! Save its contents on the stack.
		stda	%l0, [%sp]ASI_UD
		add	%sp, 8, %l0
		stda	%l2, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%l4, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%l6, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%i0, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%i2, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%i4, [%l0]ASI_UD
		add	%l0, 8, %l0
		stda	%i6, [%l0]ASI_UD
		restore                 ! Now return to the trap window.
		bclr	MCR_NF,%l6	! Set no fault off in the mmu
		sta	%l6, [%l5]ASI_RMMU
		set	RMMU_FSR_REG, %l5	! Read the FSR
		lda	[%l5]ASI_RMMU, %l6
		andcc	%l6,SFSREG_FT,%g0	! Any faults?
		bz	9f			! No - Go finish up
		  mov	%l7, %g1	! Restore %g1

		sethi	%hi(cpu_mmu_fsr), %l7	! Save the FSR
		st	%l6, [%l7 + %lo(cpu_mmu_fsr)]

		sethi	%hi(cpudibp), %l5
		ld	[%l5 + %lo(cpudibp)], %l5
  		SWITCH_TO_KERNEL(%l5)		! Save domain's registers

		set	RMMU_FAV_REG, %l7	! Save the fault address
		lda	[%l7]ASI_RMMU, %l7
		sethi	%hi(cpu_mmu_addr), %l6
		call	handle_data_obstacle
		st	%l7, [%l6 + %lo(cpu_mmu_addr)]

		ba	return_from_user_exception
		nop
9:
#if DEBUG>=1
	set	0x00000f00, %l4	! Enable interrupts at level 15
	or	%l0, %l4, %l4
	mov	%l4, %psr
	wr	%l4, PSR_ET, %psr
#endif
#endif
	KERNEL_CYCLES
#if 0 /* NHxy */
    sethi %hi(xlsCnt), %l4
    ld [%l4 + %lo(xlsCnt)], %l5
    addcc %l5, 1, %l5
    st %l5, [%l4 + %lo(xlsCnt)]
    tz 0x75
#endif
	sethi	%hi(cpudibp), %l7
	sethi	%hi(lowcoreflags), %l4
	ld	[%l7 + %lo(cpudibp)], %l7	! jumper's DIB

! Begin codeing gate inline

	ldub	[%l4 + %lo(lowcoreflags)], %l4
	btst	0x80, %l4		! gatejumplogenable == 1?
!	bnz	callgatenopage		! Give up if logging jumps
        ba      callgatenopage
	  ldub	[%l7 + DIB_PERMITS], %l5
#if 0
This code is out as long as the unconditional branch just above
is in place.
	btst	DIB_GATEJUMPSPERMITTED, %l5
	srl	%i1, EXIT_ARGTYPESHIFT-2, %l5	! Get argtype * 4
	and	%l5, 0x0c, %l5		! Get switch table offset
	set	exit_argtype_tbl, %l3
	bz	callgatenopage
	  ld	[%l3 + %l5], %l3
	clr	%l4		! Set arg page CTE to NULL
	jmp	%l3
	  clr	%l5		! set string length zero
exit_argtype_tbl:
	.word	asm_keyjump	! 0 - EXIT_ARGNONE
	.word	exit_argmemory	! 1 - EXIT_ARGMEMORY
	.word	callgatenopage	! 2 - Invalid
	.word	callgatenopage	! 3 - EXIT_ARGREGS
	
exit_argmemory:			! String argument is in memory
	orcc	%i4, %g0, %l5		! Copy and test length for zero
	bz	asm_keyjump		! No string - go, length in %l5
	set	4096, %l3
	cmp	%i4, %l3
	bgu	callgatenopage		! Arg string is too long
	add	%i3, %l5, %l3		! Compute end of string address
	dec	%l3
	xor	%l3, %i3, %l3		! Is it on the same page?
	andncc	%l3, 0xfff, %g0
	bnz	callgatenopage		! No - Have gate do it
! begin map_arg_string
! begin map_arg_page
! begin resolve_address
	srl	%i3, 28, %l3		! Address too big?
	cmp	%l3, 0x0f	
	be	callgatenopage		! Yes - have gate do it
	andn	%i3, 0xfff, %l3		! Probe MMU for translation
	lda	[%l3]ASI_FLPR, %l3
	and	%l3, 3, %l4
	cmp	%l4, 2			! Translation valid?
! begin addr2cte((translation & ~0xff)<<4)
	sethi	%hi(last_memseg), %l6	! Check most recent addr->cte xlation
	bne	callgatenopage
	  ld	[%l6 + %lo(last_memseg)], %l6
	srl	%l3, 12-4, %l3		! compute busaddress>>PAGESHIFT
	ld	[%l6 + MEMSEG_PAGES_BASE], %l4
	cmp	%l3, %l4		
	bl	arg_addr2cte_wholetable
	  ld	[%l6 + MEMSEG_PAGES_END], %l5
	cmp	%l3, %l5
	bl	arg_addr2cte_gotmemseg
	nop
arg_addr2cte_wholetable:
	sethi	%hi(memsegs), %l6	! Have to scan the whole table
	ld	[%l6 + %lo(memsegs)], %l6
arg_addr2cte_loop:
	tst	%l6
	bz	callgatenopage
	  ld	[%l6 + MEMSEG_PAGES_BASE], %l4
	cmp	%l3, %l4		
	bl	arg_addr2cte_next
	  ld	[%l6 + MEMSEG_PAGES_END], %l5
	cmp	%l3, %l5
	bl	arg_addr2cte_gotnewlast
	nop
arg_addr2cte_next:
	ba	arg_addr2cte_loop
	  ld	[%l6 + MEMSEG_NEXT], %l6
	! %l6 points to memseg list entry
	! %l4 has MEMSEG_PAGES_BASE from that entry
arg_addr2cte_gotnewlast:
	sethi	%hi(last_memseg), %l5
	st	%l6, [%l5 + %lo(last_memseg)]
arg_addr2cte_gotmemseg:
	sub	%l3, %l4, %l3
#if 0x28 != CTE_SIZEOF
  Error -  Need to correct CTE multiplication code below
#endif
	sll	%l3, 5, %l4
	sll	%l3, 3, %l3
	add	%l3, %l4, %l3
	ld	[%l6 + MEMSEG_CTES], %l4
	add	%l3, %l4, %l4	! %l4 has address of CTE.
! end addr2cte
! end resolve_address
! end map_arg_page
! begin corelock_page
	ldub	[%l4 + CTE_CORELOCK], %l3
	inc	8, %l3
	stb	%l3, [%l4 + CTE_CORELOCK]
! end corelock_page
! end map_arg_string
	mov	%i4, %l5		! Set length of string for below
! Begin coding keyjump inline
asm_keyjump:
	! %l4 is pointer to arg string CTE or NULL
	! %l5 is length of argument string
	! %l7 points to the jumper's dib

	sethi	%hi(cpuargcteaddr), %l3
	st	%l4, [%l3 + %lo(cpuargcteaddr)]

	mov	kernCtx, %l3		/* Set kernel context number */
	mov	RMMU_CTX_REG, %l4
	sta	%l3, [%l4]ASI_RMMU

	sethi	%hi(cpuarglength), %l3	! Save jumper`s arg length
	st	%l5, [%l3 + %lo(cpuarglength)]

	set	numberkeyinvstarted, %l5
	ld	[%l7 + DIB_KEYSNODE], %l6
	srl	%i1, EXIT_GATE_SHIFT-KEYINDEXTOOFFSETSHIFT, %l4
	and	%l4, KEYOFFSETMASK, %l4
	add	%l4, %l6, %l6		! Addr of gate key - sizeof(nodehead)
	ldsb	[%l6 + KEY_TYPE + NODEHEAD_SIZEOF], %l4	! Get key type
	sll	%l4, 2, %l3		! Convert to table offset
	and	%l3, KEY_TYPEMASK<<2, %l3
	add	%l3, %l5, %l5
	and	%l4, (KEY_TYPEMASK+1)<<2, %l4 ! Get prep bit as hi bit of offset
	add	%l4, %l3, %l3
	ld	[%l5], %l4		! Get jumps by type counter
	inc	%l4			! Increment jumps by type counter
	st	%l4, [%l5]		! Store away jumps by type counter
	set	asm_jump_table, %l4
	ld	[%l3 + %l4], %l3
	jmp	%l3
	  ld	[%l6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l4 ! Get key subject
#endif

asm_jump_table:	.align 4
	.word	callgate	! 0 - Unprepared datakey
	.word	callgate	! 1 - Unprepared pagekey
	.word	callgate	! 2 - Unprepared segmentkey
	.word	callgate	! 3 - Unprepared nodekey
	.word	callgate	! 4 - Unprepared meterkey
	.word	callgate	! 5 - Unprepared fetchkey
	.word	callgate	! 6 - Unprepared startkey
	.word	callgate	! 7 - Unprepared resumekey
	.word	callgate	! 8 - Unprepared domainkey
	.word	_fault		! 9 - Unprepared hookkey
	.word	callgate	! 10 - Unprepared misckey
	.word	callgate	! 11 - Unprepared nrangekey
	.word	callgate	! 12 - Unprepared prangekey
	.word	callgate	! 13 - Unprepared chargesetkey
	.word	callgate	! 14 - Unprepared sensekey
	.word	callgate	! 15 - Unprepared devicekey
	.word	callgate	! 16 - Unprepared copykey
	.word	_fault		! 17 - Unprepared **invalid**
	.word	_fault		! 18 - Unprepared **invalid**
	.word	_fault		! 19 - Unprepared **invalid**
	.word	_fault		! 20 - Unprepared **invalid**
	.word	_fault		! 21 - Unprepared **invalid**
	.word	_fault		! 22 - Unprepared **invalid**
	.word	_fault		! 23 - Unprepared **invalid**
	.word	_fault		! 24 - Unprepared **invalid**
	.word	_fault		! 25 - Unprepared **invalid**
	.word	_fault		! 26 - Unprepared **invalid**
	.word	_fault		! 27 - Unprepared **invalid**
	.word	_fault		! 28 - Unprepared **invalid**
	.word	_fault		! 29 - Unprepared **invalid**
	.word	_fault		! 30 - Unprepared **invalid**
	.word	_fault		! 31 - Unprepared **invalid**
	.word	_fault		! 0 - Prepared datakey
	.word	callgate	! 1 - Prepared pagekey
	.word	asm_jsegment	! 2 - Prepared segmentkey
	.word	callgate	! 3 - Prepared nodekey
	.word	callgate	! 4 - Prepared meterkey
	.word	callgate	! 5 - Prepared fetchkey
	.word	asm_jresume	! 6 - Prepared startkey
	.word	asm_jresume	! 7 - Prepared resumekey
	.word	asm_jdomain	! 8 - Prepared domainkey
	.word	callgate	! 9 - Prepared hookkey
	.word	_fault		! 10 - Prepared misckey
	.word	_fault		! 11 - Prepared nrangekey
	.word	_fault		! 12 - Prepared prangekey
	.word	_fault		! 13 - Prepared chargesetkey
	.word	callgate	! 14 - Prepared sensekey
	.word	_fault		! 15 - Prepared devicekey
	.word	_fault		! 16 - Prepared copykey
	.word	_fault		! 17 - Prepared **invalid**
	.word	_fault		! 18 - Prepared **invalid**
	.word	_fault		! 19 - Prepared **invalid**
	.word	_fault		! 20 - Prepared **invalid**
	.word	_fault		! 21 - Prepared **invalid**
	.word	_fault		! 22 - Prepared **invalid**
	.word	_fault		! 23 - Prepared **invalid**
	.word	_fault		! 24 - Prepared **invalid**
	.word	_fault		! 25 - Prepared **invalid**
	.word	_fault		! 26 - Prepared **invalid**
	.word	_fault		! 27 - Prepared **invalid**
	.word	_fault		! 28 - Prepared **invalid**
	.word	_fault		! 29 - Prepared **invalid**
	.word	_fault		! 30 - Prepared **invalid**
	.word	_fault		! 31 - Prepared **invalid**


! At entry to all all the asm_j... routines:
	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l4 Points to the subject of the invoked key (if key is prepared)
	! %l6 points to the gate key - sizeof(nodeheader)
	! %l7 points to the jumper's dib

asm_jdomain:
! Begin coding jdomain inline
! Begin coding md_jdomain inline
	cmp	%i0, Domain_ResetSparcStuff
	bne	callgate
	  srl	%i1, EXIT_JUMPTYPE_SHIFT, %l3 ! Only returns w/4th key + memarg
	and	%l3, (EXIT_ARGTYPE+EXIT_JUMPTYPE+EXIT_KEYMASK1) >>EXIT_JUMPTYPE_SHIFT, %l3
	cmp	%l3, (EXIT_ARGMEMORY+EXIT_JUMPRETURN+EXIT_KEYMASK1) >>EXIT_JUMPTYPE_SHIFT
	bne	callgate
	  ld	[%l7 + DIB_KEYSNODE], %l6
	sll	%i1, KEYINDEXTOOFFSETSHIFT, %l3
	and	%l3, KEYOFFSETMASK, %l3
	add	%l3, %l6, %l6		! Addr of gate key - sizeof(nodehead)
	ldub	[%l6 + KEY_TYPE + NODEHEAD_SIZEOF], %l3	! Get key type
	cmp	%l3, KEY_TYPE_PREPARED+KT_RESUMEKEY	! Must be prep resume
	bne	  callgate
	  ld	[%l6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l3
	cmp	%l3, %l4
	bne	callgate		! Must return to domain being reset
! string is (64,dibregs) (64,curbackset) (16,pc npc psr (2,0) (2,trapcode) 
! 	    (8,trapcodeextension) (4,fsr) (8,1st queued fp))

	  cmp	%i4, 64+64+16+8+4+8	! Offered enough?
	blu	callgate
	  btst	0x7, %i3		! Double aligned?
	bnz	callgate
	  ldub	[%l4 + NODE_PREPCODE], %l3
	cmp	%l3, PREPASDOMAIN	! Is target domain prepared?
	bne	callgate
	  ldub	[%l4 + NODE_PREPLOCK], %l3	! Preplock jumpee
	btst	0x80, %l3
	bnz	callgate
	  mov	LOCKEDBY_ASM_JDOMAIN, %l3
	stb	%l3, [%l4 + NODE_PREPLOCK]

	! At this point we have the jumper and the jumpee prepared as domains.
	! Both the jumper and the jumpee's domain roots are preplocked.
 
	! Begin Check gate type vs. busyness

	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l4 Points to the root node of the jumpee
	! %l6 points to the resume key - sizeof(nodeheader)
	! %l7 points to the jumper's dib

	! At this point, either the jump will complete, it will stall, the
	! jumpee`s parm page isn't in, or really improbable things will happen
	! It is time to put away the jumper's registers.

	PUTAWAY_GLOBALS(%l7)
	mov	%l0, %g1		! Copy psr for putaway windows
	mov	%l6, %g6		! Save key pointer over putaway
	PUTAWAY_WINDOWS(%l7)

	ld	[%g7 + DIB_REGS + (8+1)*4], %g3		! Recover exitblock
	ldub	[%g6 + KEY_TYPE + NODEHEAD_SIZEOF], %g5	! Recover keytype
	cmp	%g5, KT_RESUMEKEY + KEY_TYPE_PREPARED
	ld	[%g6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %g4 ! Recover jumpee
	ld	[%g4 + NODE_DIB], %l7			! Get jumpee's dib

	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 The key type of the gate key
	! %g6 points to the resume key - sizeof(nodeheader)
	! %g7 points to the jumper's dib
	! %l7 points to the jumpee's dib

	! We are invoking a domain key w/resume key to same domain as 4th key


	ldub	[%l7 + DIB_READINESS], %l3
	btst	DIB_BUSY, %l3
	bz	latecallgate

	! Since Domain_ResetSparcStuff does not return a string:
	! End dry run - (but hold handle jumper until common code below)

	  bclr	DIB_TRAPPED, %l3	! Reset trapped, trapcode=0 set below
	stb	%l3, [%l7 + DIB_READINESS]

! Copy the string to the dib of the domain/resume key
! string is (64,dibregs) (64,curbackset) (16,pc npc psr (2,0) (2,trapcode) 
! 	    (8,trapcodeextension) (4,fsr) (8,1st queued fp))

	sethi	%hi(cpuargcteaddr), %g5
	ld	[%g5 + %lo(cpuargcteaddr)], %g5

	! %g5 has the jumper's page CTE address

	! on a machine with > 4 gig of memory, we would find busaddress had
	! been shifted right 4 bits to hold the high bits of the physical
	! address.  We would then use those top 4 bits to select the correct
	! copy routine (which alternate address space for the load instructions)

	ld	[%g7 + DIB_REGS + (8+3)*4], %l2	! Get jumper's virtual addr
	and	%l2, 0xfff, %l2
	ld	[%g5 + CTE_BUSADDRESS], %l1	! Get the physical address
	andn	%l1, 0xfff, %l1		! Compute starting address
	or	%l1, %l2, %l0		! %l0 holds the data's physical address


!	lda	[%g0]ASI_RMMU, %l4	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %l5
!	or	%l5, %l4, %l5
!	sta	%l5, [%g0]ASI_RMMU

	ldda	[%l0]ASI_PASSMEM, %l2		! Set DIB_REGS
	std	%l2, [%l7 + DIB_REGS]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 8]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 16]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 24]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 32]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 40]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 48]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_REGS + 56]

	ldub	[%l7 + DIB_BACKALLOC], %g1	! Set most recent backset
	add	%l7, DIB_BACKSET, %l2
	sll	%g1, 6, %g1			/* g1 has backalloc*64 */
	add	%l2, %g1, %g1			/* g1 has addr of current */
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 8]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 16]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 24]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 32]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 40]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 48]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%g1 + 56]

	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_PC]
	inc	8, %l0
	lda	[%l0]ASI_PASSMEM, %l2		! psr
#if DIB_GATEJUMPSPERMITTED != 1 || PSR_S != 0x80
... Following code is now bad
#endif
	ldub	[%l7 + DIB_PERMITS], %l5
	srl	%l2, 7, %l3	! Move PSR_S bit to DIB_GATEJUMPSPERMITTED
	bclr	DIB_GATEJUMPSPERMITTED, %l5
	sth	%g0, [%l7 + DIB_TRAPCODE]	! zero trapcode
	bclr	~DIB_GATEJUMPSPERMITTED, %l3
	bset	%l3, %l5
	stb	%l5, [%l7 + DIB_PERMITS]
	set	PSR_ICC, %l3			! (Re)set required bits in PSR
	and	%l2, %l3, %l2
	bset	PSR_S, %l2
	st	%l2, [%l7 + DIB_PSR]

	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	std	%l2, [%l7 + DIB_TRAPCODEEXTENSION]
	inc	8, %l0
	ldda	[%l0]ASI_PASSMEM, %l2
	st	%l2, [%l7 + DIB_FSR]
	mov	%l3, %l2
	inc	8, %l0
	lda	[%l0]ASI_PASSMEM, %l3
	std	%l2, [%l7 + DIB_DEFERRED_FP]

!	lda	[%g0]ASI_RMMU, %l4	! Restore alternate cachable
!	set	MCR_AC, %l5
!	andn	%l4, %l5, %l5
!	sta	%l5, [%g0]ASI_RMMU

	ldub	[%g5 + CTE_CORELOCK], %l4	! unlock jumper's CTE
	dec	8, %l4
	bset	HILRU, %l4
	stb	%l4, [%g5 + CTE_CORELOCK]

	set	EXIT_JUMPRETURN, %g3		! Set exit block from jdomain

	ldub	[%g6 + KEY_DATABYTE + NODEHEAD_SIZEOF], %l4
	cmp	%l4, RETURNRESUME
	bne,a	asm_zapresumes
	  clr	%l6		! Zero entry block for non returnresume
	ld	[%l7 + DIB_REGS + (8+2)*4], %l6	! Get entry block
	set	ENTRY_RCZERO, %l2
	ba	asm_zapresumes
	  bset	%l2, %l6
	! %l6 holds the entry block for asm_zapresumes




asm_jsegment:
! Begin coding jsegment inline
	srl	%i1, EXIT_JUMPTYPE_SHIFT, %l3	! Only process calls
	and	%l3, EXIT_JUMPTYPE>>EXIT_JUMPTYPE_SHIFT, %l3
	cmp	%l3, EXIT_JUMPCALL>>EXIT_JUMPTYPE_SHIFT
	bne	callgate
	  nop
        std     %g2, [%l7 + DIB_REGS + 8] ! Get some regsiters to use
        std     %g4, [%l7 + DIB_REGS + 16]
! Begin check_format_key
	ldub	[%l6 + KEY_DATABYTE + NODEHEAD_SIZEOF], %l3
	btst	KEY_DATABYTE_NOCALL + 0x0f, %l3
	bnz	callgaterestore		! Black or nocall -> exit
	  nop
	ldub	[%l4 + (15*KEY_SIZEOF) + KEY_TYPE + NODEHEAD_SIZEOF], %l3
	bclr	KEY_TYPE_INVOLVEDW, %l3
	cmp	%l3, KT_DATAKEY
	bnz	callgaterestore
	  nop
	ld	[%l4+(15*KEY_SIZEOF)+KEY_DK7_DATABODY+3+NODEHEAD_SIZEOF], %g3
	set	240<<8, %g2		! Check for a keeper
	and	%g2, %g3, %l5
	cmp	%g2, %l5
	be	callgaterestore		! == X'f0' --> no keeper - go
	  nop
	set	(0xa0<<24), %g2
	btst	%g2, %g3		! reserved bits?
	bnz	callgaterestore
	  nop
! End check_format_key
	set	0x40<<24, %g2		! Kernel supernode?
	btst	%g2, %g3		
	bz	callgaterestore		! No ... change for asm sn keep call
	  nop
	cmp	%i0, 42			! Order code in range for sn?
	bgu	callgaterestore		! No ... change for asm sn keep call
	  nop
	sll	%i0, 2, %g2
	set	ksn_table, %l5
	ld	[%g2 + %l5], %l5
	jmp	%l5
	  nop

ksn_table: .align 4
	.word	asm_sn_fetch	! 0
	.word	asm_sn_fetch	! 1
	.word	asm_sn_fetch	! 2
	.word	asm_sn_fetch	! 3
	.word	asm_sn_fetch	! 4
	.word	asm_sn_fetch	! 5
	.word	asm_sn_fetch	! 6
	.word	asm_sn_fetch	! 7
	.word	asm_sn_fetch	! 8
	.word	asm_sn_fetch	! 9
	.word	asm_sn_fetch	! 10
	.word	asm_sn_fetch	! 11
	.word	asm_sn_fetch	! 12
	.word	asm_sn_fetch	! 13
	.word	asm_sn_fetch	! 14
	.word	asm_sn_fetch	! 15
	.word	asm_sn_swap	! 16
	.word	asm_sn_swap	! 17
	.word	asm_sn_swap	! 18
	.word	asm_sn_swap	! 19
	.word	asm_sn_swap	! 20
	.word	asm_sn_swap	! 21
	.word	asm_sn_swap	! 22
	.word	asm_sn_swap	! 23
	.word	asm_sn_swap	! 24
	.word	asm_sn_swap	! 25
	.word	asm_sn_swap	! 26
	.word	asm_sn_swap	! 27
	.word	asm_sn_swap	! 28
	.word	asm_sn_swap	! 29
	.word	asm_sn_swap	! 30
	.word	asm_sn_swap	! 31
	.word	callgaterestore	! 32
	.word	callgaterestore	! 33
	.word	callgaterestore	! 34
	.word	callgaterestore	! 35
	.word	callgaterestore	! 36
	.word	callgaterestore	! 37
	.word	callgaterestore	! 38
	.word	callgaterestore	! 39
	.word	callgaterestore	! 40
	.word	asm_sn_fetch_s	! 41
	.word	asm_sn_swap_s	! 42

asm_sn_swap_s:
	cmp	%i4, 4		! Length 4?
	bne	callgaterestore	! No - Punt
	  nop
	btst	3, %i3		! Word aligned?
	bnz	callgaterestore
	  nop
	srl	%i1, EXIT_ARGTYPESHIFT, %l5	! Get argtype 
	and	%l5, 0x03, %l5
	cmp	%l5, EXIT_ARGMEMORY>>EXIT_ARGTYPESHIFT	! Check string type
	bne	callgaterestore
	  nop
	sethi	%hi(cpuargcteaddr), %l5
	ld	[%l5 + %lo(cpuargcteaddr)], %l5

!	lda	[%g0]ASI_RMMU, %g4	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %g5
!	or	%g5, %g4, %g5
!	sta	%g5, [%g0]ASI_RMMU

	ld	[%l5 + CTE_BUSADDRESS], %l5	! Get the physical address

	and	%i3, 0xfff, %l4

	! on a machine with > 4 gig of memory, we would find busaddress had
	! been shifted right 4 bits to hold the high bits of the physical
	! address.  We would then use those top 4 bits to select the correct
	! copy routine (which alternate address space for the load instructions)

	andn	%l5, 0xfff, %l5		! Compute starting address
	add	%l5, %l4, %l5		! %l5 holds the data's physical address
	
	lda	[%l5]ASI_PASSMEM, %l3

	sta	%g4, [%g0]ASI_RMMU	! Restore alternate cachable
	bz	1f		! No - Continue
	  ld	[%l6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l4	! Restore %l4
	ba	callgaterestore
	  nop

asm_sn_swap:
	mov	%i0, %l3	! Copy slot number
1:	
	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l3 has the supernode slot number		- aka slot
	! %l4 Points to the subject of the invoked key	- aka node
	! %l6 points to the gate key - sizeof(nodeheader)
	! %l7 points to the jumper's dib
	! %g3 has the format datakey's low word		- aka depth

	sll	%g3, 2, %g3		! Get depth * 4
	andcc	%g3, 0x0f<<2, %g3
	bz	callgaterestore
	  ldub	[%l6 + KEY_DATABYTE + NODEHEAD_SIZEOF], %l5
	btst	0x80, %l5
	bnz	callgaterestore
	  mov	15, %l5			! Set limit	- aka limit
	clr	%g2			! Set 1st index	- aka index
asm_sn_swap_loop:
#if KEY_SIZEOF != 16
 error - key size changed
#endif
	sll	%g2, 4, %g2		! Convert index to key offset
	add	%g2, %l4, %g2		! %g2 is now key pointer
	ldub	[%g2 + NODEHEAD_SIZEOF + KEY_TYPE], %g4
	btst	KEY_TYPE_INVOLVEDR, %g4
	bnz	callgaterestore
	  btst	KEY_TYPE_PREPARED, %g4
	bnz	asm_snf_endtryprep
! Start tryprep
	  and	%g4, KEY_TYPEMASK, %g5
	set	tryprep_table, %l4
	ldsb	[%l4 + %g5], %l4
	tst	%l4
	bnz	callgaterestore
	  .empty
asm_snf_endtryprep:
	tst	%g3			! Is depth zero?
	bz	asm_sn_swap_gotit
	  cmp	%g3, 8<<2
	bg	callgaterestore
	  bclr	KEY_TYPE_INVOLVEDW, %g4
	cmp	%g4, KT_NODEKEY + KEY_TYPE_PREPARED
	bne	callgaterestore
	  ld	[%g2 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l4
	ldub	[%g2 + KEY_DATABYTE + NODEHEAD_SIZEOF], %g3
	sll	%g3, 2, %g3
	srl	%l3, %g3, %g2		! %g2 is new index
	cmp	%g2, 15			! index out of range?
	bgu	callgaterestore
	  sll	%g2, %g3, %g4
	deccc	%l5
	bg	asm_sn_swap_loop
	  sub	%l3, %g4, %l3
	ba	callgaterestore
	  nop

asm_sn_swap_gotit:
	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l6 points to the invoked key - sizeof(nodeheader)
	! %l7 points to the jumper's dib
	! %g2 points to the key to swap - sizeof(nodeheader)
	! %g4 has the key type of the key to swap
	
	btst	KEY_TYPE_INVOLVEDR + KEY_TYPE_INVOLVEDW, %g4
	bnz	callgaterestore
	  ldd	[%g2 + NODEHEAD_SIZEOF], %l4	! cpup1key = keytoswap
	set	cpup1key, %g3
	std	%l4, [%g3]
	ldd	[%g2 + NODEHEAD_SIZEOF + 8], %l4
	std	%l4, [%g3 + 8]

	add	%g2, NODEHEAD_SIZEOF, %g3;	/* Ptr to slot to swap - %g3 */
	btst	KEY_TYPE_PREPARED, %g4;
	bz	2f;
	  ldd	[%g3], %g4;	/* Load links */
	st	%g4, [%g5];	/* unlink key to be overlayed */
	st	%g5, [%g4 + 4]

2:	set	EXIT_KEYMASK8, %g4;
	btst	%g4, %i1;	/* Key offered? */
	srl	%i1, EXIT_KEY1_SHIFT-KEYINDEXTOOFFSETSHIFT, %g5;
	bz	2f;		/* no - go	*/
	  and	%g5, KEYOFFSETMASK, %g5;	/* Get offered key */
	ld	[%l7 + DIB_KEYSNODE], %g4
	add	%g5, %g4, %g2			! offered key-nodehead - %g2

	ldd	[%g2 + 8 + NODEHEAD_SIZEOF], %g4	! Load offered key
	btst	KEY_TYPE_PREPARED, %g5;
	bz	4f;
	  std	%g4, [%g3 + 8];	/* Delay - store subject, db & type */
! Begin halfprep
	and	%g5, KEY_TYPEMASK, %g5
	cmp	%g5, KT_RESUMEKEY
	bnz,a	3f
	  ld	[%g4 + NODE_LEFTCHAIN], %g5
	ldub	[%g4 + NODE_PREPCODE], %g5
	cmp	%g5, PREPASDOMAIN
	bz	5f
	  nop
	ld	[%g4 + NODE_RIGHTCHAIN], %g5
7:	cmp	%g4, %g5
	be,a	3f
	  ld	[%g5 + ITEM_LEFTCHAIN], %g5
	ldub	[%g5 + KEY_TYPE], %l4
	btst	KEY_TYPE_INVOLVEDR + KEY_TYPE_INVOLVEDW, %l4
	bnz,a	7b
	  ld	[%g5 + ITEM_RIGHTCHAIN], %g5
	ba	3f
	  ld	[%g5 + ITEM_LEFTCHAIN], %g5
5:	ld	[%g4 + NODE_DIB], %g5
	ld	[%g5 + DIB_LASTINVOLVED], %g5

3:	st	%g5, [%g3 + KEY_LEFTCHAIN]
	ld	[%g5 + KEY_RIGHTCHAIN], %g2
	st	%g2, [%g3 + KEY_RIGHTCHAIN]
	st	%g3, [%g2 + ITEM_LEFTCHAIN]
	ba	1f
	  st	%g3, [%g5 + ITEM_RIGHTCHAIN]
! End halfprep

4:	ldd	[%g2 + NODEHEAD_SIZEOF], %g4;
	ba	1f
	  std	%g4, [%g3];

2:	clr	%g4
	clr	%g5
	std	%g4, [%g3 + 8]
	std	%g4, [%g3];
	
1:	sethi	%hi(cpup1key), %g2
	ba	asm_sn_end_dryrun
	  inc	%lo(cpup1key), %g2


asm_sn_fetch_s:
	cmp	%i4, 4		! Length 4?
	bne	callgaterestore	! No - Punt
	  nop
	btst	3, %i3		! Word aligned?
	bnz	callgaterestore
	  nop
	srl	%i1, EXIT_ARGTYPESHIFT, %l5	! Get argtype 
	and	%l5, 0x03, %l5
	cmp	%l5, EXIT_ARGMEMORY>>EXIT_ARGTYPESHIFT	! Check string type
	bne	callgaterestore
	  nop
	sethi	%hi(cpuargcteaddr), %l5
	ld	[%l5 + %lo(cpuargcteaddr)], %l5

!	lda	[%g0]ASI_RMMU, %g4	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %g5
!	or	%g5, %g4, %g5
!	sta	%g5, [%g0]ASI_RMMU

	ld	[%l5 + CTE_BUSADDRESS], %l5	! Get the physical address

	and	%i3, 0xfff, %l4

	! on a machine with > 4 gig of memory, we would find busaddress had
	! been shifted right 4 bits to hold the high bits of the physical
	! address.  We would then use those top 4 bits to select the correct
	! copy routine (which alternate address space for the load instructions)

	andn	%l5, 0xfff, %l5		! Compute starting address
	add	%l5, %l4, %l5		! %l5 holds the data's physical address
	
	lda	[%l5]ASI_PASSMEM, %l3

	sta	%g4, [%g0]ASI_RMMU	! Restore alternate cachable
	bz	1f		! No - Continue
	  ld	[%l6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l4	! Restore %l4
	ba	callgaterestore
	  nop

asm_sn_fetch:
	mov	%i0, %l3	! Copy slot number
1:	
	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l3 has the supernode slot number		- aka slot
	! %l4 Points to the subject of the invoked key	- aka node
	! %l6 points to the gate key - sizeof(nodeheader)
	! %l7 points to the jumper's dib
	! %g3 has the format datakey's low word		- aka depth

	sll	%g3, 2, %g3		! Get depth * 4
	andcc	%g3, 0x0f<<2, %g3
	bz	callgaterestore
	  nop
	mov	15, %l5			! Set limit	- aka limit
	clr	%g2			! Set 1st index	- aka index
asm_sn_fetch_loop:
#if KEY_SIZEOF != 16
 error - key size changed
#endif
	sll	%g2, 4, %g2		! Convert index to key offset
	add	%g2, %l4, %g2		! %g2 is now key pointer
	ldub	[%g2 + NODEHEAD_SIZEOF + KEY_TYPE], %g4
	btst	KEY_TYPE_INVOLVEDR, %g4
	bnz	callgaterestore
	  nop
	btst	KEY_TYPE_PREPARED, %g4
	bnz	asm_endtryprep
	  nop
! Start tryprep
	and	%g4, KEY_TYPEMASK, %g5
	set	tryprep_table, %l4
	ldsb	[%l4 + %g5], %l4
	tst	%l4
	bnz	callgaterestore
	  nop
asm_endtryprep:
	tst	%g3			! Is depth zero?
	bz	asm_sn_fetch_gotit
	  nop
	cmp	%g3, 8<<2
	bg	callgaterestore
	  nop
	bclr	KEY_TYPE_INVOLVEDW, %g4
	cmp	%g4, KT_NODEKEY + KEY_TYPE_PREPARED
	bne	callgaterestore
	  nop
	ld	[%g2 + KEY_SUBJECT + NODEHEAD_SIZEOF], %l4
	ldub	[%g2 + KEY_DATABYTE + NODEHEAD_SIZEOF], %g3
	sll	%g3, 2, %g3
	srl	%l3, %g3, %g2		! %g2 is new index
	cmp	%g2, 15			! index out of range?
	bgu	callgaterestore
	  nop
	sll	%g2, %g3, %g4
	sub	%l3, %g4, %l3
	deccc	%l5
	bg	asm_sn_fetch_loop
	  nop
	ba	callgaterestore
	  nop

#define asm_sn_fault 2
tryprep_table:
	.byte	0		! 0 - datakey
	.byte	-1		! 1 - pagekey
	.byte	1		! 2 - segmentkey
	.byte	1		! 3 - nodekey
	.byte	1		! 4 - meterkey
	.byte	1		! 5 - fetchkey
	.byte	1		! 6 - startkey
	.byte	1		! 7 - resumekey
	.byte	1		! 8 - domainkey
	.byte	asm_sn_fault	! 9 - hookkey
	.byte	0		! 10 - misckey
	.byte	0		! 11 - nrangekey
	.byte	0		! 12 - prangekey
	.byte	0		! 13 - chargesetkey
	.byte	1		! 14 - sensekey
	.byte	0		! 15 - devicekey
	.byte	0		! 16 - copykey
	.byte	asm_sn_fault	! 17 - **invalid**
	.byte	asm_sn_fault	! 18 - **invalid**
	.byte	asm_sn_fault	! 19 - **invalid**
	.byte	asm_sn_fault	! 20 - **invalid**
	.byte	asm_sn_fault	! 21 - **invalid**
	.byte	asm_sn_fault	! 22 - **invalid**
	.byte	asm_sn_fault	! 23 - **invalid**
	.byte	asm_sn_fault	! 24 - **invalid**
	.byte	asm_sn_fault	! 25 - **invalid**
	.byte	asm_sn_fault	! 26 - **invalid**
	.byte	asm_sn_fault	! 27 - **invalid**
	.byte	asm_sn_fault	! 28 - **invalid**
	.byte	asm_sn_fault	! 29 - **invalid**
	.byte	asm_sn_fault	! 30 - **invalid**
	.byte	asm_sn_fault	! 31 - **invalid**

asm_sn_fetch_gotit:
	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l6 points to the invoked key - sizeof(nodeheader)
	! %l7 points to the jumper's dib
	! %g2 points to the key to return - sizeof(nodeheader)
	! %g4 has the key type of the key to return
	
	btst	KEY_TYPE_INVOLVEDR + KEY_TYPE_INVOLVEDW, %g4
	bnz	callgaterestore
	  nop
	set	ENTRY_RESERVED, %l3	! Reserved entry block fields
	btst	%l3, %i2		! check_new_entry_block
	bnz	callgaterestore
	  nop

! End assembly dry run
asm_sn_end_dryrun:
	mov	%l2, %l1		! Advance over trap instruction
	inc	4, %l2
	set	ENTRY_RC, %l3		! Want the return code?
	btst	%l3, %i2
	bnz,a	.+8
	  clr	%i0			! Return code 0
	set	ENTRY_STR + ENTRY_STRLEN, %l3
	and	%i2, %l3, %l4		! Want string length?
	cmp	%l3, %l4
	be,a	.+8
	  clr	%l4			! string length 0
	set	ENTRY_DB, %l3		! Want databyte?
	btst	%l3, %i2
	bnz,a	.+8
	  clr	%i3
! Return key to the caller
	ld	[%l7 + DIB_KEYSNODE], %l5	! Get keys node
	add	%l5, NODEHEAD_SIZEOF, %l5	! Point at slot zero
	set	ENTRY_KEYMASK8, %g3
	btst	%g3, %i2		! Jumpee want first key
	bz	1f			! No - Done
	srl	%i2, ENTRY_KEY1_SHIFT-KEYINDEXTOOFFSETSHIFT, %g3;
	and	%g3, KEYOFFSETMASK, %g3;
	add	%g3, %l5, %g3;	/* Ptr to slot to receive key - %g3 */
	ldub	[%g3 + KEY_TYPE], %l4;
	cmp	%l4, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l4;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l4;
	bz	2f;
	ldd	[%g3], %g4;	/* Load links */
	st	%g4, [%g5];	/* unlink key to be overlayed */
	st	%g5, [%g4 + 4];
2:	ldd	[%g2 + 8 + NODEHEAD_SIZEOF], %g4;
	! We rejected involved keys above
	btst	KEY_TYPE_PREPARED, %g5;
	bz	4f;
	  std	%g4, [%g3 + 8];	/* Delay - store subject, db & type */
! Begin halfprep
	and	%g5, KEY_TYPEMASK, %g5
	cmp	%g5, KT_RESUMEKEY
	bnz,a	3f
	  ld	[%g4 + NODE_LEFTCHAIN], %g5
	ldub	[%g4 + NODE_PREPCODE], %g5
	cmp	%g5, PREPASDOMAIN
	bz	5f
	  nop
	ld	[%g4 + NODE_RIGHTCHAIN], %g5
7:	cmp	%g4, %g5
	be,a	3f
	  ld	[%g5 + ITEM_LEFTCHAIN], %g5
	ldub	[%g5 + KEY_TYPE], %l4
	btst	KEY_TYPE_INVOLVEDR + KEY_TYPE_INVOLVEDW, %l4
	bnz,a	7b
	  ld	[%g5 + ITEM_RIGHTCHAIN], %g5
	ba	3f
	  ld	[%g5 + ITEM_LEFTCHAIN], %g5
5:	ld	[%g4 + NODE_DIB], %g5
	ld	[%g5 + DIB_LASTINVOLVED], %g5

3:	st	%g5, [%g3 + KEY_LEFTCHAIN]
	ld	[%g5 + KEY_RIGHTCHAIN], %g2
	st	%g2, [%g3 + KEY_RIGHTCHAIN]
	st	%g3, [%g2 + ITEM_LEFTCHAIN]
	st	%g3, [%g5 + ITEM_RIGHTCHAIN]
! End halfprep
	ba	1f
	  nop

4:	ldd	[%g2 + NODEHEAD_SIZEOF], %g4;
	std	%g4, [%g3];
1:	

! Zero any other keys
!	%l5 points to slot zero of the keys node
	set	ENTRY_KEYMASK4, %g3
	btst	%g3, %i2		! Jumpee want second key
	bz	1f			! No - Done
	srl	%i2, ENTRY_KEY2_SHIFT-KEYINDEXTOOFFSETSHIFT, %g3;
	and	%g3, KEYOFFSETMASK, %g3;
	add	%g3, %l5, %g3;	/* Ptr to slot to receive key - %g3 */
	ldub	[%g3 + KEY_TYPE], %l4;
	cmp	%l4, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l4;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l4;
	bz	2f;
	ldd	[%g3], %g4;	/* Load links */
	st	%g4, [%g5];	/* unlink key to be overlayed */
	st	%g5, [%g4 + 4];
2:	clr	%g4
	clr	%g5
	std	%g4, [%g3 + 8]
	std	%g4, [%g3];
	
1:	set	ENTRY_KEYMASK2, %g3
	btst	%g3, %i2		! Jumpee want second key
	bz	1f			! No - Done
	and	%g3, KEYOFFSETMASK, %g3;
	add	%g3, %l5, %g3;	/* Ptr to slot to receive key - %g3 */
	ldub	[%g3 + KEY_TYPE], %l4;
	cmp	%l4, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l4;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l4;
	bz	2f;
	ldd	[%g3], %g4;	/* Load links */
	st	%g4, [%g5];	/* unlink key to be overlayed */
	st	%g5, [%g4 + 4];
2:	clr	%g4
	clr	%g5
	std	%g4, [%g3 + 8]
	std	%g4, [%g3];	
1:	set	ENTRY_KEYMASK1, %g3
	btst	%g3, %i2		! Jumpee want second key
	bz	1f			! No - Done
	sll	%i2, KEYINDEXTOOFFSETSHIFT, %g3;
	and	%g3, KEYOFFSETMASK, %g3;
	add	%g3, %l5, %g3;	/* Ptr to slot to receive key - %g3 */
	ldub	[%g3 + KEY_TYPE], %l4;
	cmp	%l4, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l4;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l4;
	bz	2f;
	ldd	[%g3], %g4;	/* Load links */
	st	%g4, [%g5];	/* unlink key to be overlayed */
	st	%g5, [%g4 + 4];
2:	clr	%g4
	clr	%g5
	std	%g4, [%g3 + 8]
	std	%g4, [%g3];	
1:
	sethi	%hi(cpuargcteaddr), %l5
	ld	[%l5 + %lo(cpuargcteaddr)], %l5
	tst	%l5
	bz	1f
	  nop
	ldub	[%l5 + CTE_CORELOCK], %l4	! unlock jumper's CTE
	dec	8, %l4
	bset	HILRU, %l4
	stb	%l4, [%l5 + CTE_CORELOCK]
1:
        ldd     [%l7 + DIB_REGS + 8], %g2	! Restore registers
        ldd     [%l7 + DIB_REGS + 16], %g4

	ld	[%l7 + DIB_MAP], %l5	/* Set domain's context number */
	mov	RMMU_CTX_REG, %l4
	sta	%l5, [%l4]ASI_RMMU

	/* Get the cycle and instruction counts */
	sethi	%hi(lowcoreflags), %l7;	
	ldub	[%l7 + %lo(lowcoreflags)], %l7;
	btst	0x40, %l7;			/* counters == 1? */
	bz	1f;				/* No - skip */	

! The following code assumes a small amount of time has passed since the
! kernel cycle/instruction counts were started.  Specifically
! that the hardware counters havn't wrapped more than once.

	lda	[%g0]ASI_MCTRV, %l6;		/* read counter */
	srl	%l6 , MCTRV_ICNT_SHIFT, %l7;	/* get inst count */
	set	MCTRV_CCNT_LIMIT - 1, %l4;	/* mask for count */
	and	%l4, %l6, %l6;			/* hw cycle count */
	set	MCTRV_CCNT_LIMIT, %l5;		/* cycle limit */

	set	cpu_cycle_start, %l3;
	sub	%l5, %l6, %l6;			/* cycles since intr */
	lduh	[%l3 + 6], %l4;			/* Get start hw cycles */
	subcc	%l6, %l4, %l6			/* Elapsed cycles */
	bge	2f				/* Plus - go */
	  sub	%l5, %l7, %l7;			/* inst since intr */
	set	MCTRV_CCNT_LIMIT, %l5
	add	%l6, %l5, %l6			/* Actual elapsed value */
2:	ldd	[%l3], %l4			/* Old start cycles */
	addcc	%l5, %l6, %l5			/* Calculate new start */
	addx	%l4, 0, %l4
	std	%l4, [%l3]

	sethi	%hi(cpudibp), %l3;
	ld	[%l3 + %lo(cpudibp)], %l3;	/* jumper's DIB */
	ldd	[%l3 + DIB_KER_CYCLES], %l4;	/* Add to dib ctr */
	addcc	%l6, %l5, %l5;
	addx	%l4, 0, %l4;
	std	%l4, [%l3 + DIB_KER_CYCLES];

	set	cpu_inst_start, %l6;
	lduh	[%l6 + 6], %l5;			/* Get start hw instrus */
	subcc	%l7, %l5, %l7			/* Elapsed instrus */
	bge	3f				/* Plus - go */
	  .empty
	set	MCTRV_CCNT_LIMIT, %l5
	add	%l7, %l5, %l7			/* Actual elapsed value */
3:	ldd	[%l6], %l4			/* Old start instrus */
	addcc	%l5, %l7, %l5			/* Calculate new start */
	addx	%l4, 0, %l4
	std	%l4, [%l6]

	ldd	[%l3 + DIB_KER_INST], %l4;	/* Add to dib ctr */
	addcc	%l7, %l5, %l5;
	addx	%l4, 0, %l4;
	std	%l4, [%l3 + DIB_KER_INST];
1:
	mov	%l0, %psr			! Restore int condition code
	ba jmp_rett; nop ! Resume domain code
	! nop; nop					! psr delay
	! jmp	%l1				! psr delay
	! rett	%l2				! psr delay

callgaterestore:
        ldd     [%l7 + DIB_REGS + 8], %g2	! Restore registers
	ba	callgate
          ldd     [%l7 + DIB_REGS + 16], %g4


asm_jresume:
! Begin codeing jresume inline
	ldub	[%l4 + NODE_PREPLOCK], %l3	! Preplock jumpee
	btst	0x80, %l3
	bnz	callgate
	  mov	LOCKEDBY_ASM_JRESUME, %l3
	stb	%l3, [%l4 + NODE_PREPLOCK]

	ldub	[%l4 + NODE_PREPCODE], %l3
	cmp	%l3, PREPASDOMAIN
	bne	callgate_unpreplock
	  .empty		! next instruction is a store
	! At this point we have the jumper and the jumpee prepared as domains.
	! Both the jumper and the jumpee's domain roots are preplocked.
 
	! Begin Check gate type vs. busyness

	! %l0 - psr
	! %l1 - PC
	! %l2 - nPC
	! %l4 Points to the root node of the jumpee
	! %l6 points to the gate key - sizeof(nodeheader)
	! %l7 points to the jumper's dib

	! At this point, either the jump will complete, it will stall, the
	! jumpee`s parm page isn't in, or really improbable things will happen
	! It is time to put away the jumper's registers.

	PUTAWAY_GLOBALS(%l7)
	mov	%l0, %g1		! Copy psr for putaway windows
	mov	%l6, %g6		! Save key pointer over putaway
	PUTAWAY_WINDOWS(%l7)

	ld	[%g7 + DIB_REGS + (8+1)*4], %g3		! Recover exitblock
	ldub	[%g6 + KEY_TYPE + NODEHEAD_SIZEOF], %g5	! Recover keytype
	cmp	%g5, KT_RESUMEKEY + KEY_TYPE_PREPARED
	ld	[%g6 + KEY_SUBJECT + NODEHEAD_SIZEOF], %g4 ! Recover jumpee

	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 The key type of the gate key
	! %g6 points to the gate key - sizeof(nodeheader)
	! %g7 points to the jumper's dib
	! icc is result of compare of gate key type with resumekey+prepared

	bne	asm_keyjump_startkey
	  ld	[%g4 + NODE_DIB], %l7
	! We are invoking a resume key
	ldub	[%l7 + DIB_READINESS], %l3
	btst	DIB_BUSY, %l3
	bz	latecallgate
	  ldub	[%g6 + KEY_DATABYTE + NODEHEAD_SIZEOF], %l4
	cmp	%l4, RETURNRESUME
	bne,a	asm_zapresumes
	   clr	%l6		! Zero entry block for non returnresume

	set	asm_setupdestpageandmovestr, %g1
	jmpl	%g1, %g1
	  ld	[%l7 + DIB_REGS + (8+2)*4], %l6	! Get entry block
	! End dry run - (but hold handle jumper until common code below)
	! %l6 holds the entry block
asm_zapresumes:
	set	ENTRY_DB, %l5
	btst	%l5, %l6
	ld	[%l7 + DIB_LASTINVOLVED], %g2
	bnz,a	.+8		! Set zero databyte if it is wanted
	   clr	[%l7 + DIB_REGS + (8+3)*4]
	ld	[%g2 + KEY_RIGHTCHAIN], %l5
asm_zapresumes_zaploop:
	cmp	%l5, %g4		! Key doesn't point to rootnode
	be	asm_zapresumes_rechain
	  ldub	[%l5 + KEY_TYPE], %l4
	cmp	%l4, KT_RESUMEKEY + KEY_TYPE_PREPARED
	bne	asm_zapresumes_rechain
	  ld	[%l5 + KEY_RIGHTCHAIN], %l4
	clr	%g1
	std	%g0, [%l5]
	std	%g0, [%l5+8]
	ba	asm_zapresumes_zaploop
	  mov	%l4, %l5
asm_zapresumes_rechain:
	! rechain the backchain
	st	%l5, [%g2 + KEY_RIGHTCHAIN]
	! st	%g2, [%l5 + KEY_LEFTCHAIN]	Moved below next branch
	! All prepared resumes are zapped. Now zap unprepared resumes
	ldub	[%g4 + NODE_FLAGS], %l4
	btst	NFCALLIDUSED, %l4
	bz	asm_enddryrun
	  st	%g2, [%l5 + KEY_LEFTCHAIN]	! Moved from above
	ld	[%g4 + NODE_CALLID], %l3
	inc	%l3
	tst	%l3
	bz	_fault
	st	%l3, [%g4 + NODE_CALLID]
	bclr	NFCALLIDUSED, %l4
	bset	NFDIRTY, %l4
	ba	asm_enddryrun
	  stb	%l4, [%g4 + NODE_FLAGS]

asm_keyjump_startkey:
! We are invoking a start key
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g6 points to the gate key - sizeof(nodeheader)
	! %g7 points to the jumper's dib
	! %l7 points to the jumpee's dib

	ldub	[%l7 + DIB_READINESS], %l3
	btst	DIB_BUSY, %l3		! If jumpee is busy, get out
	bnz	latecallgate
	  .empty

	set	asm_setupdestpageandmovestr, %g1
	jmpl	%g1, %g1
	  ld	[%l7 + DIB_REGS + (8+2)*4], %l6	! Get entry block

	! End dry run - (but hold handle jumper until common code below)

asm_pass_databyte:
	ldub	[%g6 + KEY_DATABYTE + NODEHEAD_SIZEOF], %l4
	set	ENTRY_DB, %l5
	btst	%l5, %l6
	bnz,a	.+8		! Set databyte if it is wanted
	   st	%l4, [%l7 + DIB_REGS + (8+3)*4]
	
asm_enddryrun:
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

!   We now have verified that the busyness state of the jumpee will
!   allow the jump to proceed.  The data string has been passed.
 
!   Set up key parameters - KEEP IN KEYSNODE since each domain has its own
!	N.B. domain keeper trap manfactures all keys passed

! Begin handlejumper
	ld	[%g7 + DIB_NPC], %l2	! Advance jumper's PC over trap
	st	%l2, [%g7 + DIB_PC]
	add	%l2, 4, %l2
	st	%l2, [%g7 + DIB_NPC]

asm_trap_common:		! Where dom_trap enters
	set	asm_jumptype_table, %l4			! JT Dispatch
	srl	%g3, EXIT_JUMPTYPE_SHIFT-2, %l3		! JT Dispatch
	and	%l3, EXIT_JUMPTYPE >> (EXIT_JUMPTYPE_SHIFT-2), %l3 ! JT Dispatch
	ld	[%l3 + %l4], %l3
	jmp	%l3
	  ld	[%g7 + DIB_ROOTNODE], %g5

asm_jumptype_table:	.align 4
	.word	asm_putawaydomain	! Trap
	.word	asm_putawaydomain	! Call
	.word	asm_return		! Return
	.word	asm_fork		! Fork

asm_return:			! Here for a return

! Begin makeready
	ldub	[%g7 + DIB_READINESS], %l3
	bclr	DIB_BUSY, %l3		! The jumper is now not busy
	ldub	[%g5 + NODE_FLAGS], %g1
	btst	NFREJECT, %g1		! Is there a stall queue?
	ld	[%g5 + NODE_RIGHTCHAIN], %l1
	bz	asm_putawaydomain	! No - Go
	  stb	%l3, [%g7 + DIB_READINESS]

! Begin rundom(stallee)
! Begin zaphook
	! %l1 points to the hook key so we know stallee should have a hook
	! %g5 points to jumper's rootnode (designated by the hook)
	ldub	[%l1 + KEY_TYPE], %l2
	cmp	%l2, KT_PIHK
	bne	_fault			! Not a hook key - error
	! Leave hooked bit on, he goes to cpuqueue
	! We know that the subject should point to the jumper who is prepared
	ld	[%g7 + DIB_LASTINVOLVED], %l2	! Is this hook lastinvolved?
	cmp	%l1, %l2
	bne	zaphook_notlastinvolved
	  ld	[%l1 + KEY_LEFTCHAIN], %l3
	ld	[%g5 + NODE_RIGHTCHAIN], %l0
	cmp	%l0, %l2
	bne	zaphook_morestallees
	  st	%l3, [%g7 + DIB_LASTINVOLVED]	! New lastinvolved

	bclr	NFREJECT, %g1		! Is there nolonger a stall queue
	stb	%g1, [%g5 + NODE_FLAGS]
zaphook_morestallees:
zaphook_notlastinvolved:
	! %l3 has leftchain of hook key
	ld	[%l1 + KEY_RIGHTCHAIN], %l2
	st	%l2, [%l3 + ITEM_RIGHTCHAIN]
	st	%l3, [%l2 + ITEM_LEFTCHAIN]
	! Since we are putting stallee on the cpu queue, 
	!   we don't have to:  -  Set hook to involved dk(1)
	! HOOKED should still be marked in the DIB of the stallee
! End zaphook

#if SCHEDULER == 0 || SCHEDULER == 2
! start read_system_time
	! Already running at system hi interrupt level
	sethi	%hi(system_time), %g2
	ld	[%g2 + %lo(system_time)], %g2
	! Don't need unique times for scheduling - don't muck w/offset
! end read_system_time
! start updprio(stallee)
	! domprio key is involved datakey, no need to check
	ld	[%l1 + NODE_KPRIOTIME - NODE_DOMHOOKKEY], %l3
	subcc	%g2, %l3, %l3	! Compute kclock.hi - kpriotime(np)
	bz	asm_queue_stallee
	  ld	[%l1 + NODE_KPRIO - NODE_DOMHOOKKEY], %l2
	srl	%l3, 6, %l3
	st	%g2, [%l1 + NODE_KPRIOTIME - NODE_DOMHOOKKEY]
	srl	%l2, %l3, %l2	! Decay priority
	st	%l2, [%l1 + NODE_KPRIO - NODE_DOMHOOKKEY]
! end updprio(stallee)
asm_queue_stallee:
	set	cpuqueue, %l5	! Put stallee in correct place on cpuqueue
	mov	%l5, %l4	! Save CPU queue pointer
	ld	[%l5 + QUEUEHEAD_HEAD], %l5	! load right chain
asm_nextinqueue:
	cmp	%l4, %l5
	be	asm_stallee_endofqueue
	  nop	
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l3
	cmp	%l2, %l3
	bgu,a	asm_nextinqueue
	ld	[%l5 + QUEUEHEAD_HEAD], %l5	! load right chain
#if SCHEDULER == 0
! start updprio(entry in cpu queue)
	ldub	[%l5 + NODE_DOMPRIO + KEY_TYPE - NODE_DOMHOOKKEY], %l0
	and	%l0, KEY_TYPEMASK, %l0	! check key type for datakey
	cmp	%l0, KT_DATAKEY
	bne	asm_stallee_endofqueue
	nop
	ld	[%l5 + NODE_KPRIOTIME - NODE_DOMHOOKKEY], %l3
	sub	%g2, %l3, %l3	! Compute kclock.hi - kpriotime(np)
	srl	%l3, 6, %l3
	st	%g2, [%l5 + NODE_KPRIOTIME - NODE_DOMHOOKKEY]
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
	srl	%l0, %l3, %l0	! Decay priority
	st	%l0, [%l5 + NODE_KPRIO - NODE_DOMHOOKKEY]
! end updprio(entry in cpu queue)
#else
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
#endif
	cmp	%l2, %l0
	bgu	asm_nextinqueue
	  nop
asm_stallee_endofqueue:

#else
	set	cpuqueue, %l5	! W/O scheduler put him at tail of cpu queue
	mov	%l5, %l4
#endif

! Stallee goes to left of the item pointed to by %l5
! %l4 points to the cpu queue
	mov	%l5, %l3	! Hook key's right ptr is %l5
	ld	[%l5], %l2	! Hook key's left ptr is %l5.left
	st	%l1, [%l3]	! old%l5.right->left = hook
	mov	KT_PIHK+(1<<8), %l5	! KT is prepared involved hook, db=1
	st	%l1, [%l2 + 4]	! %l5->left.right = hook
	std	%l2, [%l1]	! Store the hook key
	! std  %l4, [%l1 + 8] in delay slot below
! end rundom(stallee)
	! %g1 holds jumper's rootnode flags
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 points to the jumper`s domain root
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib
	btst	NFREJECT, %g1		! Is there a stall queue?
	bz	asm_putawaydomain	! No - Go
	  std	%l4, [%l1 + 8]		! Finish storing hook key
	! Put jumper on the worry queue
	!  Jumper should be on no queue

	ldub	[%g7 + DIB_READINESS], %l5	! Mark as HOOKED
	bset	DIB_HOOKED, %l5
	stb	%l5, [%g7 + DIB_READINESS]

	add	%g5, NODE_DOMHOOKKEY, %l1	! Point at hook key
	set	worryqueue, %l4
	mov	%l4, %l3	! Hook key's right ptr is worryqueue
	ld	[%l4], %l2	! Hook key's left ptr is worryqueue.tail
	mov	KT_PIHK, %l5	! KT is prepared involved hook, db=0
	st	%l1, [%l4]	! worryqueue.tail = hook
	std	%l2, [%l1]	! Store the hook key
	st	%l1, [%l2 + 4]	! oldworryqueue.right->left = hook

	!!!!! startworrier !!!!!!
! end 
	ba	asm_putawaydomain
	  std	%l4, [%l1 + 8]		! Finish storing worry queue hook

asm_fork:	! putawaydomain for a fork jump
! Begin rundom(cpudibp->rootnode)
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 points to the jumper`s domain root
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

! Jumper is on no queue, so no need to zaphook

#if SCHEDULER == 0
! start read_system_time
	! Already running at system hi interrupt level
	sethi	%hi(system_time), %g2
	ld	[%g2 + %lo(system_time)], %g2
	! Don't need unique times for scheduling - don't muck w/offset
! end read_system_time
! start updprio(jumper)
	! domprio key is involved datakey, no need to check
	ld	[%g5 + NODE_KPRIOTIME], %l3
	sub	%g2, %l3, %l3	! Compute kclock.hi - kpriotime(np)
	srl	%l3, 6, %l3
	st	%g2, [%g5 + NODE_KPRIOTIME]
	ld	[%g5 + NODE_KPRIO], %l2
	srl	%l2, %l3, %l2	! Decay priority
	st	%l2, [%g5 + NODE_KPRIO]
! end updprio(jumper)
#endif
#if SCHEDULER == 0 || SCHEDULER == 2
	set	cpuqueue, %l4	! Put jumper in correct place on cpuqueue
	ld	[%l4 + QUEUEHEAD_HEAD], %l5	! load right chain
#if SCHEDULER == 0
1:
	cmp	%l4, %l5
	be	2f
	  nop
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l3
	cmp	%l2, %l3
	bgu,a	1b
	  ld	[%l5 + QUEUEHEAD_HEAD], %l5	! load right chain
! start updprio(entry in cpu queue)
	ldub	[%l5 + NODE_DOMPRIO + KEY_TYPE - NODE_DOMHOOKKEY], %l0
	and	%l0, KEY_TYPEMASK, %l0	! check key type for datakey
	cmp	%l0, KT_DATAKEY
	bne	2f
	nop
	ld	[%l5 + NODE_KPRIOTIME - NODE_DOMHOOKKEY], %l3
	sub	%g2, %l3, %l3	! Compute kclock.hi - kpriotime(np)
	srl	%l3, 6, %l3
	st	%g2, [%l5 + NODE_KPRIOTIME - NODE_DOMHOOKKEY]
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
	srl	%l0, %l3, %l0	! Decay priority
	st	%l0, [%l5 + NODE_KPRIO - NODE_DOMHOOKKEY]
! end updprio(entry in cpu queue)
	ld	[%l5 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
	cmp	%l2, %l0
	bgu	1b
	  nop
2:
#endif
		! With Neanderthal scheduler, jumper goes to head
#else
	set	cpuqueue, %l5	! W/O Scheduler, jumper goes to tail
	mov	%l5, %l4
#endif

! Jumper goes to left of the item pointed to by %l5
! %l4 points to the cpu queue
	mov	%l5, %l3	! Hook key's right ptr is %l5
	ld	[%l5], %l2	! Hook key's left ptr is %l5.left
	add	%g5, NODE_DOMHOOKKEY, %l1	! Point at hook key
	mov	KT_PIHK+(1<<8), %l5	! KT is prepared involved hook, db=1
	st	%l1, [%l2 + 4]	! %l5->left.right = hook
	std	%l2, [%l1]	! Store the hook key
	st	%l1, [%l3]	! %l5->left = hook
	std	%l4, [%l1 + 8]

	ldub	[%g7 + DIB_READINESS], %l2	! Mark jumper hooked
	bset	DIB_HOOKED, %l2
	stb	%l2, [%g7 + DIB_READINESS]
! end rundom(cpudibp->rootnode)

asm_putawaydomain:
! start putawaydomain()

#if SCHEDULER == 0 || SCHEDULER == 1
! start uncachecpuallocation
! start read_process_timer
	sethi	%hi(process_timer), %l5
	ld	[%l5 + %lo(process_timer)], %l5
! end read_process_timer
	ld	[%g7 + DIB_CPUCACHE], %l4
	add	%l5, %l4, %l4
	st	%l4, [%g7 + DIB_CPUCACHE]
	sethi	%hi(slicecache), %l2
	ld	[%l2 + %lo(slicecache)], %l3
	add	%l3, %l5, %l3
	st	%l3, [%l2 + %lo(slicecache)]
! end uncachecpuallocation
#if SCHEDULER == 1
	sethi	%hi(cpuslicestart), %l2
	ld	[%l2 + %lo(cpuslicestart)], %l1
	sub	%l1, %l3, %l2
	ld	[%g5 + NODE_KPRIO], %l3
	add	%l2, %l3, %l3
	st	%l3, [%g5 + NODE_KPRIO]
#endif
#endif

! md_putawaydomain is a nop
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 points to the jumper`s domain root
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

	/* Get the cycle and instruction counts */
	sethi	%hi(lowcoreflags), %l4;
	ldub	[%l4 + %lo(lowcoreflags)], %l4;
	btst	0x40, %l4;			/* counters == 1? */
	bz	1f;				/* No - skip */
	  .empty				/* Suppress error msg */
	set	cpu_cycle_count, %l0;		/* master counter */
	lda	[%g0]ASI_MCTRV, %l3;		/* read counter */
	ldd	[%l0], %l4;			/* sw cycle count */
	srl	%l3, MCTRV_ICNT_SHIFT, %g1;	/* get inst count */
	set	MCTRV_CCNT_LIMIT - 1, %l0;	/* mask for count */
	and	%l3, %l0, %l0;			/* hw cycle count */
	set	MCTRV_CCNT_LIMIT, %l3;		/* cycle limit */
	sub	%l3, %l0, %l0;			/* cycles since intr */
	sub	%l3, %g1, %g1;			/* inst since intr */
	set	cpu_cycle_start, %l3;
	addcc	%l5, %l0, %l5;			/* add hw and sw */
	ldd	[%l3], %l0;			/* Get start count */
	addx	%l4, 0, %l4;
	subcc	%l5, %l1, %l1;			/* Compute diff */
	subxcc	%l4, %l0, %l0;
	bge	2f;				/* Not neg, go */
	  sethi	%hi(MCTRV_CCNT_LIMIT), %g2;
	addcc	%g2, %l5, %l5;			/* Adjust new start */
	addx	%l4, 0, %l4;
	addcc	%g2, %l1, %l1;			/* Adjust difference */
	addx	%l0, 0, %l0;
2:	std	%l4, [%l3];			/* Save new start value */
	ldd	[%g7 + DIB_KER_CYCLES], %l4;	/* Add to dib ctr */
	addcc	%l1, %l5, %l5;
	addx	%l0, %l4, %l4;
	std	%l4, [%g7 + DIB_KER_CYCLES];

	set	cpu_inst_count, %l0;		/* master counter */
	ldd	[%l0], %l4;			/* sw cycle count */
	set	cpu_inst_start, %l3;
	addcc	%l5, %g1, %l5;			/* add hw and sw */
	ldd	[%l3], %l0;			/* Get start count */
	addx	%l4, 0, %l4;
	subcc	%l5, %l1, %l1;			/* Compute diff */
	subxcc	%l4, %l0, %l0;
	bge	3f;				/* Not neg, go */
	  sethi	%hi(MCTRV_CCNT_LIMIT), %g2;
	addcc	%g2, %l5, %l5;			/* Adjust new start */
	addx	%l4, 0, %l4;
	addcc	%g2, %l1, %l1;			/* Adjust difference */
	addx	%l0, 0, %l0;
3:	std	%l4, [%l3];			/* Save new start value */
	ldd	[%g7 + DIB_KER_INST], %l4;	/* Add to dib ctr */
	addcc	%l1, %l5, %l5;
	addx	%l0, %l4, %l4;
	std	%l4, [%g7 + DIB_KER_INST];

1:	mov	HILRU, %l3		! Unpreplock jumper
	stb	%l3, [%g5 + NODE_PREPLOCK]
! Don't null cpudibp and cpuactor, they will be set in startdom below
! end putawaydomain()
! End handlejumper
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 points to the jumper`s domain root
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

	sethi	%hi(cpuactor), %l3
	ldub	[%l7 + DIB_READINESS], %l1 ! jedib->readiness |= BUSY;
	bset	DIB_BUSY, %l1

	! %l1 holds DIB_READINESS from jumpee's dib

	st	%g4, [%l3 + %lo(cpuactor)]	! cpuactor = jumpee
! Begin startdom(jedib);

#if SCHEDULER == 0 || SCHEDULER == 2
! start read_system_time
	! Already running at system hi interrupt level
	sethi	%hi(system_time), %g2
	ld	[%g2 + %lo(system_time)], %g2
	! Don't need unique times for scheduling - don't muck w/offset
! end read_system_time
#if SCHEDULER == 2
	set	cpuqueue, %l5		! Set for later
#endif
! start updprio(jumpee)
	! domprio key is involved datakey, no need to check
	ld	[%g4 + NODE_KPRIOTIME], %l3
	subcc	%g2, %l3, %l3	! Compute kclock.hi - kpriotime(np)
	be	1f
	  ld	[%g4 + NODE_KPRIO], %l2
	srl	%l3, 6, %l3
	st	%g2, [%g4 + NODE_KPRIOTIME]
	srl	%l2, %l3, %l2	! Decay priority
	st	%l2, [%g4 + NODE_KPRIO]
1:
#endif
#if SCHEDULER == 2
	! Use %l0=otherprio
	!     %l2=thisprio
	! DISPATCHINGDOMAINSINHIBITED == 0 since one was running
	ld	[%l5 + QUEUEHEAD_HEAD], %l4
	cmp	%l4, %l5		! Is queue empty?
	be	1f
	  nop		! Empty queue priority not likely in cache
	ld	[%l4 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
	cmp	%l2, %l0		! Compare priorities
	bg,a	.+8
	  bset	DIB_LOWPRIORITY, %l1
1:
#endif
	stb	%l1, [%l7 + DIB_READINESS]
#if SCHEDULER == 0
! end updprio(jumpee)
	! Use %l0=otherprio
	!     %l1=slice
	!     %l2=thisprio
	! DISPATCHINGDOMAINSINHIBITED == 0 since one was running
	set	cpuqueue, %l5
	ld	[%l5 + QUEUEHEAD_HEAD], %l4
	cmp	%l4, %l5		! Is queue empty?
	be	got_slice
	sethi	%hi(1 << 20), %l1		! Set maximum slice
	ld	[%l4 + NODE_KPRIO - NODE_DOMHOOKKEY], %l0
	add	%l0, 4095, %l0		! Add minslice
	srl	%l0, 1, %l3		! Inc by 1/2 to avoid thrashing
	add	%l0, %l3, %l0
	subcc	%l0, %l2, %l0
	bl,a	got_slice
	  clr	%l1
	cmp	%l1, %l0
	bgu,a	got_slice
	  mov	%l0, %l1
got_slice:
	sll	%l1, 4, %l1		! Convert to timer units
#endif
#if SCHEDULER == 1

	sethi	%hi((1<<20)<<4), %l1	! W/O scheduler, maxslice in timer units
#endif

! start sidle  (Set IDLE)
	sethi	%hi(cpudibp), %l0
	st	%l7, [%l0 + %lo(cpudibp)]
#if SCHEDULER == 0 || SCHEDULER == 1
	sethi	%hi(cpuslicestart), %l0
	st	%l1, [%l0 + %lo(cpuslicestart)]
! start loadpt
	ld	[%l7 + DIB_CPUCACHE], %l2
	cmp	%l1, %l2
	or	%l1, %g0, %l3
	bgu,a	.+8
	  or	%l2, %g0, %l3
	sub	%l1, %l3, %l1
	sethi	%hi(slicecache), %l0
	st	%l1, [%l0 + %lo(slicecache)]
	sub	%l2, %l3, %l2
	!!!	st	%l2, [%l7 + DIB_CPUCACHE]
! start set_process_timer
	! Already running at system hi priority
	tst	%l3
	sethi	%hi(process_timer), %l0
	! st	%l3, [%l0 + %lo(process_timer)] Moved to after bnz below
! start checkptwakeup
	bnz	end_checkptwakeup
	  st	%l3, [%l0 + %lo(process_timer)]
	sethi	%hi(processtimerktactive), %l0
	ldub	[%l0 + %lo(processtimerktactive)], %l3
	tst	%l3	! is process timer kernel task already active?
	bne	end_checkptwakeup
	mov	1, %l3
	stb	%l3, [%l0 + %lo(processtimerktactive)] ! now active
! start enqueue_kernel_task
	! Already at system hi interrupt level
	sethi	%hi(kernel_task_queue_head), %l4
	ld	[%l4 + %lo(kernel_task_queue_head)], %l3
	set	processtimerkt, %l2
	st	%l3, [%l2 + KERNEL_TASK_NEXT]
	st	%l2, [%l4 + %lo(kernel_task_queue_head)]
! end enqueue_kernel_task
end_checkptwakeup:
! end checkptwakeup
! end set_process_timer
! end loadpt
! start start_process_timer
	mov	1, %l3
	sethi	%hi(process_timer_on), %l2
	st	%l3, [%l2 + %lo(process_timer_on)]	
! end start_process_timer
! end sidle  (Set IDLE)
#endif
	! set_memory_management is a nop
	! md_startdom is a nop
! End startdom(jedib)

! Begin deliver_message()
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g5 points to the jumper`s domain root
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib
	ld	[%l7 + DIB_KEYSNODE], %l5	! Get jumpee's keys node
	clr	%g1				! Get source of DK(0)
	add	%l5, NODEHEAD_SIZEOF, %l5	! Point at slot zero
	ld	[%g7 + DIB_KEYSNODE], %g5	! Get jumper's keys node
	add	%g5, NODEHEAD_SIZEOF, %g5	! Point at slot zero
	set	ENTRY_KEYMASK8, %l0
	btst	%l0, %l6		! Jumpee want first key
	bz	trykey2			! No - Try second

	srl	%l6, ENTRY_KEY1_SHIFT-KEYINDEXTOOFFSETSHIFT, %l0;
	and	%l0, KEYOFFSETMASK, %l0;
	add	%l0, %l5, %l0;	/* Ptr to slot to receive key - %l0 */
	ldub	[%l0 + KEY_TYPE], %l1;
	cmp	%l1, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l1;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l1;
	bz	2f;
	ldd	[%l0], %l2;	/* Load links */
	st	%l2, [%l3];	/* unlink key to be overlayed */
	st	%l3, [%l2 + 4];
2:	set	EXIT_KEYMASK8, %l2;
	btst	%l2, %g3;	/* Key offered? */
	srl	%g3, EXIT_KEY1_SHIFT-KEYINDEXTOOFFSETSHIFT, %l1;
	bz	2f;		/* no - go	*/
	and	%l1, KEYOFFSETMASK, %l1;	/* Get offered key */
	add	%l1, %g5, %l1;
	ldd	[%l1 + 8], %l2;
	btst	KEY_TYPE_INVOLVEDR, %l3;
	bz	3f;		/* Key is not involved */
	and	%l3, 0xff, %l4;	/* Get type */
	cmp	%l4, KT_PIHK;	/* Is it a hook key? */
	bne	_fault;
	andn	%l3, 0x100, %l2;/* hookdb=1 --> dk(0) */
	btog	0x100, %l2;	/* hookdb=0 --> dk(1) */
	std	%g0, [%l0]	/* Store data key */	
	srl	%l2, 8, %l2;
	clr	%l3;
	ba	1f;
	std	%l2, [%l0 + 8];	/* Store rest of data key */

3:	btst	KEY_TYPE_PREPARED, %l3;
	bz	4f;
	std	%l2, [%l0 + 8];	/* Delay - store subject, db & type */
/* We shortcut halfprep by saying a key can always go next to itself */
	ld	[%l1], %l2;	/* Get left link */
	st	%l0, [%l1];	/* oldkey.left = newkey */
	st	%l0, [%l2 + 4];	/* left->right = newkey */
	mov	%l1, %l3;	/* new.right = oldkey */
	ba	1f;
	std	%l2, [%l0];	/* Store links in newkey */

4:	ldd	[%l1], %l2;
	ba	1f;
	std	%l2, [%l0];

2:	std	%g0, [%l0];	/* Store DK(0) */
	std	%g0, [%l0 + 8];
1:	
trykey2:
	set	ENTRY_KEYMASK4, %l0
	btst	%l0, %l6		! Jumpee want first key
	srl	%l6, ENTRY_KEY2_SHIFT-KEYINDEXTOOFFSETSHIFT, %l0;
	bz	trykey3			! No - Try second

	and	%l0, KEYOFFSETMASK, %l0;
	add	%l0, %l5, %l0;	/* Ptr to slot to receive key - %l0 */
	ldub	[%l0 + KEY_TYPE], %l1;
	cmp	%l1, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l1;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l1;
	bz	2f;
	ldd	[%l0], %l2;	/* Load links */
	st	%l2, [%l3];	/* unlink key to be overlayed */
	st	%l3, [%l2 + 4];
2:	set	EXIT_KEYMASK4, %l2;
	btst	%l2, %g3;	/* Key offered? */
	srl	%g3, EXIT_KEY2_SHIFT-KEYINDEXTOOFFSETSHIFT, %l1;
	bz	2f;		/* no - go	*/
	and	%l1, KEYOFFSETMASK, %l1;	/* Get offered key */
	add	%l1, %g5, %l1;
	ldd	[%l1 + 8], %l2;
	btst	KEY_TYPE_INVOLVEDR, %l3;
	bz	3f;		/* Key is not involved */
	and	%l3, 0xff, %l4;	/* Get type */
	cmp	%l4, KT_PIHK;	/* Is it a hook key? */
	bne	_fault;
	andn	%l3, 0x100, %l2;/* hookdb=1 --> dk(0) */
	btog	0x100, %l2;	/* hookdb=0 --> dk(1) */
	std	%g0, [%l0]	/* Store data key */	
	srl	%l2, 8, %l2;
	clr	%l3;
	ba	1f;
	std	%l2, [%l0 + 8];	/* Store rest of data key */

3:	btst	KEY_TYPE_PREPARED, %l3;
	bz	4f;
	std	%l2, [%l0 + 8];	/* Delay - store subject, db & type */
/* We shortcut halfprep by saying a key can always go next to itself */
	ld	[%l1], %l2;	/* Get left link */
	st	%l0, [%l1];	/* oldkey.left = newkey */
	st	%l0, [%l2 + 4];	/* left->right = newkey */
	mov	%l1, %l3;	/* new.right = oldkey */
	ba	1f;
	std	%l2, [%l0];	/* Store links in newkey */

4:	ldd	[%l1], %l2;
	ba	1f;
	std	%l2, [%l0];

2:	std	%g0, [%l0];	/* Store DK(0) */
	std	%g0, [%l0 + 8];
1:	
trykey3:
	set	ENTRY_KEYMASK2, %l0
	btst	%l0, %l6		! Jumpee want first key
	and	%l6, KEYOFFSETMASK, %l0;
	bz	trykey4			! No - Try second

	add	%l0, %l5, %l0;	/* Ptr to slot to receive key - %l0 */
	ldub	[%l0 + KEY_TYPE], %l1;
	cmp	%l1, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l1;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l1;
	bz	2f;
	ldd	[%l0], %l2;	/* Load links */
	st	%l2, [%l3];	/* unlink key to be overlayed */
	st	%l3, [%l2 + 4];
2:	set	EXIT_KEYMASK2, %l2;
	and	%g3, KEYOFFSETMASK, %l1;	/* Get offered key */
	btst	%l2, %g3;	/* Key offered? */
	bz	2f;		/* no - go	*/
	add	%l1, %g5, %l1;
	ldd	[%l1 + 8], %l2;
	btst	KEY_TYPE_INVOLVEDR, %l3;
	bz	3f;		/* Key is not involved */
	and	%l3, 0xff, %l4;	/* Get type */
	cmp	%l4, KT_PIHK;	/* Is it a hook key? */
	andn	%l3, 0x100, %l2;/* hookdb=1 --> dk(0) */
	btog	0x100, %l2;	/* hookdb=0 --> dk(1) */
	std	%g0, [%l0]	/* Store data key */	
	srl	%l2, 8, %l2;
	clr	%l3;
	ba	1f;
	std	%l2, [%l0 + 8];	/* Store rest of data key */

3:	btst	KEY_TYPE_PREPARED, %l3;
	bz	4f;
	std	%l2, [%l0 + 8];	/* Delay - store subject, db & type */
/* We shortcut halfprep by saying a key can always go next to itself */
	ld	[%l1], %l2;	/* Get left link */
	st	%l0, [%l1];	/* oldkey.left = newkey */
	st	%l0, [%l2 + 4];	/* left->right = newkey */
	mov	%l1, %l3;	/* new.right = oldkey */
	ba	1f;
	std	%l2, [%l0];	/* Store links in newkey */

4:	ldd	[%l1], %l2;
	ba	1f;
	std	%l2, [%l0];

2:	set	ENTRY_DOMKEEP, %l2	! Special handling for domain trap?
	btst	%l2, %l6
	bz	2f			! No - Go
	  nop
	ld	[%g7 + DIB_ROOTNODE], %l2	! Get jumper's root node
	set	KT_DOMAINKEY + KEY_TYPE_PREPARED, %l3
	std	%l2, [%l0 + 8];		! store subject, db & type
	ld	[%l2 + NODE_LEFTCHAIN], %l1
	st	%l1, [%l0 + KEY_LEFTCHAIN]
	ld	[%l1 + ITEM_RIGHTCHAIN], %l2
	st	%l2, [%l0 + KEY_RIGHTCHAIN]
	st	%l0, [%l2 + ITEM_LEFTCHAIN]
	ba	1f
	  st	%l0, [%l1 + ITEM_RIGHTCHAIN]

2:	std	%g0, [%l0];	/* Store DK(0) */
	std	%g0, [%l0 + 8];
1:	
trykey4:
	set	ENTRY_KEYMASK1, %l0
	btst	%l0, %l6		! Jumpee want fourth key
	sll	%l6, KEYINDEXTOOFFSETSHIFT, %l0;
	bz	done_with_keys		! No - Done with keys

	and	%l0, KEYOFFSETMASK, %l0;
	add	%l0, %l5, %l0;	/* Ptr to slot to receive key - %l0 */
	ldub	[%l0 + KEY_TYPE], %l1;
	cmp	%l1, KT_PIHK;	/* Is there a Hook key already there? */	
	be	1f;		/* Yes - Don't overlay*/
	btst	KEY_TYPE_INVOLVEDW, %l1;
	bnz	_fault;
	btst	KEY_TYPE_PREPARED, %l1;
	bz	2f;
	ldd	[%l0], %l2;	/* Load links */
	st	%l2, [%l3];	/* unlink key to be overlayed */
	st	%l3, [%l2 + 4];
2:	srl	%g3, EXIT_JUMPTYPE_SHIFT, %l1;
	and	%l1, EXIT_JUMPTYPE>>EXIT_JUMPTYPE_SHIFT, %l1;
	cmp	%l1, EXIT_JUMPCALL>>EXIT_JUMPTYPE_SHIFT
	bgu	2f;
		/* Manufacture a resume key */
		/* returnresume for call, faultresume for implicit */
#if RETURNRESUME != 2 || FAULTRESUME != 4 || EXIT_JUMPCALL !=  \
               1<<EXIT_JUMPTYPE_SHIFT || EXIT_JUMPIMPLICIT != 0
... some assumption changed
#endif
	! l1 has 1 for jumpcall and 0 for implicit

	  ld	[%g7 + DIB_ROOTNODE], %l2; /* Designate jumper's rootnode */
	set	(FAULTRESUME<<8) + KT_RESUMEKEY + KEY_TYPE_PREPARED, %l3;
	sll	%l1, 8+1, %l1
	ld	[%g7 + DIB_LASTINVOLVED], %l4;
	sub	%l3, %l1, %l3
	std	%l2, [%l0 + 8];
	ld	[%l4 + ITEM_RIGHTCHAIN], %l5;
	st	%l0, [%l4 + 4];
	st	%l0, [%l5];
	ba	1f;
	  std	%l4, [%l0];

2:	set	EXIT_KEYMASK1, %l2;
	btst	%l2, %g3;	/* Key offered? */
	bz	2f;		/* no - go	*/
	sll	%g3, KEYINDEXTOOFFSETSHIFT, %l1;
	and	%l1, KEYOFFSETMASK, %l1;	/* Get offered key */
	add	%l1, %g5, %l1;
	ldd	[%l1 + 8], %l2;
	btst	KEY_TYPE_INVOLVEDR, %l3;
	bz	3f;		/* Key is not involved */
	and	%l3, 0xff, %l4;	/* Get type */
	cmp	%l4, KT_PIHK;	/* Is it a hook key? */
	bne	_fault;
	andn	%l3, 0x100, %l2;/* hookdb=1 --> dk(0) */
	btog	0x100, %l2;	/* hookdb=0 --> dk(1) */
	std	%g0, [%l0]	/* Store data key */	
	srl	%l2, 8, %l2;
	clr	%l3;
	ba	1f;
	std	%l2, [%l0 + 8];	/* Store rest of data key */

3:	btst	KEY_TYPE_PREPARED, %l3;
	bz	4f;
	std	%l2, [%l0 + 8];	/* Delay - store subject, db & type */
/* We shortcut halfprep by saying a key can always go next to itself */
	ld	[%l1], %l2;	/* Get left link */
	st	%l0, [%l1];	/* oldkey.left = newkey */
	st	%l0, [%l2 + 4];	/* left->right = newkey */
	mov	%l1, %l3;	/* new.right = oldkey */
	ba	1f;
	std	%l2, [%l0];	/* Store links in newkey */

4:	ldd	[%l1], %l2;
	ba	1f;
	std	%l2, [%l0];

2:	std	%g0, [%l0];	/* Store DK(0) */
	std	%g0, [%l0 + 8];
1:	
done_with_keys:
	! %g3 Holds the exit block
	! %g4 Points to the root node of the jumpee
	! %g7 points to the jumper's dib
	! %l6 holds the entry block
	! %l7 points to the jumpee's dib

	! Pass order code
	tst	%l6			! Entry block all zeroes
	be,a	testreadiness		! Yes - non-restart resume invoked
	  mov	%l7, %g7		! Set jumpee into cpudibp register

	set	ENTRY_DOMKEEP+ENTRY_RCZERO, %l2	! Special handling?
	btst	%l2, %l6
	bz,a	2f			! No - Go
	  ld	[%g7 + DIB_REGS + (8+0)*4], %l1	! Load the order/return code
	set	ENTRY_RCZERO, %l2	! Make zero return code?
	btst	%l2, %l6
	bnz,a	2f			! No - Go
	  clr	%l1
	lduh	[%g7 + DIB_TRAPCODE], %l1
	set	0x80000000, %l2
	or	%l2, %l1, %l1
2:
	mov	%l7, %g7		! Set jumpee into cpudibp register

	set	ENTRY_RC, %l0		! Want the order code
	btst	%l0, %l6
	bnz	passordercode		! Yes - Go
	  ldub	[%l7 + DIB_READINESS], %l0
	tst	%l1			! Is order code zero
	bz	testreadiness		! Yes - OK

	! Trap domain if invoked key is not a restart resume key
	mov	0x100, %l2
	sth	%l2, [%l7 + DIB_TRAPCODE]
	st	%l1, [%l7 + DIB_TRAPCODEEXTENSION]
	st	%g0, [%l7 + DIB_TRAPCODEEXTENSION+4]
	bset	DIB_TRAPPED, %l0	! And mark domain trapped
	stb	%l0, [%l7 + DIB_READINESS]
	bz	asm_doslowstart
	nop

passordercode:	
	st	%l1, [%l7 + DIB_REGS + (8+0)*4] ! Store the order code

testreadiness:
	btst	0xff - DIB_BUSY, %l0
	sethi	%hi(kernel_task_queue_head), %l5
	bnz	asm_doslowstart

	! check kernel task queue
	ld	[%l5 + %lo(kernel_task_queue_head)], %l5
	sethi	%hi(check_counter), %l6
	tst	%l5
	bz	asm_checkcheck
	  ld	[%l6 + %lo(check_counter)], %l5
	SETUP_KERNEL_STACK
	!call	stop_process_timer
	!nop
	call	do_a_kernel_task
	nop
	!call	stop_process_timer
	!nop
	ba	return_from_user_exception
	nop

asm_checkcheck:
	subcc	%l5, 1, %l5
	bg	rundomain
	st	%l5, [%l6 + %lo(check_counter)]
		SETUP_KERNEL_STACK
		call	checkrunning
		sethi	%hi(check_freq), %l5
		ld	[%l5 + %lo(check_freq)], %l5
		ba	get_cpudibp
		st	%l5, [%l6 + %lo(check_counter)]

asm_doslowstart:
	SETUP_KERNEL_STACK
	ba	doslowstart
	nop



latecallgate:
	! %g4 Points to the root node of the jumpee
	! %g7 points to the jumper's dib
	mov	HILRU, %l3		! Unpreplock jumpee
	stb	%l3, [%g4 + NODE_PREPLOCK]

	SETUP_KERNEL_STACK
	ld	[%g7 + (8+0)*4], %o0
	ld	[%g7 + (8+1)*4], %o1
	ld	[%g7 + (8+3)*4], %o3
	ld	[%g7 + (8+4)*4], %o4
	ld	[%g7 + DIB_PC], %l1
	ld	[%g7 + DIB_NPC], %l2
	sethi	%hi(cpuordercode), %l5
	st	%o0, [%l5 + %lo(cpuordercode)]
	sethi	%hi(cpuexitblock), %l5
	st	%o1, [%l5 + %lo(cpuexitblock)]
	! cpuarglength already set
	sethi	%hi(cpuargaddr), %l5
	and	%o3, 0xfff, %l4
	st	%l4, [%l5 + %lo(cpuargaddr)]

	! Save PC and nPC for back_up_jumper and advance them
        sethi	%hi(cpubackupamount), %l5
	mov	2, %l3
	st	%l3, [%l5 + %lo(cpubackupamount)]
	sethi	%hi(cpu_int_pc),%l5
	st	%l1, [%l5 + %lo(cpu_int_pc)]
	sethi	%hi(cpu_int_npc),%l5
	st	%l2, [%l5 + %lo(cpu_int_npc)]
	st	%l2, [%g7 + DIB_PC]
	add	%l2, 4, %l2
	sethi	%hi(cpuargcteaddr), %l5
	ld	[%g7 + DIB_KEYSNODE], %l6
	srl	%o1, EXIT_GATE_SHIFT-KEYINDEXTOOFFSETSHIFT, %l4
	and	%l4, KEYOFFSETMASK, %l4
	ld	[%l5 + %lo(cpuargcteaddr)], %l5
	add	%l4, %l6, %l6		! Addr of gate key - sizeof(nodehead)
	add	NODEHEAD_SIZEOF, %l6,%o0	! Pass gate key address

	call	keyjump
	  st	%l2, [%g7 + DIB_NPC]	! Delay

	tst	%l5
	bz	return_from_user_exception
	  nop
	ldub	[%l5 + CTE_CORELOCK], %l4	! unlock jumper's CTE
	dec	8, %l4
	bset	HILRU, %l4
	ba	return_from_user_exception
	  stb	%l4, [%l5 + CTE_CORELOCK]

callgate_unpreplock:
	mov	HILRU, %l3
	stb	%l3, [%l4 + NODE_PREPLOCK]
	
callgate:
	mov	kernCtx, %l3		/* Set kernel context number */
	mov	RMMU_CTX_REG, %l4
	sta	%l3, [%l4]ASI_RMMU

	sethi	%hi(cpuordercode), %l5
	st	%i0, [%l5 + %lo(cpuordercode)]
	sethi	%hi(cpuexitblock), %l5
	st	%i1, [%l5 + %lo(cpuexitblock)]
	! cpuarglength already set
	sethi	%hi(cpuargaddr), %l5
	and	%i3, 0xfff, %l4
	st	%l4, [%l5 + %lo(cpuargaddr)]

	! Save PC and nPC for back_up_jumper and advance them
	sethi	%hi(cpu_int_pc),%l5
	st	%l1, [%l5 + %lo(cpu_int_pc)]
	sethi	%hi(cpu_int_npc),%l5
	st	%l2, [%l5 + %lo(cpu_int_npc)]
	mov	%l2, %l1
	add	%l2, 4, %l2
        sethi	%hi(cpubackupamount), %l3
	mov	2, %l4
	st	%l4, [%l3 + %lo(cpubackupamount)]

	! Save registers to the DIB
	SWITCH_TO_KERNEL(%l7)

	ld	[%g7 + (8+1)*4], %o1
	ld	[%g7 + DIB_KEYSNODE], %l6
	srl	%o1, EXIT_GATE_SHIFT-KEYINDEXTOOFFSETSHIFT, %l4
	and	%l4, KEYOFFSETMASK, %l4
	add	%l4, %l6, %l6		! Addr of gate key - sizeof(nodehead)

	call	keyjump
	  add	NODEHEAD_SIZEOF, %l6,%o0	! Pass gate key address

	sethi	%hi(cpuargcteaddr), %l5
	ld	[%l5 + %lo(cpuargcteaddr)], %l5
	tst	%l5
	bz	return_from_user_exception
	  nop
	ldub	[%l5 + CTE_CORELOCK], %l4	! unlock jumper's CTE
	dec	8, %l4
	bset	HILRU, %l4
	ba	return_from_user_exception
	  stb	%l4, [%l5 + CTE_CORELOCK]

callgatenopage:
	mov	kernCtx, %l3		/* Set kernel context number */
	mov	RMMU_CTX_REG, %l4
	sta	%l3, [%l4]ASI_RMMU

	sethi	%hi(cpuordercode), %l5
	st	%i0, [%l5 + %lo(cpuordercode)]
	sethi	%hi(cpuexitblock), %l5
	st	%i1, [%l5 + %lo(cpuexitblock)]
	sethi	%hi(cpuarglength), %l5
	st	%i4, [%l5 + %lo(cpuarglength)]

	! Save PC and nPC for back_up_jumper and advance them
	sethi	%hi(cpu_int_pc),%l5
	st	%l1, [%l5 + %lo(cpu_int_pc)]
	sethi	%hi(cpu_int_npc),%l5
	st	%l2, [%l5 + %lo(cpu_int_npc)]
	mov	%l2, %l1
	add	%l2, 4, %l2

	! Save registers to the DIB
	SWITCH_TO_KERNEL(%l7)

	call	gate
	nop

return_from_user_exception:
	! Return from user exception
	! If cpudibp is set, run that domain.

	! checkodometer
	sethi	%hi(check_counter), %l6
	ld	[%l6 + %lo(check_counter)], %l5
	subcc	%l5, 1, %l5
	bg	get_cpudibp
	st	%l5, [%l6 + %lo(check_counter)]
		call	checkrunning
		sethi	%hi(check_freq), %l5
		ld	[%l5 + %lo(check_freq)], %l5
		st	%l5, [%l6 + %lo(check_counter)]
get_cpudibp:
	sethi	%hi(cpudibp), %g7
	ld	[%g7 + %lo(cpudibp)], %g7
	tst	%g7
	bnz	gotdomain
	nop

.global nodomain
nodomain:
	! No domain to run. See if there is a kernel task to run
	sethi	%hi(kernel_task_queue_head), %l5
	ld	[%l5 + %lo(kernel_task_queue_head)], %l5
	tst	%l5
	bz	getdomain		! No kernel tasks to run
	nop
	call	do_a_kernel_task
	nop
	ba	return_from_user_exception
	nop

	! Try to find a domain to run
getdomain:
	call	select_domain_to_run
	nop
	sethi	%hi(cpudibp), %g7
	ld	[%g7 + %lo(cpudibp)], %g7

	! Now we've got something to run, let's run it.
gotdomain:
#if LATER
	! First see if it has any data access obstacles
	ld	[%g7 + DIB_DATA_ACCESS], %l5
	set	DMT_VALID_BIT, %l6
	andcc	%l5, %l6, %g0
	bne	no_data_obstacle
	nop
	call	handle_data_obstacle
	nop
	ba	return_from_user_exception
	nop
#endif

no_data_obstacle:
	ldub	[%g7 + DIB_READINESS], %l5
	btst	0xff - DIB_BUSY, %l5
	bnz	doslowstart
	nop

	! check kernel task queue
	sethi	%hi(kernel_task_queue_head), %l5
	ld	[%l5 + %lo(kernel_task_queue_head)], %l5
	tst	%l5
	bz	rundomain
	nop
	!call	stop_process_timer
	!nop
	call	do_a_kernel_task
	nop
	!call	stop_process_timer
	!nop
	ba	return_from_user_exception
	nop

doslowstart:
	call	slowstart
	nop
	ba	return_from_user_exception
	nop

rundomain:
	! Run the domain specified by the DIB pointed to by %g7
	! N.B. sparc_domain.c
	!  sparc_jdomain.c, and the interrupt routines all ensure that the
	!  psr in the dib has ET == 0.  (ET will become 1 when rett is issued.)
	!  They also ensure that PIL == 0 and S == 1.

	ld	[%g7 + DIB_MAP], %l5	! Set the context number
	mov	RMMU_CTX_REG, %l4
	sta	%l5, [%l4]ASI_RMMU

	RESTORE_REGS

! The following 2 instructions are needed if the counters logic is moved
! to run after the call to restore regs
	! mov	%l0, %psr		! Restore int condition code
	! nop; nop				! psr delay
jmp_rett:
! At this point we are ready to go back to user mode.
! This is a good spot to test for various insanities.
! It turns out that we come thru here to return from
! an interruption to the kernel as well
! when we come from level10_windowvalid.
#if 0
    mov %wim, %l5
    mov 0, %wim
    restore ! see what domain will see.
    save %i3, 0, %l3 ! snarf the domain's %i3
    mov %l5, %wim
    srl %l3, 24, %l5
    mov %psr, %l0
    andn %l0, 0xfe0, %l4
    or %l4, 0xfa0, %l4
    mov %l4, %psr; nop; nop; nop
    cmp %l5, 0xf0
    tz 0x3e; nop
    mov %l0, %psr; nop; nop
#endif
! End of test area!!
    jmp %l1			! psr delay
	rett %l2

        SET_SIZE(keykos_trap)


! Copy from bus address to virtual address
	ENTRY(movba2va)
! int movba2va(void *toaddr, int offset, int length)
!  Moves data from the page cpuargcteaddr + offset to toaddr for length
	! %o0 has toaddr
	! %o1 has offset which together with the bus address in the CTE located by
	! the current contents of cpuargcteaddr, is where we copy from.
	! %o2 has length
! Returns: 1==data moved, 0==potental destructive overlap

	! %o3 will have the physical address of the source
	! %o4 and %o5 are used for branch table logic and as a doubleword buffer
	tst	%o2
	bnz	1f
	  mov	%psr, %g1		! Set interrupts at level 15
	retl
	  mov	1, %o0

1:	or	%g1, 0xf00, %o4
	mov	%o4, %psr

!	lda	[%g0]ASI_RMMU, %o4	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %o5
!	or	%o5, %o4, %o5
!	sta	%o5, [%g0]ASI_RMMU

	sethi	%hi(cpuargcteaddr), %o3
	ld	[%o3 + %lo(cpuargcteaddr)], %o3
	or	%o0, %o1, %o4		! Select on data alignments
	and	%o4, 7, %o4
	ld	[%o3 + CTE_BUSADDRESS], %o3	! Get the physical address
	sll	%o4, 2, %o4		! times entry size

	andn	%o0, 0xfff, %o5
	lda	[%o5]ASI_FLPR, %o5	! get real address of to field
	sll	%o5, 4, %o5		! Shift to match %o3 address
	andn	%o5, 0xfff, %o5		! Mask off control information
	cmp	%o3, %o5		! Test for destructive overlap
	be,a	movba2va_retl
	  clr	%o0			! Return data not moved

	! on a machine with > 4 gig of memory, we would find busaddress had
	! been shifted right 4 bits to hold the high bits of the physical
	! address.  We would then use those top 4 bits to select the correct
	! copy routine (which alternate address space for the load instructions)

	set	movba2va_move_table, %o5
	andn	%o3, 0xfff, %o3		! Compute starting address
	ld	[%o4 + %o5], %o4
	add	%o3, %o1, %o3		! %o3 holds the data's physical address
	
	jmp	%o4
	  sub	%o0, %o3, %o0		! Calc difference in addresses
movba2va_move_table:
	.word	movba2va_move_double	! addresses end in 3 zeroes
	.word	movba2va_move_byte	! addresses end in 0 zeroes
	.word	movba2va_move_half	! addresses end in 1 zeroes
	.word	movba2va_move_byte	! addresses end in 0 zeroes
	.word	movba2va_move_word	! addresses end in 2 zeroes
	.word	movba2va_move_byte	! addresses end in 0 zeroes
	.word	movba2va_move_half	! addresses end in 1 zeroes
	.word	movba2va_move_byte	! addresses end in 0 zeroes

	! %o0 has the jumpee's address - low 32 bits of jumper`s physical addr
	! %o3 has the low 32 bits of the jumper's data's physical address
	! %o2 has the length
movba2va_move_double:
	deccc	8, %o2
	bl,a	2f
	  inccc	4, %o2
1:	ldda	[%o3]ASI_PASSMEM, %o4
	deccc	8, %o2
	std	%o4, [%o3 + %o0]
	bge	1b
	  inc	8, %o3

	inc	8, %o2
movba2va_move_word:
	deccc	4, %o2
2:	bl,a	2f
	  inccc	2, %o2
1:	lda	[%o3]ASI_PASSMEM, %o4
	deccc	4, %o2
	st	%o4, [%o3 + %o0]
	bge	1b
	  inc	4, %o3

	inc	4, %o2
movba2va_move_half:
	deccc	2, %o2
2:	bl,a	2f
	  inccc	1, %o2
1:	lduha	[%o3]ASI_PASSMEM, %o4
	deccc	2, %o2
	sth	%o4, [%o3 + %o0]
	bge	1b
	  inc	2, %o3

	inc	2, %o2
movba2va_move_byte:
	deccc	1, %o2
2:	bl	2f
	  nop
1:	lduba	[%o3]ASI_PASSMEM, %o4
	deccc	1, %o2
	stb	%o4, [%o3 + %o0]
	bge	1b
	  inc	1, %o3

2:	mov	1, %o0
movba2va_retl:	
!	lda	[%g0]ASI_RMMU, %o4	! Restore alternate cachable
!	set	MCR_AC, %o5
!	andn	%o4, %o5, %o5
!	sta	%o5, [%g0]ASI_RMMU
	mov	%g1, %psr
	nop
	retl
	  nop

	SET_SIZE(movba2va)



! CLEAN_FP - Move owner out of the floating point hardware
!  Entered with PSR enabled for floating point, provides delay for PSR_EF
!  r1, r2, r3 are work registers.

#define CLEAN_FP(r1,r2,r3,r4) \
	sethi	%hi(cpufpowner), r1;					\
	ld	[r1 + %lo(cpufpowner)], r1;				\
	tst	r1;							\
	bz	4f;			/* owner==NULL, get out */	\
	mov	DIB_DEFERRED_FP, r3;	/* psr delay, set FQ offset */	\
1:	st	%fsr, [r1 + DIB_FSR];					\
	ld	[r1 + DIB_FSR], r2;	/* See if any deffered Q */	\
	set	FSR_QNE, r4;		/* Is there a trap queue? */	\
	btst	r4, r2;							\
	bz	2f;							\
	nop;								\
	std	%fq, [r1 + r3];		/* Store FQ entry */		\
	inc	8, r3;			/* Increment pointer */		\
	ba	1b;			/* Go check for more Q */	\
	nop;								\
2:	mov	DIB_DEFERRED_FP + 4*8, r2;	/* Full queue? */	\
	cmp	r3, r2;							\
	be	3f;			/* Yes - Skip zeroing entry */	\
	nop;								\
	st	%g0, [r1 + r3];						\
	inc	4, r3;							\
	st	%g0, [r1 + r3];						\
3:	ld	[r1 + DIB_PSR], r2;	/* Disable his floating poing */\
	set	PSR_EF, r3;						\
	andn	r2, r3, r2;						\
	st	r2, [r1 + DIB_PSR];					\
	std	%f0, [r1 + DIB_FP_REGS];	/* Save FP regs */	\
	std	%f2, [r1 + DIB_FP_REGS + 8];				\
	std	%f4, [r1 + DIB_FP_REGS + 16];				\
	std	%f6, [r1 + DIB_FP_REGS + 24];				\
	std	%f8, [r1 + DIB_FP_REGS + 32];				\
	std	%f10, [r1 + DIB_FP_REGS + 40];				\
	std	%f12, [r1 + DIB_FP_REGS + 48];				\
	std	%f14, [r1 + DIB_FP_REGS + 56];				\
	std	%f16, [r1 + DIB_FP_REGS + 64];				\
	std	%f18, [r1 + DIB_FP_REGS + 72];				\
	std	%f20, [r1 + DIB_FP_REGS + 80];				\
	std	%f22, [r1 + DIB_FP_REGS + 88];				\
	std	%f24, [r1 + DIB_FP_REGS + 88];				\
	std	%f26, [r1 + DIB_FP_REGS + 104];				\
	std	%f28, [r1 + DIB_FP_REGS + 112];				\
	std	%f30, [r1 + DIB_FP_REGS + 120];				\
4:


! Clear out the floating point hardware
	ENTRY(clean_fp)
! void clean_fp(void);
	mov	%psr, %o5		! enable floating point
	set	PSR_EF, %o4
	wr	%o5, %o4, %psr
	CLEAN_FP(%o4,%o3,%o2,%o1)
	mov	%o5, %psr		! disable floating point
	sethi	%hi(cpufpowner), %o4
	retl
	st	%g0, [%o4 + %lo(cpufpowner)]	! owner=NULL

	SET_SIZE(clean_fp)



	
! Handle floating point disabled traps
	ENTRY(fp_disabled)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc

	btst	PSR_PS, %l0		! This shouldn't occur to the kernel
	bnz	_fault
#if DEBUG>=1
		set	0x00000f00, %l4
		or	%l0, %l4, %l4
		mov	%l4, %psr
		wr	%l4, PSR_ET, %psr
#endif
	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
	ldub	[%l5 + DIB_PERMITS], %l6
	btst	DIB_FPPERMITTED, %l6	! Is floating point permitted?
	bz	fp_disabled_trap
	nop

	set	PSR_EF, %l4
	or	%l4, %l0, %l0
#if DEBUG>=1
		set	0x00000f00+PSR_EF, %l4
		or	%l0, %l4, %l4
		mov	%l4, %psr
		wr	%l4, PSR_ET, %psr ! Enable traps and floating point
#else
	mov	%l0, %psr		! Enable floating point
#endif

	CLEAN_FP(%l7,%l6,%l4,%l3)	! Throw out current user

	ld	[%l5 + DIB_FSR], %fsr
	ldd	[%l5 + DIB_FP_REGS], %f0
	ldd	[%l5 + DIB_FP_REGS + 8], %f2
	ldd	[%l5 + DIB_FP_REGS + 16], %f4
	ldd	[%l5 + DIB_FP_REGS + 24], %f6
	ldd	[%l5 + DIB_FP_REGS + 32], %f8
	ldd	[%l5 + DIB_FP_REGS + 40], %f10
	ldd	[%l5 + DIB_FP_REGS + 48], %f12
	ldd	[%l5 + DIB_FP_REGS + 56], %f14
	ldd	[%l5 + DIB_FP_REGS + 64], %f16
	ldd	[%l5 + DIB_FP_REGS + 72], %f18
	ldd	[%l5 + DIB_FP_REGS + 80], %f20
	ldd	[%l5 + DIB_FP_REGS + 88], %f22
	ldd	[%l5 + DIB_FP_REGS + 96], %f24
	ldd	[%l5 + DIB_FP_REGS + 104], %f26
	ldd	[%l5 + DIB_FP_REGS + 112], %f28
	ldd	[%l5 + DIB_FP_REGS + 120], %f30
	sethi	%hi(cpufpowner), %l4
	st	%l5, [%l4 + %lo(cpufpowner)]	! New owner

	! Now see if domain has a deferred FP queue

	ldd	[%l5 + DIB_DEFERRED_FP], %l6
	tst	%l7
	bz	fp_disabled_run
	nop
	set	fprestartqueue, %l4
	st	%l7, [%l4]		! Store instruction in restart stream
	set	cpu_fpa_map, %l3
	st	%l6, [%l3]		! And its address in the fp map
	ldd	[%l5 + DIB_DEFERRED_FP+8], %l6
	tst	%l6
	bz	fp_disabled_run_queue
	inc	4, %l4
	st	%l7, [%l4]		! Store instruction in restart stream
	st	%l6, [%l3 + 4]		! And its address in the fp map
	ldd	[%l5 + DIB_DEFERRED_FP+16], %l6
	tst	%l6
	bz	fp_disabled_run_queue
	inc	4, %l4
	st	%l7, [%l4]		! Store instruction in restart stream
	st	%l6, [%l3 + 8]		! And its address in the fp map
	ldd	[%l5 + DIB_DEFERRED_FP+24], %l6
	tst	%l6
	bz	fp_disabled_run_queue
	inc	4, %l4
	st	%l7, [%l4]		! Store instruction in restart stream
	st	%l6, [%l3 + 12]		! And its address in the fp map
	inc	4, %l4

fp_disabled_run_queue:		! Build a return instruction at the end
	set	0x81c00017, %l7		! jmp %l7
	st	%l7, [%l4]
	set	0x01000000, %l7		! nop
	st	%l7, [%l4 + 4]
	flush	%l4
	set	fprestartqueue, %l4
	jmpl	%l4, %l7		! Call generated code to restart queue
	nop

fp_disabled_run:
	mov	%l0, %psr
	nop			! PSR delay
#if DEBUG>=1
		nop; nop;	! Additional PSR delay because PSR_ET==1
#endif
	ba jmp_rett; nop
	! nop; jmp	%l1
	! rett	%l2

fp_disabled_trap:
#if DEBUG>=1
		mov	%l0, %psr
		nop; nop; nop
#endif
	KERNEL_CYCLES
	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
  	SWITCH_TO_KERNEL(%l5)		! Save the trapped domain's registers
	mov	4, %l4			! Trap is floating point disabled
	sth	%l4, [%l5 + DIB_TRAPCODE]	! Set the trap code
	ldub	[%l5 + DIB_READINESS], %l4	! And mark domain trapped
	bset	DIB_TRAPPED, %l4
	stb	%l4, [%l5 + DIB_READINESS]

	ba	gotdomain
	nop

	SET_SIZE(fp_disabled)




! Handle floating point exception traps
	ENTRY(fp_exception)
! Processor is disabled for interrupts.  Registers contain:
!	%l0 - %psr immediately after trap
!	%l1 - trapped pc
!	%l2 - trapped npc

	btst	PSR_PS, %l0		! This shouldn't occur to the kernel
	bnz	_fault
	KERNEL_CYCLES
	sethi	%hi(cpudibp), %l5
	ld	[%l5 + %lo(cpudibp)], %l5
  	SWITCH_TO_KERNEL(%l5)		! Save the trapped domain's registers
	CLEAN_FP(%o4,%o3,%o2,%o1)	! Remove data from FPU
	sethi	%hi(cpufpowner), %o4
	st	%g0, [%o4 + %lo(cpufpowner)]	! owner=NULL

	set	cpu_fpa_map, %l2	! First map entry
	set	fprestartqueue, %l3	! See if any queue entries are ours
	add	%g7, DIB_DEFERRED_FP, %l5	! First FQ entry
	clr	%l4			! Loop offset
fp_exception_look:
	ldd	[%l5 + %l4], %l6		! Get [address, instruction]
	tst	%l7			! If instruction zero, then end
	bz	fp_exception_trap
	nop
	sub	%l6, %l3, %o1
	cmp	%o1, 12			! One of our restart instructions?
	bgu	fp_exception_trap	! Not ours, neither are rest
	nop
	ld	[%l2 + %l4], %l6	! Replace address with map entry
	st	%l6, [%l5]
	inc	8, %l5
	cmp	%l4, 12
	bl	fp_exception_look
	inc	4, %l4

fp_exception_trap:	
	mov	8, %l4			! Trap is floating point exception
	sth	%l4, [%l5 + DIB_TRAPCODE]	! Set the trap code
	ldub	[%l5 + DIB_READINESS], %l4	! And mark domain trapped
	bset	DIB_TRAPPED, %l4
	stb	%l4, [%l5 + DIB_READINESS]

	ba	gotdomain
	nop

	SET_SIZE(fp_exception)

/*
 * Support for CC Block Copy
 */
        ENTRY_NP(enable_traps)
        .volatile
        wr      %o0, %psr
        nop; nop; nop                   ! SPARC V8 requirement
        retl
        nop
        .nonvolatile
        SET_SIZE(enable_traps)

        ENTRY_NP(disable_traps)
        rd      %psr, %o0               ! save old value
        andn    %o0, PSR_ET, %o1        ! disable traps
        wr      %o1, %psr               ! write psr
        nop; nop; nop                   ! SPARC V8 requirement
        retl
        nop
        SET_SIZE(disable_traps)

/*
 * Processor Unit
 */
#define ASI_CC          0x02
#define CC_BASE         0x01c00000
 
/*
 * CC Block Copy routines
 */
#define CC_SDR  0x00000000
#define CC_SSAR 0x00000100
#define CC_SDAR 0x00000200

        ENTRY(xdb_cc_ssar_get)
        set     CC_BASE+CC_SSAR, %o0
        retl
        ldda    [%o0]ASI_CC, %o0        /* delay slot */
        SET_SIZE(xdb_cc_ssar_get)

        ENTRY(xdb_cc_ssar_set)
        set     CC_BASE+CC_SSAR, %o2
        retl
        stda    %o0, [%o2]ASI_CC        /* delay slot */
        SET_SIZE(xdb_cc_ssar_set)

        ENTRY(xdb_cc_sdar_get)
        set     CC_BASE+CC_SDAR, %o0
        retl
        ldda    [%o0]ASI_CC, %o0        /* delay slot */
        SET_SIZE(xdb_cc_sdar_get)

        ENTRY(xdb_cc_sdar_set)
        set     CC_BASE+CC_SDAR, %o2
        retl
        stda    %o0, [%o2]ASI_CC        /* delay slot */
        SET_SIZE(xdb_cc_sdar_set)


.section ".data"
faultstr:
	.asciz "Kernel trap: trap type = 0x%x\n"
.section ".text"
badTrap: ! Pretend that OB's orginal trap vector had been invoked.
! We must test for a user mode old prs. ...
 	sethi %hi(ob_debug_entry), %l2
	ld [%l2+%lo(ob_debug_entry)], %l2 ! OB's bad trap entry
  	jmp %l2; nop ! We were never there!

#if 0
	! For now, enable traps although I don't know why we have to
	set	PSR_ET, %l3
	or	%l0, %l3, %l3
	mov	%l3, %psr
	nop;nop;nop
#endif
!	lda	[%g0]ASI_RMMU, %l5	! Set alternate cachable in RMMU_CTL_REG
!	set	MCR_AC, %l6
!	andn	%l5, %l6, %l6
!	sta	%l6, [%g0]ASI_RMMU

	set	0x200, %l5
	lda	[%l5]4, %l5
	sethi	%hi(Context), %l6
        st	%l5, [%lo(Context) + %l6]
	set	omak_enteromak, %l3
	jmpl	%l3, %g0
	nop
	! we shouldn't get here, if we do, enter the prom
	set	faultstr, %o0
	call prom_naked_enter
	nop

        .word 0
        .skip 8000
        .word 0

#if trapHeist
.section ".data"

	.global TrapHistory
	.align 32
td:
#define TrapHistorySize 252
TrapHistoryCursor: 
	.word TrapHistory
TrapHistory:
	.skip TrapHistorySize
StopSet: .word 0x00000000, -1, -1, -1, 0x00000000, -1, -1, -1
RecSet:  .word 0, 0, 0, 0, 0, 0, 0, 0
	

.section ".text"

!  trapHeist
! In this scheme there are three trap tables:
! 1. The ROM's table, which I presume no one modifies,
! 2. The assembled trap table, ATT, modified by early locore.s code.
! 3. The trap table, RTT, as generated here with trapHeist switched on.
! Here we allocate a frame to hold RTT.
! RTT will appear read-only at virtual f8000000 after locore is finished
! mucking with ATT.
! We populate RTT with traps to flh.
! The assembled trap table has already been loaded and mapped

.align 8
! This leans towards simplicity!
! As we begin execution at flh below virtual page 0x5xxx (=scth) is available
! to store a history of interrupts.
logTrap:
!* or %l0, 0x20, %l3
!* mov %l3, %psr ! dangerous hack to enable breakpoints (and other traps) in the following code.
!* sethi %hi(OBsTB), %l3
!* ld [%l3+%lo(OBsTB)], %l4
	mov %tbr, %l3
!* mov %l4, %tbr
	set td, %l5
	ld [%l5+TrapHistoryCursor-td], %l6
	srl %l3, 4, %l7
and %l0, 0x40, %l4 ! isolate the PS bit
or %l4, %l7, %l7  ! 40 in trap history indicates from supervisor mode
	stb %l7, [%l6]
	add %l6, 1, %l6
	! reset the cursor to the beginning of the buffer if necessary
	set TrapHistory+TrapHistorySize, %l4
	cmp %l6, %l4
	bne 1f; and %l3, 0xff0, %l3
		set TrapHistory, %l6
1: 	st %l6, [%l5+TrapHistoryCursor-td]		! put back cursor
       srl %l3, 4+5-2, %l6
       and %l6, 7*4, %l6 ! Offset into trap-number indexed bit arrays.
       and %l7, 32-1, %l7 ! Bit number in word
       sethi %hi(0x80000000), %l4 ! Bit 0 (sorry about that!)
       srl %l4, %l7, %l4
       add %l5, %l6, %l5
       ld [%l5+StopSet-td], %l7
       andcc %l7, %l4, %g0
       bnz stopIt
       mov %l0, %psr ! to restore icc
       ld [%l5+RecSet-td], %l7
       or %l7, %l4, %l7
       st %l7, [%l5+RecSet-td]       
       sethi %hi(scb), %l6	! arrange to jump to the corresponding trap
       or %l6, %l3, %l6		! in the primary trap table
	jmp %l6+%lo(scb); nop
#endif

_fault: stopIt:
    set 0,%l3
    set 0, %l4
lp:
    ! Next 6 instructions copy MMU regs to real 0 for observation.
    lda [%l3]0x4, %l5
    sta %l5, [%l4]0x20
    add %l4, 4, %l4
    subcc %l4, 20, %g0
    bne lp
    add %l3, 0x100, %l3
    
    ! Next instructions equip current context with ROM code map.
    sethi %hi(oKRT), %l4
    ld [%l4+%lo(oKRT)], %l4 ! real address of descriptors for
    orcc %l4, %g0, %g0
    add %l4, (256-2)*4, %l4    ! two segment tables that map ROM code.
    be alreadyMapped
    ldda [%l4]0x20, %l4 ! and %l5 too
    set 0x100, %l3
    lda [%l3]4, %l3 
    sll %l3, 4, %l3 ! real address of current context table
    set 0x200, %l7
    lda [%l7]4, %l7 ! current context number
    sll %l7, 2, %l7 ! Offset into context table
    add %l3, %l7, %l3 ! real address of current region table locator.
    lda [%l3]0x20, %l3  ! current region table locator
    andn %l3, 3, %l3 ! knock off damn control bits!!
    sll %l3, 4, %l3 ! Address of current region table   
    add %l3, (256-2)*4, %l3 ! address of regions -2 & -1 of current context.
    stda %l4, [%l3]0x20
alreadyMapped:
    sethi %hi(OBsTB), %l3
    ld [%l3+%lo(OBsTB)], %l3
    jmp %l3+0x810; mov %l0, %psr 

/*	Debugging support in the kernel  */
#define DEBUG_TRAP_64 \
	DEBUG_TRAP_8; DEBUG_TRAP_8; DEBUG_TRAP_8; DEBUG_TRAP_8 \
	DEBUG_TRAP_8; DEBUG_TRAP_8; DEBUG_TRAP_8; DEBUG_TRAP_8

#define DEBUG_TRAP_8 \
	DEBUG_TRAP; DEBUG_TRAP; DEBUG_TRAP; DEBUG_TRAP \
	DEBUG_TRAP; DEBUG_TRAP; DEBUG_TRAP; DEBUG_TRAP

#define DEBUG_TRAP \
	sethi %hi(logTrap),%l3; jmp %l3+%lo(logTrap); \
	mov %psr,%l0; nop;

/*
 * Debugging Trap vector table.
 *
 * TAKE THESE COMMENTS WITH A GRAIN OF SALT
 * When a trap is taken, we vector to DEBUGSTART+(TT*16) and we have
 * the following state:
 *	2) traps are disabled
 *	3) the previous state of PSR_S is in PSR_PS
 *	4) the CWP has been decremented into the trap window
 *	5) the previous pc and npc is in %l1 and %l2 respectively.
 *
 * Registers:
 *	%l0 - %psr immediately after trap
 *	%l1 - trapped pc
 *	%l2 - trapped npc
 */
#if trapHeist
	.section ".trapdebug", #alloc | #execinstr
	.align 0x1000
	.global TrapTableDebug
	.type TrapTableDebug, #function
TrapTableDebug:
	DEBUG_TRAP_64; DEBUG_TRAP_64; DEBUG_TRAP_64; DEBUG_TRAP_64
#endif

.reserve Context, 4, ".bss", 4
