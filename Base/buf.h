/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _SYS_BUF_H
#define	_SYS_BUF_H

#include "types.h"
#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	A simplified version of UNIX buf structure.
 */
typedef struct	buf {
	int	b_flags;		/* see defines below */
	unsigned b_bcount;		/* transfer count */
	union {
		caddr_t b_addr;		/* low order core address */
		int	*b_words;	/* words for clearing */
	} b_un;
        unsigned int b_resid;           /* words not transferred after error */

#define	paddr(X)	(paddr_t)(X->b_un.b_addr)

} buf_t;

/*
 * These flags are kept in b_flags.
 * The first group is part of the DDI
 */
#define	B_BUSY		0x0001	/* not on av_forw/back list */
#define	B_DONE		0x0002	/* transaction finished */
#define	B_ERROR		0x0004	/* transaction aborted */
#define	B_KERNBUF	0x0008	/* buffer is a kernel buffer */
#define	B_PAGEIO	0x0010	/* do I/O to pages on bp->p_pages */
#define	B_PHYS		0x0020	/* Physical IO potentially using UNIBUS map */
#define	B_READ		0x0040	/* read when I/O occurs */
#define	B_WANTED	0x0080	/* issue wakeup when BUSY goes off */
#define	B_WRITE		0x0100	/* non-read pseudo-flag */

/* Not part of the DDI */
#define	B_AGE		0x000200	/* delayed write for correct aging */
#define	B_ASYNC		0x000400	/* don't wait for I/O completion */
#define	B_DELWRI	0x000800	/* delayed write-wait til buf needed */
#define	B_STALE		0x001000
#define	B_DONTNEED	0x002000	/* after write, need not be cached */
#define	B_REMAPPED	0x004000	/* buffer is kernel addressable */
#define	B_FREE		0x008000	/* free page when done */
#define	B_INVAL		0x010000	/* does not contain valid info  */
#define	B_FORCE		0x020000	/* semi-permanent removal from cache */
#define	B_HEAD		0x040000	/* a buffer header, not a buffer */
#define	B_NOCACHE	0x080000 	/* don't cache block when released */
#define	B_TRUNC		0x100000	/* truncate page without I/O */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BUF_H */
