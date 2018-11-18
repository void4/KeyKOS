/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************
    CCK definitions - applies to CCK thru CCK4
 
 KC CCK(CCK_RecoverKeys) KEYSTO(SIK,SOK,CCK)
 KC CCK(CCK_Disconnect) 
 KC CCK(DESTROY_OC)
 
**************************************************************/

#ifndef _H_cck
#define _H_cck
 
#define CCK_AKT                    0x20E
#define CCK2_AKT                   0x40E
#define CCK3_AKT                   0x50E
#define ZMK_AKT                  0x1050E
 
#define CCK_RecoverKeys  0
#define CCK_Disconnect   2
#define CCK_TAP          4

#define ZMK_Connect      0
#define ZMK_WaitForDisconnect 1
#define ZMK_Disconnect   2

#define ZMK_AlreadyConnected 1
#define ZMK_AlreadyWaiting   1
#define ZMK_NoCircuit        2

#define CCK_TerminateConnection        2
#define CCK_DestroyUnderlyingObject    4
#define CCK_DefineCharacterSet         0
#define CCK_GetTerminalCharacteristics 1
#define CCK_SetStatusMessage           14
#define CCK_DefinePFKey                32
#define CCK_SetActivationMask          256
#define CCK_SetEchoMask                512
#define CCK_ActivateNow                6
#define CCK_EchoCRAsCRLF               8
#define CCK_EchoCRAsCR                 9
#define CCK_EchoLFAsLFCR               10
#define CCK_EchoLFAsLF                 11
#define CCK_SetTabStops                5
#define CCK_StartGobbling              3
#define CCK_StopGobbling               7
 
 struct CCK_CharacterSetValue {
    UINT16 CharacterSet;
#define CCK_ASCII   0
#define CCK_EBCDIC  1
 };
 
 struct CCK_TerminalCharacteristics {
    UINT16 CharacterSet;
    UINT16 Model;           /* 3270 model number */
    UINT16 Columns;
    UINT16 Rows;
 };
 
 struct CCK_TabTable {
    UCHAR  Bits[32];     /* bit per column */
 };

#endif
