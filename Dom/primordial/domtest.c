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

    {OC(0);XB(0x00E00000);NB(0x00E0A980);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);XB(0x00900000);RC(rc);NB(0x08100009);cjcc(0x00000000,&_jumpbuf); }

    strcpy(buf,"KeyTECH SPARC Jump timing test.. CR to begin\r\n");
    {OC(0);PS2(buf,strlen(buf));XB(0x04900000);RC(rc);NB(0x08100009);cjcc(0x08000000,&_jumpbuf); }

    {OC(8192+80);XB(0x00A00000);RC(rc);RS2(buf,80);NB(0x0B10000A);cjcc(0x00080000,&_jumpbuf); }

    for(i=0;i<100000;i++) {
      {OC(0);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
    }

    strcpy(buf,"End test of 200,000 jumps (100,000 call/return)\r\n");
    {OC(0);PS2(buf,strlen(buf));XB(0x04900000);RC(rc);NB(0x08100009);cjcc(0x08000000,&_jumpbuf); }

    {OC(0);XB(0x00200000); }
    {rj(0x00000000,&_jumpbuf); }
}


