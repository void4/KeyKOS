/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************       
                                                                        
  Trusted Directory Defines (same as Record Collection)                 
                                                                        
   KC (TDOF,TDOF_CreateEntrySequence) KEYSFROM(sb,m,sb) KEYSTO(TDO)     
   KC (TDOF,TDOF_CreateNameSequence) KEYSFROM(sb,m,sb) KEYSTO(TDO)      
                                                                        
   KC (TDO,oc) CHARFROM(pstring,len) CHARTO(rstring,len)                
      KEYSFROM(key) KEYSTO(key) RCTO(rc);                               
                                                                        
      oc ->          U32INT (see below)                                 
      pstring  ->    (1,nl)(nl,name)(data)- no structure available      
      rstring  ->    (1,nl)(nl,name)(data)- no structure available      
      rc ->          U32INT (see KOR under RCOL)                        
****************************************************************/       
#ifndef _H_tdo
#define _H_tdo
                                                                        
#define TDOF_AKT              0x0F                                      
#define TDO_ESAKT             0x16                                      
#define TDO_NSAKT             0x17                                      
                                                                        
#define TDOF_CreateEntrySequence 0                                      
#define TDOF_CreateNameSequence  1                                      
                                                                        
#define TDO_Weaken              0                                       
#define TDO_Empty               1                                       
#define TDO_AddReplace          2                                       
#define TDO_Add                 3                                       
#define TDO_Replace             4                                       
#define TDO_AddReplaceKey       5                                       
#define TDO_AddKey              6                                       
#define TDO_ReplaceKey          7                                       
#define TDO_GetFirst            8                                       
#define TDO_GetLessThan         9                                       
#define TDO_GetLessEqual       10                                       
#define TDO_GetEqual           11                                       
#define TDO_GetGreaterEqual    12                                       
#define TDO_GetGreaterThan     13                                       
#define TDO_GetLast            14                                       
#define TDO_Delete             15                                       
#define TDO_TruncateAtName     16                                       
#define TDO_TruncateAfterName  17                                       
#define TDO_WriteUserData      18                                       
#define TDO_ReadUserData       19                                       
#endif
