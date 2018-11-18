/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*                                                                          
      MODULE  AVAILTEST
   
      test domain make available release of stall queue

*/

#include "keykos.h"
#include <string.h>
#include "domain.h"
#include "dc.h"
#include "node.h"
#include "sb.h"
#include <stdio.h>
#include "wait.h"


  KEY   COMP       = 0;
#define COMPWAITF  2
  KEY   SB       = 1;
  KEY   CALLER   = 2;
  KEY   DOMKEY   = 3;
  KEY   PSB      = 4;
  KEY   METER    = 5;
  KEY   DC       = 6;

  KEY   BUSYD    = 7;
  KEY   BUSYE    = 8;
  KEY   ME       = 9;

  KEY   WAIT     = 10;

  KEY   K2       = 13;
  KEY   K1       = 14;
  KEY   K0       = 15;

      char title [] = "AVAILTST";

factory() 
{
   UINT32 oc,rc;
   int i;

   unsigned long long waittime = 0x0000000000200000;  /* 2 seconds */
#define MAXDOM 4

   JUMPBUF;
 
   KC (COMP,Node_Fetch+COMPWAITF) KEYSTO(WAIT);
   KC (WAIT,WaitF_Create) KEYSFROM(SB,METER,SB) KEYSTO(WAIT);

   KC (DOMKEY, Domain_MakeStart) KEYSTO(ME);
   LDEXBL (CALLER,0) KEYSFROM(ME);

   for(;;) {
      LDENBL OCTO(oc) KEYSTO(,,,CALLER);
      RETJUMP();

      if(oc == KT+4) {
         KC (WAIT,KT+4) RCTO(rc);
         exit(0);
      }

      if(!fork1()) {   /* domain that will be busy */
          int bcnt;

          KC (COMP,0) KEYSTO(,,CALLER);
          LDENBL OCTO(oc) KEYSTO(,,,K2);
          LDEXBL (ME,0);
          RETJUMP();
          
          bcnt=0;
          while(bcnt < MAXDOM) {
             KC (ME,0) RCTO(rc);   /* get counted */
             LDENBL OCTO(oc) KEYSTO(,,,K2);
             LDEXBL (K2,0);
             if(bcnt == MAXDOM-1) FORKJUMP();
             else RETJUMP();
             
             bcnt++;
          } 
          exit(0);
      }
      KC (DOMKEY, Domain_GetKey+K0) KEYSTO(BUSYD);
      KC (BUSYD, Domain_MakeStart) KEYSTO(BUSYE);
      KC (BUSYD, Domain_MakeBusy) KEYSTO(K2) RCTO(rc);
      LDEXBL (K2,0);
      RETJUMP();

/* Busy domain is now available ready to receive calls */

      KC (BUSYD, Domain_MakeBusy) RCTO(rc);

      for(i=0;i<MAXDOM;i++) {
         if(!fork()) {
             KC (COMP,0) KEYSTO(,,CALLER);
             KC(BUSYE,0) RCTO(rc);   /* get on stall queue */
             exit(0);        
         }
      } 

      KC (WAIT,Wait_SetIntervalAndWait) STRUCTFROM(waittime) RCTO(rc);

      KC (BUSYD, Domain_MakeAvailable) RCTO(rc);

      LDEXBL (COMP,0);              
      for(i=0;i<(MAXDOM+1);i++) {   /* got to count the one going available */
         LDENBL OCTO(oc) KEYSTO(,,,K1);
         if(i == MAXDOM) FORKJUMP();
         else RETJUMP();
         LDEXBL (K1,99);
      }
      
      LDEXBL (CALLER,MAXDOM);
   }

}

