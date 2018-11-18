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
         {OC(0);PS2(buf,actlen);XB(0x04200000); }
         {RC(rc);RS3(buf,80,actlen);NB(0x0B100002); }
         {rj(0x08100000,&_jumpbuf); }
     }
}

