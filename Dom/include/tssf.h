/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************************
  tssf.h

******************************************************************************/
#ifndef _H_tssf
#define _H_tssf

#define TSSF_AKT      0x90E

#define TSSF_CreateCCK2  2
#define TSSF_CreateZMK   3
#define TSSF_CreateTMMK  4

#define TSSF_Unsupported 3

#define TMMF_InvalidSwitch 3

#define TMMK_CreateBranch 0
#define TMMK_DestroyBranch 1
#define TMMK_SwitchInput  2
#define TMMK_SwitchOutput 3
#define TMMK_DestroyTMM   4
#define TMMK_GenerateASCIIInput 5
#define TMMK_WaitForActiveBranch 7
#define TMMK_BranchStatus 8

#define TMMK_AKT      0x1060E

#define TMMK_NOSPACE      1
#define TMMK_NOTABRANCH   1
#define TMMK_ALREADYWAITING -1
#define TMMK_CANTDESTROYCONTROL 2

#endif
