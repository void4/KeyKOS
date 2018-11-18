
#ifndef _REG_H
#define _REG_H

#define NWINDOW 8
/*
 * Location of the users' stored registers relative to R0.
 * Usage is as an index into a gregset_t array or as u.u_ar0[XX].
 */
#define	REG_PSR	(0)
#define	REG_PC	(1)
#define	REG_nPC	(2)
#define	REG_Y	(3)
#define	REG_G1	(4)
#define	REG_G2	(5)
#define	REG_G3	(6)
#define	REG_G4	(7)
#define	REG_G5	(8)
#define	REG_G6	(9)
#define	REG_G7	(10)
#define	REG_O0	(11)
#define	REG_O1	(12)
#define	REG_O2	(13)
#define	REG_O3	(14)
#define	REG_O4	(15)
#define	REG_O5	(16)
#define	REG_O6	(17)
#define	REG_O7	(18)

/* the following defines are for portability */
#define	REG_PS	REG_PSR
#define	REG_SP	REG_O6
#define	REG_R0	REG_O0
#define	REG_R1	REG_O1

#define	NGREG	19
#define	SPARC_MAXREGWINDOW	31		/* max windows in SPARC arch. */
#endif /* _REG_H */

