/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
  Callseg Defines
 
   KC (Callseg,Callseg_ReturnSegmentKey    KEYSTO(kn)
   KC (Callseg,Callseg_ReplaceSegmentKey   KEYSFROM(kn)
   KC (Callseg,Callseg_ReadSegmentData)
               CHARFROM(offset,length) CHARTO(returned_data)
   KC (Callseg,Callseg_WriteSegmentData)
               CHARFROM(offset,data)
 
****************************************************************/
#ifndef _H_callseg
#define _H_callseg

#define Callseg_AKT               0x110D
#define CallsegF_AKT              0x120D
 
#define Callseg_ReturnSegmentKey     0
#define Callseg_ReplaceSegmentKey    1
#define Callseg_ReadSegmentData      2
#define Callseg_WriteSegmentData     3
 
#endif
