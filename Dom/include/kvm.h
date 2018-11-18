/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/******************************************************************

   Definitions for KVM (Micro Java VM)

   KVMF(KVM_CreateKVM{0};SB,M,SB) => (rc;KVM)
   KVM(KVM_InstallClassLibrary;Class) => (rc) 
   KVM(KVM_InstallFileDirectory;Directory) => (rc)
   KVM(KVM_InstallCircuitKey;Circuit) => (rc) - probably a socket
   KVM(KVM_InstallTerminalKeys;SIK,SOK;CCK) => (rc) - only needed if no Circuit installed
   KVM(KVM_InstallConsoleKey;CCK) => (rc) - only if no circuit or Terminal
   KVM(KVM_SetNameOfMain,(name)) => (rc)
   KVM(KVM_StartApplication;Key1,Key2,Key3,...) => (rc;Key1,Key2,Key3,...)
   KVM(KVM_FreezeDryApplication) => (rc;AKVMF)

******************************************************************/

#ifndef _H_kvm
#define _H_kvm

#define KVM_AKT                     0x0066
#define KVMF_AKT                    0x0166

#define KVMF_CreateKVM              0
#define KVM_InstallClassLibrary     1
#define KVM_InstallFileDirectory    2
#define KVM_InstallCircuitKey       3
#define KVM_InstallTerminalKeys     4
#define KVM_InstallConsole          5
#define KVM_SetNameOfMain           6
#define KVM_StartApplication        7
#define KVM_StartApplicationFreeze  8
#define KVM_EnableDebug             42

#endif
