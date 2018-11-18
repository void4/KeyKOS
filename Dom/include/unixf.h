/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************

   KC (UnixF,UnixF_Create) KEYSFROM(SB,M,SB) KEYSTO(UNIX)

   KC (UNIX,UNIX_SetEnv) CHARFROM(Environmentstring)
   KC (UNIX,UNIX_MakeFrozenEnvFactory) KEYSFROM(SB,,SB) KEYSTO(EFUNIXF,EFUNIXFB)
   KC (UNIX,UNIX_AddDevice) CHARFROM(name) KEYSFROM(UART)
 
****************************************************************/
#ifndef _H_Unixf
#define _H_Unixf

#define UnixF_Create  0

#define UNIX_SetEnv   2
#define UNIX_MakeFrozenEnvFactory 3
#define UNIX_AddDevice   4

#define UNIX_FreezedryHack  42
#define UNIX_TrussON        256


#endif
