/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "kktypes.h"
#include "keykos.h"
#include "sb.h"
#include "node.h"
#include "tdo.h"
#include "domain.h"
#include "fs.h"
#include "consdefs.h"
#include "bsload.h"
#include "vcs.h"
#include "factory.h"
#include "vdk.h"
#include "cck.h"
#include "mkeeper.h"
#include "ocrc.h"
#include "switcherf.h"
#include "kuart.h"
#include "wait.h"

//#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "setjmp.h"

#define SYSNUNIX 12
#define UNIXTARNODE 1

 
 struct scanblk {
   char str[256];
   char *ptr;
 };

 struct tarhdr {
       char name[100];
       char mode[8];
       char uid[8];
       char gid[8];
       char size[12];
       char mtime[12];
       char chksum[8];
       char typeflag;
       char lnk[100];
       char fill2[255];
     };

    KEY COMP=0;
    KEY DIR =1;
    KEY CALLER=2;
    KEY DOM =3;
    KEY SB  =4;
    KEY M   =5;
    KEY DOMCRE = 6;

    KEY SIK =7;
    KEY SOK =8;
    KEY TEMPDIR=9;

    KEY K0  =10;
    KEY K1  =11;
    KEY K2  =12;
    KEY K3  =13;
 
    KEY DIRSCRATCH=14;  /* for putkey */
    KEY SAVESB = 15;

#define COMPRCC 2
#define COMPKEEP 4
#define COMPFSF 6

  struct scanblk inbuf;
  struct scanblk token;
  char subtoken[256];
  char parm[8192],retstr[4096];
  char rcname[4096];
  int retlen,parmlen;

  long long buf[512],oldbuf[512];
  int oldnbytes=0; 

  char context[33]="";
  char branch[32];

  JUMPBUF;

  unsigned cvtnum();
  int getnum();
  char toupper(char);

  jmp_buf jump_buffer;
  UINT32 trap_ext;

    UINT32 rc;

   struct {
       long procaddr;
       long jmpbufaddr;
       long parm2;
       unsigned long seconds;
   } setup;

/* Storage for "extra" keys in the memory node */

#define KEEPERSLOT 4
#define CCKSLOT 5
#define UNTARFS 6

/* mapping slots for file access */

#define CMDSEGSLOT 7
#define CMDSEGADDR 0x70000000
#define FILEOUTSLOT 8
#define FILEOUTADDR 0x80000000
#define SEGOUTSLOT 9
#define SEGOUTADDR  0x90000000
#define TARSEGSLOT 10
#define TARSEGADDR  0xA0000000

   int outsegtype;
#define OUTSEG 1
#define OUTUNIXSEG 2
   int isoutseg=0;
   int outcnt=0;
   char *outaddr;
   struct FS_UnixMeta outsegum;

   int tartype;
   int tarlength;  /* if unix seg */
#define TARSEG 1
#define TARUNIXSEG 2

   int cmdcnt=0;
   int cmdtype;
#define CMDSEG 1
#define CMDUNIXSEG 2
   int cmdlength;     /* if unixseg then this is the length */
   char *cmdaddr;
   int isforcmdfile=0;
   struct FS_UnixMeta um;

   int consolesw=0;
   int havefs=0;

   int type=0;

   struct Node_KeyValues meternkv ={3,5,
     {{0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff}}
     };

    char title[]="PCS     ";

    void trap_function();

factory(oc,ord) 
   UINT32 oc;
   UINT32 ord;
{
   struct Node_DataByteValue ndb7 = {7};
   unsigned long errorcode;
   int actlen;

/* at this point DIR is SB and SB is PSB.  PCS does not use PSB */
   KC (DOM,Domain_GetKey+DIR) KEYSTO(SB);

   KC (SB,SB_CreateNode) KEYSTO(K0);
   KC (K0,Node_MakeNodeKey) STRUCTFROM(ndb7) KEYSTO(K0);
   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   KC (K0,Node_Swap+0) KEYSFROM(K1);
   KC (DOM,Domain_SwapMemory) KEYSFROM(K0);

   KC (CALLER,KT+5) KEYSTO(,,SIK,CALLER) RCTO(oc);  /* SIK has the terminal CCK key */
                                                      /* creation specifies SIK,SOK,CCK */
                                                      /* current definition allows recovery from CCK */
   if(oc == KT+5) {  /* a cmdfile, or new form */
      KC (CALLER,KT+5) KEYSTO(DIR,K3,K2,CALLER) RCTO(oc);  /* Keys depend on type*/
   }
   isforcmdfile=0;
   type=oc;
   switch(oc) {
   case 2:   /* New SYSTOOL DIR is actually ROOTNODE K3 and K2 are DK(0) */

      crash("Not implmented");

   case 3:   /* INIT form, DIR is Rootnode, K3 is tarseg, K2 is Outseg */

      isforcmdfile=1;  /* no new bank */

      KC (DOM,Domain_GetKey+DIR) KEYSTO(K1);

      KC (COMP,Node_Fetch+COMPRCC) KEYSTO(DIR);
      KC (DIR,TDOF_CreateNameSequence) KEYSFROM(SB,M,SB) KEYSTO(DIR);  /* local directory */

      adddir("rootnode",K1,DIR,"System Root Node",0);
      adddir("sb",SB,DIR,"Init Spacebank",0);
      adddir("m",M,DIR,"Init Meter",0);
      adddir("domcre",DOMCRE,DIR,"Domain Creator",0);

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+CCKSLOT) KEYSFROM(SIK); /* SIK is currently the terminal CCK key from creation */

      adddir("conscck",SIK,DIR,"CCK key for Console",0);
      KC (SIK,0) KEYSTO(SIK,SOK) RCTO(rc);  /* ignore CCK for now */

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+SEGOUTSLOT) KEYSFROM(K2);  /* set up for output segment */
   
      outcnt = 0;
      outaddr = (char *)SEGOUTADDR+4;
      isoutseg=1;
      outsegtype=OUTSEG;
      KC (K2,KT) RCTO(rc);
      if(rc == FS_AKT) {
         outsegtype=OUTUNIXSEG;
         outaddr = (char *)SEGOUTADDR;
      }

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+TARSEGSLOT) KEYSFROM(K3);
    
      tartype = TARSEG;  
      untar("initdir");

      strcpy(inbuf.str,"");
      inbuf.ptr=inbuf.str;

      getkey("initdir/init.cmd",K3,0,0); 

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+CMDSEGSLOT) KEYSFROM(K3);

      cmdtype=CMDSEG;
      cmdaddr=(char *)CMDSEGADDR;
      cmdcnt=*((int *)cmdaddr);
      cmdaddr +=4;

      KC (K3,KT) RCTO(rc);
      if(rc == FS_AKT) {  /* bound to be true after untar */
          struct FS_UnixMeta um;
          KC (K3,FS_GetMetaData) STRUCTTO(um) RCTO(rc);
          if(!rc && um.length) {  /* a UnixSeg */
              cmdtype=CMDUNIXSEG;
              cmdaddr=(char *)CMDSEGADDR;
              cmdcnt=1;
              cmdlength=um.length;
          }
      }
      isforcmdfile=1;
      
      break;

   case 0:
            /* This form which becomes the standard is used by the context switcher */
            /* no sik,sok,cck, there is a switcher key in User.   DIR is local directory  */   
            /* no private spacebank and meter are created */

      KC (DOM,Domain_GetKey+SB) KEYSTO(SAVESB);  /* for compatibility */

      adddir("domcre",DOMCRE,DIR,"Domain Creator",0);

      KC (DOM,Domain_GetMemory) KEYSTO(K0);      /* for compatibility */

      if(getkey("switcher",K1,0,0)) {  /* there is a switcher, get branch */
          KC (K1,Switcher_CreateBranch) KEYSTO(SIK,SOK,K1) CHARTO(branch,32,actlen);
          branch[actlen]=0;
          KC (K0,Node_Swap+CCKSLOT) KEYSFROM(K1); 
          adddir("conscck",K1,DIR,"CCK key for trace",0);
      }
      else crash("bad call");

      getkey("user/admin/testtarseg",K3,0,0);
      adddir("tarseg",K3,DIR,"Test program segment",0);

      cmdcnt=0;

      getvdk();

      outsok("");
      outsok("\n\nWelcome to Pacific PCS-01\n\n");

      break;

   case 1:  /* all the same now */
   case 4:   /* USER form,  DIR is the USER. Directory */

      KC (DOM,Domain_GetKey+SB) KEYSTO(SAVESB);
      KC (SB,SB_CreateBank) KEYSTO(SB);   /* we want this to be the master bank */

      KC (DOM,Domain_GetKey+DIR) KEYSTO(K3);

      KC (COMP,Node_Fetch+COMPRCC) KEYSTO(DIR);
      KC (DIR,TDOF_CreateNameSequence) KEYSFROM(SB,M,SB) KEYSTO(DIR);  /* local directory */

      adddir("user/",K3,DIR,"User Directory",0);
      adddir("domcre",DOMCRE,DIR,"Domain Creator",0);
    
      KC (DOM,Domain_GetMemory) KEYSTO(K0);

/* PCS always runs on the birth meter */

      adddir("sb",SB,DIR,"Current Spacebank",0);
      if(getkey("user/sys/mkeeperf",K0,0,0)) {
         KC (K0, MKeeperF_Create) KEYSFROM(SB,M) KEYSTO(K0);
         adddir("mkeeper",K0,DIR,"Master Meter Keeper",0);
      }

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+CCKSLOT) KEYSFROM(SIK); /* SIK is currently the terminal CCK key from creation */

