.ident "@(#)sparc_ddi.s	1.1 05 Mar 1995 15:42:10 %n%"

#include <sys/asm_linkage.h>

	ALTENTRY(drv_usecwait)
	ENTRY(usec_delay)
	or	%g0, 0x18, %o4          ! microsecond countdown counter, Cpudelay=0x18
	orcc	%o4, 0, %o3		! set cc bits to nz
	
1:	bnz	1b			! microsecond countdown loop
	subcc	%o3, 1, %o3		! 2 instructions in loop

	subcc	%o0, 1, %o0		! now, for each microsecond...
	bg	1b			! go n times through above loop
	orcc    %o4, 0, %o3
	retl
	nop
	SET_SIZE(usec_delay)
	SET_SIZE(drv_usecwait)

