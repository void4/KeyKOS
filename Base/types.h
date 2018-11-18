/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef TYPES_H
#define TYPES_H

#if defined(UNIX_BUILD)
#include <sys/types.h>
#else
typedef char char_t;
typedef short short_t;
typedef int int_t;
typedef long long_t;
typedef long long longlong_t;

typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;
typedef unsigned long u_long;
typedef unsigned long ulong;
typedef unsigned long long ulonglong_t;

typedef uint_t u_int;
typedef uchar_t u_char;
typedef ulonglong_t u_longlong_t;

typedef ulonglong_t pa_t;	/* Physical-address type */

#ifndef _SIZE_T
#define	_SIZE_T
typedef	uint_t	size_t;
#endif

typedef char *caddr_t;
typedef long            daddr_t;        /* <disk address> type */
// typedef long pid_t;
typedef	struct	_label_t { int	val[2]; } label_t;

#if defined(KOM)
typedef uint_t domain_t;
#endif

#endif /* defined(UNIX_BUILD) */

#endif /* TYPES_H */