/* HACK  When PCS is created by context switcher ZMK will be used to disconnect */

      adddir("conscck",SIK,DIR,"CCK key for Console",0);  /* This must become ZMK */

      strcpy(branch,"A"); 
      if(getkey("switcher",K1,0,0)) {  /* there is a switcher, get branch */
          KC (K1,Switcher_CreateBranch) KEYSTO(SIK,SOK,K1) CHARTO(branch,32,actlen);
          branch[actlen]=0;
          KC (K0,Node_Swap+CCKSLOT) KEYSFROM(K1); 
      }
      else {
         if(getkey("user/sys/switcherf",K2,0,0)) {
            KC (SIK,0) KEYSTO(SIK,SOK,K1);
            KC (DOM,Domain_GetMemory) KEYSTO(K0);
            KC (K2,KT+5) KEYSFROM(SB,M,SB) KEYSTO(,,,K2) RCTO(rc);
            KC (K2,SwitcherF_Create) KEYSFROM(SIK,SOK,K1) KEYSTO(K2);
            adddir("switcher",K2,DIR,"Switcher Key",0);
            KC (K2,Switcher_CreateBranch) KEYSTO(SIK,SOK,K1) CHARTO(branch,32,actlen);
            branch[actlen]=0;
            KC (K0,Node_Swap+CCKSLOT) KEYSFROM(K1);
         }
         else {  /* old old fashioned */
            KC (SIK,0) KEYSTO(SIK,SOK,K0) RCTO(rc);
         }
      }

      getkey("user/admin/testtarseg",K3,0,0);
      adddir("tarseg",K3,DIR,"Test program segment",0);

      cmdcnt=0;

      getvdk();
      outsok("");
      outsok("\n\nWelcome to KeyKOS PCS-01\n\n");

      break;

   case 5:   /* COMMAND FILE   DIR is local directory K3 is command file segment */

      isforcmdfile=1;

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+CMDSEGSLOT) KEYSFROM(K3);   /* WARNING ON K3 USE */
 
      cmdaddr=(char *)CMDSEGADDR;
      cmdtype=CMDSEG;
      cmdcnt=*((int *)cmdaddr);
      cmdaddr +=4;

      KC (K3,KT) RCTO(rc);
      if(rc == FS_AKT) {  /* possible UnixSeg */
          KC (K3,FS_GetMetaData) STRUCTTO(um) RCTO(rc);
          if(!rc) {
             if(um.length) {
                 cmdlength=um.length;
                 cmdaddr=(char *)CMDSEGADDR;
                 cmdtype=CMDUNIXSEG;
                 cmdcnt=1;            /* trigger getbuf */
             }
          }
      }

      KC (DOM,Domain_GetMemory) KEYSTO(K0);
      KC (K0,Node_Swap+CCKSLOT) KEYSFROM(SIK); /* SIK is currently the terminal CCK key from creation */
      KC (SIK,0) KEYSTO(SIK,SOK) RCTO(rc);  /* ignore CCK for now */

      break;
 
   default:
      crash("Undefined form");
   }

/* set up keeper */
//   if(type == 3) goto nokeeper;  /* there is no keeper for INIT */
   if(isforcmdfile) goto nokeeper; /* no timeouts for CMDFILES */
   KC (COMP,Node_Fetch+COMPKEEP) KEYSTO(K0);
   KC (K0,KT+5) KEYSFROM(SB,M,SB) KEYSTO(,,,K0) RCTO(rc);
   KC (K0,0) KEYSFROM(DOM) KEYSTO(K0);
   setup.procaddr=(long)trap_function;
   setup.jmpbufaddr=(long)jump_buffer;
   setup.parm2=(long)&trap_ext;
   setup.seconds=30;
   KC (K0,0) STRUCTFROM(setup);

   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   KC (K1,Node_Swap+KEEPERSLOT) KEYSFROM(K0);

   if(errorcode=setjmp(jump_buffer)) {  /* a trap of some kind */
      char buf[100];

      KC(DOM,Domain_GetMemory) KEYSTO(K0);
      KC(K0,Node_Fetch+CCKSLOT) KEYSTO(K0);

      KC (K0,0) KEYSTO(SIK,SOK) RCTO(rc);  /* ignore CCK for now */
      KC (SOK,0) KEYSTO(,,,SOK) RCTO(rc);
      if(4==errorcode) { /* timeout */
          outsok("\n\nTIMEOUT!!\n\n");
      }
      else {
          sprintf(buf,"\n\nATC %X Trap Extension %X\n\n",errorcode,trap_ext);
          outsok(buf);

          KC(DOM,Domain_GetMemory) KEYSTO(K0);
          KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);

          KC(K0,2) RCTO(rc);
      }
      cmdcnt=0;
      if(isforcmdfile) goto cmdfileexit;
   }
nokeeper:
/* end set up keeper */
 
/* begin processing commands */

   rcname[0]=0;
   rcname[4095]=0;

   for (;;) {
     if(readbuf(&inbuf)) break;;

//     KC(DOM,Domain_GetMemory) KEYSTO(K0);
//     KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);
//     if(type != 0) KC(K0,1) RCTO(rc);

     if(docommand()) break;  /* logoff */

//     KC(DOM,Domain_GetMemory) KEYSTO(K0);
//     KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);
//     if(type != 0) KC(K0,2) RCTO(rc);
   }

cmdfileexit:

   KC(DOM,Domain_GetMemory) KEYSTO(K0);
   KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);

   KC(K0,KT+4) RCTO(rc);

   if(isoutseg && (outsegtype==OUTUNIXSEG)) {
       KC (DOM,Domain_GetMemory) KEYSTO(K0);
       KC (K0,Node_Fetch+SEGOUTSLOT) KEYSTO(K0);
       KC (K0,FS_SetMetaData) STRUCTFROM(outsegum) RCTO(rc);
   }

/* begin tear down */

   KC (DOM,Domain_GetMemory) KEYSTO(K0);
   KC (K0,Node_Fetch+0) KEYSTO(K1);
   KC (DOM,Domain_SwapMemory) KEYSFROM(K1);

   if(!isforcmdfile) KC (SAVESB,SB_DestroyNode) KEYSFROM(K0);
   else KC (SB,SB_DestroyNode) KEYSFROM(K0);

   if(isforcmdfile) exit(0);

   outsok("Reclaim all space\n");

   if(getkey("switcher",K0,0,0)) {
      KC (K0,DESTROY_OC) RCTO(rc);
   }

   getkey("meternode",K0,0,0);      /* meternode key in master directory */
   KC (K0,Node_Swap+3);   /* stop meter  and all context meters */
   
   KC (SB,SB_DestroyBankAndSpace);   /* this zaps all context spaces as well */
   KC (DOM,Domain_GetKey+SAVESB) KEYSTO(SB);
}

getname(place)
    char *place;
{
   if(!gettoken(&inbuf," ",&token)) {   /* switch to master */
       return 0;
   }
   if(strlen(token.str) > 32) {
       outsok("Context name too long\n");
       return 0;
   }
  
   place[0]=1;   /* pretty non-printable */
   place[1]=0;
   strcat(place,token.str);
   strcat(place,"/");
   return 1;
}

getvdk()
{
     UINT32 vdktype;

     vdktype=VDKF_CreateCCK;
     if(getkey("user/sys/vdkf",K0,0,0)) {
       KC (DOM,Domain_GetMemory) KEYSTO(K1);
       if(getkey("switcher",K1,0,0)) {
           vdktype=VDKF_CreateSwitcher;
       } 
       else {
           KC (K1,Node_Fetch+CCKSLOT) KEYSTO(K1);
       }    
       KC (K0,KT+5) KEYSFROM(SB,M,SB) KEYSTO(,,,K0) RCTO(rc);
       if(rc == KT+5) {
           KC(K0,vdktype) KEYSFROM(K1) KEYSTO(K0) RCTO(rc);
           if(!rc) {
              putkey("vdk",K0,0,0);
           }
       } 
     }
     else if(getkey("user/admin/error",K0,0,0)) {  /* we are a good guy  */
          putkey("vdk",K0,0,0);
     }
     else {}
}

adddir(name,slot,dirslot,des,deslen)
   char *name;
   KEY slot;
   KEY dirslot;
   char *des;
   int deslen;
{
   char *ptr;
   int len;	

   char buf[256];
 
   *rcname=strlen(name);
   rcname[1]=0;
   strcat(rcname,name);
   len=strlen(rcname);
   if(des) {
     ptr=&rcname[len];
     if(!deslen) deslen=strlen(des);
     memcpy(ptr,des,deslen);
     len=len+deslen;
   }
   KC (dirslot,TDO_AddReplaceKey) CHARFROM(rcname,len)
     KEYSFROM(slot) RCTO(rc);
}
outsok(str)
   char *str;
{
    int len;
    char *t;
    UINT32 sokrc;

    t=str;
    while(*t) {
      if(*t < 32) {
         if (*t != 0x0a && *t != 0x0d) *t='_';
      }
      t++;
    }
    len=strlen(str);
    t=str;
    if(!isoutseg || (consolesw==1)) {
      while(len > 256) {
        KC (SOK,0) CHARFROM(t,256) KEYSTO(,,,SOK) RCTO(sokrc);
        len=len-256;
        t=t+256;
      }
      KC (SOK,0) CHARFROM(t,len) KEYSTO(,,,SOK) RCTO(sokrc);
    }

    if(isoutseg) {
        if(outsegtype == OUTSEG) {
           if(str[len-1] == '\n') len--;  /* don't put nl in segment */
           if(!strcmp(str,"F:")) len=0;
           if(len) {
              memcpy(outaddr,&len,4);
              outaddr += 4;
              strcpy(outaddr,str);
              outaddr += len;
              outcnt++;
              memcpy((char *)SEGOUTADDR,&outcnt,4);
           }
        }
        if(outsegtype == OUTUNIXSEG) {
           if(!strcmp(str,"F:\n")) len=0;
           if(len) {
              memcpy(outaddr,str,len);
              outaddr += len;
              outsegum.groupid=0;
              outsegum.userid=0;
              outsegum.mode=0x1ff;
              outsegum.inode=1;
              outsegum.length = outaddr-(char *)SEGOUTADDR;
           }
        }
    }
}
readbuf(in)
  struct scanblk *in;
{
   int buflen;
   char *newcmdaddr;
   char pbuf[36];

    if(cmdcnt) {  /* read from segment */
       if(cmdtype==CMDSEG) {
          memcpy((char *)&buflen,cmdaddr,4);
          cmdaddr += 4;
          newcmdaddr = cmdaddr+buflen;
          if(buflen > 255) buflen=255;
          strncpy(in->str,cmdaddr,buflen);
          cmdaddr=newcmdaddr;
          in->str[buflen]=0;
          outsok("F:");
          outsok(in->str);
          outsok("\n");
          cmdcnt--;
          in->ptr=in->str;
          return 0;
       }
       if(cmdtype==CMDUNIXSEG) {
          char *ptr;
          /* scan forward for '\n' */
          ptr=cmdaddr;
          while((ptr-(char *)CMDSEGADDR) < cmdlength) {   /* scan till NL or end */
             if(*ptr == '\n') break;   /* end of record */
             ptr++;
          } 
          /* ptr is at NL or implied NL after last character */
          if( (ptr-(char *)CMDSEGADDR) >= (cmdlength-1) ) cmdcnt=0;  /* signal no more */
          buflen=ptr-cmdaddr;
          if(buflen > 255) buflen=255;
          strncpy(in->str,cmdaddr,buflen);
          cmdaddr=ptr+1;
          in->str[buflen]=0;
          in->ptr=in->str;
          outsok("F:");
          outsok(in->str);
          outsok("\n");
          return 0; 
       }
    } 

    if(isforcmdfile) return 1;

readagain:
    if(*context) {           /* prompt with context name */
       strcpy(pbuf,context);
       strcat(pbuf,":");
    }
    else {
//       strcpy(pbuf,"A:");
         strcpy(pbuf,branch);
         strcat(pbuf,":");
    }
    outsok(pbuf);

