/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*         TITLE 'LSF CODE TO BUILD/DESTROY MEMORY TREE FOR KEYNIX' */
/********************************************************************/
/********************************************************************/
/**************  These are the C subroutines for the LSFSIMS.SK *****/
/**************  code which is the base for the LSF Simulator *******/
/********************************************************************/
/********************************************************************/
 
/*  THIS CODE BUILDS A MEMORY TREE BASED ON AN ELF FILE */
/*  COPYING THE DATA AREA AND POINTING TO THE CODE AREA */
 
/* RESTRICTION!!  AT THIS TIME THIS CODE HANDLES ONLY 1 MEG OF CODE */
/*                AND DATA SPACE EITHER COPIED OR NOT */
 
/* THE RESULTANT MEMORY TREE FOR 2 hdr types IS ... */
/*    000000-0FFFFF   CODE/DATA MAY BE COPIED DEPENDING ON SWITCH */
/*      Data may extend up to 8FFFFFF  */
/*    900000-9FFFFF   STACK  */
/*     SLOT 10        Reserved  for SBRK function in domain 
/*     SLOT 11        Reserved  for SBRK function in domain
/*     SLOT 12        BACKGROUND KEY */
/*     SLOT 13        THE BUILD/DESTROY CODE */
/*     SLOT 14        DATAKEY WITH start of unallocated memory */
/*                    and Stack  bottom (used by SBRK )  */
/*     SLOT 15        FORMAT KEY  */
 
#define LSEGFORMAT 15
#define LSEGDATA 14
#define LSEGME 13
#define LSEGBASE 12
#define LSEGSTACK 9

#define LSFBUILD 0
#define LSFAOUT 1
#define LSFRO 2
#define LSFFSC 3
#define LSFSTACK 4

/***************************************************************/
/*                                                             */
/*  The LSFSIM Node description                                */
/*  This may actually be a fetcher factory                     */
/*                                                             */
/*      Slot 0  - LSFSIM code (elf loader unloader)            */
/*           1  - ELF File to load                             */
/*           2  - ELF File to load                             */
/*           3  - Fresh Segment Creator                        */
/*           4  - Data key with stack size                     */
/*           5                                                 */
/*                                                             */
/***************************************************************/

#include "keykos.h"
#include <sys/elf.h>
#include "kktypes.h"
#include "sb.h"
#include "node.h"
/***************************************************************** */
 
  KEY CALLER=2;
  KEY DOMKEY=3;     /* MY DOMAIN KEY */
  KEY SB=4;         /* SPACEBANK */
  KEY M=5;          /* METER  */
  KEY DOMCRE=6;
  KEY LSFNODE=7;    /* FETCHER OR SENSEKEY ON BUILD */
  KEY LSS4=8;       /* LEAVE THESE SLOTS ALONE FOR AUX2SUBR */
  KEY LSS3=9;
  KEY REDROOT=10;
  KEY BUILDER=11;   /* BUILDER SEGMENT KEY (ME) */
  KEY AOUT=12;      /* THE A.OUT FILE */
  KEY PAGE=13;
  KEY ROOT=14;      /* THIS BECOMES THE MEMORY OF THE DOMAIN */
/*                       AND ON DESTROY SEQUENCE IS THE MEMORY TREE */
 
/*********************************************************************/
/*  This code is based on SVR4 (Elf) standards and SPARC ABI         */
/*********************************************************************/ 

#define FILEPTR 0x10000000
 
         Elf32_Ehdr *ehdr = (Elf32_Ehdr *)FILEPTR;  /* at 1 meg */
buildls()
{
  JUMPBUF;

  static struct Node_DataByteValue db3={3};
  static struct Node_DataByteValue db4={4};
  static UINT32 format = 0x0FFCFFA5;
  Elf32_Phdr *phdr;

  char *dataptr;
  UINT32 datastart,dataend,npages,nbss;
  UINT32 dataoffset,byteslastdata;
  UINT32 backkey,rc;
  int i,stacksize,stackbottom,n3slot,n4slot,oldn4slot,n5slot,oldn5slot;
  int ndatapages;

  struct Node_KeyValues nkv;
  Node_KeyData stk={0};
  
  KALL (SB, SB_CreateNode) KEYSTO(REDROOT);
  memset(&nkv.Slots[0],0,Node_KEYLENGTH);
  memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-4],&format,4);
  nkv.StartSlot=LSEGFORMAT;
  nkv.EndSlot=LSEGFORMAT;
  KALL (REDROOT, Node_WriteData) STRUCTFROM(nkv,Node_KEYLENGTH+8);
  KALL(REDROOT, Node_Swap+LSEGBASE) KEYSFROM (AOUT); /* BACKGROUND KEY */
  KALL(REDROOT, Node_Swap+LSEGME) KEYSFROM (BUILDER); /* FOR DESTRUCTION */
  KALL(REDROOT, Node_MakeNodeKey) KEYSTO (REDROOT); /* MUST GET TO 14 */

  KALL(SB, SB_CreateNode) KEYSTO (LSS4); /* 1 meg code space */
  KALL(LSS4, Node_MakeNodeKey) STRUCTFROM(db4) KEYSTO (LSS4); 
  KALL(REDROOT, Node_Swap+0) KEYSFROM (LSS4); /* at location 0 */
 
