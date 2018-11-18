/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* WKC - Builds and services the Womb */

#include "wombdefs.h"
#include "keykos.h"
#include "kktypes.h"
#include <cvt.h>
#include <string.h>
#include "domain.h"
#include "node.h"
#include "dc.h"
#include "sb.h"
#include "consdefs.h"
#include "page.h"
#include "rootnode.h"

JUMPBUF;

#define SNode_Fetch 41
#define SNode_Swap 42
#define int2b long2b
 
extern void crash();
 
/* KEY SLOTS: */
KEY ROOTNODE  = 0;   /* from ckitems */
KEY SB        = 1;   /* from ckitems */
KEY M         = 2;   /* from ckitems */
KEY AUXLIST   = 3;   /* from ckitems */
KEY D         = 4;
KEY DC        = 5; 
KEY K0        = 6;
KEY K1        = 7;
KEY K2        = 8;
KEY K3        = 9;

KEY CONSOLE   = 10;

KEY WOMBMEMROOT = 12;
KEY EARLYNODE = 13;  /* from ckitems */
KEY FIRSTOBJ  = 14;  /* from ckitems */
KEY FIRSTOBJB = 15;  /* from ckitems */


/* Define's to shorten external names for the loader. */
#define instcomps instcoms
#define compfacts compfacs
/* WKC - Builds and services the Womb */
 
#define TRUE 1
#define FALSE 0
 
#define max(a,b) (a<b ? b : a)
 
/* external functions */
 
extern void UnixInstall();
 
char charstr[128]; /* for building parameter strings */
static unsigned long rc; /* for return codes */

/* Get the name from the AUX node */
char auxname[8];    /* saves the name here */
void GetAuxName()
{
  char datakey[16];
 KALL(AUXLIST, Node_Fetch+6) KEYSTO (K0) RCTO(rc);
 if (rc == 0) {
    KALL(K0, 1) RCTO(rc) CHARTO(datakey,16);
           /* Get first 11 characters of system name */
    strcpy(auxname,&datakey[5]);  /* get 11 bytes from 16 byte datakey */
 }
 else /* end of list, AUXLIST is DK(0) */
    memset(auxname,0,8);
}
 
/* Make a new Space Bank */
#define NEWBANK(slot) newbanksubr(slot,0x7FFFFFFF,0x7FFFFFFF)
/* Leaves a new bank in slot SB and also in slot n of node
   EARLYBANKSNODE, which is accessible via node EARLYNODE. */

void newbanksubr(slot, nodes, pages)
int slot;  /* slot number in EARLYBANKSNODE */
unsigned long nodes, pages;
{
   struct {unsigned long nodes,pages;} parms;
   parms.nodes = nodes;
   parms.pages = pages;

 KALL(EARLYNODE, Node_Fetch+EARLYPRIMESB) KEYSTO(SB);  /* PRIME BANK */
 KALL(EARLYNODE, Node_Fetch+EARLYSBT) KEYSTO(K0);

 KALL(K0, 5) CHARFROM(&parms, 8) KEYSFROM (SB) KEYSTO (SB);
      /* GET SUB BANK. */
 KALL(EARLYNODE, Node_Fetch+EARLYBANKS) KEYSTO (K0);
 KALL(K0, Node_Swap+slot) KEYSFROM (SB);
} /* end of newbanksubr */
 