    KC (SIK,8192+255) CHARTO(in->str,255,buflen) KEYSTO(,,,SIK) RCTO(rc);
    if(rc) {    /* if we lost this somehow recover it now */
        KC (DOM,Domain_GetMemory) KEYSTO(SIK);
        KC (SIK,Node_Fetch+CCKSLOT) KEYSTO(SIK);
        KC (SIK,CCK_RecoverKeys) KEYSTO(SIK,SOK) RCTO(rc);
        KC (SOK,0) KEYSTO(,,,SOK) RCTO(rc);
        buflen=0;
    }
    if(!buflen) {
        goto readagain;
    }
    buflen--;
    in->str[buflen]=0;
    in->ptr=in->str;
    return 0;
}

dolistfile()
{
  UINT32 oc,rc,actlen,i;
  char buf[258];

  KC (DOM,Domain_GetKey+DIR) KEYSTO(K0);
  if(gettoken(&inbuf,", ",&token)) {
     if(!getkey(token.str,K0,0,0)) {
       outsok("Name Unknown\n");
       return 0;
     }
  }

  KC (K0,KT) RCTO(rc);
  if (rc != 0x17) {
    outsok("Not Record Collection\n");
    return 0;
  }

  oc=8;
  rc=1;

  while(rc <= 2) {
    if ((oc == 13)  && (strlen(rcname) == 0)) break; // next of null is first
    KC (K0,oc)  CHARFROM(rcname,strlen(rcname)) 
      CHARTO(rcname,4096,actlen) RCTO(rc);
    rcname[actlen]=0;
    if(rc <= 2) {
      if(actlen > rcname[0]+1) {  /* data present */
         oc=rcname[0];
         strncpy(buf,&rcname[1],oc);
         buf[oc]=',';
         buf[oc+1]=0;
         outsok(buf);
         if((rcname[oc+1] < 0x31) || (rcname[oc+1] > 'z')) {
            puthex(&rcname[oc+1],buf,actlen-rcname[0]-1);
            outsok(buf);
         }
         else {
            outsok(&rcname[oc+1]);
         }
      }
      else {
         outsok(&rcname[1]);
      }
      outsok("\n");
    }
    oc=13;
  }
}
 
docommand()
{
    if(!gettoken(&inbuf," ",&token)) return 0;

    if(!strcmp(token.str,"logoff")) {
          if(getkey("zmk",K0,0,0)) {
              outsok("Must use disconnect\n");
              return 0;
          }
          getkey("conscck",K0,0,0);
//          KC(DOM,Domain_GetMemory) KEYSTO(K0);
//          KC(K0,Node_Fetch+CCKSLOT) KEYSTO(K0);
          KC(K0,KT) RCTO(rc);
          if( rc == CCK3_AKT) {
              outsok("Can't loggoff with permanent virtual circut, use 'disconnect'\n");
              return 0;
          }
          return 1;
    }
    else if(!strcmp(token.str,"cfact")) docfact();
    else if(!strcmp(token.str,"checkpoint")) docheckpoint();
    else if(!strcmp(token.str,"console")) doconsole();
    else if(!strcmp(token.str,"counters")) docounters();
    else if(!strcmp(token.str,"cmdfile")) docmdfile();
    else if(!strcmp(token.str,"datakey")) dodatakey();
    else if(!strcmp(token.str,"disconnect")) dodisconnect();
    else if(!strcmp(token.str,"edit")) doedit();
    else if(!strcmp(token.str,"factory")) dofactory(0);
    else if(!strcmp(token.str,"getfile")) dogetfile(0);
    else if(!strcmp(token.str,"listfile")) dolistfile();
    else if(!strcmp(token.str,"ls")) dolistfile();
    else if(!strcmp(token.str,"kc")) dojump(0);
    else if(!strcmp(token.str,"kfork")) dojump(1);
    else if(!strcmp(token.str,"kt")) dokt();
    else if(!strcmp(token.str,"readmeter")) doreadmeter();
    else if(!strcmp(token.str,"space")) dospace();
    else if(!strcmp(token.str,"newbank")) donewbank();
    else if(!strcmp(token.str,"newmeter")) donewmeter();
    else if(!strcmp(token.str,"trace")) dotrace();
    else if(!strcmp(token.str,"ufact")) dofactory(1);
    else if(!strcmp(token.str,"untar")) dountar();
    else if(!strcmp(token.str,"?")) dohelp();
    else if(!strcmp(token.str,"#")) docomment();
    else if(!strcmp(token.str,"timeout")) docomment();
else if(!strcmp(token.str,"spaceavailable")) dospaceavailable();
    else outsok("\nCommand Unknown\n");
    if(!isforcmdfile) outsok("\n");
    return 0;
}

dospaceavailable()
{
    int i;
    char *ptr;
    char buf[256];

    outaddr = (char *)SEGOUTADDR;
    getkey("user/sys/fsf",K1,0,0);
    KC (K1,0) KEYSFROM(SB,M,SB) KEYSTO(K1);
    KC (DOM,Domain_GetMemory) KEYSTO(K0);
    KC (K0,Node_Swap+SEGOUTSLOT) KEYSFROM(K1);

    for(i=0;i<8192;i++) {  /* 32 megs worth of pages */
      ptr=outaddr+i*4096;
      *ptr=1;
      if(!(i % 256))  {
        sprintf(buf,"%d megabytes so far\n",(i / 256));
        outsok(buf);
      }
    }
    KC (K1,KT+4) RCTO(rc);
}   

dodisconnect()
{
//   KC (DOM,Domain_GetMemory) KEYSTO(K1);
//   KC (K1,Node_Fetch+CCKSLOT) KEYSTO(K1);

   if(getkey("zmk",K1,0,0)) {  /* zapper */
//      KC(DOM,Domain_GetMemory) KEYSTO(K0);
//      KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);
//      KC(K0,2) RCTO(rc);
      KC(K1,ZMK_Disconnect) RCTO(rc);
      return;
   }
    
   getkey("conscck",K1,0,0);
   KC (K1,KT) RCTO(rc);
   if (rc != CCK3_AKT) {
       outsok("Can't disconnect without permanent virtual circuit, use 'logoff'\n");
       return 0;
   }

//   KC(DOM,Domain_GetMemory) KEYSTO(K0);
//   KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);
//   KC(K0,2) RCTO(rc);

   KC (K1,CCK_Disconnect) RCTO(rc);
}


docomment()
{
}

dohelp()
{
   outsok("#\n");
   outsok("?\n");
   outsok("cfact factname segname akt [stack=] [ord] H|F|SN=\n");
   outsok("checkpoint\n");
   outsok("console on|off\n");
   outsok("counters\n");
   outsok("cmdfile name\n");
   outsok("datakey name string\n");
   outsok("disconnect\n");
   outsok("edit name\n");
   outsok("factory factname segment akt [ord] H|F|SN=\n");
   outsok("getfile remotename localname [progressindication]\n");
   outsok("kc keyname oc (string,k1,k2,k3) (string,k1,k2,k3)\n");
   outsok("kfork keyname oc (string,k1,k2,k3)\n");
   outsok("kt keyname\n");
   outsok("listfile [name]\n");
   outsok("logoff\n");
   outsok("ls [name]\n");
   outsok("newbank name [superior bank]\n");
   outsok("newmeter name [superior meter]\n");
   outsok("readmeter name\n");
   outsok("space name\n");
   outsok("trace on|of\n");
   outsok("ufact factname segment akt [ord] H|F|SN=\n");
   outsok("untar segment [directory]\n");
}  


/* USES existing SB and METER .. be careful */
docfact() {

   int stacksize;
   char factname[256];
   char keyname[256];
   unsigned long akt,ordinal; 
   char buf[128];
   char *saveptr;
   int  lsfakt = 0x10F0D;
   int  trc;
   struct Bsload_StartAddr sa; 

   if(!gettoken(&inbuf," ",&token)) {
       outsok("No factory name\n");
       return 0;
   }
   strcpy(factname,token.str);
   if(!gettoken(&inbuf," ",&token)) {
       outsok("No segment specified\n");
       return 0;
   }
   if(!getkey(token.str,K2,0,0)) {
       outsok("Segment not found\n");
       return 0;
   }
   /* assumed to be FS */
   KC (K2,KT) RCTO(rc);
   if(rc != 0x1005) {
      KC (K2,3) KEYSTO(K2) RCTO(rc);
      if(rc) {
         outsok("Segment not FS\n");
         return 0;
      }
   }

   if(!gettoken(&inbuf," ",&token)) {
       outsok("AKT missing\n");
       return 0;
   }
   akt = (unsigned)cvtnum(token.str);

   ordinal=0;
   saveptr=inbuf.ptr;  /* in case no ordinal */
   if(gettoken(&inbuf," =",&token)) {  /* might be ordinal or first component */
      if((*token.str >= '0') && (*token.str <= '9')) {  /* ordinal */
          ordinal = (unsigned)cvtnum(token.str);
      }
      else {
          inbuf.ptr=saveptr;
      }
   }

   stacksize=0;
   saveptr=inbuf.ptr;  /* in case no stacksize */
   if(gettoken(&inbuf,"=",&token)) {  /* might be ordinal or first component */
      if(!strcmp(token.str,"stack")) {  /* stack= */
           if(!gettoken(&inbuf," ",&token)) {
               outsok("Stacksize malformed\n");
               return 0;
           }
           stacksize = (unsigned)cvtnum(token.str);
           stacksize = (stacksize + 4095) & 0xFFFFF000;
      }
      else {
          inbuf.ptr=saveptr;
      }
   }
   if(stacksize < 4096) stacksize=4096;

   if(!getkey("user/sys/factoryc",K3,0,0)) {
       outsok("Can't find factoryc\n");
       return 0;  /* small hole count */
   }

   /*  need fetcher factory for lsfsim node (into K0)  K2 has code segment K3 has factoryc */
   /*  need to get LSFSIMCODE ... */

   if(!getkey("user/sys/lsfsimcode",K1,0,0)) {
       outsok("Can't find lsfsimcode\n");
       return 0;
   }
   KC (K3,FC_Create) KEYSFROM(SB) KEYSTO(K0) RCTO(rc);
   if(rc) {
      outsok("FactoryCreate failed \n");
      return 0;
   }
 
   KC (K0,FactoryB_AssignKT) STRUCTFROM(lsfakt) RCTO(rc);
   if(rc) {
       outsok("Fetcher assign AKT failed\n");
       return 0;
   }
   trc=0;
   KC (K0,FactoryB_InstallSensory+0) KEYSFROM(K1) RCTO(rc);
   trc += rc;
   KC (K0,FactoryB_InstallSensory+1) KEYSFROM(K2) RCTO(rc);
   trc += rc;
   KC (K0,FactoryB_InstallSensory+2) KEYSFROM(K2) RCTO(rc);
   trc += rc;
   memset(buf,0,6);
   memcpy(buf+2,&stacksize,4);
   if(!getkey("user/sys/dkc",K1,0,0)) {
       outsok("Can't find DKC\n");
       return 0;
   }
   KC (K1,0) CHARFROM(buf,6) KEYSTO(K1) RCTO(rc);
   KC (K0,FactoryB_InstallSensory+4) KEYSFROM(K1);
   KC (COMP,Node_Fetch+COMPFSF) KEYSTO(K1);
//   if(!getkey("user/sys/fsf",K1,0,0)) {
//       outsok("Can't find FSF\n");
//       return 0;
//   }
   KC (K0,FactoryB_InstallFactory+3) KEYSFROM(K1) RCTO(rc);
   trc += rc;
   if(trc) {
       outsok("FetcherFactory comonent installation failed\n");
       return 0;
   }
   KC (K0,FactoryB_MakeRequestor) KEYSTO(K2) RCTO(rc);
   if(rc) {
       outsok("FetchFactory failed\n");
       return 0;
   }

/* At this time K2 has the .program */

   KC (K3,FC_Create) KEYSFROM(SB) KEYSTO(K3) RCTO(rc);
   if(rc) {
       outsok("FactoryCreator failed \n");
       return 0;
   }
   strcpy(keyname,factname);
   strcat(keyname,".builder");
   putkey(keyname,K3,0,0);

   sa.sa=-1;  // start address of signal to use LSF
   finishfact(factname,sa,akt,ordinal,0);  // Builder in K3, .program in K2
}

