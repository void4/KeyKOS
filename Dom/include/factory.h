/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
/***************************************************************
    Factory Definitions
 
 Factory Creator
 
    FC(FC_Create) KEYSFROM(psb) KEYSTO(FactoryBuilder)
    FC(FC_CreateDC) KEYSFROM(psb,DCF) KEYSTO(FactoryBuilder)
    FC(FC_CreateMHF) KEYSFROM(psb,,sb) STRUCTFROM(Factory_HoleCapacity)
        KEYSTO(FactoryBuilder)
    FC(FC_CreateDCMHF) KEYSFROM(psb,DCF,sb)
        STRUCTFROM(Factory_HoldCapacity)  KEYSTO(FactoryBuilder)
    FC(FC_RecallFetcher) KEYSFROM(FactoryR) KEYSTO(FactoryF)
    FC(FC_RecallBuilder) KEYSFROM(FactoryR) KEYSTO(FactoryB)
    FC(FC_DisableFetcher) KEYSTO(FC)
    FC(FC_DisableBuilder) KEYSTO(FC)
 
 Factory Builder
 
    FactoryB(FactoryB_Install...) KEYSFROM(component)
        STRUCTFROM(FactoryB_...)
    FactoryB(FactoryB_Assign...)
    FactoryB(FactoryB_MakeRequestor) KEYSTO(FactoryR,FactoryF)
    FactoryB(FactoryB_MakeCopy) KEYSTO(FactoryC)
    FactoryB(FactoryB_GetMiscellaneous) KEYSTO(component)
        STRUCTTO(Factory_GetMiscellaneousValues)
    FactoryB(FactoryB_GetDC) KEYSTO(DC)
 
 Factory Copy
 
    FactoryC(FactoryC_MakeCopy) KEYSFROM(psb,,{sb})
       STRUCTFROM(Factory_Capacity) KEYSTO(FactoryB)
    FactoryC(FactoryC_GetMiscellaneous) KEYSTO(component)
       STRUCTTO(Factory_GetMiscellaneousValues)
 
 Factory Fetcher
 
    FactoryF(FactoryF_Fetch) KEYSTO(component)
    FactoryF(FactoryF_MakeRequestor) KEYSTO(FactoryR,FactoryF)
    FactoryF(FactoryF_Compare) KEYSFROM(FactoryR) RCTO(rc)
    FactoryF(FactoryF_GetMiscellaneous) KEYSTO(component)
        STRUCTTO(Factory_GetMiscellaneousValues)
 
 Factory Requestor
 
    FactoryR(FactoryR_Create) KEYSFROM(psb,meter,anykey)
       KEYSTO(....) CHARTO(....) RCTO(rc)
           {return determined by object created }
    FactoryR(FactoryR_Compare) KEYSFROM(FactoryR) RCTO(rc)
 
**************************************************************/
#ifndef _H_factory
#define _H_factory

#include <kktypes.h>
 
 
#define DBBUILDER         0
#define DBREQUESTOR       1
#define DBFETCHER         2
#define DBCOPY            3
 
#define DBNORECALLBUILDER 1
#define DBNORECALL        2
 
#define FCC_AKT           0x031E
#define FC_AKT            0x1E
#define FB_AKT            0x011E
#define FCopy_AKT         0x021E
#define FR_AKT            0xFF1E

#define FactoryDCNotValid  5

#define FC_Create          0
#define FC_CreateDC        1
#define FC_CreateMHF       2
#define FC_CreateDCMHF     3
#define FC_DisableFetcher  4
#define FC_DisableBuilder  5
#define FC_RecallFetcher   6
#define FC_RecallBuilder   7
 
#define FactoryB_InstallSensory   0
#define FactoryB_InstallFactory   32
#define FactoryB_InstallHole      128
#define FactoryB_AssignKT         64
#define FactoryB_AssignOrdinal    65
#define FactoryB_MakeRequestor    66
#define FactoryB_MakeCopy         67
#define FactoryB_GetDC            69
#define FactoryB_GetMiscellaneous 96
 
#define FactoryC_Copy             0
#define FactoryC_GetMiscellaneous 96
 
#define FactoryF_Fetch            0
#define FactoryF_MakeRequestor    66
#define FactoryF_GetMiscellaneous 96
#define FactoryF_Compare          0xFFFFFFFFu
 
#define FactoryR_Create           0
#define FactoryR_Compare          0xFFFFFFFFu
 
 struct Factory_HoleCapacity {
    UINT32 Capacity;
 };
 
 struct FactoryB_Address {
    UINT32 Address;    /* starting address */
 };
 
 struct FactoryB_Value {
    UINT32 Value;      /* KT or Ordinal Value */
 };
 
 struct Factory_GetMiscellaneousValues {
    UINT32 Address;   /* Starting address */
    UINT32 AKT;       /* KT of Requestor Key */
    UCHAR  Bits[3]; /* Component description bits */
    UCHAR  Flags;     /* Bit 6 is 1 if not a Fetcher Factory */
#define COMPLETE 4
#define DOTPROGRAM 2
#define SHAREDDC 1
    UINT32 Ordinal;   /* Ordinal of Requestor Key */
 };
#endif
