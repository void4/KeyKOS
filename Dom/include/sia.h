/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************
  Small Integer Allocator

  Factory call:
  KC (SIAF,SIAF_Create) KEYSFROM(sb,m,sb) KEYSTO(SIA)
 
  Object call:
  KC SIA(SIA_AllocateNewInteger) RCTO(nsi)
        nsi = smallest unallocated integer > 0 or -1 if none can
              be allocated
  KC SIA(SIA_FreeInteger+si)
         si = integer > 0, si is deallocated and c = 0 if it was
              previously allocated and 1 if it was not.
  KC SIA(SIA_ReturnLowestAllocated)
         c = smallest allocated integer or zero if none are
             alllocated
  KC SIA(KT) RCTO(rc=X'26')
  KC SIA(KT+4)
         destroys the SIA domain
 
 ****************************************************************/
#ifndef _H_sia
#define _H_sia
 
#define SIA_AKT                     0x0026
#define SIAF_AKT                    0x0126

#define SIAF_Create                 0
 
#define SIA_AllocateNewInteger      0
#define SIA_FreeInteger
#define SIA_ReturnLowestAllocated   0xFFFFFFFF
#endif