dofactory(uni)
   int uni;     // if 1 means a Unix object
{
   UINT32 ktvalue;
   char factname[256];
   char keyname[256];
   struct Bsload_StartAddr sa;
   unsigned long akt,ordinal;
   unsigned long progbase,loadersa;
   unsigned long proglength;
   unsigned long long seglength;
   unsigned long loaderbase = 0x0F200000;
   char buf[128];
   char *saveptr;

   if(!gettoken(&inbuf," ",&token)) {
	outsok("No factory name\n");
 	return 0;
   }
   strcpy(factname,token.str);
   strcpy(keyname,factname);
   strcat(keyname,"/sb");
   KC (SB,SB_CreateBank) KEYSTO(K0);
   putkey(keyname,K0,0,0);

   KC (K0,SB_CreateNode) KEYSTO(K1);
   KC (K1,Node_WriteData) STRUCTFROM(meternkv);
   KC (K1,Node_Swap+1) KEYSFROM(M); /* pcs meter */

   strcpy(keyname,factname);
   strcat(keyname,"/meternode");
   putkey(keyname,K1,0,0);   /* factory meternode */

   KC (K1,Node_MakeMeterKey) KEYSTO(K1);  /* factory Meter */

   strcpy(keyname,factname);
   strcat(keyname,"/m");
   putkey(keyname,K1,0,0);

   if(!getkey("user/sys/vcsf",K2,0,0)) return 0;
   KC (K2,VCSF_Create) KEYSFROM(K0,K1,K0) KEYSTO(K2) RCTO(rc);
   if(rc) {
       outsok("VCSF failed\n");
       return 0;
   }
   strcpy(keyname,factname);
   strcat(keyname,"/vcsf");
   putkey(keyname,K2,0,0);
   if(!getkey("user/sys/bsloadf",K3,0,0)) return 0;
   KC (K3,Bsloadf_Create) KEYSFROM(K0,K1,K0) KEYSTO(K3) RCTO(rc);
   if(rc) {
       outsok("BSLOADF failed\n");
       return 0;
   }

   if(!gettoken(&inbuf," ",&token)) {
	outsok("No segmentkey name\n");
        return 0;
   }
   if(!getkey(token.str,K1,0,0)) {  /* don't need meter just now */
        outsok("Key unknown\n");
        return 0;
   }
/* WE NEED TO check to see if this is a directory and look inside if it is */

   if(uni) {
         KC (K3,Bsload_LoadDynamicElf) KEYSFROM(K1,K2) STRUCTTO(sa) RCTO(rc);

         progbase=sa.sa;

         if(!rc) {  /* load the loader */
            rc=1-getkey("user/sys/ld.so.1",K1,0,0);
            if(!rc) {
                 KC (K3,Bsload_LoadSimpleElf) STRUCTFROM(loaderbase) KEYSFROM(K1,K2) 
                       STRUCTTO(sa) RCTO(rc);
                 loadersa=sa.sa;
                 if(!rc) {
                     rc=1-getkey("user/sys/uwrapper",K1,0,0);
                     if(!rc) {
                          KC (K3,Bsload_LoadSimpleElf) KEYSFROM(K1,K2) 
                              STRUCTTO(sa) RCTO(rc);   // wrappersa is in sa
                     }
                 }
            }
         }
   }
   else {
       KC (K3,Bsload_LoadSimpleElf) KEYSFROM(K1,K2) STRUCTTO(sa) RCTO(rc);
   }
   if(rc) {
        KC (K3,KT+4) RCTO(rc);
        outsok("BSLOAD failed\n");
        return 0;
   }

   KC (K3,KT+4) RCTO(rc);
   KC (K2,VCS_Freeze) KEYSFROM(K0) KEYSTO(K2) RCTO(rc);
   if(rc) {
        outsok("VCS Freeze failed\n");
        return 0;
   }
/* K0 has SB, K1 is free, K2 has .program ,  K3 is free */
   
   if(!getkey("user/sys/factoryc",K3,0,0)) return 0;  /* small hole count */
   KC (K3,FC_Create) KEYSFROM(K0) KEYSTO(K3) RCTO(rc);
   if(rc) {
       outsok("FactoryCreator failed \n");
       return 0;
   }
   strcpy(keyname,factname);
   strcat(keyname,"/builder");
   putkey(keyname,K3,0,0);

   if(!gettoken(&inbuf," ",&token)) {
       outsok("AKT missing\n");
       return 0;
   }
   akt = (unsigned)cvtnum(token.str);

   ordinal=0;
   saveptr=inbuf.ptr;  /* in case no ordinal */
   if(gettoken(&inbuf," =",&token)) {  /* might be ordinal or first component */
      if((*token.str >= '0') && (*token.str <= '9')) {  /* ordinal */
          ordinal = (unsigned)cvtnum(token.str);
      }
      else {
          inbuf.ptr=saveptr;
      }
   }

   if(uni) {  // need to build info node and install as component 0, set sa to wrappersa
       struct Node_KeyValues nkv;
       int ilen;

       nkv.StartSlot=0;
       nkv.EndSlot=4;
       memset(&nkv.Slots[0],0,Node_KEYLENGTH);
       memset(&nkv.Slots[1],0,Node_KEYLENGTH);
       memset(&nkv.Slots[2],0,Node_KEYLENGTH);
       memset(&nkv.Slots[3],0,Node_KEYLENGTH);
       memset(&nkv.Slots[4],0,Node_KEYLENGTH);
       ilen=strlen(factname);
       if(ilen > 11) ilen=11;
       memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-ilen],factname,ilen);
       memcpy(&nkv.Slots[1].Byte[Node_KEYLENGTH-4],&progbase,4);
       memcpy(&nkv.Slots[2].Byte[Node_KEYLENGTH-4],&proglength,4);
       memcpy(&nkv.Slots[3].Byte[Node_KEYLENGTH-4],&loaderbase,4); 
       memcpy(&nkv.Slots[4].Byte[Node_KEYLENGTH-4],&loadersa,4);

       KC(K0,SB_CreateNode) KEYSTO(K1) RCTO(rc);
       if(rc) {
           outsok("Create Node failed\n");
           return 0;
       }
       KC (K1,Node_WriteData) STRUCTFROM(nkv);
       strcpy(keyname,factname);
       strcat(keyname,"/node");
       putkey(keyname,K1,0,0);
       KC (K1,Node_MakeSenseKey) KEYSTO(K1);
          
       KC (K3,FactoryB_InstallSensory+0) KEYSFROM(K1) RCTO(rc);
       if(rc) {
           outsok("Install Sense Node failed\n");
           return 0;
       }
   }
   finishfact(factname,sa,akt,ordinal,uni);
   return 0;
}
finishfact(factname,sa,a,o,uni)
   char *factname;
   struct Bsload_StartAddr sa;
   unsigned long a,o;
   int uni;     // unix factory needs special keeper
{
   char comp[32];
   int compnum;
   char buf[128];
   unsigned long akt,ordinal;
   unsigned long ktvalue;

   akt=a;
   ordinal=o;

   KC (K3,FactoryB_InstallFactory+17) STRUCTFROM(sa) KEYSFROM(K2) RCTO(rc);
   if(rc) {
      KC (K2,KT) RCTO(ktvalue);
      sprintf(buf,"Install .program (kt=%x) rc=%x\n",ktvalue,rc);
      outsok(buf);
      return 0;
   } 

   if(uni) {
     if(getkey("user/sys/ukeeperf",K2,0,0)) {
        KC (K3,FactoryB_InstallFactory+16) KEYSFROM(K2) RCTO(rc);
        if(rc) {
            outsok("Keeper installation failed\n");
            return 0;
        }
     }
   }
   else {

     if(getkey("vdk",K2,0,0)) {  /* get the command system keeper */

//        KC (K2,KT) RCTO(rc);
//        sprintf(buf,"Installing VDK kt=%x\n",rc);
//        outsok(buf);

        KC (K3,FactoryB_InstallHole+16) KEYSFROM(K2) RCTO(rc);
        if(rc) {
            outsok("Keeper installation failed\n");
            return 0;
        }
     }
   }

   KC (K3,FactoryB_AssignKT) STRUCTFROM(akt) RCTO(rc);
   if(rc) {
      outsok("Assign AKT failed\n");
      return 0;
   }
   KC (K3,FactoryB_AssignOrdinal) STRUCTFROM(ordinal) RCTO(rc);
   if(rc) {
      outsok("Assign Ordinal failed\n");
      return 0;
   }
   while(gettoken(&inbuf,"=",&token)) {
      strcpy(comp,token.str);
      if(!gettoken(&inbuf," ",&token)) {
          outsok("Malformed component spec\n");
          return 0;
      }
      if(!getkey(token.str,K2,0,0)) {
          outsok("Component key missing\n");
          return 0;
      }
      compnum=cvtnum(comp+1);
//      KC (K2,KT) RCTO(ktvalue);
      if     (*comp == 's') KC (K3,FactoryB_InstallSensory+compnum) KEYSFROM(K2) RCTO(rc);
      else if(*comp == 'f') KC (K3,FactoryB_InstallFactory+compnum) KEYSFROM(K2) RCTO(rc);
      else if(*comp == 'h') KC (K3,FactoryB_InstallHole+compnum) KEYSFROM(K2) RCTO(rc);
      else {
          outsok("Component specification error\n");
          return 0;
      }
      if(rc) {
          sprintf(buf,"Component %c%d rc=%d\n",*comp,compnum,rc);
          outsok(buf);
          return 0;
      }
   }
   KC (K3,FactoryB_MakeRequestor) KEYSTO(K3);
   putkey(factname,K3,0,0);

   return 0;
}

