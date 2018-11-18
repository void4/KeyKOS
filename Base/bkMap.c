/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
// #include "bootconf.h"
#include "kktypes.h"
#include "sysdefs.h"
#include "keyh.h"
#include "sparc_mem.h"
#include "memomdh.h"
#include "wsh.h"
// #include "iommu.h"
#include "realkermap.h"
#include "sparc_asm.h"

ME *WindowPageTable = 0;
/* The above is the kernel's virtual address for a portion
of a page table which is modified as the kernel runs,
to provide the kernel with access to varying locations, 
often in some domain's address space.
There are TOTAL_MAPWIN_SIZE windows for the kernel to use.
They cost very little. They tend to be allocated one per
kernel function needing such access. There is little economy
in sharing them. */
// There was interesting code here but it has been largely supplanted
// by the micro_loader.s stuff.

uchar * map_any_window(int window, uint32 busaddr, int rw)
{ if((uint32)window >= TOTAL_MAPWIN_SIZE) crash("Invalid kernel window");
  WindowPageTable[window] = (busaddr >> 4) | (rw ? 0x7E : 0x7A);
//  WindowPageTable[window] = (busaddr >> 4) | (rw ? 0xFE : 0xFA);
  {uchar * vw = vWindows + (window<<12);
  sta03((int)vw, 0); return vw;}}


uchar * map_uncached_window(int window, uint32 busaddr36, int rw)
{ if((uint32)window >= TOTAL_MAPWIN_SIZE) crash("Invalid kernel window");
  WindowPageTable[window] = busaddr36 | (rw ? 0x7E : 0x7A);
  {uchar * vw = vWindows + (window<<12);
  sta03((int)vw, 0); return vw;}}
