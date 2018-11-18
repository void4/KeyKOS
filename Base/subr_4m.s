/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
/* #include "assym.s" */

#include "asm_linkage.h"
#include "iommu.h"


#if defined(lint)

/*ARGSUSED*/
void
iommu_set_ctl(u_int value)
{}

#else	/* lint */

#define	IOMMU_SREG(Reg, Offset)	\
    !   set	v_iommu_addr, %o1				;\
  ta 0x57                                      ;\
	ld	[%o1], %o1					;\
	inc	Offset, %o1					;\
	retl							;\
	st	Reg, [%o1]
	
	/*
 	 * Set iommu ctlr reg.
 	 * iommu_set_ctl(value)
 	 */
	ENTRY(iommu_set_ctl)
#if (IOMMU_CTL_REG != 0)
	IOMMU_SREG(%o0, IOMMU_CTL_REG)
#else
    !   set	v_iommu_addr, %o1	
  ta 0x57
	ld	[%o1], %o1
	retl
	st	%o0, [%o1]
#endif
	SET_SIZE(iommu_set_ctl)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
iommu_set_base(u_int value)
{}

void
iommu_flush_all(void)
{}

/*ARGSUSED*/
void
iommu_addr_flush(int addr)
{}

#else	/* lint */

	/*
 	 * Set iommu base addr reg.
 	 */
	ENTRY(iommu_set_base)
	IOMMU_SREG(%o0, IOMMU_BASE_REG)
	SET_SIZE(iommu_set_base)

	/*
 	 * iommu flush all TLBs 
 	 */
	ENTRY(iommu_flush_all)
	IOMMU_SREG(%g0, IOMMU_FLUSH_ALL_REG)
	SET_SIZE(iommu_flush_all)

	/*
 	 * iommu addr flush 
	 */
	ENTRY(iommu_addr_flush)
	IOMMU_SREG(%o0, IOMMU_ADDR_FLUSH_REG)
	SET_SIZE(iommu_addr_flush)

#endif	/* lint */