dogetfile(type)
   int type;
{
   char remotename[256],localname[256];
   char response[256];
   char *ptr,*previousrecord,*iptr;
   int *rcount;
   int actlen,firstr,startrecord,length;
   unsigned short getfileok=GETFILEOK,getfilerdy=GETFILERDY; 
   unsigned short getfileeof=GETFILEEOF,getfileerr=GETFILEERR;
   unsigned short getfileresend=GETFILERESEND;
   struct FS_UnixMeta um;
   int progress,counter;
   unsigned long long delay;

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Remote Name given\n");
      return 0;
   }

   strcpy(remotename,token.str);

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Local Name given\n");
      return 0;
   }
   strcpy(localname,token.str);

   if(!gettoken(&inbuf," ",&token)) {
       progress = 0;
   }
   else progress = 1;


   if(!getkey("user/admin/uartnode",K0,0,0)) {
      outsok("Uart Node not available\n");
      return 0;
   }
 
   if(getkey("user/sys/waitf",K3,0,0)) {
      KC (K3,WaitF_Create) KEYSFROM(SB,M,SB) KEYSTO(K3);
   }
   else {
      KC (DOM,Domain_GetKey+K3) KEYSTO(,,K3); /* DK(0) */
   } 

   if(getkey(localname,K1,0,0)) {  /* have object */
      KC (K1,KT) RCTO(rc);
      if(rc != FS_AKT) {
          if(rc == KT+1) {
             getkey("user/sys/fsf",K1,0,0);
             KC (K1,0) KEYSFROM(SB,M,SB) KEYSTO(K1);
          }
          else {
             outsok("Can't overlay local name\n");
             return 0;
          }
      }
   }
   else {
      getkey("user/sys/fsf",K1,0,0);
      KC (K1,0) KEYSFROM(SB,M,SB) KEYSTO(K1);
   }

   KC (DOM,Domain_GetMemory) KEYSTO(K2);
   KC (K2,Node_Swap+TARSEGSLOT) KEYSFROM(K1);
   ptr=(char *)TARSEGADDR;
   rcount=(int *)TARSEGADDR;

   KC (K0,Node_Fetch+1) KEYSTO(K0);      /* port b */
   KC (K0,UART_MakeCurrentKey) KEYSTO(K0);  /* make current */
   KC (K0,UART_EnableInput);             /* enable reader */

/* Begin by sending file name and getting response */

   KC (K0,UART_PutDataGetResponse+2) CHARFROM(remotename,strlen(remotename))
        CHARTO(response,2,actlen) RCTO(rc);

   if(rc == 0xFFFFFFFF) {
      outsok("FileGet not enabled\n");
      KC (K1,KT+4) RCTO(rc);
      return 0;
   } 
   if(memcmp(response,&getfileok,2)) {
      outsok("Remote File not found\n");
      KC (K1,KT+4) RCTO(rc);
      return 0;
   } 

   counter=0;
   delay=100000;  /* .1 seconds */
   firstr=1;
   startrecord=1;
   previousrecord=0;
   while(1) {
      if(progress) {
         sprintf(remotename,"     %s - %d     \r",localname,(ptr - (char *)TARSEGADDR) );
         outsok(remotename);
      }
      if(!(counter %5)) {
         KC (K3,Wait_SetIntervalAndWait) STRUCTFROM(delay) RCTO(rc);
      }
      counter++;
      KC (K0,UART_SendRdyGetData+256) CHARFROM(&getfilerdy,2) CHARTO(response,256,actlen)
         RCTO(rc);
      if(rc == 0xFFFFFFFF) { /* timeout */
         outsok("Getfile Timeout\n");
         KC (K1,KT+4) RCTO(rc);
         return 0;
      }

      if(rc == GETFILEEOF) break;

      if(rc != 0) { /* some error */
         sprintf(remotename,"Getfile error RC=%x, actlen=%d\n",rc,actlen);
         outsok(remotename);
         KC (K1,KT+4) RCTO(rc);
         return 0;
      }

      if(actlen > 256) {
         outsok("Getfile too much data\n");
         KC (K1,KT+4) RCTO(rc);
         return 0;
      }
      /* have data */
      if(!type) {  /* binary is easy */
         memcpy(ptr,response,actlen);
         ptr += actlen;
      }
#ifdef xx
      else if(type==1) {  /* text */
         if(firstr) {
            (*rcount)=0;
            firstr=0;
            ptr += 4;
         }
         iptr=response;

         while(actlen) {
            if(startrecord) {
               if(previousrecord) {  /* must calculate length and store here */
                  length=ptr-previousrecord-4;
                  memcpy(previousrecord,&length,4);         
                  (*rcount)++;
               }
               previousrecord=ptr;
               ptr += 4;
               startrecord=0; 
            }
            if(*iptr == '\n') {  /* End of record */
               startrecord=1;
            }
            else {
               *ptr=*iptr;
               ptr++;
            }
            iptr++;
            actlen--; 
         }
      }
      else if(type==2) {  /* sorta binary */
         if(firstr) {
            (*rcount) = 1;
            ptr += 8;
            firstr=0;
         }
         memcpy(ptr,response,actlen);
         ptr += actlen;
      }
#endif
   }  /* received eof */
#ifdef xx
   if(type == 1) {  /* text closeout */
      if(previousrecord) {
          length=ptr-previousrecord-4;
          memcpy(previousrecord,&length,4);         
      }
      (*rcount)++;
   }
   if(type == 2) {
       length = ptr-(char *)TARSEGADDR - 8;  /* length of tar file */
       rcount++;  /* to record length */
       (*rcount) = length;
   }
#endif
   if(!type) {  /*  binary must set unix meta data */
      um.length = ptr-(char *)TARSEGADDR;
      um.groupid=0;
      um.userid=0;
      um.inode=1;
      um.mode=0x1ff;

      KC (K1,FS_SetMetaData) STRUCTFROM(um);
   }

   putkey(localname,K1,0,0);  /* add to directory */
   return 0; 
}
dountar()
{
   char name[256];
   struct FS_UnixMeta um;

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Key given\n");
      return 0;
   }
   if(!getkey(token.str,K0,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }
   if(!gettoken(&inbuf," ",&token)) {
      outsok("test/ assumed\n");
      strcpy(name,"test");
   }
   else {
      strcpy(name,token.str);
   }

   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   KC (K1,Node_Swap+TARSEGSLOT) KEYSFROM(K0);
   tartype=TARSEG;

   KC (K0,KT) RCTO(rc);
   if(rc == FS_AKT) {  /* possible Unix segment */
      KC (K0,FS_GetMetaData) STRUCTTO(um) RCTO(rc); 
      if(!rc && um.length) {
         tartype=TARUNIXSEG;
         tarlength=um.length;
      } 
   }
   untar(name);
}
 

docmdfile()
{
   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Key given\n");
      return 0;
   }
   if(!getkey(token.str,K3,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }
   if(getkey("user/sys/discrim",K0,0,0)) {
      KC (K0,0) KEYSFROM(K3) RCTO(rc);
      if(rc != 3) {  /* some form of memory */
          outsok("Cmdfile not memory\n");
          return 0;
      }
   }

   if(!getkey("user/sys/pcsf",K0,0,0)) {
      outsok("Cannot get PCS Factory\n");
      return 0;
   }
   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   KC (K1,Node_Fetch+CCKSLOT) KEYSTO(K1);
//   KC (DOM,Domain_Get+10) KEYSTO(K1);

   KC (K0,KT+5) KEYSFROM(SB,M,SB) KEYSTO(,,,K0) RCTO(rc);
   if(rc != KT+5) {
      outsok("PCSF not behaving correctly\n");
      return 0;
   }
   KC (K0,KT+5) KEYSFROM(,,K1) KEYSTO(,,,K0) RCTO(rc);
   if(rc != KT+5) {
      outsok("PCSF not behaving correctly\n");
      return 0;
   }
   KC (K0,5) KEYSFROM(DIR,K3) RCTO(rc);

   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   KC (K1,Node_Fetch+CCKSLOT) KEYSTO(K1);
   KC (K1,CCK_RecoverKeys) KEYSTO(SIK,SOK) RCTO(rc);
   KC (SOK,0) KEYSTO(,,,SOK) RCTO(rc);
   return 0;

#ifdef xx
   KC (DOM,Domain_GetMemory) KEYSTO(K0);
   KC (K0,Node_Swap+CMDSEGSLOT) KEYSFROM(K3);
   cmdaddr=(char *)CMDSEGADDR;

   cmdcnt=*((int *)cmdaddr);
   cmdaddr +=4;
#endif
   return 0;
}
doconsole() 
{
    if(!gettoken(&inbuf," ",&token)) {
       outsok("Console on  - starts console output\n");
       outsok("Console off - stops console output\n");
       return 0;
    }
    if(!strcmp(token.str,"on")) {
       consolesw=1;
    }
    else if(!strcmp(token.str,"off")) {
       consolesw=0;
    }
    else {
       outsok("console on|off\n");
    }
    return 0;
}


dotrace()
{
    if(!gettoken(&inbuf," ",&token)) {
       outsok("Trace on  - starts gate jump trace\n");
       outsok("Trace off - stops gate jump trace\n");
       return 0;
    }
//    KC (DOM,Domain_GetMemory) KEYSTO(K0);
//    KC (K0,Node_Fetch+CCKSLOT) KEYSTO(K0);
    getkey("conscck",K0,0,0);  /* get the base CCK not the branch CCK */
                               /* perhaps the branch CCK should pass this on */


    if(!strcmp(token.str,"on")) {
       KC (K0,CONCCK__START_LOG) RCTO(rc);
    }
    else if(!strcmp(token.str,"off")) {
       KC (K0,CONCCK__STOP_LOG) RCTO(rc);
    }
    else {
       outsok("Trace on|off\n");
    }
    return 0;
}

dotimeout()
{
    int i;
    long temp;

    if(!gettoken(&inbuf," ",&token)) {
        outsok("Must specify timeout value in seconds\n");
        return 0;
    }
    i=getnum(token.str,&temp);
    if(!i) {
        outsok("Timeout value conversion error\n");
        return 0; 
    }
    if(temp < 1) {
        outsok("Timeout value must be greater than 1 (second)\n");
        return 0;
    }
    setup.seconds=temp;

    KC(DOM,Domain_GetMemory) KEYSTO(K0);
    KC(K0,Node_Fetch+KEEPERSLOT) KEYSTO(K0);
//    KC (DOM,Domain_Get+11) KEYSTO(K0);

    KC(K0,0) STRUCTFROM(setup) RCTO(rc);

    return 0;
}
dokt()
  {
  char buf[32];

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Key given\n");
      return 0;
   }
   if(!getkey(token.str,K1,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }

   if(getkey("user/sys/discrim",K0,0,0)) {
      KC (K0,0) KEYSFROM(K1) RCTO(rc);
      if(rc == 2) {
          outsok("KT of Resume Key\n");
          return 0;
      }
   }
   KC (K1,KT) RCTO(rc);
   sprintf(buf,"RC=%0lX\n",rc);
   outsok(buf);
   return 0;
}

