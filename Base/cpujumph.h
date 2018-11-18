/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef CPUJUMPH_H
#define CPUJUMPH_H
/* The following fields make up the jump parameters */
 
/* Fields defined by the exit block (etc.) of the jumper (or fault) */

struct exitblock {
   unsigned keymask:4;
   unsigned argtype:2;
#define arg_none 0
#define arg_memory 1
#define arg_regs 3
     /* arg_regs is used in the kernel for implicit jumps to indicate */
     /* that the argument is in kernel address space, argaddr will be */
     /* in the DIB for regs and elsewhere in the kernel for implicit */
     /* jumps */
   unsigned jumptype:2;
#define jump_implicit 0
#define jump_call 1
#define jump_return 2
#define jump_fork 3
   unsigned gate:4;
   unsigned reserved:4;
   unsigned key1:4;
   unsigned key2:4;
   unsigned key3:4;
   unsigned key4:4;
};
/* Fields defined by the entry block (etc.) of the jumpee */
 
struct entryblock {
   unsigned reserved1:2;
   unsigned regsparm:1;
   unsigned reserved2:1;
   unsigned rc:1;
   unsigned db:1;
   unsigned str:1;
   unsigned strl:1;
   unsigned keymask:4;
   unsigned reserved3:4;
   unsigned key1:4;
   unsigned key2:4;
   unsigned key3:4;
   unsigned key4:4;
};
/* Values for cpup3switch: */
#define CPUP3_UNLOCKED     0 /* cpup3node not locked */
#define CPUP3_LOCKED       1 
               /* cpup3node is corelocked, so not store into it. */
#define CPUP3_JUMPERKEY 0x10
       /* plus a slot number from 0 through 15. cpup3node is corelocked,
    the jumper's third key is in cpustore3key and should be stored in "slot" */
 
#include "percpu.h"
 #endif /* CPUJUMPH_H */