/* MAKE A NEW METER. */
/* LEAVES NODE KEY IN K0, METER KEY IN K3. */
/* LEAVES NODE KEY IN EARLYMETERS(slot) from EARLYNODE */
void makemeter(slot)
{
   static struct {unsigned long first,last;
        unsigned char value[3][16];} meterdata = {3, 5,
        {{0,0,0,0,0,0,0,0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
         {0,0,0,0,0,0,0,0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
         {0,0,0,0,0,0,0,0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} } };
 KALL(SB, SB_CreateNode) KEYSTO (K0);
 KALL(K0, Node_WriteData) CHARFROM(&meterdata,sizeof(meterdata));
 KALL(EARLYNODE, Node_Fetch+EARLYMETERS) KEYSTO(K3);
 KALL(K3,Node_Fetch+METERSSYSTEM) KEYSTO(K3);
 KALL(K3,Node_MakeMeterKey) KEYSTO(K3);
 KALL(K0, Node_Swap+1) KEYSFROM (K3);
 KALL(EARLYNODE, Node_Fetch+EARLYMETERS) KEYSTO(K3);
 KALL(K3, Node_Swap+slot) KEYSFROM(K0);
 KALL(K0, Node_MakeMeterKey) KEYSTO (K3);
}
 

void logName(name)
char *name;
{
   unsigned long rc;

   KALL (EARLYNODE, Node_Fetch+EARLYCONSOLE) KEYSTO(K0);
   KALL (K0,0) KEYSTO(,K0) RCTO(rc);
   KALL (K0,0) CHARFROM(name,strlen(name)) KEYSTO(,,,K0) RCTO(rc);
   KALL (K0,0) CHARFROM("\n",1) RCTO(rc);
}
 
/* objectIs: Check if the current object is what we think. */
/* Name of current object must be in auxname. */
/* Only checks the first 8 characters of the name. */
/* Returns 1 (TRUE) if names match, 0 (FALSE) if not. */

int objectIs(name)
char *name; /* the name, null-terminated. */
{ int namelen = strlen(name);
 if (namelen > 8) namelen = 8;
 logName(name);
 return (memcmp(auxname, name, namelen) == 0);
}

/* aux2subr
 
   input is AUXLIST.
   Builds a domain from info in the AUXLIST node.
   Returns the return code from calling the domain,
     keys in K0, K1. */
unsigned long aux2subr(newdc,lsfsim)
   int newdc;  /* if non-zero use new dc and give to domain */
   int lsfsim; /* if non-zero use LSFSIM logic */
               /*  lsfsim is stack size */
{ 
   struct Domain_SPARCControlData psw=
    {0x000000AC,0x000000B0,0x00000080,0,0,0,0,0,0}; 
   unsigned long rc; /* for return codes */
   char data[6];
   struct Domain_SPARCRegData regs=  {
      {0,0,0,0,0,0,0,0},  /* g */
      {0,0, (0x08F00000 +(WOMB_FACIL<<12) + (WOMB_MEM<<8)
      + (WOMB_DOMKEY<<4) + WOMB_CALLER),    /* entry block */
             0,0,0,0,0},  /* o */
      {0,0,0,0,0,0,0,0},  /* l */
      {0,0,0,0,0,0,0,0}   /* i */
    };
    
 if (!newdc)
   KALL(DC, DC_CreateDomain) KEYSFROM (SB) KEYSTO (D);
 else {
   KALL(EARLYNODE, Node_Fetch+EARLYDCC) KEYSTO(K1);
   KALL(K1, 0) KEYSFROM (SB, K3) KEYSTO (K1); /* MAKE private DC */
   KALL(K1, DC_CreateDomain) KEYSFROM (SB) KEYSTO(D);
   KALL(D, Domain_SwapKey+6) KEYSFROM(K1); /* give to domain */
 }
 KALL(AUXLIST, Node_Swap+14) KEYSFROM (D);
    /* SAVE DOMAIN KEY IN AUX NODE */
 /* makemeter(); */
 KALL(AUXLIST, Node_Swap+13) KEYSFROM (K0);
    /* SAVE NODE KEY TO METER */
 KALL(D, Domain_Swap+1) KEYSFROM (K3); /* INSTALL NEW METER */
 KALL(ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K1);
 KALL(K1, Node_Fetch+KERNELERROR) KEYSTO(K1);
 KALL(D, Domain_Swap+2) KEYSFROM (K1);
 KALL (SB,SB_CreateNode) KEYSTO(K1);              /* node for lsfsimulator */
 KALL (EARLYNODE, Node_Fetch+EARLYLSFSIMCODE) KEYSTO(K0);
 KALL (K1,Node_Swap+0) KEYSFROM(K0);
 KALL (AUXLIST, Node_Fetch+0) KEYSTO(K0);
 KALL (K1,Node_Swap+1) KEYSFROM(K0);
 KALL (K1,Node_Swap+2) KEYSFROM(K0);
 KALL (ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K0);
 KALL (K0, Node_Fetch+KERNELDKC) KEYSTO(K0);
 data[0]=0;    /*......   AUXLIST(5) will have the stack size */
 data[1]=0;
 memcpy(&data[2],&lsfsim,4);
 KALL (K0,0) CHARFROM(data,6) KEYSTO(K0);
 KALL (K1,Node_Swap+4) KEYSFROM(K0);
 KALL (AUXLIST,Node_Fetch+2) KEYSTO(K0);
 KALL (K0,0) CHARTO(&psw.PC,4) RCTO(rc);
 psw.NPC=psw.PC+4;
 KALL (D, Domain_SwapKey+7) KEYSFROM(K1); /*node for code to find*/
 KALL(AUXLIST, Node_Fetch+3) KEYSTO (K0); /* GET KEY4 */
 KALL(D, Domain_SwapKey+8) KEYSFROM (K0); /* KEY8 in lsfsim  */
 KALL(AUXLIST, Node_Fetch+4) KEYSTO (K0); /* GET KEY5 */
 KALL(D, Domain_SwapKey+9) KEYSFROM (K0); /* KEY9 in lsfsim  */
 KALL(D, Domain_SwapKey+4) KEYSFROM (SB); /* give spacebank */
 KALL(D, Domain_SwapKey+5) KEYSFROM (K3); /* give meter */
 KALL (K1, Node_Fetch+0) KEYSTO(K1);   /* code to run */
 KALL(D, Domain_PutSPARCControl) STRUCTFROM(psw); /* WRITE  CONTROL INFO */
 KALL(D, Domain_Swap+3) KEYSFROM (K1);
 KALL(AUXLIST, Node_Fetch+1) KEYSTO (K0); /* GET SYMS */
 KALL(D, Domain_Swap+10) KEYSFROM (K0);
 KALL(D, Domain_PutSPARCRegs)  STRUCTFROM(regs); /* put regs */
 KALL(D, Domain_MakeStart) KEYSTO (K0);
 KALL(EARLYNODE, Node_Fetch+EARLYWOMBFACIL) KEYSTO(K2);
 KALL(K2, Node_MakeFetchKey) KEYSTO (K2);
 KALL(K0, 0) KEYSFROM (K2, K1, D) RCTO(rc) KEYSTO (K0, K1);
 return(rc);
}
 
void instcomp(node,slot,installOC)
/* Installs a component in a factory. */
KEY node;  /* node containing the component */
int slot;  /* slot in the node which contains the component */
int installOC; /* builder's key order code to install the component */
/* K1 has the builder's key in which to install the component. */
/* K0 is used as a scratch slot. */
{
 KALL(node, Node_Fetch+slot) KEYSTO (K0);
 KALL(K1, installOC) KEYSFROM (K0);
}
 
void StepAuxlistAndCompleteFactory()
/* Gets the next AUXWOMB defining node into AUXLIST
   and completes the factory whose builder's key is in K1,
   returning the requestor's key in K0 ,builder key in K1*/
{
 KALL(AUXLIST, Node_Fetch+15) KEYSTO (AUXLIST); /* NEXT IN LIST */
 GetAuxName();    /* Get name of next object */
 KALL(K1, 66) KEYSTO (K0); /* COMPLETE FACTORY */
}
 
void compfs(swindex)
/* Completes a factory and adds the information node to SWOMBLIST. */
int swindex; /* the supernode slot in SWOMBLIST */
/* Returns a requestor's key to the factory in K1 */
{
 StepAuxlistAndCompleteFactory();
}
 
/*  MAKE A FACTORY */
/* Output: K1 is builder's key to a new factory.  The requestor's KT */
/*         will be akt.  A keeper from node MORE(DOMKEEP), a */
/*         program from AUXLIST, and symbols from AUXLIST have been */
/*         installed. */
/*         The builder's key has been saved the AUXLIST node. */

void makefact(akt,lsfsim)
unsigned long akt;  /* the alleged key type of the requestor's key */
int lsfsim; /* non-zero means use LSFSIM technology for mem tree */
            /*  the non-zero value is the stack size for the object */
{
   unsigned char ronc;
   unsigned int lsfakt=0x10F0D;
   char data[6];
   int lsfstart=-1;  /* signal to use LSFSIMULATOR */
 
 KALL(FIRSTOBJ, Node_Fetch+FIRSTFC) KEYSTO(K0);
 KALL(EARLYNODE, Node_Fetch+EARLYMETERS) KEYSTO(K3);
 KALL(K3,Node_Fetch+METERSFACT) KEYSTO(K3);

 KALL(K0, 0) KEYSFROM (SB, K3) KEYSTO (K1); /* CREATE A NEW FACTORY */
 KALL(K1, 64) CHARFROM(&akt,4);

 KALL(ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K0);
 KALL(K0, Node_Fetch+KERNELERROR) KEYSTO(K0);

 KALL(K1, 128+16) KEYSFROM (K0); /* INSTALL .KEEPER */
 KALL(EARLYNODE, Node_Fetch+EARLYLSFSIMCODE) KEYSTO(K0);

 KALL(FIRSTOBJ, Node_Fetch+FIRSTFC) KEYSTO(K2);
 KALL(K2, 0) KEYSFROM (SB, K3) KEYSTO (K2); /* CREATE fetcher */

 KALL(K2,64) CHARFROM(&lsfakt,4);
 KALL(K2,0) KEYSFROM(K0);  /* sense key to build segment */
 KALL(AUXLIST, Node_Fetch+0) KEYSTO(K0);  /* should be RO Segment key */
 KALL(K2,1) KEYSFROM(K0);
 KALL(K2,2) KEYSFROM(K0);
 KALL (ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K0);
 KALL (K0, Node_Fetch+KERNELDKC) KEYSTO(K0);
 data[0]=0;
 data[1]=0;
 memcpy(&data[2],&lsfsim,4);  /* stack size */
 KALL(K0,0) CHARFROM(data,6) KEYSTO(K0);
 KALL(K2,4) KEYSFROM(K0);      /* to lsf component */
 KALL(K2,66) KEYSTO(K2);  /* fetcher factory (LSF) */

 KALL(K1,17+32) CHARFROM(&lsfstart,4) KEYSFROM(K2);
 KALL(AUXLIST, Node_Fetch+1) KEYSTO (K0); /* SYMBOL SEGMENT */
 KALL(K1, 18) KEYSFROM (K0);
}
 
    int bootwomb=1;  /* ignore most of cfstart */
    int stacksiz=4096;
    char title[]="WOMBKEEP";

factory()
{
 
/* INITIALIZE AUXSUBR */
 
 NEWBANK(BANKSAUX);  
 makemeter(METERSAUX);

 KALL(EARLYNODE, Node_Fetch+EARLYDCC) KEYSTO(DC);
 KALL(DC, 0) KEYSFROM (SB, K3) KEYSTO (DC); /* MAKE A DC FOR AUXSUBR */
 
/* INSTALL KIDC */
 if (aux2subr(1,4096) != 1) crash(); /* two keys */
 KALL(FIRSTOBJ, Node_Swap+FIRSTKIDC) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTKIDCDC) KEYSFROM(DC); 
 KALL(EARLYNODE, Node_Fetch+EARLYPRIVNODE) KEYSTO(D);
 KALL(D,Node_Swap+6) KEYSFROM(K0);   /* KIDC to PRIV node for FCC TEMPTEMPTEMP */


 KALL(AUXLIST, Node_Fetch+15) KEYSTO(AUXLIST);
 
/* INSTALL THE FACTORY */
 if (aux2subr(0,4096) != 0) crash(); /* SHOULD RETURN ONE KEY */
 KALL(FIRSTOBJ, Node_Swap+FIRSTFCC) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTFCCDC) KEYSFROM(DC);

    /* INSTALL FCC */

 NEWBANK(BANKSFACT);
 makemeter(METERSFACT);  /* need a place to stash these */

 KALL(FIRSTOBJ, Node_Fetch+FIRSTFCC) KEYSTO (K0);     /* Get FCC back. */
 KALL(K0, 0) KEYSFROM (SB, K3) KEYSTO (K0); /* Make standard FC */
 KALL(FIRSTOBJ, Node_Swap+FIRSTFC) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTFCDC) KEYSFROM(DC);

 KALL(K0, 4) KEYSTO (K0);     /* Rescind recall rights */
 KALL(FIRSTOBJ, Node_Swap+FIRSTFCNORECALL) KEYSFROM(K0);

 KALL(AUXLIST, Node_Fetch+15) KEYSTO (AUXLIST); /* FOLLOW LIST */
 GetAuxName();

