/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
   auth.h
 
   KC (AUTH,AUTH_Authenticate,(packet);sik,sok,cck)
   KC (AUTH,AUTH_ChangePass,(packet);sik,sok,cck)
 
****************************************************************/
#ifndef _H_auth
#define _H_auth

#define AUTH_AKT              0x64
#define AUTHF_AKT             0x164

#define AUTH_Fail               1
#define AUTH_ConnectFail        2

#define CONNECT_ZAPPER 0
#define CONNECT_FACTORY 1
#define CONNECT_TRANSACTION 2
#define CONNECT_NULL 3
#define CONNECT_FACTORY_NO_PARAM 4
 
#define AUTH_MakeConnection   0
#define AUTH_ChangePass     1

#define AUTHM_ReturnAUTH   1
#define AUTHM_AddDirectoryKey  2
#define AUTHM_GetFirstKey      3
#define AUTHM_GetNextKey       4
#define AUTHM_PutConnection    5
#define AUTHM_PutEncryptionService 6
#define AUTHM_PutPassword 7
 
#endif
