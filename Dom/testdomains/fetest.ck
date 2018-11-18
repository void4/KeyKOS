/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*                                                                          
      MODULE  FETEST
   
      test new front end key 

*/

#include "keykos.h"
#include <string.h>
#include "domain.h"
#include "dc.h"
#include "node.h"
#include "sb.h"
#include <stdio.h>


  KEY   K0       = 0;
  KEY   SB       = 1;
  KEY   CALLER   = 2;
  KEY   DOMKEY   = 3;
  KEY   PSB      = 4;
  KEY   K5    = 5;
  KEY   DC       = 6;

  KEY   N1       = 7;
  KEY   N2       = 8;
  KEY   N3       = 9;
  KEY   N4       = 10;
  KEY   N5       = 11;

  KEY   K4       = 12;
  KEY   K3       = 13;
  KEY   K2       = 14;
  KEY   K1       = 15;

      char title [] = "FETEST";

factory() 
{
   UINT32 oc,rc;

   JUMPBUF;

   /* first nibble = 1, true front end key */
   /* first nibble = 0, extended databyte, second byte is slot for key */

   static struct Node_KeyValues FEformat= {15,15,
     {Format1K(1,15,15,14,0,0)}
   };

   static struct Node_KeyValues EXformat= {15,15,
     {Format1K(0,13,15,14,0,0)}
   };

   struct Node_KeyValues nkv;
   char databytes[6];
       
   KC (SB,SB_CreateThreeNodes) KEYSTO(N1,N2,N3);
   KC (SB,SB_CreateNode) KEYSTO(N4);
   KC (SB,SB_CreateNode) KEYSTO(N5);
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);

   KC (N1,Node_Swap+14) KEYSFROM(K0);    /* N1 will be straight fe key */
   KC (N2,Node_Swap+14) KEYSFROM(K0);    /* N2 will be extended databyte */
   KC (N3,Node_Swap+14) KEYSFROM(K0);
   KC (N4,Node_Swap+14) KEYSFROM(K0);
   KC (N5,Node_Swap+14) KEYSFROM(K0);

   KC (N1,Node_WriteData) STRUCTFROM(FEformat);
   KC (N2,Node_WriteData) STRUCTFROM(FEformat);
   KC (N3,Node_WriteData) STRUCTFROM(FEformat);
   KC (N4,Node_WriteData) STRUCTFROM(EXformat);
   KC (N5,Node_WriteData) STRUCTFROM(EXformat);

   KC (N1,Node_MakeFrontendKey) KEYSTO(K1);   /* K1 is an fe */
   KC (N2,Node_MakeFrontendKey) KEYSTO(K2);   /* K2 is an fe */
   KC (N3,Node_MakeFrontendKey) KEYSTO(K3);   /* K3 is an fe */
   KC (N4,Node_MakeFrontendKey) KEYSTO(K4);   /* K4 is an ex */
   KC (N5,Node_MakeFrontendKey) KEYSTO(K5);   /* K5 is an ex */

   nkv.Slots[0].Byte[15]=1;
   nkv.StartSlot=0;
   nkv.EndSlot=0;
   KC (N4,Node_WriteData) STRUCTFROM(nkv);
   nkv.Slots[0].Byte[15]=2;
   KC (N5,Node_WriteData) STRUCTFROM(nkv);

/* Ok K1 is a straight FE key */
/*    K2 is a fe to K1        */
/*    k4 is an ex key  1      */
/*    k5 is an ex key  2      */
/*    K3 is an fe key to K5   */

   KC (N2, Node_Swap+14) KEYSFROM(K1);
   KC (N3, Node_Swap+14) KEYSFROM(K5);

   KC (N4, Node_Swap+14) KEYSFROM(K5);
//   KC (N2, Node_Swap+14) KEYSFROM(K2);  /* deliberate loop */

/* return K1 K2 K4 K3 */
                                       

   LDEXBL (CALLER,0) KEYSFROM(K1,K2,K4,K3);

   for(;;) {
      LDENBL OCTO(oc) KEYSTO(,,K0,CALLER);
      RETJUMP();

      if(oc == KT) {
            KC(DC,DC_IdentifySegment) KEYSFROM(K5) KEYSTO(,K0) RCTO(rc);
            LDEXBL(CALLER,0xFE00+rc) KEYSFROM(K0);
            continue;
      }
      if(oc == KT+4) {
         exit(0);
      }

      KC (K0,KT) RCTO(rc);  /* see what we get */
      if(rc == 3) {  /* a node */
         KC (K0,Node_Fetch+0) KEYSTO(K0);
         KC (K0,0) CHARTO(databytes,6) RCTO(rc);
         LDEXBL(CALLER,4096+databytes[5]);
         continue;
      }
      LDEXBL(CALLER,0);

   }

}

