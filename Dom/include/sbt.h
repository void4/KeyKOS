/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_sbt
#define _H_sbt
/*
  Proprietary Material of Key Logic  COPYRIGHT (c) 1990 Key Logic
*/
/***************************************************************
  Definitions for SpaceBankTransformer (SBT)
 
  SB(SBT_Verify) KEYSFROM(sb)
  SB(SBT_CreateSubrange) KEYSFROM(sb) STRUCTFROM (SBT_Subrange)
                          KEYSTO(newsb)
  SB(SBT_Create) KEYSFROM(sb) KEYSTO (newsb)
  SB(SBT_CreateLimited) KEYSFROM(sb) STRUCTFROM (SBT_Limits)
                          KEYSTO(newsb)
 
***************************************************************/
#include <kktypes.h>
 
#define SBT_AKT                  0x040C
 
#define SBT_Verify                    0
#define SBT_CreateSubrange            1
#define SBT_Create                    2
#define SBT_CreateLimited             5
 
  struct SBT_Limits {
    UINT32 NodeLimit;
    UINT32 PageLimit;
  };
 
  struct SBT_Subrange {
    UINT32 LowerNodeLimit;
    UINT32 UpperNodeLimit;
    UINT32 LowerPageLimit;
    UINT32 UpperPageLimit;
  };
 
#endif
