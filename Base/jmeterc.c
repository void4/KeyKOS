/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "primcomh.h"
#include "kernkeyh.h"


void jmeter(key)   /* Handle jumps to meter keys */
struct Key *key;
{
   if (cpuordercode == KT) simplest(6);
   else simplest(KT+2);
} /* end jmeter */
