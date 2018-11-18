/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
    KID Defines
 
    KC (KIDC,KIDC_Create) KEYSFROM(PSB,M,SB) STRUCTFROM(int)
                 KEYSTO(KID) RCTO(rc);
 
    KC (KID,KID_AddEntry) STRUCTFROM(int) KEYSFROM(key) RCTO(rc);
    KC (KID,KID_Identify) KEYSFROM(key) STRUCTTO(int) RCTO(rc);
    KC (KID,KID_DeleteEntry) KEYSFROM(key) RCTO(rc);
    KC (KID,KID_TestInclusion) KEYSFROM(KID2) RCTO(rc);
    KC (KID,KID_PerfromIntersection) KEYSFROM(KID2) RCTO(rc);
    KC (KID,KID_MakeUnion) KEYSFROM(KID2) RCTO(rc);
 
****************************************************************/
#ifndef _H_kid
#define _H_kid
 
#define KIDC_AKT                 0x0127
#define KID_AKT                  0x27

#define KID_Error                1
#define KID_InternalError        3
 
#define KIDC_Create              0
 
#define KID_AddEntry             1
#define KID_Identify             2
#define KID_DeleteEntry          3
#define KID_TestInclusion        4
#define KID_PerformIntersection  5
#define KID_MakeUnion            6
 
#endif
