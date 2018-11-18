
#ifndef _SYS_PARAM_H
#define	_SYS_PARAM_H

#define	PAGESIZE	4096
#define	PAGESHIFT	12
#define	PAGEOFFSET	0xfff

/*
 * pages to bytes, and back (with and without rounding)
 */
#define	ptob(x)		((x) << PAGESHIFT)
#define	btop(x)		(((unsigned)(x)) >> PAGESHIFT)
#define	btopr(x)	((((unsigned)(x) + PAGEOFFSET) >> PAGESHIFT))


#endif	/* _SYS_PARAM_H */
