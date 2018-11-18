/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"

  KEY caller = 2;
 
  char title[]="DOMHELP ";

  int  bootwomb = 1;


factory(oc,ord)
    int oc,ord;
{
     unsigned long rc;
     int actlen=0;
     char buf[80];

     JUMPBUF; 

     for (;;) {
         LDEXBL (caller,0) CHARFROM(buf,actlen) ;
         LDENBL CHARTO(buf,80,actlen) KEYSTO(,,,caller) RCTO(rc);
         RETJUMP();
     }   
}

