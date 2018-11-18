/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include "domain.h"
#include "ukeeper.h"
#include "node.h"
#include "sb.h"
#include "page.h"
#include "factory.h"
#include "ocrc.h"
#include "unixf.h"

#include <sys/elf.h>
#include <sys/auxv.h>

    KEY  COMP    = 0;
/* COMPNODE is a node with data keys describing the details needed by Unix support */
#define COMPNODE      0
#define COMPDIRECTORY 1
#define COMPCOPY      3
/* COMPENV  is a page key with a set of environment strings  A=B */
#define COMPENV       4

    KEY  CALLER  = 2;
    KEY  DOMKEY  = 3;
    KEY  PSB     = 4;
   
    KEY  SIK     = 8;
    KEY  SOK     = 9;
    KEY  DIRECTORY = 10;

    KEY  K2      = 13;
    KEY  K1      = 14;
    KEY  K0      = 15;

    char env1[] = "HOME=/";
    char env2[] = "FREEZEDRY=YES";

    char title[]="UWRAPPER"; 

    JUMPBUF;

    struct UKeeper_Name ukn;
    struct Node_DataByteValue ndb7={7};

    int  frozen = 0;

    int  fixedenv = 0;
    char *pageaddr = (char *)0x10000000;
    char envarray[4096];
    char argsarray[4096];
    char buf[4096];   /* receive args and env strings */
    int  _start();
    int  thaw();