docheckpoint()
{
   long long tod;

   if(!getkey("user/checkpoint",K0,0,0)) {
     outsok("No Checkpoint Key\n");
     return 0;
   }
   if(!getkey("user/systimer",K1,0,0)) {
     outsok("No Systimer Key\n");
     return 0;
   }
   KC (K1,7) STRUCTTO(tod) RCTO(rc);
   if(rc) {
     outsok("Systimer RC not 0\n");
     return 0;
   }
   KC (K0,1) STRUCTFROM(tod) RCTO(rc);
} 

doedit()
{
   UINT32 sikrc;

   if(!gettoken(&inbuf," ",&token)) {
     outsok("No Key given\n");
     return 0;
   }
   if(!getkey(token.str,K1,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }
   if(!getkey("editor",K2,0,0)) {
      if(!getkey("user/sys/editf",K2,0,0)) {
         outsok("Can't find 'user.sys.editf'\n");
         return 0;
      }
      KC (K2,0) KEYSFROM(SB,M,SB) KEYSTO(K2) RCTO(rc);
      if(rc) {
         outsok("Can't build editor\n");
         return 0;
      }
      adddir("editor",K2,DIR,"A Text Editor",0);
   }
   KC (SB,0) KEYSTO(K3);               /* buy node */
   KC (K3,Node_Swap+1) KEYSFROM(K1);   /* the segment */
   KC (K2,0) KEYSFROM(SIK,SOK,K3);     /* sik sok node */
   KC (SB,1) KEYSFROM(K3);             /* sell node */

   KC(DOM,Domain_GetMemory) KEYSTO(SIK);
   KC(SIK,Node_Fetch+CCKSLOT) KEYSTO(SIK);  /* get keys from CCK */
   KC (SIK,CCK_RecoverKeys) KEYSTO(SIK,SOK) RCTO(sikrc);  /* ignore CCK for now */
   KC (SOK,0) KEYSTO(,,,SOK) RCTO(sikrc);

   return 0;
}
dodatakey()
{
    char name[256];
    char datavalue[16];

    if(!gettoken(&inbuf," ",name)) {
        outsok("No name\n");
        return 0;
    }   // the keyname
    if(!gettoken(&inbuf," ",&token)) {
        outsok("No string\n");
        return 0;
    }  // the string 

    if(*token.str == '%') {
        if(toupper(*(token.str+1))== 'A') {
           getascii(token.str+2,parm);
           parmlen=strlen(parm);
        }
        else if(toupper(*(token.str+1)) == 'X') {
           gethex(token.str+2,parm,&parmlen);
        }
        else {
           outsok("Invalid string spec\n");
           return 0;
        }
    }
    else {
       if(!getkeystr(token.str,parm,&parmlen)) {
           outsok("Can't find key\n");
           return 0;
       }
    } 

    if(!getkey("user/sys/dkc",K0,0,0)) {
        outsok("Can't find user/sys/dkc\n");
        return 0;
    }
    if(parmlen > 11) parmlen=11;
    memset(datavalue,0,16);
    memcpy(datavalue+16-parmlen,parm,parmlen);
    KC (K0,1) CHARFROM(datavalue,16) KEYSTO(K0) RCTO(rc);
    putkey(name,K0,0,0);
    return 0;
}

dojump(fork)
    int fork;
{
 unsigned int oc;
 UINT32 sikrc,callrc;
 char *ret;
 int i;
 
    KC (COMP,0) KEYSTO(,K1,K2,K3);
    KC (COMP,0) KEYSTO(,DIRSCRATCH);

    if(!gettoken(&inbuf," ",&token)) goto error;
    if(!getkey(token.str,K0,0,0)) {
        outsok("Key not found\n");
       return 0;
    }
    if(!gettoken(&inbuf," ",&token)) goto error;
    i=getnum(token.str,&oc);
    if(!i) goto error;
    if(!gettoken(&inbuf,")",&token)) goto docall;
    strcpy(parm,"");
    parmlen=0;
    if(*token.str != '(') goto error;
    token.ptr=&token.str[1];
    if(!gettoken(&token,",)",subtoken)) goto docall;  /* string */
    if(*subtoken) {
        if(*subtoken == '%') {
            if(toupper(*(subtoken+1))== 'A') {
               getascii(subtoken+2,parm);
               parmlen=strlen(parm);
            }
            else if(toupper(*(subtoken+1)) == 'E') {
               getascii(subtoken+2,parm);
               parmlen=strlen(parm);
            }
            else if(toupper(*(subtoken+1)) == 'X') {
               gethex(subtoken+2,parm,&parmlen);
            }
            else goto error;
        }
        else {
            if(!getkeystr(subtoken,parm,&parmlen)) goto error;
        }
    }
    if(!gettoken(&token,",)",subtoken)) goto docall;
    if(*subtoken) {
          if(!getkey(subtoken,K1,0,0)) goto error;
    }
    if(!gettoken(&token,",)",subtoken)) goto docall;
    if(*subtoken) {
          if(!getkey(subtoken,K2,0,0)) goto error;
    }
    if(!gettoken(&token,",)",subtoken)) goto docall;
    if(*subtoken) {
          if(!getkey(subtoken,K3,0,0)) goto error;
    }
    if(gettoken(&token,",)",subtoken)) {
extloop:
        if(fork) {
          if(*subtoken) {
            if(!getkey(subtoken,DIRSCRATCH,0,0)) goto error;
            goto docall;
          }
        }
        KC (K0,KT+5) KEYSFROM(K1,K2,K3) KEYSTO(,,,K0) RCTO(rc);
        KC (COMP,0) KEYSTO(,K1,K2,K3);
        if(*subtoken) {if(!getkey(subtoken,K1,0,0)) goto error;}
        if(!gettoken(&token,",)",subtoken)) goto docall;
        if(*subtoken) {if(!getkey(subtoken,K2,0,0)) goto error;}
        if(!gettoken(&token,",)",subtoken)) goto docall;
        if(*subtoken) {if(!getkey(subtoken,K3,0,0)) goto error;}
        if(gettoken(&token,",)",subtoken)) goto extloop;
    }
docall:
    retstr[0]=0;
    retstr[4095]=0;
    if (fork) {
       LDEXBL (K0,oc) CHARFROM(parm,parmlen) KEYSFROM(K1,K2,K3,DIRSCRATCH);
       FORKJUMP();
       return 0;
    }
    KC (K0,oc) CHARFROM(parm,parmlen) KEYSFROM(K1,K2,K3)
      CHARTO(retstr,4096,retlen) KEYSTO(K0,K1,K2,K3) RCTO(callrc);
    retstr[retlen]=0;
/*
   recover SIK,SOK in case we gave them to someone

   Yes this is costly but it is safe  WILL BE SET ASSIDE for a while
   SHOULD USE DISCRIM on SIK and SOK and only recover if not present
*/

   if(getkey("user/sys/discrim",TEMPDIR,0,0)) {
      UINT32 t1,t2;
      KC(TEMPDIR,0) KEYSFROM(SOK) RCTO(t1);
      KC(TEMPDIR,0) KEYSFROM(SIK) RCTO(t2);
      if((t1+t2) != 4) {
          KC (DOM,Domain_GetMemory) KEYSTO(SOK);
          KC (SOK,Node_Fetch+CCKSLOT) KEYSTO(SOK);
          KC (SOK,CCK_RecoverKeys) KEYSTO(SIK,SOK) RCTO(sikrc);
          KC (SOK,0) KEYSTO(,,,SOK) RCTO(sikrc);
      }
    }

    if(callrc) {
       sprintf(parm,"RC = %0lX\n",callrc);
       outsok(parm);
    }

    if(!gettoken(&inbuf," ",&token)) return 0;
    if(*token.str != '(') goto error;
    token.ptr=&token.str[1];
    if(!gettoken(&token,",)",subtoken)) return 0;  /* string */
    if(*subtoken) {
        ret=subtoken;
        while(*ret) {
            if(*ret == '%') {
                ret++;
                if(toupper(*ret) == 'A')
                   putascii(retstr,parm);
                else if(toupper(*ret) == 'E')
                   putascii(retstr,parm);
                else if(toupper(*ret) == 'X')
                   puthex(retstr,parm,retlen);
                else goto error;
                ret++;
            }
            else {
                putkeystr(ret,retstr,retlen);
                break;
            }
            outsok(parm);
            outsok("\n");
        }
    }
    if(!gettoken(&token,",)",subtoken)) return 0;
    if(*subtoken) {
          putkey(subtoken,K0,0,0);
    }
    if(!gettoken(&token,",)",subtoken)) return 0;
    if(*subtoken) {
          putkey(subtoken,K1,0,0);
    }
    if(!gettoken(&token,",)",subtoken)) return 0;
    if(*subtoken) {
          putkey(subtoken,K2,0,0);
    }
    if(!gettoken(&token,",)",subtoken)) return 0;
    if(*subtoken) {
          putkey(subtoken,K3,0,0);
    }
    return 0;
error:
    outsok("Error\n");
    return 0;
}
gettoken(blk,delimit,tok) /* returns 1 while tokens 0 at end */
    struct scanblk *blk;
    char *delimit,*tok;
{
    char *begin;
 
    begin=tok;
    *tok=0;
    while(*blk->ptr && (*blk->ptr != '#')) {
        if(*blk->ptr != ' ') break;
        blk->ptr++;
    }
    while(*blk->ptr && (*blk->ptr != '#')) {
        if(strchr(delimit,*blk->ptr)) {
            blk->ptr++;
            break;
        }
        else {
            *tok++=*blk->ptr++;
        }
    }
    *tok=0;
    if(*begin) return 1;
    if(*blk->ptr && (*blk->ptr != '#')) return 1;
    return 0;
}
getascii(str1,str2)
    char *str1,*str2;
{
    strcpy(str2,str1);
    return 0;
}
putascii(str1,str2)
    char *str1,*str2;
{
    strcpy(str2,str1);
    return 0;
}
puthex(str1,str2,len)
    char *str1,*str2;
    int len;
{
    static char hextab[]="0123456789ABCDEF";
    int left,right;
    int wordc,linec;
 
    wordc=0;
    linec=0;
    while(len) {
        left=(*str1>>4) & 0x0F;
        right=(*str1 & 0x0F);
        *str2++=hextab[left];
        *str2++=hextab[right];
        str1++;
        len--;
        wordc++;
        if(wordc==4) { /* did a word */
            *str2++=' ';
            wordc=0;
            linec++;
            if(linec==8) { /* 8 words per line (64 bytes) */
               *str2++='\n';
               linec=0;
            }
        }
    }
    *str2=0;
    return 0;
}
gethex(str1,str2,len)
    unsigned char *str1;
    char *str2;
    int *len;
{
    int slen;
    
    slen=strlen((char *)str1);
    if(slen % 2) slen++;
    *len=slen/2;
    while(slen) {
      if(*str1 & 0x10) {  /* number */
         *str2=(*str1 & 0x0F) << 4;
      }
      else  {  /* letter */
         *str2=((*str1 & 0x0F) +9) << 4;
      }
      slen--;
      *str1++;
      if(*str1 & 0x10) {  /* number */
         *str2 |= (*str1 & 0x0F);
      }
      else  {  /* letter */
         *str2 |= ((*str1 & 0x0F) +9);
      }
      slen--;
      str1++;
      str2++;
    }
    return 0;
}
getkeystr(str1,str2,len)
    char *str1,*str2;
    int *len;
{
    return getkey(str1,DIRSCRATCH,str2,len);
}
putkeystr(keyname,str2,len)
    char *keyname,*str2;
    int len;	
{
    int i;
 
    i=putkey(keyname,COMP,str2,len);
    return 0;
}
getkey(str1,slot,des,len)
    char *str1;
    KEY slot;
    char *des;
    int *len;
{
    char portion[256],*ptr;
    int i;
    char *tptr;

    if(!strcmp(str1,"sik")) {
      KC (DOM,Domain_GetKey+SIK) KEYSTO(slot);
      return 1;
    }
    if(!strcmp(str1,"sok")) {
      KC (DOM,Domain_GetKey+SOK) KEYSTO(slot);
      return 1;
    }
    if(!strcmp(str1,"cck")) {
      KC (DOM,Domain_GetMemory) KEYSTO(slot);
      KC (slot,Node_Fetch+CCKSLOT) KEYSTO(slot);
      return 1;
    }
    KC (DOM,Domain_GetKey+DIR) KEYSTO(slot);

    if(!strcmp(str1,"directory/")) {    /* may need to pass this to an object */
      return 1;
    }
 
kloop:
    strcpy(portion,str1);
    ptr=str1;
    if( (tptr=strchr(str1,'/')) ) {
        str1=tptr+1;
        portion[str1-ptr]=0;
        i=fetchkey(portion,slot,des,len); /*new dir key to user's slot */
        if(!i) return 0;
        goto kloop;
    }
    if(*portion) {
      i=fetchkey(portion,slot,des,len);  /* get key to user's slot */
    }
    else i=1;
    return i;
}
fetchkey(portion,slot,des,len)
    char *portion;
    KEY slot;
    char *des;
    int *len;
{
   int rclen,nl; 

   *rcname=strlen(portion);
   rcname[1]=0;
   strcat(rcname,portion);
   KC (slot,TDO_GetEqual) CHARFROM(rcname,strlen(rcname))
     KEYSTO(slot) CHARTO(rcname,4096,rclen) RCTO(rc);
   if(rc==1) {
      if(des) {
         nl=rcname[0];
         rclen=rclen-(nl+1);
         *len=rclen;
         memcpy(des,&rcname[nl+1],rclen);
         des[rclen]=0;
      } 
      return 1;
   }
   return 0;
}
putkey(str1,slot,des,deslen)
    char *str1;
    KEY slot;
    char *des;
    int deslen;
{
    char portion[256],*ptr;
    int i;
    char *tptr;
 
    KC (DOM,Domain_GetKey+DIR) KEYSTO(TEMPDIR);
 
kloop:
    strcpy(portion,str1);
    ptr=str1;
    if((tptr=strchr(str1,'/')) ) {
        str1=tptr+1;
        if(!(*str1)) goto dostash; /* ends with '/' */
        portion[str1-ptr]=0;
        KC (DOM,Domain_GetKey+TEMPDIR) KEYSTO(DIRSCRATCH);
        i=fetchkey(portion,TEMPDIR,0,0); 
        if(!i) {
            KC (COMP,COMPRCC) KEYSTO(TEMPDIR);
            KC (TEMPDIR,1) KEYSFROM(SB,M,SB) KEYSTO(TEMPDIR);
            adddir(portion,TEMPDIR,DIRSCRATCH,0,0);
        }
        else {
            /* ..  KT the user's slot.  if not a directory do the above */
        }
        goto kloop;
    }
dostash:
    i=stashkey(portion,slot,des,deslen,TEMPDIR);  /* get key to user's slot */
    return i;
}
stashkey(portion,slot,des,deslen,dirslot)
    char *portion;
    KEY slot;
    KEY dirslot;
    char *des;
    int deslen;
{
    adddir(portion,slot,dirslot,des,deslen);
    return 1;
}
 getnum(str,num)
    char *str;
    unsigned long *num;
{
    char *str1;
    char *tptr;
    unsigned int rc;

    if(strcspn(str,"0123456789k")) return 0;  // not a digit or k of kt

    rc=(unsigned)cvtnum(str);
    str1=str+1;
    for(;;) {
        if (tptr=strchr(str1,'+')) {
            str1=tptr+1;
            rc=rc+(unsigned)cvtnum(str1);
        }
        else if (tptr=strchr(str1,'-')) {
            str1=tptr+1;
            rc=rc-(unsigned)cvtnum(str1);
        }
        else break;
    }
    *num=rc;
    return 1;
}
unsigned cvtnum(str)
    char *str;
{
    unsigned int i;

    if(!strncmp(str,"kt",2)) return 0x80000000lu;
// return (unsigned)atoi(str);
    i=(unsigned long)strtol(str,0,0);  /* converts decimal octal and hex */
    return i;
}

docounters()
{
    int nbytes;
    unsigned long rc;
    long long diftimer,nowtimer;
    int *counter,*oldcounter;
    int odd,diff,cfd,isoldbuf,ncount,ntime,i,j,k;
    long long *timer,*oldtimer;
    char n10[32];
    char obuf[100];

    if(!getkey("user/admin/peek",K0,0,0)) {
       outsok("Cannot find PEEK key\n");
       return 0;
    }

    isoldbuf=0;
    KC (K0,2) CHARTO(buf,4096,nbytes) RCTO(rc);
    if(rc) {
       outsok("Peek key, bad return code\n");
       return 0;
    }
    if(nbytes==oldnbytes) isoldbuf=1;

    if(nbytes > 8) {
       timer=(long long *)buf;
       counter=(int *)buf;
       counter += 2;
       ncount=(*counter);
       counter += 1;
       ntime=(*counter);
       counter += 1;
 
       if(isoldbuf) {
         oldtimer=(long long *)oldbuf;
         oldcounter=(int *)oldbuf;
         oldcounter += 2;
         oldcounter += 2;
       }
       else oldcounter=counter;
       diftimer=0;

       if(isoldbuf) {
          diftimer=(*timer) - (*oldtimer);
       }
       sprintf(obuf,"Counters (%d)\n\n",ncount);
       outsok(obuf);
       nowtimer=(*timer);
       nowtimer=nowtimer/4096;
       nowtimer=nowtimer/1000000;

       diftimer=diftimer/4096;
       diftimer=diftimer/1000000;

       i=nowtimer;
       j=diftimer;
       sprintf(obuf,"Time   %d/%d\n",i,j);
       outsok(obuf);
       odd=0;
       for(i=0;i<ncount;i++) {
          diff=0;
          if(isoldbuf) diff=*counter-*oldcounter;
	  strcpy(n10,(char *)counter+4);
          while((int)strlen(n10) < 28) strcat(n10," ");
          sprintf(obuf," %28s %6d/%6d",n10,*counter,diff);
          outsok(obuf);
          counter += 8;
          if(isoldbuf) oldcounter += 8;
          odd++;
          if(odd == 2) {
             sprintf(obuf,"\n");
             outsok(obuf);
             odd=0;
          }
       }   
       sprintf(obuf,"\n");
       outsok(obuf);

       timer=(long long *)counter;
       if(isoldbuf) oldtimer=(long long*)oldcounter;
       sprintf(obuf,"Timers (%d)\n",ntime);
       outsok(obuf);
       for(i=0;i<ntime;i++) {
          diftimer=0;
          if(isoldbuf) {
             diftimer=(*timer)-(*oldtimer);
          }
	  strcpy(n10,(char *)counter+8);
          while((int)strlen(n10) < 28) strcat(n10," ");
          
       nowtimer=(*timer);
       nowtimer=nowtimer/4096;
       nowtimer=nowtimer/1000000;
       diftimer=diftimer/4096;
       diftimer-diftimer/1000000;

       j=nowtimer;
       k=diftimer;
          sprintf(obuf," %28s %6d/%6d\n",n10,j,k);
          outsok(obuf);
          counter += 10;
          if(isoldbuf) oldcounter += 10;
          timer=(long long *)counter;
          if(isoldbuf) oldtimer=(long long*)oldcounter;
       } 
    }
    memcpy(oldbuf,buf,4096);
    oldnbytes=nbytes;

    return 0;
}
donewbank()
{
   char bankname[64];

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Name given\n");
      return 0;
   }
   strcpy(bankname,token.str);

   if(gettoken(&inbuf," ",&token)) {  /* parent bank named */
      if(!getkey(token.str,K0,0,0)) {
          outsok("Parent bank key not found\n");
          return 0;
      }    
      KC (K0,SB_CreateBank) KEYSTO(K0); 
   }
   else {
      KC (SB,SB_CreateBank) KEYSTO(K0);
   }
   putkey(bankname,K0,"Named Bank",10);
}

