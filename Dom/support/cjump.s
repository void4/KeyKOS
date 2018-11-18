/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************************

   These routines are coded as leaf functions to avoid generating
   back windows simply to do a jump.   The new kernel design will
   save backwindows in the stack instead of in the DIB.  When jumps
   are leaf routines a long series of jumps without subroutine calls
   (very common) will not require any backwindow save/restore

   According to the API global registers 5,6,7 are reseved to the
   runtime environment.  I am declaring this to be the runtime environment.

   g1 is also available

   o0 and o1 will be the input (not i0 and i1 as documented) and these
   will be copied to g5 and g6 throughout the jump. G7 holds the return.  g1 will 
   be used if absolutely necessary.   Code order will be chosen so that
   the O registers can be used as scratch registers right up to the time
   they need to be loaded for the jump.   g1 is used in the jump so it
   also must be loaded at the last opportunity.  

   care must be taken to observer that PASS_STR_LEN and REC_STR_LEN are the
   same register.
*****************************************************************************/

! The routines in this file are used passed a structure containing
! jump information along with a info bits register describing various
! characteristics of the jump information. This information is
! manipulated and placed in the jump registers. A trap call is then
! issued to enter the micro-kernel. Upon return from the micro-kernel
! the results which have been stored in the jump registers are 
! placed in the appropriate locations as specified by the jump buffer
! and its bits register.
!
! inputs:
!	%i0 - bits register
!	      Format of bits register (bit 0 is rightmost bit)
!		Bit 20:	BR_ACCEPT_ACTLENGTH 
!			upon return, store actual length in location 
!			specified by jump buffer.
!
!		Bit 25:	BR_EXIT_VARKEYS
!			The keys registers in the exit block were not
!			determined at compile time. If this bit is 
!			set, run-time manipulations are required to set
!			the keys register values in the exit block
!			register.
!
!		Bit 17:	BR_ENTRY_VARKEYS
!			The keys registers in the entry block were not
!			determined at compile time. If this bit is 
!			set, run-time manipulations are required to set
!			the keys register values in the entry block
!			register.
!
!	%i1 - pointer to jump buffer (layout of jump buffer is
!	      described in keykos.h)
!
!

! The following defines give the bit offsets for various fields in the
! jump buffer. Ideally, these should be computed automatically based on
! the C structure. At some point that should be done.
!
#define JB_EXITBLK		 0
#define JB_ORDERCODE		 4
#define JB_PASS_STRP		 8
#define JB_PASS_STR_LEN		12
#define JB_INVOKE_KEY		16
#define JB_PASS_KEY1		20
#define JB_PASS_KEY2		22
#define JB_PASS_KEY3		24
#define JB_PASS_KEY4		26
#define JB_ENTRYBLK		28
#define JB_RETCODEP		32
#define JB_DATA_BYTEP		36
#define JB_REC_STRP		40
#define JB_REC_STR_LEN		44
#define JB_REC_STR_MAXLEN	48
#define JB_REC_STR_ACTLENP	52
#define JB_REC_KEY1		56
#define JB_REC_KEY2		58
#define JB_REC_KEY3		60
#define JB_REC_KEY4		62

#define JB_SCRATCH1             64
#define JB_SCRATCH2             68
#define JB_SCRATCH3             72
#define JB_SCRATCH4             76 
#define JB_SCRATCH5             80
#define JB_SCRATCH6             84
#define JB_SCRATCH7             88
#define JB_SCRATCH8             92

!
! The following definitions decribe the bit masks for each of the
! information bits in the bits register.
!
#define BR_ACCEPT_ACTLENGTH	0x00100000
#define BR_ENTRY_VARKEYS	0x00020000
#define BR_EXIT_VARKEYS		0x02000000

!
! The following definitions describe the bits in the entry
! block register which are used to determine if certain data needs
! to be copied (bits 27-24).
!
#define NB_ACCEPT_STRING	0x02000000
#define NB_ACCEPT_DATA_BYTE	0x04000000
#define NB_ACCEPT_RETCODE	0x08000000

! The following definitions describe the registers used for passing
! jump information to the micro-kernel. Note that ORDERCODE_REG
! and RETCODE_REG are the same register. Similarly, PASS_STR_LEN_REG
! and REC_STR_LEN_REG are the same register.
!
#define ORDERCODE_REG		%o0
#define EXITBLK_REG		%o1
#define ENTRYBLK_REG		%o2
#define PASS_STR_REG		%o3
#define PASS_STR_LEN_REG	%o4
#define REC_STR_REG		%o5
#define REC_STR_LEN_REG		%o4
#define REC_STR_MAXLEN_REG	%g1
#define RETCODE_REG		%o0
#define DATA_BYTE_REG		%o3

