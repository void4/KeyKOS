/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
  
  KEY CALLER = 2;

  KEY HELP = 15;
  KEY CONSOLE = 14;

  KEY CCK   =  8;
  KEY SOK   =  9;
  KEY SIK   = 10;

   char title[]="DOMTEST ";

   int bootwomb = 1;

factory(oc,ord)
    int oc,ord;
{
    unsigned long rc;
    int i;
    char buf[80];
    JUMPBUF;

    KC (CONSOLE,0) KEYSTO(SIK,SOK,CCK);
    KC (SOK,0) KEYSTO(,,,SOK) RCTO(rc);

    strcpy(buf,"KeyTECH SPARC Jump timing test.. CR to begin\r\n");
    KC (SOK,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,SOK) RCTO(rc);
 
    KC (SIK,8192+80) CHARTO(buf,80) KEYSTO(,,,SIK) RCTO(rc);

    for(i=0;i<100000;i++) {
      KC (HELP,0) RCTO(rc);
    }

    strcpy(buf,"End test of 200,000 jumps (100,000 call/return)\r\n");
    KC (SOK,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,SOK) RCTO(rc);
    
    LDEXBL (CALLER,0);
    RETJUMP();
}
    

