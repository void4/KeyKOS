/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*                                                                          
      MODULE  STACKTEST

      This program is designed to test the register window management code
      in the sparc implementation

      It has only  1 method  (oc = 0) which runs the exercise and reports
      sucess or failure.

      KT = 11111
*/

#include "keykos.h"
#include <string.h>
#include "domain.h"
#include "node.h"
#include "sb.h"
#include <stdio.h>


  KEY   COMP     = 0;
  KEY   SB       = 1;
  KEY   CALLER   = 2;
  KEY   DOMKEY   = 3;
  KEY   PSB      = 4;
  KEY   METER    = 5;
  KEY   DC       = 6;

  KEY   K2       = 13;
  KEY   K1       = 14;
  KEY   K0       = 15;

      char title [] = "STACKT";

  int push(int);

  JUMPBUF;

factory() {
   UINT32 oc,rc;

   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   LDEXBL (CALLER,0) KEYSFROM(K0);

   for(;;) {
      LDENBL OCTO(oc) KEYSTO(,,,CALLER);
      RETJUMP();

      if(oc == KT) {
         LDEXBL(CALLER,0x11111);
         continue;
      }
      if(oc == KT+4) {
         exit(0);
      }

      rc = push(16);

      LDEXBL(CALLER,rc);
   }

}
int push(int level)
{
    int t[1000];
    int i;
    int rc;

    if(!level) {
         KC (COMP,3) KEYSTO(K1);
         return 0;
    }

    for(i=0;i<1000;i++) {
       t[i]=i*level;
    }

    rc=push(level-1);

    for(i=0;i<1000;i++) {
       if(t[i] != i*level) return 1;
    }

    return rc;
}

