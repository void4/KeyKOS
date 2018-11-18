/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
  Defines for Virtual Copy Segment
 
  KC (VCSF,VCSF_Create) KEYSFROM(sb,m,sb) KEYSTO(VCS)
 
  KC (VCS,VCS_CreateROSegmentKey)   KEYSTO(VCSRO)
  KC (VCS,VCS_CreateNCSegmentKey)   KEYSTO(VCSNC)
  KC (VCS,VCS_CreateRONCSegmentKey) KEYSTO(VCSRONO)
  KC (VCS,VCS_ReturnBaseSegmentKey) KEYSTO(RONOCSEG)
  KC (VCS,VCS_ReturnLength) CHARTO(length,8) 
  KC (VCS,VCS_Truncate) 

  KC (VCS,VCS_Freeze) KEYSFROM(sb) KEYSTO(VCSF)
 
***************************************************************/
#ifndef _H_vcs
#define _H_vcs

#include <lli.h>
 
#define VCSF_AKT               0x1B0D
#define VCS_AKT                 0xB0D
 
#define VCSF_Create                 0
 
#define VCS_CreateROSegmentKey      0
#define VCS_ReturnLength            1
#define VCS_CreateNCSegmentKey      2
#define VCS_CreateRONCSegmentKey    3
#define VCS_TruncateSegment         5
#define VCS_ReturnBaseSegmentKey    6
#define VCS_Freeze                 17
 
 struct VCS_SegmentLength {
   unsigned long long Length;
 };

#endif
