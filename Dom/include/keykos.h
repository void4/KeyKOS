/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_keykos
#define _H_keykos


/* This file contains the definition of the jump buffer structure
 * which is used by the keykos preprocessor for passing information
 * from a domain to the cjump routine which uses the information to
 * populate machine registers with entry and exit block information.
 * Domain code is required to specify the following in any function
 * wishing to issue CALL's, FORK's or RETURN's:
 *		foo()
 *		{
 *			JUMPBUF;
 *			:
 *			function code
 *			:
 *		}
 *
 *
 * Note: The exitblk and entryblk fields are not always fully populated
 * when the jump buffer is passed to cjump. Some of the fields may need
 * to be calculated at run-time based on the values of other fields in
 * the jump buffer. 
 */

typedef unsigned long ulong_t;

/* Jump buffer structure. Used to hold information which will be
 * manipulated and placed into jump registers prior to trapping into
 * kernel and after returning from the kernel.
 */
typedef struct {
	/* exit block related info */
	ulong_t	exitblk; 
	ulong_t	ordercode;
	char	*pass_strp;
	ulong_t	pass_str_len;
	long	invoke_key;
	short	pass_keys[4];

	/* entry block related info */
	ulong_t	entryblk;
	ulong_t	*returncodep;
	short	*databytep;
	char	*rec_strp;
	ulong_t	rec_str_len;
	ulong_t	rec_str_maxlen;
	ulong_t	*rec_str_actlenp;
	short	rec_keys[4];
        ulong_t scratch[8];   // for use by jump library
} jump_buf_t;

#define JUMPBUF jump_buf_t _jumpbuf


/* Macros used to populate the jump buffer structure */

#define IK(ik) \
	(_jumpbuf.invoke_key = (ik))

#define PS1(psp) \
	(_jumpbuf.pass_strp = (char *)(psp))

#define PS2(psp, psl) \
	(_jumpbuf.pass_strp = (char *)(psp), \
	_jumpbuf.pass_str_len = (psl))

#define KP(pk4, pk3, pk2, pk1) \
	(_jumpbuf.pass_keys[0] = (pk1), \
	_jumpbuf.pass_keys[1] = (pk2), \
	_jumpbuf.pass_keys[2] = (pk3), \
	_jumpbuf.pass_keys[3] = (pk4))

#define RS1(rsp) \
	(_jumpbuf.rec_strp = (char *)(rsp))

#define RS2(rsp, rsl) \
	(_jumpbuf.rec_strp = (char *)(rsp), \
	_jumpbuf.rec_str_maxlen = (rsl))

#define RS3(rsp, rsl, rsal) \
	(_jumpbuf.rec_strp = (char *)(rsp), \
	_jumpbuf.rec_str_maxlen = (rsl), \
	_jumpbuf.rec_str_actlenp = (ulong_t *)&(rsal))

#define KRN(rk4, rk3, rk2, rk1) \
	(_jumpbuf.rec_keys[0] = (rk1), \
	_jumpbuf.rec_keys[1] = (rk2), \
	_jumpbuf.rec_keys[2] = (rk3), \
	_jumpbuf.rec_keys[3] = (rk4))

#define RC(rc) 	(_jumpbuf.returncodep = &(rc))
#define OC(oc)	(_jumpbuf.ordercode = (oc))
#define DB(db)	(_jumpbuf.databytep = &(db))
#define XB(xb)	(_jumpbuf.exitblk = (xb))
#define NB(nb)	(_jumpbuf.entryblk = (nb))

/* Miscellaneous defines */
#define KT	0x80000000u
#define EXIT	_cend()
typedef short KEY;
static KEY _0 = 0;

#if !defined(NULL)
#define NULL 0
#endif
#endif
