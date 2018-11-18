/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
    Discrim Defines
 
    KC (Discrim,Discrim_Type) KEYSFROM(key) RCTO(rc);
    KC (Discrim,Discrim_Discreet) KEYSFROM(key) RCTO(rc);
    KC (Discrim,Discrim_Compare) KEYSFROM(key1,key2) RCTO(rc);
 
****************************************************************/

#ifndef _H_discrim
#define _H_discrim
 
#define Discrim_Type       0
 
#define Discrim_TypeData   1
#define Discrim_TypeResume 2
#define Discrim_TypeMemory 3
#define Discrim_TypeMeter  4
#define Discrim_TypeOther  5
 
#define Discrim_Discreet   1
#define Discrim_Compare    2
 
#endif