/* Get Bank and Meter for Most system objects */

 NEWBANK(BANKSINIT);

/* MAKE FS FACTORY */
 makefact(0x40D,4096);
 KC (ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K2);
 instcomp(K2,KERNELRETURNER,0); /* RETURNER IS COMPONENT 0 */
 instcomp(K2,KERNELDKC,1); /* DATA KEY CREATOR AS COMPONENT 1 */
 StepAuxlistAndCompleteFactory();
 KALL(FIRSTOBJ, Node_Swap+FIRSTFSF) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTFSFBUILD) KEYSFROM(K1);
 
/* MAKE SNODE FACTORY */
 makefact(0x20D,4096);
 KC (ROOTNODE, Node_Fetch+ROOTKERNELKEYS) KEYSTO(K2);
 instcomp(K2,KERNELRETURNER,0); /* RETURNER IS COMPONENT 0 */
 StepAuxlistAndCompleteFactory();
 KALL(FIRSTOBJ, Node_Swap+FIRSTSNODEF) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTSNODEFBUILD) KEYSFROM(K1);
 
/* RCF - RECORD COLLECTION FACTORY */
 makefact(0x0F,8192);
 instcomp(FIRSTOBJ,FIRSTFSF,32+0);
 instcomp(FIRSTOBJ,FIRSTSNODEF,32+1);
 StepAuxlistAndCompleteFactory();
 KALL(FIRSTOBJ, Node_Swap+FIRSTTDOF) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTTDOFBUILD) KEYSFROM(K1);