uwrapper(foc,ord)
    UINT32 foc,ord;
{
    UINT32 *args;
    UINT32 oc,rc;
    UINT32 mainoffset,ldstart,ldbase;
    char *ptr;
    unsigned long d[4];  // read data key here
    int  actlen,i;
    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdr;
    unsigned long brkaddress;
    UINT32 *nargs;
    int (*restartptr)();

    if(!frozen) {
         memset(argsarray,0,4096);
         memset(envarray,0,4096);
    }

/* must see if there is an environment args component.  If so copy the data to the */
/* envarray and set the fixedenv flag.  Will have to temporarily map the component page */

    KC (COMP,Node_Fetch+COMPENV) KEYSTO(K2);
    KC (K2,KT) RCTO(rc);
/* DISCRIM would be really handy here, unless we require a RO Page Key */
    if((rc == Page_ROAKT) && !frozen) {   /* yep have to step up memory and map page */
                                          /* sure would be nice to have a Page_Read call!! */
        mapK2();
        memcpy(envarray,pageaddr,4096);
        unmapK2();
        fixedenv = 1;
    }

    KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
    LDEXBL (CALLER,0) KEYSFROM(K0);
    for(;;) {
       LDENBL OCTO(oc) CHARTO(buf,4096,actlen) KEYSTO(SIK,SOK,K1,CALLER);
       RETJUMP();

       if(oc == KT) {
           LDEXBL (CALLER,0x999);
          continue;
       }

       if(oc == UNIX_SetEnv) { /* set environment strings */
          if(fixedenv) {   /* if set by factory then cannot do this */
             LDEXBL(CALLER,INVALIDOC_RC);
             continue;
          }
          if(actlen > 4095) actlen=4095;
          memcpy(envarray,buf,actlen);

          LDEXBL(CALLER,OK_RC);
          continue;
       } 

       if(oc == UNIX_MakeFrozenEnvFactory) {  /* create copy factory with environment page */
           if(!*envarray) { /* hey, no strings here */
               LDEXBL(CALLER,INVALIDOC_RC);
               continue;
           }
/* SIK,SOK,K1   is PSB,,SB */
           KC (SIK,SB_CreatePage) KEYSTO(K2) RCTO(rc);
           if(rc) {
                LDEXBL(CALLER,NOSPACE_RC);
                continue;
           }
           mapK2();
           memcpy(pageaddr,envarray,4096);
           unmapK2();
           KC (K2, Page_MakeReadOnlyKey) KEYSTO(K2);
           KC (COMP, Node_Fetch+COMPCOPY) KEYSTO(K0);
           KC (K0, FactoryC_Copy) KEYSFROM(SIK,,K1) KEYSTO(K0);
           KC (K0, FactoryB_InstallSensory+COMPENV) KEYSFROM(K2);
           KC (K0, FactoryB_MakeRequestor) KEYSTO(K1) RCTO(rc);
           if(rc) {
               KC (SIK,SB_DestroyPage) KEYSFROM(K2) RCTO(rc);
               LDEXBL (CALLER,NOSPACE_RC);
               continue;
           }
           LDEXBL (CALLER,OK_RC) KEYSFROM(K1,K0); 
           continue;
       }
       if(oc == UNIX_AddDevice) {  /* set the /dev/tty device key in keeper */
          buf[actlen]=0;
          KC (DOMKEY,Domain_GetKeeper) KEYSTO(K0);
          KC (K0,UKeeper_AddDevice) CHARFROM(buf,actlen+1) KEYSFROM(SIK,,DOMKEY);
          LDEXBL (CALLER,OK_RC);
          continue;
       }

/* Once the following calls (oc = 0, 256, 42, 256+42) This object will be busy or GONE */
/* one never needs to worry about parsing the environment vector twice so it is ok to  */
/* put the zero bytes into the string to terminate each env string */

       if((oc & 0xFF) == 0 || (oc & 0xFF) == 42) {  /* get stuff from node and run */

          KC (DOMKEY,Domain_GetKeeper) KEYSTO(K0);

          KC (K0,UKeeper_SetSikSok) KEYSFROM(SIK,SOK,DOMKEY);
          KC (DOMKEY,Domain_GetKey+K1) KEYSTO(DIRECTORY);   /* save for later */

          if(oc & 0x100) {  /* truss hack */
             KC (K0,UKeeper_TrussOn) KEYSFROM(,,DOMKEY);
          }

/* we will want to check for an extended jump request as part of thaw here */
/* this will be based on parameters set at freeze.  We will have to ask the keeper */
/* for these parameters */

          if(frozen) {
             KC (K0,UKeeper_SetDirectory) KEYSFROM(DIRECTORY,,DOMKEY);  /* re-establish mapping */
             thaw();  /* this will return to the freezedry request (or open()) */
          }

          if(actlen > 4095) actlen=4095;
          memcpy(argsarray,buf,actlen);

          KC (COMP,Node_Fetch+COMPNODE) KEYSTO(K0); 
          KC (K0,Node_Fetch+0) KEYSTO(K1);
          KC (K1,1) CHARTO(buf,16) RCTO(rc);
          buf[16]=0; // terminate name string
          buf[17]='A';
          buf[18]=0; 
          ptr=buf;
          while(!*ptr) ptr++;
          strcpy(ukn.name,ptr);
          KC (K0,Node_Fetch+1) KEYSTO(K1);
          KC (K1,1) CHARTO(d,16) RCTO(rc);
          mainoffset=d[3];
          KC (K0,Node_Fetch+2) KEYSTO(K1);
          KC (K1,1) CHARTO(d,16) RCTO(rc);
          ukn.length=d[3];
          KC (K0,Node_Fetch+3) KEYSTO(K1);
          KC (K1,1) CHARTO(d,16) RCTO(rc);
          ldbase=d[3];
          KC (K0,Node_Fetch+4) KEYSTO(K1);
          KC (K1,1) CHARTO(d,16) RCTO(rc);
          ldstart=d[3];

          KC (DOMKEY,Domain_GetKeeper) KEYSTO(K0);
          KC (K0,UKeeper_SetName) STRUCTFROM(ukn) KEYSFROM(,,DOMKEY);
          restartptr=_start;
          KC (K0,UKeeper_SetRestartAddr) STRUCTFROM(restartptr) KEYSFROM(,,DOMKEY);
          brkaddress=(unsigned long)&frozen;
          KC (K0,UKeeper_SetFrozenAddr) STRUCTFROM(brkaddress) KEYSFROM(,,DOMKEY);

          ehdr = (Elf32_Ehdr *)mainoffset;
          phdr = (Elf32_Phdr *)((char *)ehdr + ehdr->e_phoff);
          brkaddress=0;
          for(i=0;i<ehdr->e_phnum;i++) {
              if(phdr->p_type & PT_LOAD) {
                   if(phdr->p_vaddr+phdr->p_memsz > brkaddress)  {
                         brkaddress=phdr->p_vaddr+phdr->p_memsz;
                   }
              }
              phdr++;
          }
          KC (K0,UKeeper_SetBrk) STRUCTFROM(brkaddress) KEYSFROM(,,DOMKEY);
          KC (K0,UKeeper_Init) KEYSFROM(,,DOMKEY);  // initialize tables
          KC (K0,UKeeper_SetDirectory) KEYSFROM(DIRECTORY,,DOMKEY);  /* must be after init */

          if(foc == 42 || (oc & 0xFF)  == 42) {
             KC (K0,UKeeper_FreezeDryHack) KEYSFROM(,,DOMKEY);
          }

/* tell keeper about the brk address calculated from elf header at mainoffset */

          args = (UINT32 *)0x0F000000;   /* just above stack */
          nargs = args;
          *args++ = 1;   /* argc will have to parse passed string */
          *args++ = (UINT32)ukn.name;
// must parse and add args here 
          if(actlen) {
              char *ptr,*eptr;
              ptr=argsarray;
              while(*ptr) {
                 *args++ = (UINT32)ptr;
                 *nargs = *nargs + 1; 
                 if(eptr=(char *)strchr(ptr,' ')) {
                     *eptr=0;
                     ptr=eptr+1;
                     while(*ptr) {
                        if(*ptr != ' ') break;
                     }
                     if(!*ptr) break;
                 } 
                 else break;
              }
          }
          *args++ = 0;     /* end of args */

          if(!*envarray) {  /* no environment strings so force one */
              *args++ = (UINT32)env1;  
          }
          if(foc == 42 || (oc & 0xFF) == 42) {
              *args++ = (UINT32)env2;
          }

/* now put other env pointers in as in arguments */

          actlen=strlen(envarray);
          if(actlen) {
              char *ptr,*eptr;
              ptr=envarray;
              while(*ptr) {
                 *args++ = (UINT32)ptr;
                 if(eptr=(char *)strchr(ptr,' ')) {
                     *eptr=0;
                     ptr=eptr+1;
                     while(*ptr) {
                        if(*ptr != ' ') break;
                     }
                     if(!*ptr) break;
                 } 
                 else break;
              }
          }

          *args++ = 0;     /* end of ENV */
 
/* now the aux vector */
/* GET THIS STUFF FROM THE ELFHEADER AT mainoffset */
          *args++ = AT_PHDR;
          *args++ = mainoffset + ehdr->e_phoff;
          *args++ = AT_PHENT;
          *args++ = ehdr->e_phentsize;
          *args++ = AT_PHNUM;
          *args++ = ehdr->e_phnum;
          *args++ = AT_ENTRY;
          *args++ = ehdr->e_entry;
          *args++ = AT_BASE;
          *args++ = ldbase;
          *args++ = AT_FLAGS;
          *args++ = 0;
          *args++ = AT_PAGESZ;
          *args++ = 4096;
          *args++ = AT_SUN_UID;
          *args++ = 0;
          *args++ = AT_SUN_RUID;
          *args++ = 0;
          *args++ = AT_SUN_GID;
          *args++ = 0;
          *args++ = AT_SUN_RGID;
          *args++ = 0;
          *args++ = AT_NULL;
          *args++ = 0;
          
          return (ldstart);   /* return to bounce pass */
       }
   }
}
mapK2()
{
    KC (PSB,SB_CreateNode) KEYSTO(K0);
    KC (DOMKEY,Domain_GetMemory) KEYSTO(K1);
    KC (K0,Node_Swap+0) KEYSFROM(K1);
    KC (K0,Node_MakeNodeKey) STRUCTFROM(ndb7) KEYSTO(K0);
    KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K0);
    KC (K0,Node_Swap+1) KEYSFROM(K2);
}
unmapK2()
{
    KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
    KC (K0, Node_Fetch+0) KEYSTO(K1);
    KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K1);
    KC (PSB,SB_DestroyNode) KEYSFROM(K0);
}

