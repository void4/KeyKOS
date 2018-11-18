/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

// #include "stack.h"
#ifdef _ASM	/* only for assembly files */

/*
 * Symbolic section definitions.
 */
#define	RODATA	".rodata"

/*
 * profiling causes defintions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#ifdef GPROF

#define MCOUNT(x) \
        save    %sp, -SA(MINFRAME), %sp; \
        call    _mcount; \
        nop; \
        restore;

#endif /* GPROF */

#ifdef PROF

#define MCOUNT(x) \
        save    %sp, -SA(MINFRAME), %sp; \
/* CSTYLED */ \
        sethi   %hi(.L_/**/x/**/1), %o0; \
        call    _mcount; \
/* CSTYLED */ \
        or      %o0, %lo(.L_/**/x/**/1), %o0; \
        restore; \
/* CSTYLED */ \
        .common .L_/**/x/**/1, 4, 4

#endif /* PROF */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if !defined(PROF) && !defined(GPROF)
#define MCOUNT(x)
#endif /* !defined(PROF) && !defined(GPROF) */

#define RTMCOUNT(x)     MCOUNT(x)

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:	MCOUNT(x)

#define	ENTRY_NP(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:

#define	RTENTRY(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y) \
	.section	".text"; \
	.align	4; \
	.global	x, y; \
	.type	x, #function; \
	.type	y, #function; \
/* CSTYLED */ \
x:	; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.section	".text"; \
	.align	4; \
	.global	x, y; \
	.type	x, #function; \
	.type	y, #function; \
/* CSTYLED */ \
x:	; \
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.global x; \
	.type	x, #function; \
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 */
#define	DGDEF2(name, sz) \
	.section	".data"; \
	.global name; \
	.type	name, #object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF2(name, 4)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x) \
	.size	x, (.-x)

/*
 * Macros for saving/restoring registers.
 */

#define	SAVE_GLOBALS(RP) \
	st	%g1, [RP + REG_G1*4]; \
	std	%g2, [RP + REG_G2*4]; \
	std	%g4, [RP + REG_G4*4]; \
	std	%g6, [RP + REG_G6*4]; \
	mov	%y, %g1; \
	st	%g1, [RP + REG_Y*4]

#define	RESTORE_GLOBALS(RP) \
	ld	[RP + REG_Y*4], %g1; \
	mov	%g1, %y; \
	ld	[RP + REG_G1*4], %g1; \
	ldd	[RP + REG_G2*4], %g2; \
	ldd	[RP + REG_G4*4], %g4; \
	ldd	[RP + REG_G6*4], %g6;

#define	SAVE_OUTS(RP) \
	std	%i0, [RP + REG_O0*4]; \
	std	%i2, [RP + REG_O2*4]; \
	std	%i4, [RP + REG_O4*4]; \
	std	%i6, [RP + REG_O6*4];

#define	RESTORE_OUTS(RP) \
	ldd	[RP + REG_O0*4], %i0; \
	ldd	[RP + REG_O2*4], %i2; \
	ldd	[RP + REG_O4*4], %i4; \
	ldd	[RP + REG_O6*4], %i6;

#define	SAVE_WINDOW(SBP) \
	std	%l0, [SBP + (0*4)]; \
	std	%l2, [SBP + (2*4)]; \
	std	%l4, [SBP + (4*4)]; \
	std	%l6, [SBP + (6*4)]; \
	std	%i0, [SBP + (8*4)]; \
	std	%i2, [SBP + (10*4)]; \
	std	%i4, [SBP + (12*4)]; \
	std	%i6, [SBP + (14*4)];

#define	RESTORE_WINDOW(SBP) \
	ldd	[SBP + (0*4)], %l0; \
	ldd	[SBP + (2*4)], %l2; \
	ldd	[SBP + (4*4)], %l4; \
	ldd	[SBP + (6*4)], %l6; \
	ldd	[SBP + (8*4)], %i0; \
	ldd	[SBP + (10*4)], %i2; \
	ldd	[SBP + (12*4)], %i4; \
	ldd	[SBP + (14*4)], %i6;

#define	STORE_FPREGS(FP) \
	std	%f0, [FP]; \
	std	%f2, [FP + 8]; \
	std	%f4, [FP + 16]; \
	std	%f6, [FP + 24]; \
	std	%f8, [FP + 32]; \
	std	%f10, [FP + 40]; \
	std	%f12, [FP + 48]; \
	std	%f14, [FP + 56]; \
	std	%f16, [FP + 64]; \
	std	%f18, [FP + 72]; \
	std	%f20, [FP + 80]; \
	std	%f22, [FP + 88]; \
	std	%f24, [FP + 96]; \
	std	%f26, [FP + 104]; \
	std	%f28, [FP + 112]; \
	std	%f30, [FP + 120];

#define	LOAD_FPREGS(FP) \
	ldd	[FP], %f0; \
	ldd	[FP + 8], %f2; \
	ldd	[FP + 16], %f4; \
	ldd	[FP + 24], %f6; \
	ldd	[FP + 32], %f8; \
	ldd	[FP + 40], %f10; \
	ldd	[FP + 48], %f12; \
	ldd	[FP + 56], %f14; \
	ldd	[FP + 64], %f16; \
	ldd	[FP + 72], %f18; \
	ldd	[FP + 80], %f20; \
	ldd	[FP + 88], %f22; \
	ldd	[FP + 96], %f24; \
	ldd	[FP + 104], %f26; \
	ldd	[FP + 112], %f28; \
	ldd	[FP + 120], %f30;

#endif /* _ASM */
#endif	/* _SYS_ASM_LINKAGE_H */
