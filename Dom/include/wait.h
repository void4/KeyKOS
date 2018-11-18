/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************
 
  Defines for WAIT
 
  KC (WaitF,WaitF_Create) KEYSTO(Wait)
 
  KC (Wait,Wait_Wait)
  KC (Wait,Wait_SetTOD)             STRUCTFROM(Wait_TOD)
  KC (Wait,Wait_SetTODAndWait)      STRUCTFROM(Wait_TOD)
  KC (Wait,Wait_SetInterval)        STRUCTFROM(Wait_Interval)
  KC (Wait,Wait_SetIntervalAndWait) STRUCTFROM(Wait_Interval)
  KC (Wait,Wait_ShowTime)           STRUCTTO(Wait_Value)
  KC (Wait,Wait_ShowTOD)            STRUCTTO(Wait_TOD)
 
***************************************************************/
#ifndef _H_wait
#define _H_wait

#include <lli.h>
 
#define Wait_AKT              0x25
#define WaitF_AKT             0x0325
 
#define WaitF_Create             0
 
#define Wait_Wait                0
#define Wait_SetTOD              1
#define Wait_SetInterval         2
#define Wait_SetTODAndWait       3
#define Wait_SetIntervalAndWait  4
#define Wait_ShowTime            5
#define Wait_ShowTOD             7
 
 struct WaitTOD {
   LLI Epoch;
 };
 
 struct Wait_Interval {
   LLI Microseconds;
 };
 
 struct Wait_Value {
   char daytime[32];
 };
 
#endif
