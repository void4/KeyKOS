/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
/*
   This is a SIK/SOK simulator for the UART device on the LUNA88K
*/

 
#include "kktypes.h"         /* KeyKOS data types                     */
#include "keykos.h"
#include "domain.h"
#include "sb.h"
#include "node.h"
#include "kuart.h"

KEY comp      = 0;
KEY sb        = 1;
KEY caller    = 2;
KEY domkey    = 3;
KEY psb       = 4;
KEY m         = 5;
KEY domcre    = 6;

KEY uart      = 7;
KEY sikdom    = 8;
KEY sokdom    = 9;
KEY smallsb   = 10;
KEY cck       = 11;

KEY k1        = 14;
KEY k0        = 15; /* scratch */
 
#define COMPUART 1

       char title[]="SIK2SIM";
       int stacksiz=4096;

#define AKT 0x60e

       unsigned long reinit();

factory(factoc,factord)
   int factoc,factord;
{
   JUMPBUF;

   uint32 oc,rc;
   
   KC (comp,COMPUART) KEYSTO(uart);
   KC (sb,SB_CreateBank) KEYSTO(smallsb);
   KC (domkey,Domain_MakeStart) KEYSTO(cck);
   LDEXBL (caller,0) KEYSFROM(cck);
   for(;;) {
     LDENBL OCTO(oc) KEYSTO(,,,caller);
     RETJUMP();      /* the CCK domain */

     if(oc==KT) {
        LDEXBL (caller,AKT);
        continue;
     }
     if(oc==0) { /* make new keys */

        while(KT+3==reinit());
/*        KC (uart,UART_WriteData) CHARFROM("UART Wakeup\r\n",13) RCTO(rc); */

        KC(smallsb,SB_DestroyBankAndSpace) RCTO(rc);
        KC(sb,SB_CreateBank) KEYSTO(smallsb);
        KC(domkey,Domain_SwapKey+sb) KEYSFROM(smallsb) KEYSTO(smallsb);  /* use smallsb */
        if(!fork()) {   /* SIK domain */
          dosik();
          exit(0);
        }
        LDEXBL(comp,0);
        LDENBL OCTO(oc) KEYSTO(sikdom,,,k0);
	RETJUMP();  /* wait for return with domain key */
        LDEXBL (k0,0);
        FORKJUMP();   /* allow SIK domain to get ready */

        if(!fork()) {   /* SOK domain */
           dosok();
           exit(0);
        }
        LDEXBL(comp,0);
        LDENBL OCTO(oc) KEYSTO(sokdom,,,k0);
	RETJUMP();  /* wait for return with domain key */
        LDEXBL (k0,0);
        FORKJUMP();   /* allow SOK domain to get ready */

        KC(domkey,Domain_SwapKey+sb) KEYSFROM(smallsb) KEYSTO(smallsb); /* restore sb */

        KC(sikdom,Domain_MakeStart) KEYSTO(k0);
        KC(sokdom,Domain_MakeStart) KEYSTO(k1);
        LDEXBL (caller,0) KEYSFROM(k0,k1,cck);
     }
     else {
        LDEXBL (caller,0);  /* what ever you want to do */
     }   
   } 
}
dosik()
{
   JUMPBUF;
   uint32 oc,rc;
   char buf[256];
   int len,type;
   char *ptr;
   int accum;

   KC(cck,0) KEYSFROM(domkey);
   KC(domkey,Domain_MakeStart) KEYSTO(k0);
   LDEXBL(comp,0);
   for(;;) {
      LDENBL OCTO(oc) KEYSTO(,,,caller);
      RETJUMP();
      len=oc & 0xFFF;
      type=oc >> 12;   /* 0 means no echo, no BS, activate on cr  */ 
                       /* 1 means no echo, no bs, activate each character */
                       /* 2 means echo, bs, activate on CR */
      ptr=buf; 
      accum=0;
      while(len) {
sikre:
         KC(uart,UART_WaitandReadData+1) CHARTO(ptr,1) RCTO(rc);
         if(KT+3==rc) {while(KT+3==reinit());goto sikre;}  
         *ptr=*ptr & 0x7F;
         switch(type) {
           case 1:   /* raw */
              len=0;
              accum=1;
              ptr++;
              break;
           case 2:   /* echo, bs, activate on CR */
              if(*ptr==8 && accum){ /* backspace */
                KC(uart,UART_WriteData) CHARFROM(ptr,1) RCTO(rc);
                KC(uart,UART_WriteData) CHARFROM(" ",1) RCTO(rc);
                KC(uart,UART_WriteData) CHARFROM(ptr,1) RCTO(rc);
                ptr--;
                accum--;
                len++;
                break;
              } 
              KC(uart,UART_WriteData) CHARFROM(ptr,1) RCTO(rc);
              if(*ptr==13) KC(uart,UART_WriteData) CHARFROM("\n",1) RCTO(rc);
              ptr++;
              accum++;
              len--;
              if(*(ptr-1)==13) len=0;  /* activate on CR */ 
              break;
           case 0:  /* no echo, no bs, activate on CR */
              accum++;
              ptr++;
              len--;
              if(*(ptr-1)==13) len=0; /* activate on CR */
              break;
         }
         if(accum >= 256) len=0;  /* force activation */
      }
      *ptr=0;
      LDEXBL (caller,0) CHARFROM(buf,accum) KEYSFROM(,,,k0);
   }
}

dosok()
{
   JUMPBUF;
   uint32 oc,rc;
   char buf[256];
   int len;
   char *ptr;

   KC(cck,0) KEYSFROM(domkey);
   KC(domkey,Domain_MakeStart) KEYSTO(k0);
   LDEXBL(comp,0);
   for(;;) {
     LDENBL OCTO(oc) CHARTO(buf,256,len) KEYSTO(,,,caller);
     RETJUMP();
     ptr=buf;
     while(len) {
sokrestart:
        KC(uart,UART_WriteData) CHARFROM(ptr,1) RCTO(rc);
        if(0x0A==*ptr && !oc) KC(uart,UART_WriteData) CHARFROM("\r",1) RCTO(rc);
        if(KT+3 == rc) {reinit();goto sokrestart;}
        ptr++;
        len--;
     }
     LDEXBL (caller,256) KEYSFROM(,,,k0);
   } 
}
unsigned long reinit()
{
   JUMPBUF;
   uint32 rc;
   
   KC(uart,UART_MakeCurrentKey) KEYSTO(uart);
   KC(uart,UART_EnableInput) RCTO(rc);

   return rc;
}
