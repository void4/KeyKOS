
/* Fields of the floating point status register */

#define FSR_RD   0xc0000000	/* Rounding direction */
#define FSR_TEM  0x0f800000	/* Trap Enable Mask */
#define FSR_NS   0x00400000	/* Non-standard FP model */
#define FSR_VER  0x000e0000     /* FPU architecture version */
#define FSR_FTT  0x0001c000     /* Floating point Trap Type */
#define FSR_QNE  0x00002000     /* Deferred FP Queue Not Empty */
#define FSR_FCC  0x00000c00     /* Floating point Condition Code */
#define FSR_AEXC 0x000003e0     /* Accrued exceptions */
#define FSR_CEXC 0x0000001f     /* Current exception */
