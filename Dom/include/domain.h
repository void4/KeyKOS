/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************
   Domain Definitions
 
   KC (Domain,Domain_Get+(1,2,3,10,11)) KEYSTO(k)
   KC (Domain,Domain_GetMeter)      KEYSTO(meter)
   KC (Domain,Domain_GetKeeper)     KEYSTO(keeper)
   KC (Domain,Domain_GetMemory)     KEYSTO(memory)
   KC (Domain,Domain_GetKey)        KEYSTO(key)    - Keys Regs
   KC (Domain,Domain_SwapMeter)     KEYSFROM(meter) KEYSTO(meter)
   KC (Domain,Domain_SwapKeeper) KEYSFROM(keeper) KEYSTO(keeper)
   KC (Domain,Domain_SwapMemory) KEYSFROM(memory) KEYSTO(memory)
   KC (Domain,Domain_SwapKey)    KEYSFROM(key) KEYSTO(key)
   KC (Domain,Domain_MakeStart)  STRUCTFROM(Domain_DataByte)
                                 KEYSTO(startkey)
   KC (Domain,Domain_MakeAvailable)
   KC (Domain,Domain_MakeFaultKey)  KEYSTO(faultkey)
   KC (Domain,Domain_MakeBusy)      KEYSTO(faultkey)
   KC (Domain,Domain_GetControl) STRUCTTO(Domain_ControlData)
   KC (Domain,Domain_PutControl) STRUCTFROM(Domain_ControlData)
   KC (Domain,Domain_GetRegs)    STRUCTTO(Domain_Registers)
   KC (Domain,Domain_PutRegs)    STRUCTFROM(Domain_Registers)
   KC (Domain,Domain_MakeReturnKey) KEYSTO(returnkey)
   KC (Domain,Domain_ReplaceMemory) KEYSFROM(memory)
                                    STRUCTFROM(Domain_Address)
   KC (Domain,Domain_PutStuff) STRUCTFROM(Domain_RegistersAndControl)
   KC (Domain,Domain_Compare) KEYSFROM(somekey)
 
*****************************************************************/
#ifndef _H_domain
#define _H_domain

#include "kktypes.h"
 
#define Domain_AKT                   7
 
#define Domain_Get                   0
#define Domain_GetMeter              1
#define Domain_GetKeeper             2
#define Domain_GetMemory             3
#define Domain_GetKey               16
#define Domain_Swap                 32
#define Domain_SwapMeter            33
#define Domain_SwapKeeper           34
#define Domain_SwapMemory           35
#define Domain_SwapKey              48
#define Domain_MakeStart            64
#define Domain_MakeAvailable        65
#define Domain_MakeFaultKey         66
#define Domain_MakeBusy             67
#define Domain_GetControl           68
#define Domain_PutControl           69
#define Domain_GetRegs              70
#define Domain_PutRegs              71
#define Domain_MakeReturnKey        73
#define Domain_ReplaceMemory        74
#define Domain_PutStuff             75
#define Domain_Compare              80

#define Domain_Get88KControl       150
#define Domain_Put88KControl       151
#define Domain_Reset88KControl     152
#define Domain_Get88KRegs          153
#define Domain_Put88KRegs          154
#define Domain_Get88KStuff         155
#define Domain_Put88KStuff         156
#define Domain_Reset88KStuff       157
#define Domain_Get88KDataSpace     158
#define Domain_Get88KInstructionSpace  159
#define Domain_Put88KDataSpace    160
#define Domain_Put88KInstructionSpace  161

#define Domain_GetSPARCControl     201
#define Domain_PutSPARCControl     202
#define Domain_ResetSPARCControl   203
#define Domain_GetSPARCRegs        204
#define Domain_PutSPARCRegs        205
#define Domain_GetSPARCStuff       206
#define Domain_PutSPARCStuff       207
#define Domain_ResetSPARCStuff     208
#define Domain_GetSPARCFQ          209
#define Domain_ClearSPARCFQ        210
#define Domain_GetSPARCOldWindows  211
#define Domain_ClearSPARCOldWindows 212
#define Domain_SPARCCopyCaller     213

 struct Domain_DataByte {
    UCHAR Databyte;
 };
 
 typedef UCHAR Domain_KeyData[6];
 typedef UCHAR Domain_RegData[4];
 typedef UCHAR Domain_FltData[8];
 
 struct Domain_Address {
    void  *Address;
 };
 
#ifdef NOTSPARC
 struct Domain_ControlData {
   Domain_KeyData PSW;    /* program status word */
   Domain_KeyData TC;     /* trap code */
   Domain_KeyData MC;     /* Monitor control */
   Domain_KeyData PER1;   /* per control word 1 */
   Domain_KeyData PER2;   /* per control word 2 */
   Domain_KeyData PC;     /* Per Code and address */
 };
 
 struct pipe_stage {
    long DMT,DMD,DMA;
 };

 struct Domain_88KControlData {
   UINT32 FCR62,FCR63,PSR,XIP,NIP,FIP;
   UINT32 TRAPCODE;
   UINT32 TRAPEXT[5];
   struct pipe_stage data_access[3];
 };

 
 struct Domain_Registers {
   Domain_RegData Regs[16];
   Domain_FltData FRegs[4];
 };

 struct Domain_88KRegData {
   UINT32 Regs[32];
 };
 
 struct Domain_RegistersAndControl {
   struct Domain_Registers Regs;
   struct Domain_ControlData Control;
 };
 
 struct Domain_88KRegistersAndControl {
   struct Domain_88KRegData Regs;
   struct Domain_88KControlData Control;
 };
#endif

 struct Domain_SPARCRegData {
    UINT32 g[8];    /* g0 is really the Y register */
    UINT32 o[8];
    UINT32 l[8];
    UINT32 i[8];
 };

 struct Domain_SPARCControlData {
    UINT32 PC,NPC,PSR,TRAPCODE,TRAPEXT[2],FSR,FPDTQ[2];
 };

#define PSR_JUMPS_ALLOWED  0x00000080

 struct Domain_SPARCRegistersAndControl {
   struct Domain_SPARCRegData Regs;
   struct Domain_SPARCControlData Control;
 };

 struct Domain_SPARCFQ {
   long long FQ[1];
 };
 
 struct Domain_SPARCOldWindow {
    UINT32 l[8];
    UINT32 i[8];  
 };

 struct Domain_SPARCOldWindows {
    struct Domain_SPARCOldWindow window[1];
 };

 struct Domain_SPARCCopyInstructions {
    unsigned copyregisters:1;
    unsigned copyfloats:1;
    unsigned copykeys:1;
    unsigned copyslot10:1;
    unsigned copyslot11:1;
    unsigned substitutedomain:1;
    
    unsigned char domainkeyslot;
    unsigned short keymask;
    unsigned long unused;   
 };
 
#endif
