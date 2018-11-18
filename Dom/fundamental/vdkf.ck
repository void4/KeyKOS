/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports the VDKF  Virtual Domain Creator

  This keeper will grow into a GDB container but initially
  will only dump information and exit leaving the domain
  trapped.

*************************************************************/

/*************************************************************
  LINKING STYLE: 1 RO Section - NO RW Global storage
*************************************************************/

#include "keykos.h"
#include "kktypes.h"
#include "domain.h"
#include "node.h"
#include "page.h"
#include "sb.h"
#include "vdk.h"
#include "switcherf.h"
 
   KEY COMP        = 0;
   KEY SB          = 1;    /* Space bank parameter */
   KEY CALLER      = 2;
   KEY DOMKEY      = 3;
   KEY PSB         = 4;
   KEY METER       = 5;
   KEY DC          = 6;

   KEY HISDOM      = 7;
   KEY CCK         = 8;
   KEY SOK         = 9;
 
   KEY K2          = 13;
   KEY K1          = 14;
   KEY K0          = 15;
 
#define COMPCONSOLE  15
 
    char title[]="VDKF    ";

    void outsok(char *);

UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc;
   struct Domain_SPARCRegistersAndControl drac;
   UINT32 type;   /* VDKF_CreateSilent, VDKF_CreateSwitcher, VDKF_CreateCCK */
 
   if (factoc != KT+5) exit(KT+2);
   KC (CALLER,KT+5) KEYSTO(CCK,,,CALLER) RCTO(type);

   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);

   LDEXBL (CALLER,0) KEYSFROM(K0);
   for (;;) {
     LDENBL OCTO(oc) KEYSTO(,,HISDOM,CALLER) STRUCTTO(drac);
     RETJUMP();
     if (oc == KT) {
        LDEXBL (CALLER,VDK_AKT);
        continue;
     }
     if (oc < KT) {
        if(oc == 4) break;   // destruction
        LDEXBL (CALLER,KT+2);
        continue;
     }

     if ( (oc & 0xFFFFFF00) == 0x80000400) {  /* bad meter, should not happen */
                                               /* but does because kernel violates */
                                               /* a principle */
        KC (COMP,Node_Fetch+0) KEYSTO(,,CALLER);
        LDEXBL (CALLER,0);                     /* leave domain alone */   
        continue;
     }
/* Trap here */  
     if(!fork()) {  /* the DK */
        char buf[256];
        int nwindows;
        int i;
        SINT32 actlen;
        struct Domain_SPARCOldWindow bw[32];  /* enough for 32 old windows */

        if(type == VDKF_CreateCCK) {  /* CCK is a CCK Key */
           KC (CCK,0) KEYSTO(,SOK) RCTO(rc);  /* forget SIK for now */
        }
        else if(type == VDKF_CreateSwitcher) {
           KC (CCK,Switcher_CreateBranch) KEYSTO(,SOK) RCTO(rc);  /* sik,sok,cck */
        } 
 
        sprintf(buf,"-> FAULT: PC=%08x NPC=%08x, TC=%08x, TCE=%08x %08x\n\n",
            drac.Control.PC,drac.Control.NPC,drac.Control.TRAPCODE,
            drac.Control.TRAPEXT[0],drac.Control.TRAPEXT[1]);
        outsok(buf);
        sprintf(buf,"   REGS:G %08x %08x %08x %08x %08x %08x %08x %08x\n",
            drac.Regs.g[0],drac.Regs.g[1],drac.Regs.g[2],drac.Regs.g[3],
            drac.Regs.g[4],drac.Regs.g[5],drac.Regs.g[6],drac.Regs.g[7]);
        outsok(buf);
        sprintf(buf,"   REGS:I %08x %08x %08x %08x %08x %08x %08x %08x\n",
            drac.Regs.i[0],drac.Regs.i[1],drac.Regs.i[2],drac.Regs.i[3],
            drac.Regs.i[4],drac.Regs.i[5],drac.Regs.i[6],drac.Regs.i[7]);
        outsok(buf);
        sprintf(buf,"   REGS:L %08x %08x %08x %08x %08x %08x %08x %08x\n",
            drac.Regs.l[0],drac.Regs.l[1],drac.Regs.l[2],drac.Regs.l[3],
            drac.Regs.l[4],drac.Regs.l[5],drac.Regs.l[6],drac.Regs.l[7]);
        outsok(buf);
        sprintf(buf,"   REGS:O %08x %08x %08x %08x %08x %08x %08x %08x\n",
            drac.Regs.o[0],drac.Regs.o[1],drac.Regs.o[2],drac.Regs.o[3],
            drac.Regs.o[4],drac.Regs.o[5],drac.Regs.o[6],drac.Regs.o[7]);
        outsok(buf);

        KC (HISDOM,Domain_GetSPARCOldWindows) 
              CHARTO(bw,32*sizeof(struct Domain_SPARCOldWindow),actlen) RCTO(rc);
        outsok("\nBackWindows\n");
        if(!rc) {
           nwindows=actlen/sizeof(struct Domain_SPARCOldWindow);
           for(i=0;i<nwindows;i++) {
                sprintf(buf,"   REGS:I %08x %08x %08x %08x %08x %08x %08x %08x\n",
                    bw[i].i[0],bw[i].i[1],bw[i].i[2],bw[i].i[3],
                    bw[i].i[4],bw[i].i[5],bw[i].i[6],bw[i].i[7]);
                outsok(buf);
                sprintf(buf,"   REGS:L %08x %08x %08x %08x %08x %08x %08x %08x\n\n",
                    bw[i].l[0],bw[i].l[1],bw[i].l[2],bw[i].l[3],
                    bw[i].l[4],bw[i].l[5],bw[i].l[6],bw[i].l[7]);
                outsok(buf);
           } 
        }
        KC (COMP,0) KEYSTO(,,CALLER);  /* don't restart domain */
        exit(0);    /* leave domain trapped */
     }
     
     KC (COMP,0) KEYSTO(,,CALLER);  /* let DK have the fault return key */
 
     LDEXBL (CALLER,0);
   }
}

void outsok(char *str) {
     JUMPBUF;
     UINT32 rc;

     KC (SOK,0) CHARFROM(str,strlen(str)) KEYSTO(,,,SOK) RCTO(rc);
}