!
! The following definitions describe the shift values needed to put
! a key register # into the correct bits of the exit and entry blocks.
! The shift value assumes the 4 key bits have already been shifted to
! the high order 4 bits of the register. Thus these are right shift
! values.
!
#define INVOKE_KEY_SHIFT	 8	

#define PASS_KEY1_SHIFT		16
#define PASS_KEY2_SHIFT		20 
#define PASS_KEY3_SHIFT		24
#define PASS_KEY4_SHIFT		28

#define REC_KEY1_SHIFT		16
#define REC_KEY2_SHIFT		20
#define REC_KEY3_SHIFT		24 
#define REC_KEY4_SHIFT		28 

!
! The following definitions describe the possible JUMP types in the
! exit block JUMP type field (bits 25-24).
!
#define CT_CALL			0x01000000
#define CT_RETURN		0x02000000
#define CT_FORK			0x03000000

!
! The following definition specifies the TRAP table entry for handling
! jumps.
!

#include "asm_linkage.h"
#include "trap.h"

.global cj
.type cj, #function

cj:
	!
	! Key CALL with variable key registers or invoke key.
	!
!	save	%sp, -SA(MINFRAME), %sp
        or      %o7,%g0,%g7   ! save return
        or      %o0,%g0,%g5   ! save calling param
        or      %o1,%g0,%g6   ! save calling param

	call	loadexitblock
        nop
	set	CT_CALL, ENTRYBLK_REG  ! use entry block reg
	or	EXITBLK_REG, ENTRYBLK_REG, EXITBLK_REG
	call	loadentryblock
	nop
	ta	ST_KEYJUMP	

	b	fill_jb_and_return
	nop

.global cjcc
.type cjcc, #function

cjcc:
	!
	! Key CALL with fixed key registers and invoke key.
	!
!	save %sp, -SA(MINFRAME), %sp
        or      %o7,%g0,%g7   ! save return
        or      %o0,%g0,%g5   ! save calling param
        or      %o1,%g0,%g6   ! save calling param
!
! populate the registers used to communicate between domains
!
	ld	[%g6 + JB_PASS_STRP], PASS_STR_REG
	ld	[%g6 + JB_PASS_STR_LEN], PASS_STR_LEN_REG
	ld	[%g6 + JB_ENTRYBLK], ENTRYBLK_REG
	ld	[%g6 + JB_REC_STRP], REC_STR_REG
	ld	[%g6 + JB_REC_STR_MAXLEN], REC_STR_MAXLEN_REG
! must make sure receive string is writable by the kernel
        set     NB_ACCEPT_STRING, ORDERCODE_REG
        btst    ORDERCODE_REG, ENTRYBLK_REG
        bz      nostring1
        nop
        ldub    [REC_STR_REG], ORDERCODE_REG
        stb     ORDERCODE_REG, [REC_STR_REG]  ! first byte
        add     REC_STR_REG , REC_STR_MAXLEN_REG, ORDERCODE_REG
        add     ORDERCODE_REG, -1, ORDERCODE_REG
        ldub    [ORDERCODE_REG],  EXITBLK_REG
        stb     EXITBLK_REG, [ORDERCODE_REG]  ! last byte
nostring1:
	ld	[%g6 + JB_EXITBLK], EXITBLK_REG
! mostly wasted 6 instructions to insure recieved string is writable by kernel
	!
	!	indicate that this is a call
	!
	set	CT_CALL, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG
	ld	[%g6 + JB_ORDERCODE], ORDERCODE_REG
	ta	ST_KEYJUMP 	

	b	fill_jb_and_return
	nop

.global rj
.type rj, #function
rj:
	!
	! Key RETURN
	!
!	save	%sp, -SA(MINFRAME), %sp
        or      %o7,%g0,%g7   ! save return
        or      %o0,%g0,%g5   ! save calling param
        or      %o1,%g0,%g6   ! save calling param

	call	loadexitblock
        nop
	set	CT_RETURN, ENTRYBLK_REG
	or	EXITBLK_REG, ENTRYBLK_REG, EXITBLK_REG
	call	loadentryblock
	nop

	ta	ST_KEYJUMP	

	ld	[%g6 + JB_ENTRYBLK], EXITBLK_REG
	set	NB_ACCEPT_DATA_BYTE, ENTRYBLK_REG
	btst	ENTRYBLK_REG, EXITBLK_REG
	bz	nodatabyte
        nop
        ld	[%g6 + JB_DATA_BYTEP] , EXITBLK_REG
	sth	DATA_BYTE_REG, [EXITBLK_REG]
