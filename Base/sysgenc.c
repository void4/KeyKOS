/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* sysgenc.c - Allocation of device blocks */
#include "sysdefs.h"
#include "devmdh.h"
#include "kermap.h" /* for windows */
#include "sysgenh.h"

struct Device ldev1st[MAXDEVICES];
struct Device *ldevlast = ldev1st;

struct PhysDevice physdevs[MAXPHYSDEVS];
struct PhysDevice *physdevlast = physdevs;

