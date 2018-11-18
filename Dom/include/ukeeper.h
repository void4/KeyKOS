/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
   ukkeeper.h

   Unix system call handler

   KC (UKeeper,UKeeper_Destroy) 
   KC (UKeeper,UKeeper_Putaway) KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_WakeUp)  KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_SetName) STRUCTFROM(UKeeper_Name) KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_SetBrk)  STRUCTFROM(brkaddress)   KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_SetDirectory) KEYSFROM(Directory,,domain)
   KC (UKeeper,UKeeper_SetSikSok) KEYSFROM(SIK,SOK,domain)
   KC (UKeeper,UKeeper_Init) KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_SetRestartAddr)  STRUCTFROM(address)   KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_SetFrozenAddr)  STRUCTFROM(address)   KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_FreezeDryHack)  KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_TrussOn)        KEYSFROM(,,domain)
   KC (UKeeper,UKeeper_AddDevice)      CHARFROM(name) KEYSFROM(key,,domain)
 
 
****************************************************************/
#ifndef _H_UKeeper
#define _H_UKeeper

#define UKeeper_AKT           0x066
#define UKeeperF_AKT          0x166

#define UKeeper_Destroy       4
#define UKeeper_PutAway       5
#define UKeeper_WakeUp        6
#define UKeeper_SetName       7
#define UKeeper_SetBrk        8
#define UKeeper_SetDirectory  9
#define UKeeper_SetSikSok    10
#define UKeeper_Init         11
#define UKeeper_SetRestartAddr 13
#define UKeeper_SetFrozenAddr 14
#define UKeeper_FreezeDryHack 15
#define UKeeper_TrussOn       16
#define UKeeper_AddDevice     18

struct UKeeper_Name {
     int length;  // length of file
     char name[256]; // name of program
};

#endif
