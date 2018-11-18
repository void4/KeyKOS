/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* sysgenh.h - Allocation of device blocks */
#if !defined(diskless_kernel)
#include "devmdh.h"
#endif
#include "kermap.h" /* for windows */
#define MAXDEVICES 4

extern struct Device ldev1st[MAXDEVICES];
extern struct Device *ldevlast; /* last+1 */

extern struct PhysDevice physdevs[MAXPHYSDEVS];
extern struct PhysDevice *physdevlast; /* last+1 */