/* position phdr to first loadable section (ie text) */

  phdr=(Elf32_Phdr *)(FILEPTR + ehdr->e_phoff);
  while (PT_LOAD != phdr->p_type) phdr++;

  npages=((phdr->p_vaddr & 0xFFF) + phdr->p_filesz + 4095) >> 12;

  n5slot=0;
  n4slot=(phdr->p_vaddr >> 16) & 0xF;   /* only look at 1 nibble */
  n3slot=(phdr->p_vaddr >> 12) & 0xF;   /* really likely to be zero */

  KALL(SB, SB_CreateNode) KEYSTO (LSS3);
  KALL(LSS3, Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO (LSS3);
  KALL(LSS4, Node_Swap+n4slot) KEYSFROM (LSS3);

  backkey=0x0000000F;   /* background over address 0 */
 
  for(i=0;i<npages;i++) {
    nkv.StartSlot=n3slot;
    nkv.EndSlot=n3slot;
    memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-4],&backkey,4);
    KALL (LSS3,Node_WriteData) STRUCTFROM(nkv,Node_KEYLENGTH+8);
    n3slot++;
    if(n3slot==16) {
       n4slot++;
       if(n4slot==16) {
           n5slot++;
           if(n5slot == LSEGSTACK) crash("Code too large");
           KALL (SB,SB_CreateNode) KEYSTO(LSS4);
           KALL (LSS4,Node_MakeNodeKey) STRUCTFROM(db4) KEYSTO(LSS4);
           KALL (REDROOT,Node_Swap+n5slot) KEYSFROM(LSS4);
           n4slot=0;
       }
       KALL(SB, SB_CreateNode) KEYSTO (LSS3);
       KALL(LSS3, Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO (LSS3);
       KALL(LSS4,Node_Swap+n4slot) KEYSFROM (LSS3);
       n3slot=0;
    }
    backkey=backkey+0x00001000;   /* next page */
  }   /* map all text pages to background key */

