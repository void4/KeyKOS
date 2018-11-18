/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_diskkeyh
#define _H_diskkeyh

#include "keyh.h"

union DiskKey {
	/* N.B. keytype is at the same offset in all of these */
	struct {
		unsigned char keytype;  /* must be datakey, misckey,
					 * chargesetkey, or devicekey */
		unsigned char databody11[11];
	} dkdk;
	struct {
		unsigned char keytype;  /* must be prangekey or nrangekey */
		CDA cda;
		unsigned char rangesize[5];
	} rangekey;
	struct {
		unsigned char keytype;  /* other keytypes */
		CDA cda;
		unsigned char allocationid[4];
		unsigned char databyte;
	} ik;
};
typedef union DiskKey DISKKEY;
#endif

