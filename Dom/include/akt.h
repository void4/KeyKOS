/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*akt.h

  Define the alleged key types

  This is drawn from the online manual (<p3,akt>)
  but is here for the convenience of progrmmers.  The individual header 
  files will no longer contain the Alleged key types of the object.

  This file is advisory at this time.  No source includes it. 
  This is a single place to assign AKTs for new objects
******************************************************************************/
#ifndef _H_akt
#define _H_akt

/****************************************************************************
  Standard Alleged Types 
****************************************************************************/

#define ALWAYS_AKT               0x0000

#define Page_AKT                 0x0202
#define Page_ROAKT               0x1202
#define PageRange_AKT            0x0302

#define Node_NODEAKT             0x0003
#define NodeRange_AKT            0x0303

#define Node_SENSEAKT            0x0001
#define Node_FETCHAKT            0x0004

#define Node_SEGMENTMASK     0xFFFFF0FF
#define Node_SEGMENTAKT          0x0005
#define Node_ROSEGMENTAKT        0x1005

#define Meter_AKT                0x0006
#define MeterKeeper_AKT          0x0106
#define MeterKeeperFactory_AKT   0x0206
#define MeterSwitch_AKT          0x0306
#define MeterSwitchFactory_AKT   0x0406

#define Domain_AKT               0x0007
/*  0xR007 includes restrictions */

#define DomTool_AKT              0x0109
#define Uart_AKT                 0x0209
#define DatakeyCreator_AKT       0x0309
#define Discrim_AKT              0x0409
#define Systimer_AKT             0x0509
#define Calclock_AKT             0x0609
#define Peek_AKT                 0x0909
#define SimpleChargeSet_AKT      0x0A09
#define JournalizePage_AKT       0x0B09
#define Console_AKT              0x0C09
#define KDIAG_AKT                0x0D09

#define ChargeSet_AKT            0x000B

#define SB_AKT                   0x000C
#define Account_AKT              0x010C
#define SBT_AKT                  0x040C

#define DC_AKT                   0x000D
#define DCC_AKT                  0x010D
#define SNodeF_AKT               0x020D
#define SNode_AKT                0x030D
#define FSF_AKT                  0x040D
#define FS_AKT                   0x050D
#define GDBF_AKT                0x1080D
#define Switcher_AKT             0x090D
#define SwitcherF_AKT           0x1090D
#define VDKF_AKT                 0x0A0D
#define VDK_AKT                 0x10A0D
#define VDK2RCF_AKT             0x20A0D
#define VDK2RC_AKT              0x30A0D
#define VCS_AKT                  0x0B0D
#define VCSF_AKT                 0x1B0D
#define CDUMP_AKT                0x0E0D
#define BinderF_AKT              0x0F0D
#define LSF_AKT                 0x10F0D
#define Binder_AKT              0x20F0D
#define LS_AKT                  0x30F0D
#define CKERN_AKT               0x40F0D
#define CKERNF_AKT              0x50F0D
#define Waakener_AKT             0x100D
#define Callseg_AKT              0x110D
#define CallsegF_AKT             0x120D

#define CCK_AKT                  0x020E
#define CCK2_AKT                 0x040E
#define CCK3_AKT                 0x050E
#define CCK5_AKT                 0x070E
#define CCK6_AKT                 0x080E
#define CCK7_AKT                 0x0A0E
#define Sik2simF_AKT             0x160E
#define Sik2simCCK_AKT           0x060E
#define ZMK_AKT                 0x1050E
#define TMMK_AKT                0x1060E
#define TSSF_AKT                 0x090E

#define RCF_AKT                  0x000F
#define TDOF_AKT                 0x000F

#define BsloadF_AKT              0x0111
#define Bsload_AKT               0x0011

#define ClockF_AKT               0x0112
#define Clock_AKT                0x0012

#define EditF_AKT                0x0313
#define Edit_AKT                 0x0213

#define TDO_ESAKT                0x0016
#define TDO_NSAKT                0x0017
#define RC_ESAKT                 0x0016
#define RC_NSAKT                 0x0017

#define BSP_AKT                  0x0019
#define BSC_AKT                  0x001A
#define BSCRF_AKT                0x001B

#define SCSC_AKT                 0x001D
#define ForkF_AKT                0x003D
#define ForkControl_AKT          0x013D

#define FCC_AKT                  0x031E
#define FC_AKT                   0x001E
#define FB_AKT                   0x011E
#define FCopy_AKT                0x021E
#define FR_AKT                   0xFF1E

#define PCSF_AKT                 0x0023
#define CSF_AKT                  0x0123
#define PCSKeepF_AKT             0x0223

#define WaitF_AKT                0x0325
#define Wait_AKT                 0x0025
#define MBWait2_AKT              0x0425
#define MBWait2F_AKT             0x0525
#define KIWait_AKT               0x0625

#define SIA_AKT                  0x0026
#define SIAF_AKT                 0x0126

#define KID_AKT                  0x0027
#define KIDC_AKT                 0x0127

#define Registry_AKT             0x0029
#define RegistryF_AKT            0x0129

#define Addludky_AKT             0x002a
#define AddludkyRecep_AKT        0x012a
#define Chgludky_AKT             0x022a
#define AddludkyF_AKT            0x032a

#define Tape_AKT                 0x002b

#define Device_AKT               0x002c
#define BDevice_AKT              0x012c

#define CswitchF_AKT             0x002d
#define CswitchS_AKT             0x012d
#define CswitchD_AKT             0x020d

#define Metermonitor_AKT         0x002E
#define MeterMonitorF_AKT        0x032E

#define Join_AKT                 0x0031
#define JoinF_AKT                0x0131

#define ForkF_AKT                0x003D
#define ForkC_AKT                0x013D

#define Recep_AKT                0x003E
#define RecepF_AKT               0x023E

#define Datacopy_AKT             0x0064
#define DatacopyF_AKT            0x0164

#define AUTH_AKT                 0x0065
#define AUTHF_AKT                0x0165

#define KVM_AKT                  0x0066
#define KVMF_AKT                 0x0166

#define UKeeper_AKT              0x0067
#define UKeeperF_AKT             0x0167

#define PSchedF_AKT              0x0168
#define PSchedAdmin_AKT          0x0068
#define PSchedNotify_AKT         0x0268
#define PSchedStatus_AKT         0x0368
#define PSchedChange_AKT         0x0468

#define UnixF_AKT                0x0169
#define UNIX_AKT                 0x0069
#define DevTTYF_AKT              0x0269
#define DevTTY_AKT               0x0369
#define DevCONSF_AKT             0x0469
#define DevCONS_AKT              0x0569


#endif