/* PCS - PCSF with no keeper (MUST BE REPLACED) */
 makefact(0x23,4096);  
 instcomp(FIRSTOBJ,FIRSTTDOF,32+2);
 instcomp(FIRSTOBJ,FIRSTFSF,32+6);
 StepAuxlistAndCompleteFactory();
 KALL(FIRSTOBJ, Node_Swap+FIRSTPCSF) KEYSFROM(K0);
 KALL(FIRSTOBJB, Node_Swap+FIRSTPCSFBUILD) KEYSFROM(K1);
 
/* might want to build a zerosegment here for component 1 using K0 and D */
 {
     int lss,i;
     char ndb;

     KALL(SB,SB_CreatePage) KEYSTO(D);
     KALL(D,Page_MakeReadOnlyKey) KEYSTO(D);
     for(lss=3;lss<12;lss++) {
       KALL(SB,SB_CreateNode) KEYSTO(K0);
       ndb=lss;
       KALL(K0,Node_MakeNodeKey) CHARFROM(&ndb,1) KEYSTO(K0);
       for(i=0;i<16;i++) {
           KALL(K0,Node_Swap+i) KEYSFROM(D);
       }
       KALL(K0,Node_MakeSenseKey) KEYSTO(D);
     }
 }
 KALL(FIRSTOBJ, Node_Swap+FIRSTVIRTUALZERO) KEYSFROM(D);

 makemeter(METERSINIT);