nodatabyte:
	b	fill_jb_and_return
	nop

.global fj
.type fj, #function
fj:
	!
	! Key FORK
	!
!	save	%sp, -SA(MINFRAME), %sp
        or      %o7,%g0,%g7   ! save return
        or      %o0,%g0,%g5   ! save calling param
        or      %o1,%g0,%g6   ! save calling param

	call	loadexitblock
	nop
	set	CT_FORK, ENTRYBLK_REG
	or	EXITBLK_REG, ENTRYBLK_REG, EXITBLK_REG
	ta	ST_KEYJUMP	

        or      %g7,%g0,%o7
	retl
        nop

loadexitblock:
	ld	[%g6 + JB_EXITBLK], EXITBLK_REG
	set	BR_EXIT_VARKEYS, ORDERCODE_REG
	btst	ORDERCODE_REG, %g5
	be	exfixedkeys
	nop
	!
	! Fill in the invoke key field of the exit block
	!
	ld	[%g6 + JB_INVOKE_KEY], ORDERCODE_REG
	sll	ORDERCODE_REG, 28, ORDERCODE_REG
	srl	ORDERCODE_REG, INVOKE_KEY_SHIFT, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG
	!
	! Fill in the key fields of the exit block
	!
	lduh	[%g6 + JB_PASS_KEY1], ORDERCODE_REG
	sll	ORDERCODE_REG, 28, ORDERCODE_REG
	srl	ORDERCODE_REG, PASS_KEY1_SHIFT, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG

	lduh	[%g6 + JB_PASS_KEY2], ORDERCODE_REG 
	sll	ORDERCODE_REG, 28, ORDERCODE_REG
	srl	ORDERCODE_REG, PASS_KEY2_SHIFT, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG

	lduh	[%g6 + JB_PASS_KEY3], ORDERCODE_REG
	sll	ORDERCODE_REG, 28, ORDERCODE_REG
	srl	ORDERCODE_REG, PASS_KEY3_SHIFT, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG

	lduh	[%g6 + JB_PASS_KEY4], ORDERCODE_REG
	sll	ORDERCODE_REG, 28, ORDERCODE_REG
	srl	ORDERCODE_REG, PASS_KEY4_SHIFT, ORDERCODE_REG
	or	EXITBLK_REG, ORDERCODE_REG, EXITBLK_REG

exfixedkeys:
	ld	[%g6 + JB_ORDERCODE], ORDERCODE_REG
	ld	[%g6 + JB_PASS_STRP], PASS_STR_REG
	ld	[%g6 + JB_PASS_STR_LEN], PASS_STR_LEN_REG
	retl
	nop

loadentryblock:
	ld	[%g6 + JB_ENTRYBLK], ENTRYBLK_REG
	ld	[%g6 + JB_REC_STRP], REC_STR_REG

! must make sure receive string is writable by the kernel
        set     NB_ACCEPT_STRING, REC_STR_MAXLEN_REG
        btst    REC_STR_MAXLEN_REG, ENTRYBLK_REG
        bz      nostring2
        nop

	ld	[%g6 + JB_REC_STR_MAXLEN], REC_STR_MAXLEN_REG
        ldub    [REC_STR_REG], ENTRYBLK_REG
        stb     ENTRYBLK_REG, [REC_STR_REG]  ! first byte
        add     REC_STR_REG, REC_STR_MAXLEN_REG, REC_STR_MAXLEN_REG 
        add     REC_STR_MAXLEN_REG, -1, REC_STR_MAXLEN_REG 
        ldub    [REC_STR_MAXLEN_REG], ENTRYBLK_REG 
        stb     ENTRYBLK_REG, [REC_STR_MAXLEN_REG]  ! last byte
nostring2:
	ld	[%g6 + JB_ENTRYBLK], ENTRYBLK_REG  ! used to touch bytes above
