/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*******************************************************************
 
  SNODE - Definitions
 
  KC (SNodeF,SnodeF_Create) KEYSFROM(SB,M,SB) KEYSTO(SNODE)
 
  KC (SNode,KT) RCTO() = 0x30D
  KC (SNode,Node_Fetch+(0-15)) KEYSTO(kn)      - Fetch  small slot
  KC (SNode,Node_Swap+(16-31)) KEYSTO(kn)      - Store  small slot
  KC (SNode,SNode_Weaken) KEYSTO(SNodeR)       - Return Read/Only key
  KC (SNode,SNode_Clear)                       - Clear
  KC (SNode,SNode_Fetch) STRUCTFROM(SNode_Slot) KEYSTO(kn) - Fetch
  KC (SNode,SNode_Swap)  STRUCTFROM(SNode_Slot) KEYSFROM(kn)
       KEYSTO(kn)                              - Swap
 
  SNode will store keys in a sparse tree of nodes with a maximum depth
  of 8.  Fetch of a key from the tree always requires DEPTH Jumps
  to walk the tree.
 
********************************************************************/
#ifndef _H_snode
#define _H_snode
 
#include <kktypes.h>
 
#define SNodeF_AKT      0x20D
#define SNode_AKT       0x30D
 
#define SNodeF_Create     0
 
#define SNode_Weaken      32
#define SNode_Clear       39
#define SNode_Fetch       41
#define SNode_Swap        42

#define SNode_MoveRegs    50
 
 struct SNode_Slot {
   UINT32 Slot;
 };
#endif
