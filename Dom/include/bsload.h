/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
  Defines for BSLOAD
 
  KC (BSLOADF,Bsloadf_Create) KEYSFROM(sb,m,sb) KEYSTO(BSLOAD)
 
  KC (BSLOAD,Bsload_LoadSimpleElf) KEYSFROM(CODESEG,MEMSEG) STRUCTTO(StartAddr)
 
***************************************************************/
#ifndef _H_bsload
#define _H_bsload

#define BsloadF_AKT            0x111
#define Bsload_AKT             0x011
 
#define Bsloadf_Create              0
 
#define Bsload_LoadSimpleElf        0
#define Bsload_LoadDynamicElf       1

struct Bsload_StartAddr {
       unsigned long sa;
};

#endif
