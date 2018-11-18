/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************       
                                                                        
   Record Collection Defines            
                                                                        
   KC (RCF,RCF_CreateEntrySequence+2*maxlength) KEYSFROM(sb,m,sb) KEYSTO(RC)     
   KC (RCF,RCF_CreateNameSequence+2*maxlength) KEYSFROM(sb,m,sb) KEYSTO(RC)      
                                                                        
   KC (RC,oc) CHARFROM(pstring,len) CHARTO(rstring,len)                
      KEYSFROM(key) KEYSTO(key) RCTO(rc);                               
                                                                        
      oc ->          U32INT (see below)                                 
      pstring  ->    (1,nl)(nl,name)(data)- no structure available      
      rstring  ->    (1,nl)(nl,name)(data)- no structure available      
      rc ->          U32INT (see KOR under RCOL)                        
****************************************************************/       
#ifndef _H_rc
#define _H_rc
                                                                        
#define RCF_AKT              0x0F                                      
#define RC_ESAKT             0x16                                      
#define RC_NSAKT             0x17                                      
                                                                        
#define RCF_CreateEntrySequence 0                                      
#define RCF_CreateNameSequence  1                                      
                                                                        
#define RC_Weaken              0                                       
#define RC_Empty               1                                       
#define RC_AddReplace          2                                       
#define RC_Add                 3                                       
#define RC_Replace             4                                       
#define RC_AddReplaceKey       5                                       
#define RC_AddKey              6                                       
#define RC_ReplaceKey          7                                       
#define RC_GetFirst            8                                       
#define RC_GetLessThan         9                                       
#define RC_GetLessEqual       10                                       
#define RC_GetEqual           11                                       
#define RC_GetGreaterEqual    12                                       
#define RC_GetGreaterThan     13                                       
#define RC_GetLast            14                                       
#define RC_Delete             15                                       
#define RC_TruncateAtName     16
#define RC_TruncateAfterName  17                                       
#define RC_WriteUserData      18                                       
#define RC_ReadUserData       19                                       

/*  Return codes */

#define RC_OKWITHKEY           1
#define RC_KEYREPLACED         1
#define RC_NOKEYREPLACED       2
#define RC_DUPNAME             3        
#define RC_NOTEXIST            4
#define RC_LENGHTNOTEQUAL      6
#define RC_ESBADKEY            7
#define RC_NOTALLOW            8
#define RC_NONODES             9
#define RC_NOPAGES             10
#define RC_LIMIT               11
#define RC_MAXLEN              12

#endif