donewmeter()
{
   char metername[64];

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Name given\n");
      return 0;
   }
   strcpy(metername,token.str);

   KC (SB,SB_CreateNode) KEYSTO(K0);  /* new meter node */

   if(gettoken(&inbuf," ",&token)) {  /* parent bank named */
      if(!getkey(token.str,K1,0,0)) {
          outsok("Parent meter key not found\n");
          return 0;
      }    
      KC (K0,Node_Swap+1) KEYSFROM(K1);
   }
   else {
      KC (K0,Node_Swap+1) KEYSFROM(M);
   }
   KC (K0,Node_WriteData) STRUCTFROM(meternkv);
   KC (K0,Node_MakeMeterKey) KEYSTO(K1);
   putkey(metername,K1,"Named Meter",11);
   strcat(metername,"node");
   putkey(metername,K0,"Named Meter Node",16);
}

dospace()
{

/* used because SB_FullStatistics uses LLI */

   struct  {
     long long nodesbought;
     long long nodessold;
     long long pagesbought;
     long long pagessold;
   } sbdata;
   char buf[100];
   long nodes,pages;

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Key given\n");
      return 0;
   }
   if(!getkey(token.str,K1,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }
 
   KC (K1,SB_QueryStatistics) STRUCTTO(sbdata) RCTO(rc);
   nodes=sbdata.nodesbought-sbdata.nodessold;
   pages=sbdata.pagesbought-sbdata.pagessold;
   sprintf(buf,"%d Nodes and %d Pages\n",nodes,pages);
   outsok(buf);

}