/* now copy the data pages */

  oldn4slot = n4slot;   /* save this for check of start of data */
  oldn5slot = n5slot;   /* save this for check of start of data */

  phdr++;  /* to data section */

  dataptr=(char *)(phdr->p_offset & 0xFFFFF000) + FILEPTR;  /* round down */
  datastart=(UINT32)phdr->p_vaddr;
  dataend=datastart+phdr->p_memsz;
  byteslastdata=(phdr->p_vaddr+phdr->p_filesz) & 0xFFF; 
  dataend=(dataend+4095) & 0xFFFFF000;

  dataoffset=(UINT32)phdr->p_vaddr & 0xFFF; /* must count overlap in size */

  npages=(phdr->p_memsz + dataoffset + 4095) >> 12;
  ndatapages=(phdr->p_filesz + dataoffset + 4095) >> 12;  /*  same if small bss */

  if(npages) {  /* if we need a data section (hdh) */

    n3slot=(datastart >> 12) & 0x0F;
    n4slot=(datastart >> 16) & 0x0F;
    n5slot=(datastart >> 20) & 0x0F;   /* start address of data */
   
    if(n5slot != oldn5slot) { /* if we have nearly 1 meg of code */
      KALL (SB,SB_CreateNode) KEYSTO(LSS4);
      KALL (LSS4,Node_MakeNodeKey) STRUCTFROM(db4) KEYSTO(LSS4);
      KALL (REDROOT,Node_Swap+n5slot) KEYSFROM(LSS4);
    }

/* may start in new LSS4 node */
/* even if not, SPARC ABI will start data in new LSS3 node */

    if((oldn5slot != n5slot) || (oldn4slot != n4slot)) { /* need new LSS3 */
      KALL (SB,SB_CreateNode) KEYSTO(LSS3);
      KALL (LSS3,Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO(LSS3);
      KALL (LSS4,Node_Swap+n4slot) KEYSFROM(LSS3);
    }

    for(i=1;i <= npages;i++) {  /* buy the data pages only copy ndatapages worth */  
      KALL (SB,SB_CreatePage) KEYSTO(PAGE);
      KALL (LSS3,Node_Swap+n3slot) KEYSFROM(PAGE);
      if(i <= ndatapages) {  /* this is a data page (not bss) */
        if(i==ndatapages) {
             KALL(PAGE,4096) CHARFROM(dataptr,byteslastdata);
        }
        else {
             KALL(PAGE,4096) CHARFROM(dataptr,4096);  /* copy data */
        }
        dataptr+=4096;  /* bump data pointer */
      }
      n3slot++;
      if(n3slot==16) {
         n4slot++;
         if(n4slot==16) {
           n5slot++;
           if(n5slot == LSEGSTACK) crash("Data too large");
           KALL (SB,SB_CreateNode) KEYSTO(LSS4);
           KALL (LSS4,Node_MakeNodeKey) STRUCTFROM(db4) KEYSTO(LSS4);
           KALL (REDROOT,Node_Swap+n5slot) KEYSFROM(LSS4);
           n4slot=0;
         }
         KALL (SB,SB_CreateNode) KEYSTO(LSS3);
         KALL (LSS3,Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO(LSS3);
         KALL (LSS4,Node_Swap+n4slot) KEYSFROM(LSS3);
         n3slot=0;
      }
    }   /* end of data page loop */
  }   /* needed data pages  */
	
/* now build the stack segment  maximum of 1 meg  */

  KALL (SB,SB_CreateNode) KEYSTO(LSS4);
  KALL (LSS4,Node_MakeNodeKey) STRUCTFROM(db4) KEYSTO(LSS4);
  KALL (REDROOT,Node_Swap+LSEGSTACK) KEYSFROM(LSS4);
  KALL (SB,SB_CreateNode) KEYSTO(LSS3);
  KALL (LSS3,Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO(LSS3);
  KALL (LSS4,Node_Swap+15) KEYSFROM(LSS3);
  
  n4slot=15;
  n3slot=15;
 
  KALL (LSFNODE,Node_Fetch+LSFSTACK) KEYSTO(PAGE);  /* this is datakey */
  KALL (PAGE,1) STRUCTTO(stk) RCTO(rc);
  memcpy(&stacksize,&stk.Byte[Node_KEYLENGTH-4],4);
  if(!stacksize) stacksize=4096;
  stacksize=((stacksize + 4095) & 0xFFFFF000);
  npages=stacksize>>12;
  
  for(i=0;i<npages;i++) {
    KALL(SB,SB_CreatePage) KEYSTO(PAGE);
    KALL(LSS3,Node_Swap+n3slot) KEYSFROM(PAGE);
    if(i == (npages-1)) break;
    n3slot--;
    if(n3slot<0) {   /* end of 64K segment */
      n4slot--;
      if(n4slot < 0) crash("Stack over 1 meg");
      KALL (SB,SB_CreateNode) KEYSTO(LSS3);
      KALL (LSS3,Node_MakeNodeKey) STRUCTFROM(db3) KEYSTO(LSS3);
      KALL (LSS4,Node_Swap+n4slot) KEYSFROM(LSS3);
      n3slot=15;
    }
  }
  stackbottom=(LSEGSTACK+1)*0x100000 - stacksize;
  memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-8],&stackbottom,4);
  memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-4],&dataend,4);
  nkv.StartSlot=LSEGDATA;
  nkv.EndSlot=LSEGDATA;
  KALL (REDROOT,Node_WriteData) STRUCTFROM(nkv,Node_KEYLENGTH+8);
  return 0;
}
 
/*  LOOP THROUGH REDROOT and for each LSS4 NODE up through LSFSTACK */ 
/*  LOOP THROUGH LSS4, FOR EACH LSS3 LOOP AND SELL ANY PAGE */
zapls()
{
  JUMPBUF;

  int n3slot,n4slot,n5slot;
  UINT32 rc;

  for(n5slot=0;n5slot <= LSEGSTACK;n5slot++) {
    KALL(REDROOT,Node_Fetch+n5slot) KEYSTO(LSS4) RCTO(rc);
    if(rc) break;  /* this means that REDROOT is not a node */
    KALL(LSS4,KT) RCTO(rc);
    if(Node_NODEAKT != rc) continue;  /* skip non node */
    for(n4slot=0;n4slot<16;n4slot++) {
      KALL(LSS4,Node_Fetch+n4slot) KEYSTO(LSS3) RCTO(rc);
      if(rc) break;   /* stack page gets nz rc here */
      KALL(LSS3,KT) RCTO(rc);
      if(Node_NODEAKT != rc) continue;  /* skip non node */
      for(n3slot=0;n3slot<16;n3slot++) { 
        KALL(LSS3,Node_Fetch+n3slot) KEYSTO(PAGE) RCTO(rc);
        if(rc) break;  /* not a node */
/* a lot of these keys are window keys - SB will reject */
        KALL(SB,SB_DestroyPage) KEYSFROM(PAGE) RCTO(rc);
      }
      KALL(SB,SB_DestroyNode) KEYSFROM(LSS3) RCTO(rc);
    }
    KALL(SB,SB_DestroyNode) KEYSFROM(LSS4) RCTO(rc);
  }
  KALL(SB,SB_DestroyNode) KEYSFROM(REDROOT) RCTO(rc);
  return 0;
}
memcpy(a,b,n)
   char *a,*b;
   int n;
{
   int i;
   for(i=0;i<n;i++) *a++=*b++;
}
memset(a,c,n)
   char *a;
   char c;
   int n;
{ 
   int i;
   for(i=0;i<n;i++) *a++=c;
}