/* NOW START INIT COMMAND FILE */

 KALL(FIRSTOBJ, Node_Fetch+FIRSTFSF) KEYSTO(K2);
 KALL(K2, 0) KEYSFROM(SB,K3,SB) KEYSTO(K2);       /* OUTSEG */
 KALL(FIRSTOBJ, Node_Swap+FIRSTOUTSEG) KEYSFROM(K2);
 KALL(ROOTNODE, Node_Fetch+ROOTTARSEGNODE) KEYSTO(K1);
 KALL(K1, Node_Fetch+TARINIT) KEYSTO(K1);
 KALL(EARLYNODE, Node_Fetch+EARLYCONSOLE) KEYSTO(D);

 KALL(FIRSTOBJ, Node_Fetch+FIRSTPCSF) KEYSTO(K0);
 KALL(K0, KT+5) KEYSFROM(SB,K3,SB) KEYSTO(,,,K0) RCTO(rc);
 if(rc != KT+5) crash("PCS not playing game");
 KALL(K0, KT+5) KEYSFROM(,,D) KEYSTO(,,,K0) RCTO(rc);
 if(rc != KT+5) crash("PCS not playing game");
 KALL(K0, 3) KEYSFROM(ROOTNODE,K1,K2) RCTO(rc);  /* run init.cmd */

