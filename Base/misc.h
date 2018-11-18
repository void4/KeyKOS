/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define REGSIZE 0x4c
#define RWINSIZE 0x40
#define KERNSTKSZ 0x2000

#define NCPU 1

#ifndef NULL
#define NULL 0
#endif

/* For sun4d breakpoint support */
#if defined(sun4d)
#define	MDIAG_ASI	0x38		/* diagnostic space */
#define	CTRV_ASI	0x49		/* counter value */
#define	CTRC_ASI	0x4a		/* counter control */
#define	CTRS_ASI	0x4b		/* counter status */
#define	ACTION_ASI	0x4c		/* breakpoint action */

/*
 * addresses used with MDIAG_ASI (SuperSPARC doc table 4-17)
 */

#define	MDIAG_BKV	(0 << 8)	/* breakpoint value */
#define	MDIAG_BKM	(1 << 8)	/* breakpoint mask */
#define	MDIAG_BKC	(2 << 8)	/* breakpoint control */
#define	MDIAG_BKS	(3 << 8)	/* breakpoint status */

/*
 * breakpoint control bits
 */

#define	BKC_CSPACE	(1 << 6)	/* code or data space */
#define	BKC_PAMD	(1 << 5)	/* physical or virtual address */
#define	BKC_CBFEN	(1 << 4)	/* code fault or interrupt */
#define	BKC_CBKEN	(1 << 3)	/* enable code breakpoints */
#define	BKC_DBFEN	(1 << 2)	/* data fault or interrupt */
#define	BKC_DBREN	(1 << 1)	/* enable data read breakpoint */
#define	BKC_DBWEN	(1 << 0)	/* enable data write breakpoint */

#define	BKC_MASK	((BKC_CSPACE << 1) - 1)

/*
 * breakpoint status bits
 */

#define	BKS_CBKIS	(1 << 3)	/* code interrupt generated */
#define	BKS_CBKFS	(1 << 2)	/* code fault */
#define	BKS_DBKIS	(1 << 1)	/* data interrupt */
#define	BKS_DBKFS	(1 << 0)	/* data fault */

#define	BKS_MASK	((BKS_CBKIS << 1) - 1)
#endif

#define RMMU_CTP_REG 0x100
