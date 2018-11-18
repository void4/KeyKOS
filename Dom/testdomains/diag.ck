/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

//#include "kktypes.h"
#include "domain.h"
#include "keykos.h"
#define maxstring 20

KEY  COMPONENTS = 0; /* Factory components node */
KEY  UPSB     = 1; // unprompt bank
KEY  CALLER     = 2;
KEY  DOMKEY   = 3; // From factory
KEY meter = 5;
KEY domcre = 6;
KEY dx = 8;
KEY  Mnode = 9;
KEY  me = 10; // Start key to me
KEY  DKZ = 11; // DK(0) to return to, to become available.
KEY  SYS      = 12;
KEY  K1       = 13;
KEY  K2       = 14;
KEY  K3       = 15;
char title[]="DIAG     ";
typedef unsigned long long uint64;

SINT32 factory(unsigned int fo)
{
   JUMPBUF;
   void sys(char * nm, KEY sn){
      uchar l = strlen(nm);
      {char rnm[l+1];
        int k;
        rnm[0] = l;
        while(l--) rnm[l+1] = nm[l];
        KALL (COMPONENTS, 0) KEYSTO(sn);
        KALL (sn, 11) CHARFROM(rnm, rnm[0]+1) KEYSTO(sn) RCTO(k);
          }}

struct {int pc; int npc; int psr; int tc; uint64 tce;} cntl;
int akt;
   KALL (UPSB, 0) KEYSTO(Mnode);
   KALL (Mnode, 35) CHARFROM("\007", 1) KEYSTO(Mnode);
   if (0) { // Perhaps run in new space
      KALL (DOMKEY, Domain_GetMemory) KEYSTO(SYS);
      KALL (Mnode, 16+0) KEYSFROM(SYS);
      KALL (DOMKEY, Domain_SwapMemory) KEYSFROM(Mnode);}
   sys("joinf", K1);
   KALL (K1, 0) KEYSFROM(UPSB,meter,UPSB) KEYSTO(K1,,,K2);
   KALL (UPSB, 16) KEYSTO(K3);
// Here: K1 is start key to join ob.
// K2 is resume key to join ob.
// K3 is page key to new page.
// Mnode is node key to node that defines our space.
   {int a = 3; int A = a << 28;
   KALL (K3, 0) KEYSTO(SYS); //make page RO
   KALL (Mnode, 16+a) KEYSFROM(SYS); // Page now at A
   {int x = 1?0xC4206000:0x91D0205A; // SPARC store instruction: st %g2, [%g1]
     KALL (K3, 4096) STRUCTFROM(x);}
     // *(int*)A = 0x91D0205A; //ta 0x5a instruction
     KALL(domcre, 0) KEYSFROM(,UPSB) KEYSTO(dx);
     //Install space and starting addr in domain.
     KALL(dx, Domain_ReplaceMemory) KEYSFROM(Mnode) STRUCTFROM(A);
     KALL(dx, Domain_SwapMeter) KEYSFROM(meter);
     KALL(dx, Domain_SwapKeeper) KEYSFROM(K2); // Install return key as keeper.
     {int x[] = {42, A, 47}; // set y, g1 & g2
       KALL(dx, Domain_PutSPARCRegs) STRUCTFROM(x);}}
   KALL(dx, Domain_MakeReturnKey) KEYSTO(SYS);
   KALL (COMPONENTS, 1) KEYSTO(K2); // Get peek key.
   KFORK(SYS, 0);
   if (0) {char rec[68]; KALL (K2, 3) STRUCTTO(rec) RCTO(akt);
      KALL (CALLER, 8) KEYSFROM(dx, SYS, K3) STRUCTFROM(rec) KEYSTO(,,,CALLER);}
   KALL(K1, 1) KEYSTO(,,SYS,K3) OCTO(akt);
   KALL(dx, Domain_GetSPARCControl) STRUCTTO(cntl);
   KALL (CALLER, akt) KEYSFROM(dx, SYS, K3) STRUCTFROM(cntl) KEYSTO(,,,CALLER);
}