/* KALL(EARLYNODE, Node_Fetch+EARLYCONSOLE) KEYSTO(CONSOLE);  */
/* KALL(CONSOLE,0) KEYSTO(,CONSOLE) RCTO(rc); */
/* KALL(CONSOLE,0) CHARFROM("PCS Returned\n",13) RCTO(rc); */
 
 {  /* release the page frames for the primordial segment */
    /* this makes the maximum space available in the diskless system */

   int slot6,slot5,slot4,slot3;
   char buf[256];
   UINT32 sokrc;
   int count,maxpages; 
   int *segsize=(int *)0x200004;

   count=0;
   maxpages=1000;

   KALL(ROOTNODE, Node_Fetch+ROOTTARSEGNODE) KEYSTO(K0);
   KALL(K0,Node_Fetch+TARINIT) KEYSTO(K0);

   KALL(WOMBMEMROOT, Node_Swap+2) KEYSFROM(K0);
   maxpages = (*segsize) >> 12;

      for(slot5=0;slot5<16;slot5++) {
         KALL(K0, Node_Fetch+slot5) KEYSTO(K1);
         KALL(K1,KT) RCTO(rc);
         if(rc == KT+1) continue;
         for(slot4=0;slot4<16;slot4++) {
            KALL(K1,Node_Fetch+slot4) KEYSTO(K2);
            KALL(K2,KT) RCTO(rc);
            if(rc == KT+1) continue;
            for(slot3=0;slot3<16;slot3++) {
               KALL(K2,Node_Fetch+slot3) KEYSTO(K3);
               KALL(K3,KT) RCTO(rc);
               if(rc == KT+1) continue;
               KALL(EARLYNODE,Node_Fetch+EARLYPRIMERANGE) KEYSTO(D);
               KALL(D,42) KEYSFROM(K3);
               count++;
               if(count >= maxpages) goto done;
            }
         }
       }
done:
  }

/* now become available.  Calling will return the root node */

 KALL (ROOTNODE,0) KEYSTO(,,K0);
 LDEXBL (K0,9);
 for (;;) {
    LDENBL OCTO(rc) KEYSTO(,,,K0);
    RETJUMP();
    LDEXBL (K0,0) KEYSFROM(ROOTNODE);
 }
 
} /* end of main */
