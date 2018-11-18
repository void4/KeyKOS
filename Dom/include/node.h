/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
    Node Defines
 
   KC (Node,Node_Fetch+(0-15))   KEYSTO(kn)              - Fetch
   KC (Node,Node_Swap+(0-15))    KEYSFROM(kn) KEYSTO(kn) - Swap
   KC (Node,Node_Compare)        KEYSFROM(node)          - Compare
   KC (Node,Node_Clear)                                  - Clear
   KC (Node,Node_DataByte)       RCTO(databyte)          - Fetch DB
   KC (Node,Node_MakeFetchKey)   KEYSTO(NodeF)           - Weaken
   KC (Node,Node_MakeSegmentKey) KEYSTO(SegmentKey)
   KC (Node,Node_MakeMeterKey)   KEYSTO(MeterKey)
   KC (Node,Node_MakeNodeKey)    KEYSTO(NodeKey)
   KC (Node,Node_MakeSenseKey)   KEYSTO(SenseKey)
   KC (Node,Node_WriteData)      STRUCTFROM(Node_KeyValues)
****************************************************************/

#ifndef _H_node
#define _H_node
 
#include "kktypes.h"
#define Node_KEYLENGTH           16
 
#define Node_SENSEAKT             1
#define Node_NODEAKT              3
#define Node_FETCHAKT             4
#define Node_SEGMENTAKT        1005
#define Node_SEGMENTMASK 0xFFFFF0FF
 
#define Node_Fetch                0
#define Node_Swap                16
#define Node_Compare             38
#define Node_Clear               39
#define Node_DataByte            40
#define Node_MakeFetchKey        32
#define Node_MakeSegmentKey      33
#define Node_MakeMeterKey        34
#define Node_MakeNodeKey         35
#define Node_MakeSenseKey        36
#define Node_MakeFrontendKey     37
#define Node_WriteData           45
 
typedef struct { 
        UCHAR Byte[Node_KEYLENGTH];
} Node_KeyData;
 
/*************************************************************
   Macro to generate Window Keys from constants
     This macro is ONLY good for initializing static slots
 
   WindowM( highbyte, Pageoffset, Slot, RO=1, Nocall=1)
 
        WindowM(0,0x00000100,2,1,0);
        Generates a RO window key over slot 2
        with offset x'100000' (256 (0x100) pages offset)
 
**************************************************************/
 
#define WindowM(a,b,c,d,e) { 0,0,0,0,0,0,0,0,0,0,\
                            (((a)&0x0F)<<4)|(b>>28),\
                            (((b)>>20)&0xF0)|(((b)>>20)&0x0F),\
                            (((b)>>12)&0xF0)|(((b)>>12)&0x0F),\
                            (((b)>>4)&0xF0)|(((b)>>4)&0x0F),\
                            (((b)&0x0F)<<4),((c)<<4)|\
                            (((d)&0x01)<<3)|(((e)&0x01)<<2)|0x02}
 
/*************************************************************
   Macro to generate Background Window Keys from constants
 
   WindowBM( highbyte, Pageoffset, Slot, RO=1, Nocall=1)
     This macro is ONLY good for initializing static slots
 
        WindowBM(0,0x00010000,2,0,0);
        Generates a write enabled window key over slot 2
        with offset x'10000'
 
**************************************************************/
 
#define WindowBM(a,b,c,d,e) { 0,0,0,0,0,0,0,0,0,0,\
                            (((a)&0x0F)<<4)|(b>>28),\
                            (((b)>>20)&0xF0)|(((b)>>20)&0x0F),\
                            (((b)>>12)&0xF0)|(((b)>>12)&0x0F),\
                            (((b)>>4)&0xF0)|(((b)>>4)&0x0F),\
                            (((b)&0x0F)<<4),((c)<<4)|\
                            (((d)&0x01)<<3)|(((e)&0x01)<<2)|0x03}
/*************************************************************
   Macro to generate Format Keys from constants
 
   FormatK( pp2=1, background slot, keepslot, init, ssc)
     This macro is ONLY good for initializing static slots
 
        FormatK(0,15,14,0,3);
        Generates a Format key with the Keeper in slot 14
        and no initial slots
 
**************************************************************/
 
#define FormatK(a,b,c,d,e)  { 0,0,0,0,0,0,0,0,0,0,0,0,\
                            ((a)<<4)|0x0F,\
                            ((b)&0x0F)|0xF0,\
                            ((c)<<4)|0x0F,\
                            ((d)<<4)|((e)&0x0F)}
/*************************************************************
   Macro to generate Format Keys from constants  (new style)
 
   Format1K( pp2=1, keyslot, background slot, keepslot, init, ssc)
     This macro is ONLY good for initializing static slots
 
        Format1K(0,1,15,14,0,3);
        Generates a Format key with the Keeper in slot 14
        and no initial slots.  Callers key 3 will go into slot 1
 
**************************************************************/
 
#define Format1K(a,b,c,d,e,f) { 0,0,0,0,0,0,0,0,0,0,0,0,\
                              ((a)<<4)|0x0F,\
                              ((b)<<4)|((c)&0x0F),\
                              ((d)<<4)|0x0F,\
                              ((e)<<4)|((f)&0x0F)}
 
  struct Node_KeyValues {
      UINT32 StartSlot;
      UINT32 EndSlot;
      Node_KeyData Slots[16];
  };
 
  struct Node_DataByteValue {
      UCHAR Byte;
  };
 
#endif
