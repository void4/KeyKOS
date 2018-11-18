/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports BSLOAD 
*************************************************************/
#include "keykos.h"
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "factory.h"
#include "consdefs.h"
#include "bsload.h"
#include "node.h"
#include "setjmp.h"
#include <sys/elf.h>
#include "ocrc.h"


   KEY COMP        = 0;
#define COMPCONSOLE   15

   KEY SB          = 1;    /* Space bank parameter */
   KEY CALLER      = 2;
   KEY DOMKEY      = 3;
   KEY PSB         = 4;
   KEY METER       = 5;
   KEY DC          = 6;
   KEY MEMNODE     = 7;   /* root segment node */
 
   KEY CODESEG      = 8;
   KEY MEMSEG       = 9;
  
   KEY MASTERDOM    =10;
   KEY MASTERKEEPER =11;

   KEY CONSOLE     = 12;
 
   KEY K2          = 13;
   KEY K0          = 14;
   KEY K1          = 15;

    char title[]="BSLOADF ";

#define FILEPTR  0x10000000
#define CODEPTR  0x20000000

void trap_function();
int  fork();
int  exit();
 
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)FILEPTR;
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc,type;
   struct Node_DataByteValue ndb = {7};
   jmp_buf jump_buffer;
   struct Bsload_StartAddr sa;
   UINT32 offset;
   int i;

   Elf32_Phdr *phdr;  /* program header */

//   KC (COMP,COMPCONSOLE) KEYSTO(CONSOLE);
//   KC (CONSOLE,0) KEYSTO(,CONSOLE) RCTO(rc); 
  
   KC (DOMKEY,Domain_GetKey+DOMKEY) KEYSTO(MASTERDOM);  /* For keeper */

   if(!(rc=fork())) {
/******************* Begin Keeper ***********************************/
       struct Domain_SPARCRegistersAndControl drac;
       UINT32 errcode;
       UINT32 oc,rc;
       char buf[64];

       KC (DOMKEY,Domain_MakeStart) KEYSTO(K0,,CALLER);  /* zap caller */
       KC (MASTERDOM,Domain_SwapKeeper) KEYSFROM(K0) KEYSTO(MASTERKEEPER);

       for(;;) {
          LDENBL OCTO(oc) KEYSTO(,,,CALLER) STRUCTTO(drac);
          RETJUMP();

 // sprintf(buf,"Keeper oc = %X\n",oc);
 // KC (CONSOLE,0) CHARFROM(buf,strlen(buf)) RCTO(rc);

          if(oc == 0) {
              LDEXBL (CALLER,errcode) STRUCTFROM(drac);
          }
          else if (oc == 4 ) {
             KC (MASTERDOM,Domain_SwapKeeper) KEYSFROM(MASTERKEEPER);
             exit();
          }
          else {
             errcode = drac.Control.TRAPCODE;
             drac.Control.PC=(int)trap_function;
             drac.Control.NPC=drac.Control.PC+4;
             drac.Regs.o[0]=(int)jump_buffer;
             drac.Regs.o[1]=errcode;
             LDEXBL (MASTERDOM,Domain_ResetSPARCStuff) KEYSFROM(,,,CALLER)
                 STRUCTFROM(drac);
          }
       }
   } 
   if(rc > 1) {  /* failure of fork */
       exit(NOSPACE_RC);
   }
/**************** End keeper  **************************************/

/* upgrade memory size */

   KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
   KC (SB,SB_CreateNode) KEYSTO(K1);
   KC (K1,Node_Swap+0) KEYSFROM(K0);
   KC (K1,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(K1);
   KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K1);   

   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);

   LDEXBL (CALLER,0) KEYSFROM(K0);
   for (;;) {
     offset=0;
     LDENBL OCTO(oc) STRUCTTO(offset) KEYSTO(CODESEG,MEMSEG,,CALLER);
     RETJUMP();
 
     if (oc == KT) {
          LDEXBL (CALLER,Bsload_AKT);
          continue;
     }

     if (oc == DESTROY_OC) { /* die die */
/* dump keeper */
        KC (DOMKEY,Domain_GetMemory) KEYSTO(K1);
        KC (K1,Node_Fetch+0) KEYSTO(K0);
        KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K0);
        KC (SB,SB_DestroyNode) KEYSFROM(K1);

        KC (DOMKEY,Domain_GetKeeper) KEYSTO(K1);
        KC (K1, 4) RCTO(rc);  /* kill keeper */

        break;
     }
     if ((oc == Bsload_LoadSimpleElf) || (oc == Bsload_LoadDynamicElf)) {

        KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
        KC (K0,Node_Swap+1) KEYSFROM(CODESEG);
        KC (K0,Node_Swap+2) KEYSFROM(MEMSEG);

        if((rc = setjmp(jump_buffer))) {
            LDEXBL (CALLER,NOSPACE_RC);   /* assume won't fit */
            continue; 
        }
        sa.sa=ehdr->e_entry+offset;
        phdr=(Elf32_Phdr *)(FILEPTR + ehdr->e_phoff);

        for(i=0;i<ehdr->e_phnum;i++) {
           if  (PT_LOAD == phdr->p_type) {  /* load all loadable sections */

               if(oc == Bsload_LoadDynamicElf) {
                   if(phdr->p_vaddr+offset < sa.sa) {
                       sa.sa=phdr->p_vaddr+offset;  // set base address for dynamic loader
                   }
               }

               memcpy((char *)(CODEPTR+phdr->p_vaddr+offset),(char *)(FILEPTR + phdr->p_offset), phdr->p_filesz);
           }
           phdr++;
        } 

        LDEXBL(CALLER,OK_RC) STRUCTFROM(sa);
        continue;
     }
  }
}

void trap_function(j,k)
    jmp_buf j;
    int k;
{
    longjmp(j,k);
}
  

