#ifndef _PSR_H
#define _PSR_H

/* SPARC PSR defines */
#define PSR_CWP         0x0000001F      /* current window pointer */
#define PSR_ET          0x00000020      /* enable traps */
#define PSR_PS          0x00000040      /* previous supervisor mode */
#define PSR_S           0x00000080      /* supervisor mode */
#define PSR_PIL         0x00000F00      /* processor interrupt level */
#define PSR_EF          0x00001000      /* enable floating point unit */
#define PSR_EC          0x00002000      /* enable coprocessor */
#define PSR_RSV         0x000FC000      /* reserved */
#define PSR_ICC         0x00F00000      /* integer condition codes */
#define PSR_C           0x00100000      /* carry bit */
#define PSR_V           0x00200000      /* overflow bit */
#define PSR_Z           0x00400000      /* zero bit */
#define PSR_N           0x00800000      /* negative bit */
#define PSR_VER         0x0F000000      /* mask version */
#define PSR_IMPL        0xF0000000      /* implementation */

#define PSR_PIL_BIT	8		/* bits to shift left for PIL*/
#endif /* _PSR_H */