doreadmeter()
{
   unsigned char data[16];
   long long cpu;
   long long starting;
   long long dif;
   long long dominst,domcycle,kerninst,kerncycle;
   int secs,micros;
   char buf[100];

   if(!gettoken(&inbuf," ",&token)) {
      outsok("No Key given\n");
      return 0;
   }
   if(!getkey(token.str,K1,0,0)) {
      outsok("Key unknown\n");
      return 0;
   }

   memset(data,0,16);
   memset(&data[9],255,7);
   memcpy((char *)&starting,&data[8],8);

/* K1 has the meter key */

   KC (K1,Node_Fetch+3) KEYSTO(K2) RCTO(rc);
   if(rc) {
      outsok("Not a NodeKey\n");
      return 0;
   }

   KC (K2,1) CHARTO(data,16) RCTO(rc);

   memcpy((char *)&cpu,&data[8],8);
   dif=starting-cpu;
   dif = dif/16;
   secs= dif/1000000;
   micros = dif - (secs*1000000);
   sprintf(buf,"CPU time used %d.%06d seconds\n",secs,micros);
   outsok(buf); 

   KC (K1,Node_Fetch+6) KEYSTO(K2);
   KC (K2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)&dominst,&data[8],8);
   KC (K1,Node_Fetch+10) KEYSTO(K2);
   KC (K2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)&domcycle,&data[8],8);
   KC (K1,Node_Fetch+11) KEYSTO(K2);
   KC (K2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)&kerninst,&data[8],8);
   KC (K1,Node_Fetch+12) KEYSTO(K2);
   KC (K2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)&kerncycle,&data[8],8);

   sprintf(buf,"Domain Instructions/Cycles %lld/%lld\n",dominst,domcycle);
   outsok(buf);
   if(dominst) {
     dominst *= 100;
     dif = dominst/domcycle;
     secs = dif/100;
     micros = dif -(secs*100);   
     sprintf(buf,"    IPC %d.%02d\n",secs,micros);
     outsok(buf);
   }
   sprintf(buf,"Kernel Instructions/Cycles %lld/%lld\n",kerninst,kerncycle);
   outsok(buf);
   if(kerninst) {
     kerninst *= 100;
     dif = kerninst/kerncycle;
     secs = dif/100;
     micros = dif -(secs*100);   
     sprintf(buf,"    IPC %d.%02d\n",secs,micros);
     outsok(buf);
   }

    return 0;
}

void trap_function(jp,atc,exttrap,p2)
    jmp_buf jp;
    UINT32 atc,exttrap;
    UINT32 *p2;
{
    *p2=exttrap;
   
     longjmp(jp,atc);
} 

char toupper(char a)
{
   if(a >= 'a' && a <= 'z') a = a & ~0x20;
   return a;
}
untar(dn) /* TARSEGADDR -> K3 */
     char *dn;
{
    struct tarhdr *tptr;
    int filesize;
    char *p,*basep, *endp;
    long size,rawsize;
    int mode,i;
    char dirname[256]; 
    int inode;
   
    inode = 2;     /* starting Inode for this "device" */

    strcpy(dirname,dn);
    i=strlen(dirname);
    if(dirname[i-1] == '/') dirname[i-1]=0;  /* no trailing slash */

    {
       char dname[256];
    
       strcpy(dname,dirname);
       strcat(dname,"/");
       makedir(dname,inode,1);
       inode++;
    }
    
    KC (COMP,Node_Fetch+COMPFSF) KEYSTO(K0);
    KC (K0,FSF_Create) KEYSFROM(SB,M,SB) KEYSTO(K0);
    {
       char fsname[256];
       char des[256];
      
       strcpy(fsname,dirname);
       strcat(fsname,"_SegKeeper");
       strcpy(des,"Master Segment Keeper");
       putkey(fsname,K0,des,strlen(des));
       KC (DOM,Domain_GetMemory) KEYSTO(K1);
       KC (K1,Node_Swap+UNTARFS) KEYSFROM(K0);
       havefs=1;
    } 

    if(tartype == TARSEG) {
       filesize = *(int *)(TARSEGADDR+4);
       basep     = (char *)(TARSEGADDR+8);
       endp = basep+filesize;
    }
    if(tartype == TARUNIXSEG) {
       filesize = tarlength;
       basep     =(char *)TARSEGADDR;
       endp = basep+filesize;
    }

    tptr = (struct tarhdr *)basep;

    while(1) {

//       outsok(tptr->name,strlen(tptr->name));
//       outsok("\n");

       if(!tptr->typeflag) break;  /* padding at end of tar file */

       rawsize=strtol(tptr->size,0,8);
       mode=strtol(tptr->mode,0,8);

       if(tptr->typeflag == '0') {  /* a real file */
       }
       else if(tptr->typeflag == '5') {   /* directory */
          char dname[512];

          strcpy(dname,dirname);
          strcat(dname,"/");
          if(tptr->name[0] == '.') {
              strcat(dname,tptr->name+2);
          }
          else {
              strcat(dname,tptr->name);
          }
          makedir(dname,inode,0);
          inode++;

          tptr++;
          continue;
       }
       else if (tptr->typeflag == '2') {  /* symbolic link */
          tptr++;
          continue;
       }
       else if (tptr->typeflag == '1') {  /* link */
          tptr++;
          continue;
       }
       else {
          tptr++;
          continue;
       }
       if(!*tptr->lnk) {
          size=rawsize+511;
          size=size/512;
          size=size*512;
          p=(char *)tptr;

          putfile(tptr->name,p+sizeof(struct tarhdr),rawsize,mode,dirname,inode);

          p += size;
          if(p >= endp) break;
          tptr=(struct tarhdr *)p;
          tptr++;
          inode++;
       }
    }		
}
makedir(name,inode,root)
   char *name;
   int inode;
   int root;
{
   struct FS_UnixMeta um;
   int len;
   int rc;

   memset(&um,0,sizeof(um));
   um.inode=inode;
   um.mode = 0x41FF;  /* directory with rwx for everybody */
   um.length = 512;    /* dummy 1 directory block */
   KC (COMP,Node_Fetch+COMPRCC) KEYSTO(K0);
   KC (K0,TDOF_CreateNameSequence) KEYSFROM(SB,M,SB) KEYSTO(K0);
   KC (K0,TDO_WriteUserData) STRUCTFROM(um);
   putkey(name,K0,0,0); 
   strcat(name,".");
   putkey(name,K0,0,0);
   strcat(name,".");

   if(!root) { /* must get parent key */
      char dname[512];
      char *ptr;

      strcpy(dname,name);
      ptr=&dname[strlen(dname)-4];
      while(ptr > dname) {
         if(*ptr == '/') { 
             *(ptr+1)=0;
             break;
         }
         ptr--;
      }
      rc=getkey(dname,K1,0,0);
      putkey(name,K1,0,0);
   } 
   else {
      putkey(name,K0,0,0);
   }   
}
putfile(name,place,size,mode,dirname,inode)
   char *name,*place;
   long size;
   int mode;
   char *dirname;
   int inode;
{
   char dir[256],element[256];
   char *ptr;
   int  doascii;
   int  doit;
   struct FS_UnixMeta um;

//   char buf[256];

   doit=0;
   doascii=0;
   getpath(name,dir,element);

//   sprintf(buf,"PUTFILE %s/[%s]\n",dir,element);
//   outsok(buf,strlen(buf));

   if(ptr = strchr(element,'.')) { /* not executable */
      if(!strncmp(ptr,".so",3)) {
           doit=1;
      }
      if(!strcmp(ptr,".cmd") || !strcmp(ptr,".ck")) {
           doit=1;
//           doascii=1;
      }
      if(!strcmp(ptr,".class")) {
           doit=1;
      }
      if(!strcmp(ptr,".zip")) {
           doit=1;
      }
      if(!strcmp(ptr,".jar")) {
           doit=1;
      }
      if(!strcmp(ptr,".conf")) {
           doit=1;
      }
   }
   else {
      if(mode & 0x49) {
        doit=1;
      }
   }
   if(!doit) return 0;

//   sprintf(buf,"   Mode %d\n",doascii);
//   outsok(buf,strlen(buf));
   
   KC (DOM,Domain_GetMemory) KEYSTO(K1);
   if(!havefs) {
      KC (COMP,Node_Fetch+COMPFSF) KEYSTO(K0);
      KC (K0,FSF_Create) KEYSFROM(SB,M,SB) KEYSTO(K0);
   }
   else {
      KC (K1,Node_Fetch+UNTARFS) KEYSTO(K0);
      KC (K0,FS_CreateSibling) KEYSFROM(SB) KEYSTO(K0);
   }
   KC (K1,Node_Swap+FILEOUTSLOT) KEYSFROM(K0);

   strcpy(dir,dirname);
   if(name[0] == '.') {
       strcat(dir,name+1);   /* skip the . of ./  */
   }
   else {
      strcat(dir,"/");
      strcat(dir,name);
   }

   memset(&um,0,sizeof(um));
   um.inode=inode;
   um.length=size;
   um.mode=0x81FF;  /* rwx for everybody regular file */
   if(doascii) {
       asci2ftseg(place,size);
   }
   else {                          /* executable */
       memcpy((char *)FILEOUTADDR,place,size);      
       KC (K0,FS_SetMetaData) STRUCTFROM(um);
   }

   putkey(dir,K0,0,0);
    
}

asci2ftseg(place,size)
     char *place;
     int  size;
{
     int rlen;
     char *start;
     char *out = (char *)FILEOUTADDR+4;
     int nrec;

     start=place;
     nrec=0; 
     rlen=0;
     while(size) {
        if(*place == '\n') {
           memcpy(out,&rlen,4);
           memcpy(out+4,start,rlen);
           out=out+4+rlen;
           rlen=-1;
           nrec++;
           start=place+1;
        }
        rlen++;
        place++;
        size--;
     }  
     if(rlen) {
         memcpy(out,&rlen,4);
         memcpy(out+4,start,rlen);
     }
     memcpy((char *)FILEOUTADDR,&nrec,4);
}

/* take the value in name and separate to 'dir' and 'element' */
getpath(name,dir,element)
   char *name,*dir,*element;
{
   char *tptr,*slashptr;
   int len;
  
   slashptr=0;

   tptr=name;
   while(tptr=strchr(tptr,'/')) {
      slashptr=tptr;
      tptr++;
   }
   if(slashptr) {
      strcpy(element,slashptr+1);
      len=slashptr-name;
      strncpy(dir,name,len);   
      *(dir+len)=0;
   }
   else {
      *dir=0;
      strcpy(element,name);
   }
}