! mostly wasted 6 instructions to insure recieved string is writable by kernel

	set	BR_ENTRY_VARKEYS, REC_STR_MAXLEN_REG
	btst	REC_STR_MAXLEN_REG, %g5
	be	enfixedkeys	! keys fields already filled in
	nop
	!
	! Fill in the key fields of the entry block
	!
	lduh	[%g6 + JB_REC_KEY1], REC_STR_MAXLEN_REG
	sll	REC_STR_MAXLEN_REG, 28, REC_STR_MAXLEN_REG
	srl	REC_STR_MAXLEN_REG, REC_KEY1_SHIFT, REC_STR_MAXLEN_REG
	or	ENTRYBLK_REG, REC_STR_MAXLEN_REG, ENTRYBLK_REG

	lduh	[%g6 + JB_REC_KEY2], REC_STR_MAXLEN_REG
	sll	REC_STR_MAXLEN_REG, 28, REC_STR_MAXLEN_REG
	srl	REC_STR_MAXLEN_REG, REC_KEY2_SHIFT, REC_STR_MAXLEN_REG
	or	ENTRYBLK_REG, REC_STR_MAXLEN_REG, ENTRYBLK_REG

	lduh	[%g6 + JB_REC_KEY3], REC_STR_MAXLEN_REG
	sll	REC_STR_MAXLEN_REG, 28, REC_STR_MAXLEN_REG
	srl	REC_STR_MAXLEN_REG, REC_KEY3_SHIFT, REC_STR_MAXLEN_REG
	or	ENTRYBLK_REG, REC_STR_MAXLEN_REG, ENTRYBLK_REG

	lduh	[%g6 + JB_REC_KEY4], REC_STR_MAXLEN_REG
	sll	REC_STR_MAXLEN_REG, 28, REC_STR_MAXLEN_REG
	srl	REC_STR_MAXLEN_REG, REC_KEY4_SHIFT, REC_STR_MAXLEN_REG
	or	ENTRYBLK_REG, REC_STR_MAXLEN_REG, ENTRYBLK_REG

enfixedkeys:
	ld	[%g6 + JB_REC_STR_MAXLEN], REC_STR_MAXLEN_REG
	retl
	nop


fill_jb_and_return:
	!
	! test the entry block to see if we should accept a return
	! code. If so, load the return code into the jump buffer,
	! otherwise, just go to noretcode.
	!
	ld	[%g6 + JB_ENTRYBLK], EXITBLK_REG
	set	NB_ACCEPT_RETCODE, PASS_STR_REG
	btst	PASS_STR_REG, EXITBLK_REG
	bz	noretcode
        nop
        ld      [%g6 + JB_RETCODEP], EXITBLK_REG
	st	RETCODE_REG, [EXITBLK_REG]
noretcode:
	!
	! test the entry block to see if we should accept a string
	! If not, return.
	! 
	ld	[%g6 + JB_ENTRYBLK], EXITBLK_REG
	set	NB_ACCEPT_STRING, PASS_STR_REG
	btst	PASS_STR_REG, EXITBLK_REG
	bz	chreturn
	nop
	cmp	REC_STR_LEN_REG, REC_STR_MAXLEN_REG  ! this looks backwards
	bge,a	lengthok                             ! but this makes it right
	mov	REC_STR_MAXLEN_REG, REC_STR_LEN_REG
lengthok:
	! test the bit register to see if we specified a location to
	! store the actual length. If so, store the length there, 
	! otherwise, just go to noactlength.
	!
	set	BR_ACCEPT_ACTLENGTH, PASS_STR_REG
	btst	PASS_STR_REG, %g5
	bz	noactlength
	nop
        ld      [%g6 + JB_REC_STR_ACTLENP], EXITBLK_REG
	st	REC_STR_LEN_REG, [EXITBLK_REG]
	b      chreturn
        nop
noactlength:
	! If the length of the string returned is shorter than the
	! length of the buffer we specified, zero out the unused bytes
	! at the end of the buffer.
	!
	cmp	REC_STR_LEN_REG, REC_STR_MAXLEN_REG
	bge	chreturn
	nop
	sub	REC_STR_MAXLEN_REG, REC_STR_LEN_REG, EXITBLK_REG
	add	REC_STR_REG, REC_STR_LEN_REG, PASS_STR_REG

	stb	%g0, [PASS_STR_REG]
chzaploop:
	!
	! loop through the string, writing out zero bytes.
	!
	deccc	EXITBLK_REG
	inc	PASS_STR_REG
	bnz,a	chzaploop
	stb	%g0, [PASS_STR_REG]
	
chreturn:
        or      %g7,%g0,%o7
	retl
        nop
