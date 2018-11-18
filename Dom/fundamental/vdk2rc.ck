/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports the VDK2RC  Virtual Domain Creator
  that stores domain keys in a record collection

  KC VDK2RCF(0;SB,M,SB,RC => rc;DOMKEEP) Silent domain keeper
             which records the domain key in RC and exits
             waiting for the next fault.  Domains with busted
             meters have their resume keys forked with KT+10

  KC VDK2RCF(3;SB,M,SB,SIKCCK => rc;DOMKEEP) GDB domain keeper
             which uses the SIK key to communicate with a
             remote GDB

             This version will only handle 1 fauling domain at
             a time.  There is only 1 serial port.   A second
             fault will stall.  There is no GDB way to break
             a domain from a stalled key (or fault).

  This is defined in the manual as requiring a CLOCK key to
  time stamp the records in the record collection.  This is
  not enabled in this abbreviated version

*************************************************************/

/*************************************************************
  LINKING STYLE: 1 RO Section - NO RW Global storage
*************************************************************/

#include "keykos.h"
#include "kktypes.h"
#include "tdo.h"
#include "lli.h"
#include "domain.h"
#include "node.h"
#include "page.h"
#include <signal.h>
#include <sb.h>
#include "kuart.h"
#include "ocrc.h"
 
   KEY COMP        = 0;
   KEY SB          = 1;    /* Space bank parameter */
   KEY CALLER      = 2;
   KEY DOMKEY      = 3;
   KEY PSB         = 4;
   KEY METER       = 5;
   KEY DC          = 6;
   KEY RC          = 7;
   KEY DISCRIM     = 8;
   KEY HISDOM      = 9;
   KEY HISKEEPER   = 10;
   KEY HISMEMORY   = 11;
 
   KEY K2          = 13;
   KEY K1          = 14;
   KEY K0          = 15;
 
#define COMPDISCRIM  0
#define COMPFSC      1
#define COMPCALLSEGF 2

#define COMPCONSOLE  15
 
    char title[]="VDK2RC  ";
    int stacksiz = 4096 * 6;
    int dogdb();
    int handle_exception();
    LLI *ctod();
    int probe();
    void sellpages();

#define hismemory 0x10000000

    char *memoryerror = (char *)0x00200000;  /* will be in the lss 5 node */

static const char hexchars[]="0123456789abcdef";

#define BUFMAX 2048

/* Number of bytes of registers.  */

#define NUMREGS 72
#define NUMREGBYTES (NUMREGS * 4)
enum regnames {G0, G1, G2, G3, G4, G5, G6, G7,
		 O0, O1, O2, O3, O4, O5, SP, O7,
		 L0, L1, L2, L3, L4, L5, L6, L7,
		 I0, I1, I2, I3, I4, I5, FP, I7,

		 F0, F1, F2, F3, F4, F5, F6, F7,
		 F8, F9, F10, F11, F12, F13, F14, F15,
		 F16, F17, F18, F19, F20, F21, F22, F23,
		 F24, F25, F26, F27, F28, F29, F30, F31,
		 Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR };
 
UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc;
   UINT32 type;
   int initialized = 0;
   unsigned char db6=6;
   unsigned char db7=7;
 
   struct {
      char len;
      LLI tod;
   } rcparm;
   struct {
      UCHAR   fnt,resv1;
      UINT16  id,ver,resv2;
      UINT32  rights,nrec;
   } ktdata;
 
   if (factoc != KT+5) return KT+2;
   KC (CALLER,KT+5) KEYSTO(RC,,,CALLER) RCTO(rc);
   type = rc;

#ifdef xx
/* do this after the fork */
   KC (SB,SB_CreateNode) KEYSTO(K0);
   KC (DOMKEY,Domain_GetMemory) KEYSTO(K1);
   KC (K0,Node_Swap+0) KEYSFROM(K1);
   KC (K0,Node_MakeNodeKey) CHARFROM(&db6,1) KEYSTO(K0);
   KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K0);             /* now have 16 megabyte slots */
#endif

   KC (COMP,Node_Fetch+COMPDISCRIM) KEYSTO(DISCRIM);
   KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
   LDEXBL (CALLER,0) KEYSFROM(K0);
   for (;;) {
     LDENBL OCTO(oc) KEYSTO(,,HISDOM,CALLER);
     RETJUMP();
     if (oc == KT) {
        LDEXBL (CALLER,0x30A0Dul);
        continue;
     }
     if (oc < KT) {
        if(oc == 4) {  // destruction
           break;  // exit loop, exit factory, die
        }
        LDEXBL (CALLER,KT+2);
        continue;
     }
     if ((oc && 0xFFFFFF00) == 0x80000400) {  // bad meter (sparc)
        KC (HISDOM,Domain_GetKey+2) KEYSTO(K1);
        KC (DISCRIM,0) KEYSFROM(K1) RCTO(rc);
        if(rc == 2) {
           LDEXBL (K1,KT+10);
           FORKJUMP();
        }
      }
 
      if(type == 3) { /* this is a GDB style */
         LDEXBL (CALLER,KT+2);
         continue;              /* disable this turkey as it can hang the system */
                                /* caller will get a DK(0) for a keeper */
/********************************************************************************
      A Basic GDB remote Domain Keeper for debugging.  Only a single
      port which must be read syncchronously in the kernel (no interrupts)
      exists so no attempt is made to multiplex
********************************************************************************/

	 if(!(rc=fork())) {  /* This becomes the domain keeper key of the faulting domain */
              static UINT32 format = 0x0FFCFF17;   /* Backround in 12 Lss = 7, Initial slots 1 */
              struct Node_KeyValues nkv;
              unsigned long backvalue;

/************************************************************************************
   initialization code here. Make me his keeper, set up his memory 
   Memories come in many flavors.  This keeper simple sets an error flag and
   skips the offending instruction.  It is up to the GDB stub to check for
   the error and take appropriate action. 
**************************************************************************************/
               KC(DOMKEY,Domain_GetKey+DOMKEY) KEYSTO(HISKEEPER);  /* my domain key for keeper to install self */ 
               KC(SB,SB_CreatePage) KEYSTO(HISMEMORY);  /* this is a communication page for the keeper */

               if(!(rc=fork())) {  /* a keeper that simply skips memory faults */

/*************************************************************************************
  Of course I need a keeper to protect agains memory faults.  This is it.  We share
  a page that the keeper marks whenerver a faulting instruction is skipped
*************************************************************************************/
                    struct Domain_SPARCRegistersAndControl drac;
                    UINT32 rc,oc;

                    KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
                    KC (K0,Node_Swap+2) KEYSFROM(HISMEMORY) KEYSTO(,,CALLER);  /* comm page and clear caller */

                    KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
                    KC (HISKEEPER,Domain_SwapKeeper) KEYSFROM(K0);  /* install as keeper of keeper */

                    LDEXBL (CALLER,0);  /* this is DK(0) */
                    for(;;) {
                        LDENBL OCTO(oc) STRUCTTO(drac) KEYSTO(,,HISDOM,CALLER);
                        RETJUMP();
 
                        if(oc == 4) {
                           exit(0);   /* not my place to destroy page */
                        }

                        drac.Control.PC=drac.Control.NPC;  /* skip offending instruction */
                        drac.Control.NPC=drac.Control.PC+4;

                        *memoryerror = 1;   /* mark page with error */

                        LDEXBL(HISDOM,Domain_ResetSPARCStuff) KEYSFROM(,,,CALLER) STRUCTFROM(drac); 
                    }
               }
               if(rc > 1) {   /* run without for now */
               }
/**************************************************************************************
               Now continue setup of target domain's keeper (me)
**************************************************************************************/
               KC(DOMKEY,Domain_GetMemory) KEYSTO(K0);
               KC(K0,Node_Swap+2) KEYSFROM(HISMEMORY);  /* communication page at 0x00200000 */

/* now bump me up to a lss = 7 memory node */
               KC (SB,SB_CreateNode) KEYSTO(K0);
               KC (DOMKEY,Domain_GetMemory) KEYSTO(K1);
               KC (K0,Node_Swap+0) KEYSFROM(K1);
               KC (K0,Node_MakeNodeKey) CHARFROM(&db7,1) KEYSTO(K0);
               KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K0);             /* now have 256 megabyte slots */

               KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
               KC (HISDOM,Domain_SwapKeeper) KEYSFROM(K0) KEYSTO(HISKEEPER); /* install me as keeper */

               KC (HISDOM,Domain_GetMemory) KEYSTO(HISMEMORY);
/*
    Ok, now that I have a keeper (a keeper's keeper) we can construct the replacement memory 
    node for the domain to be kept.  This node is also put into my memory at 0x10000000 so
    that the gdb stub can reference it.  There will be a Probe(addr,write) that will expand
    the node so that read/write page keys can replace background window keys
  
    The destruction code must walk this tree selling pages before this keeper quits
*/
               KC (SB,SB_CreateNode) KEYSTO(K0);  /* this becomes his new memory node */
               memset(&nkv.Slots[0],0,Node_KEYLENGTH);
               memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-4],&format,4);
               nkv.StartSlot=15;
               nkv.EndSlot=15;
               KC (K0,Node_WriteData) STRUCTFROM(nkv,Node_KEYLENGTH+8);
               KC (K0,Node_Swap+12) KEYSFROM(HISMEMORY);
               backvalue=0x00000003;  /* a background data key for address zero */
               memcpy(&nkv.Slots[0].Byte[Node_KEYLENGTH-4],&backvalue,4);
               nkv.StartSlot=0;
               nkv.EndSlot=0;
               KC (K0,Node_WriteData) STRUCTFROM(nkv,Node_KEYLENGTH+8);
               KC (K0,Node_MakeNodeKey) KEYSTO(K0);   /* lss=0 */

               KC (HISDOM,Domain_SwapMemory) KEYSFROM(K0);  /* replace his memory */
               KC (DOMKEY,Domain_GetMemory) KEYSTO(K1);
               KC (K1,Node_Swap+1) KEYSFROM(K0);   /* Install for mapping */


//               KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
//               KC (K0,Node_Swap+1) KEYSFROM(HISMEMORY);  /* just for read only test */


/* 
   Now after mapping his memory, lets look for the program name 
*/
               KC (COMP,COMPCONSOLE) KEYSTO(K0);
               KC (K0,0) KEYSTO(,K0) RCTO(rc);
               {
                  char namebuf[128];
                  int found = 0;
                  int offset = 0;
                  char *vaddr;

                  strcpy(namebuf,"\nDomain FAULT - Use GDB on [name] with target remote /dev/term/a -> ");
                  KC (K0,0) CHARFROM(namebuf,strlen(namebuf)) RCTO(rc);
                
                  *memoryerror=0;
                  while(offset < 200) {
                      if(!strncmp(hismemory+offset,"FACTORY",7)) {
                         found=1;
                         break;
                      }
                      if(*memoryerror) break;
                      offset++;
                  }
                  if(!found) {
                      offset=0x10000;
                      *memoryerror=0;
                      while(offset < 0x10200) {
                          if(!strncmp(hismemory+offset,"FACTORY",7)) {
                             found=1;
                             break;
                          }
                          if(*memoryerror) break;
                          offset++;
                      }
                  }
                  strcpy(namebuf,"UNKNOWN");
                  if(found) {
                      *memoryerror=0;
                      memcpy(&vaddr,(char *)hismemory+offset+12,4);
                      if(!*memoryerror) {
                          strncpy(namebuf,vaddr+hismemory,8);
                          namebuf[8]=0;
                      }
                  }
                  KC(K0,0) CHARFROM(namebuf,strlen(namebuf)) RCTO(rc);
                  KC(K0,0) CHARFROM("\n\n",2) RCTO(rc);
 
                  goto firsttime;
               }
               for (;;) {
                   LDENBL OCTO(oc) KEYSTO(,,HISDOM,CALLER); /* we could get the stuff here... */
                   RETJUMP();
firsttime:
                   if(oc == KT) {
                      LDEXBL (CALLER,0x31A0Dul);
                      continue;
                   }
                   if(oc < KT+2) {
                      LDEXBL (CALLER,KT+2);
                      continue;
                   }

                   if(dogdb()) { /*  non-zero return means to disconnect */
                       
/******************************************************************************

At this point GDB has requested a disconnect.  The old keeper has been
restored, the old memory has been restored, and the domain has been reset

First put my memory back to Lss=5 so destruction code will work at exit()

******************************************************************************/

                        KC (HISDOM,Domain_SwapMemory) KEYSFROM(HISMEMORY);  /* put back original memory */
                        KC (HISDOM,Domain_SwapKeeper) KEYSFROM(HISKEEPER);  /* and keeper */

                        KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
                        KC (K0,Node_Fetch+0) KEYSTO(K1);  /* original lss=5 */
                        KC (K0,Node_Fetch+1) KEYSTO(HISMEMORY);    /* the created mirror node */

                        KC (K1,Node_Fetch+2) KEYSTO(K2);       /* the communication page */
                        KC (SB,SB_DestroyPage) KEYSFROM(K2) RCTO(rc);

                        KC (DOMKEY,Domain_SwapMemory) KEYSFROM(K1);  /* put back Lss = 5 node */
                        KC (SB,SB_DestroyNode) KEYSFROM(K0) RCTO(rc); /* lss = 6 node */
/*
    now must walk the tree (in HISMEMORY) to sell pages
*/
                        sellpages();

                        KC (DOMKEY,Domain_GetKeeper) KEYSTO(K0);
                        KC (K0,4) RCTO(rc);  /* dump my keeper */

                        exit(0);  /* This Exits to the CALLER key with oc = exit code */   
                   }

                   LDEXBL (CALLER,0);  /* the domain was reset */
                }
         }  
         if(rc > 1) {
            LDEXBL (CALLER,NOSPACE_RC);
            continue;
         }
         KC (COMP,0) KEYSTO(,,CALLER); /* we become available, Child holds CALLER key */
      }

      else if(type ==  0) {  /* this is silent type */
type0:
         rcparm.tod=*ctod(); /* structure move */
         rcparm.len=8;
         KC (RC,TDO_AddKey) STRUCTFROM(rcparm) KEYSFROM(HISDOM);
 
  /* now trim RC so that only 100 entries in it */
 
         KC (RC,KT) STRUCTTO(ktdata) RCTO(rc);
         if (ktdata.nrec > 100) {
 
  /* first get rid of DK(0) domain keys */
 
            rcparm.len=1;
            rcparm.tod.hi=0;
            rcparm.tod.low=0;
            for(;;) {
               KC (RC,TDO_GetGreaterThan) STRUCTFROM(rcparm)
                   STRUCTTO(rcparm) KEYSTO(K0) RCTO(rc);
               if(rc > 1) break;
               KC (DISCRIM,0) KEYSFROM(K0) RCTO(rc);
               if (rc == 1) {
                  KC (RC,TDO_Delete) STRUCTFROM(rcparm) RCTO(rc);
               }
            }
 
  /* if still over 100 entries, dump early entries */
 
            for(;;) {
               KC (RC,KT) STRUCTTO(ktdata) RCTO(rc);
               if (ktdata.nrec <= 100) break;
               KC (RC,TDO_GetFirst) STRUCTTO(rcparm) RCTO(rc);
               if (rc != 1) break;
               KC (RC,TDO_Delete) STRUCTFROM(rcparm) RCTO(rc);
            }
         }
         KC (COMP,Node_Fetch+0) KEYSTO(,,CALLER) RCTO(rc);
      }
    
      else {  /* unknown type */
         KC (COMP,Node_Fetch+0) KEYSTO(,,CALLER) RCTO(rc);
      }

 /* now exit and be ready for next fault */ 

      LDEXBL (CALLER,0);
   }
}

LLI *ctod()
{
static   LLI temp;
    return &temp;
}

/******************************************************************
   routine to selectively copy pages in the mapped memory tree
   to make them writeable (it will copy readwrite pages as well)

   This routine should be called from hex2mem() when it detects
   a store failure.  hex2mem() will then repeat the store attempt
   
   probe returns 0 if the page cannot be copied 
******************************************************************/
int probe(addr)
     unsigned long addr;
{
     JUMPBUF;
     UINT32 oc,rc;
     unsigned long backkey;
     unsigned char db;
     int shift,slot,i;
     struct Node_KeyValues nkv;

     if((addr & 0xF0000000) < 0x10000000) return 1;  /* in my space */
     if((addr & 0xF0000000) > 0x10000000) return 0;  /* no ones space */
     addr = addr & 0x0FFFFFFF;   /* 256 meg address space of target domain */
   
     KC (DOMKEY,Domain_GetMemory) KEYSTO(K0);
     KC (K0,Node_Fetch+1) KEYSTO(K0);   /* starting point */

     db=6;             /* lss of first node of three */
     shift=7*4;        /* shift to least digit */

     while(db > 2)  {       /* exit when get to page level */
        slot = (addr >> shift) & 0xF;   
        KC(K0,Node_Fetch+slot) KEYSTO(K1);
        KC(K1,KT) RCTO(rc);
        if(rc == KT+1) {  /* replace data key with node of background keys */
           KC(SB,SB_CreateNode) KEYSTO(K1);
           for(i=0;i<16;i++) {
              memset(&nkv.Slots[i],0,Node_KEYLENGTH);
              backkey = i << (shift - 4);
              backkey = backkey | 0x03;
              backkey = backkey | ((addr >> shift) << shift);
/* must have the rest of the address here */
              memcpy(&nkv.Slots[i].Byte[Node_KEYLENGTH-4],&backkey,4);
           }
           nkv.StartSlot=0;
           nkv.EndSlot=15;
           KC(K1,Node_WriteData) STRUCTFROM(nkv);
           KC(K1,Node_MakeNodeKey) CHARFROM(&db,1) KEYSTO(K1);
           KC(K0,Node_Swap+slot) KEYSFROM(K1);
           KC(K0,Node_Fetch+slot) KEYSTO(K0);
        }
        else {
           KC(K0,Node_Fetch+slot) KEYSTO(K0);
        }
        db--;
        shift -= 4;
     }
/* K0 has the lss=3 node key */
     slot = (addr >> 12) & 0xF;
     KC (K0,Node_Fetch+slot) KEYSTO(K1);
     KC (K1,KT) RCTO(rc);
     if(rc == KT+1) {  /* copy page return 0 or 1 */
        KC(SB,SB_CreatePage) KEYSTO(K1);
        *memoryerror=0;
        KC(K1,Page_WriteData+0) CHARFROM(((addr + hismemory) & 0xFFFFF000),4096) RCTO(rc);
        if(*memoryerror) {  /* opps truly non addressable */
           KC(SB,SB_DestroyPage) KEYSFROM(K1);
           return 0;
        }
        KC(K0,Node_Swap+slot) KEYSFROM(K1);  /* replace data key with copied page */
        return 1;
     }
     return 1; /* already a page key , should not get here */
}
/*****************************************************************
   routine to walk the tree at HISMEMORY (An lss=6 red node)
   and sell any pages.
*****************************************************************/
void sellpages()
{
     JUMPBUF;
     UINT32 rc,oc;
     int loop6,loop5,loop4,loop3;

     KC(HISMEMORY,Node_Fetch+0) KEYSTO(DISCRIM);   /* use loop6 */
     KC(SB,SB_DestroyNode) KEYSFROM(HISMEMORY) RCTO(rc);  /* lss=7 red node */

     KC(DISCRIM,KT) RCTO(rc);
     if(rc == KT+1) return;

/* use HISMEMORY as the scratch key */
/* use DISCRIM and replace it */
     for(loop6=0;loop6<16;loop6++) { 
        KC(DISCRIM,Node_Fetch+loop6) KEYSTO(K0);
        KC(K0,KT) RCTO(rc);
        if(rc != KT+1) {
           for(loop5=0;loop5<16;loop5++) {
              KC(K0,Node_Fetch+loop5) KEYSTO(K1);
              KC(K1,KT) RCTO(rc);
              if(rc != KT+1) {  /* loop down */
                 for(loop4=0;loop4<16;loop4++) {
                    KC(K1,Node_Fetch+loop4) KEYSTO(K2);
                    KC(K2,KT) RCTO(rc);
                    if(rc != KT+1)  {  /* loop down */
                       for(loop3=0;loop3<16;loop3++) {
                          KC(K2,Node_Fetch+loop3) KEYSTO(HISMEMORY);
                          KC(HISMEMORY,KT) RCTO(rc);
                          if(rc == Page_AKT) {
                             KC(SB,SB_DestroyPage) KEYSFROM(HISMEMORY) RCTO(rc);
                          }
                       }
                       KC (SB,SB_DestroyNode) KEYSFROM(K2) RCTO(rc);
                    }
                 }
                 KC (SB,SB_DestroyNode) KEYSFROM(K1) RCTO(rc);
              }
           }
           KC (SB,SB_DestroyNode) KEYSFROM(K0) RCTO(rc);
        }
    }
    KC (SB,SB_DestroyNode) KEYSFROM(DISCRIM) RCTO(rc);

    KC (COMP,Node_Fetch+COMPDISCRIM) KEYSTO(DISCRIM);
}
/******************************************************************
   GDB ROUTINES
******************************************************************/

void putDebugChar(unsigned char c)
{
    JUMPBUF;
    UINT32 rc;

    KC(RC,UART_WriteData) CHARFROM(&c,1) RCTO(rc);

//    KC(COMP,4096+c) RCTO(rc);
}
unsigned char getDebugChar()
{
    JUMPBUF;
    UINT32 rc;
    unsigned char c; 
    
    c=0;
    while(c == 0)
         KC(RC,UART_WaitandReadData+1) CHARTO(&c,1) RCTO(rc);

    KC(COMP,8192+c) RCTO(rc);
    return c;
}

static int
hex(ch)
     unsigned char ch;
{
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

unsigned char *
getpacket (unsigned char *buffer)
{
  JUMPBUF;
  UINT32 oc,rc;

  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  unsigned char ch;
  SINT32 len;

retry1:

  KC(RC,UART_GetGDBPacket+BUFMAX) CHARTO((char *)buffer,BUFMAX,len) RCTO(rc);

/* only returns when there is a # */
 
  checksum = 0;
  xmitcsum = -1;
  count = 0;

  while(count < len) {
     if(buffer[count] == '#') {
          buffer[count]=0;
          break;
     }
     checksum = checksum + buffer[count]; 
     count++;
  }  
  if(count >= len) {  /* reached max len without # */
     putDebugChar('-');
     goto retry1;
  }

  xmitcsum = hex (buffer[count+1]) << 4;
  xmitcsum += hex (buffer[count+2]);

  if(checksum != xmitcsum) putDebugChar('-');
  else {
     putDebugChar('+');
     if(buffer[2] == ':') {
        putDebugChar(buffer[0]);
        putDebugChar(buffer[1]);
        return &buffer[3];
     }
     return &buffer[0];
  }
  goto retry1;  //  if NAK packet must get it again
}

unsigned char *
OLDgetpacket (unsigned char *buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  unsigned char ch;

  while (1)
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar ()) != '$')
	;

retry:
      checksum = 0;
      xmitcsum = -1;
      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar ();
          if (ch == '$')
            goto retry;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  ch = getDebugChar ();
	  xmitcsum = hex (ch) << 4;
	  ch = getDebugChar ();
	  xmitcsum += hex (ch);

	  if (checksum != xmitcsum)
	    {
	      putDebugChar ('-');	/* failed checksum */
	    }
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */

	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);

		  return &buffer[3];
		}

	      return &buffer[0];
	    }
	}
    }
}
/* send the packet in buffer.  */

static void
OLDputpacket(buffer)
     unsigned char *buffer;
{
  unsigned char checksum;
  int count;
  unsigned char ch;

  /*  $<packet info>#<checksum>. */
  do
    {
      putDebugChar('$');
      checksum = 0;
      count = 0;

      while (ch = buffer[count])
	{
	  putDebugChar(ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar('#');
      putDebugChar(hexchars[checksum >> 4]);
      putDebugChar(hexchars[checksum & 0xf]);

    }
  while (getDebugChar() != '+');
}

static unsigned char *
putgetpacket(buffer)
     unsigned char *buffer;
{
     unsigned char out[BUFMAX];
     unsigned char ch,checksum,xmitcsum;
     int count;
     unsigned char *ptr;

     JUMPBUF;
     UINT32 oc,rc;
     SINT32 len;

     ptr=out;
     checksum = 0;
     count = 0;

     *ptr='$';
     ptr++;

     while(ch = buffer[count]) {
        checksum += ch;
        count += 1;

        *ptr = ch;
        ptr++;
     }

     *ptr='#';
     ptr++;

     *ptr = hexchars[checksum >> 4];
     ptr++;

     *ptr = hexchars[checksum & 0xf];
     ptr++;

// write, wait for + (or retransmit), then read next packet

     KC (RC,UART_PutGetGDBPacket+BUFMAX) CHARFROM(out,count+4) CHARTO(buffer,BUFMAX,len) RCTO(rc);

/* only returns when there is a # */

retry3:
 
  checksum = 0;
  xmitcsum = -1;
  count = 0;

  while(count < len) {
     if(buffer[count] == '#') {
          buffer[count]=0;
          break;
     }
     checksum = checksum + buffer[count]; 
     count++;
  }  
  if(count >= len) {  /* reached max len without # */
     putDebugChar('-');
     KC(RC,UART_GetGDBPacket+BUFMAX) CHARTO((char *)buffer,BUFMAX,len) RCTO(rc);
     goto retry3;
  }

  xmitcsum = hex (buffer[count+1]) << 4;
  xmitcsum += hex (buffer[count+2]);

  if(checksum != xmitcsum) putDebugChar('-');
  else {
     putDebugChar('+');
     if(buffer[2] == ':') {
        putDebugChar(buffer[0]);
        putDebugChar(buffer[1]);
        return &buffer[3];
     }
     return &buffer[0];
  }
  KC(RC,UART_GetGDBPacket+BUFMAX) CHARTO((char *)buffer,BUFMAX,len) RCTO(rc);
  goto retry3;  // after NAK and re-read
}

static void 
putpacket(buffer)
     unsigned char *buffer;
{
     unsigned char out[BUFMAX];
     unsigned char ch,checksum;
     int count;
     unsigned char *ptr;

     JUMPBUF;
     UINT32 oc,rc;

     ptr=out;
     checksum = 0;
     count = 0;

     *ptr='$';
     ptr++;

     while(ch = buffer[count]) {
        checksum += ch;
        count += 1;

        *ptr = ch;
        ptr++;
     }

     *ptr='#';
     ptr++;

     *ptr = hexchars[checksum >> 4];
     ptr++;

     *ptr = hexchars[checksum & 0xf];
     ptr++;

     do {
       KC (RC,UART_PutGDBPacket) CHARFROM(out,count+4) RCTO(rc);
     } while (getDebugChar() != '+');
}

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * THESE versions are only for use with the REGISTERs structure
 */

static unsigned char *
mem2hex(mem, buf, count, may_fault)
     unsigned char *mem;
     unsigned char *buf;
     int count;
     int may_fault;
{
  unsigned char ch;

  while (count-- > 0)
    {
      ch = *mem++;
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
    }

  *buf = 0;

  return buf;
}
/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char *
hex2mem(buf, mem, count, may_fault)
     unsigned char *buf;
     unsigned char *mem;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  for (i=0; i<count; i++)
    {
      ch = hex(*buf++) << 4;
      ch |= hex(*buf++);
      *mem++ = ch;
    }

  return mem;
}
/* Convert Memory in the target address space using Callseg */
/* if there is any error return 0 */

static unsigned char *
mem2hex1(mem, buf, count, may_fault)
     unsigned char *mem;
     unsigned char *buf;
     int count;
     int may_fault;
{
  unsigned char ch;

  *memoryerror = 0;

  while (count-- > 0)
    {
      ch = *mem++;

      if(*memoryerror) return 0;

      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
    }

  *buf = 0;

  return buf;
}
/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char *
hex2mem1(buf, mem, count, may_fault)
     unsigned char *buf;
     unsigned char *mem;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  *memoryerror = 0;

  for (i=0; i<count; i++)
    {
      ch = hex(*buf++) << 4;
      ch |= hex(*buf++);
      *mem = ch;
      if(*memoryerror) {
         if(!probe(mem)) return 0;  /* can't make writable */
         *memoryerror=0;
         *mem = ch; 
         if(*memoryerror) return 0;
      }
      mem++;
    }

  return mem;
}

/* This table contains the mapping between SPARC hardware trap types, and
   signals, which are primarily what GDB understands.  It also indicates

/* This table contains the mapping between SPARC hardware trap types, and
   signals, which are primarily what GDB understands.  It also indicates
   which hardware traps we need to commandeer when initializing the stub. */

static struct hard_trap_info
{
  unsigned char tt;		/* Trap type code for SPARClite */
  unsigned char signo;		/* Signal that we map this trap into */
} hard_trap_info[] = {
  {1, SIGSEGV},			/* instruction access error */
  {2, SIGILL},			/* privileged instruction */
  {3, SIGILL},			/* illegal instruction */
  {4, SIGEMT},			/* fp disabled */
  {36, SIGEMT},			/* cp disabled */
  {7, SIGBUS},			/* mem address not aligned */
  {9, SIGSEGV},			/* data access exception */
  {10, SIGEMT},			/* tag overflow */
  {128+1, SIGTRAP},		/* ta 1 - normal breakpoint instruction */
  {0, 0}			/* Must be last */
};

/* Convert the SPARC hardware trap type code to a unix signal number. */

static int
computeSignal(tt)
     int tt;
{
  struct hard_trap_info *ht;

  for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
    if (ht->tt == tt)
      return ht->signo;

  return SIGHUP;		/* default for things we don't know about */
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int
hexToInt(char **ptr, int *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex(**ptr);
      if (hexValue < 0)
	break;

      *intValue = (*intValue << 4) | hexValue;
      numChars ++;

      (*ptr)++;
    }

  return (numChars);
}





/****************************************************************
  Handle GDB remote interface using Uart key in RC
  
  Domain key is in K0
  Restart key is in CALLER

  The Domain's memory is mapped into the 16 megabytes at 0x10000000
  using a redsegment/background key.  The domain stack is in
  writable memory.

***************************************************************/
int dogdb() 
{
    JUMPBUF;

    struct Domain_SPARCRegistersAndControl drac;
    struct Domain_SPARCOldWindow windows[8];
    UINT32 rc,oc;
    int i,j,actlen,nwindows;
    unsigned long *sp;

    unsigned long registers[NUMREGS];

/*
   at this point we have trapped in the domain.  GDB may or may not
   have established communication with this domain.
*/

   KC (HISDOM,Domain_GetSPARCStuff) STRUCTTO(drac) RCTO(rc);
#ifdef xx
   KC (HISDOM,Domain_GetMemory) KEYSTO(K1) RCTO(rc);
   KC (DOMKEY,Domain_GetMemory) KEYSTO(K2) RCTO(rc);
   KC (K2,Node_Swap+1) KEYSFROM(K1) RCTO(rc);  // Map his memory to my 0x10000000
#endif

/* We need to put all the windows on the stack for GDB */

    KC (HISDOM,Domain_GetSPARCOldWindows) STRUCTTO(windows,64*8,actlen);
    nwindows = actlen/64;

    sp=(unsigned long *)(hismemory + drac.Regs.o[6]);

    for(j=0;j<8;j++) {
        sp[j]=drac.Regs.l[j];
        sp[j+8]=drac.Regs.i[j];
    }

    sp=(unsigned long *)(hismemory + drac.Regs.i[6]);

    for(i=nwindows-1;i>=0;i--) {
        for(j=0;j<8;j++) {
            sp[j] = windows[i].l[j];
            sp[j+8] = windows[i].i[j];
        }
        sp = (unsigned long *)(hismemory + windows[i].i[6]);
    }
    KC (HISDOM,Domain_ClearSPARCOldWindows);

/* END put all windows on the stack */


   memcpy(&registers[0],&drac.Regs.g[0],32*4);
   memset(&registers[F0],0,32*4);
   registers[Y]=drac.Regs.g[0];
   registers[PSR]=drac.Control.PSR;
   registers[WIM]=0;
   registers[TBR]=(drac.Control.TRAPCODE & 0xff) << 4;
   registers[PC]=drac.Control.PC;
   registers[NPC]=drac.Control.NPC;
   registers[FPSR]=drac.Control.FSR;
   registers[CPSR]=0;

   KC(RC,UART_MakeCurrentKey) KEYSTO(RC);
   KC(RC,UART_EnableInput) RCTO(rc);

   oc=handle_exception(registers);

   memcpy(&drac.Regs.g[0],&registers[0],32*4);
   drac.Regs.g[0]=registers[Y];
   drac.Control.PSR=registers[PSR];
//   =registers[WIM];
//   =registers[TBR];
   drac.Control.PC=registers[PC];
   drac.Control.NPC=registers[NPC];
   drac.Control.FSR=registers[FPSR];
//   =registers[CPSR];

   KC (HISDOM,Domain_ResetSPARCStuff) STRUCTFROM(drac) RCTO(rc);

   return oc;
}

int
handle_exception (registers)
     unsigned long *registers;
{
  int tt;			/* Trap type */
  int sigval;
  int addr;
  int length;
  char *ptr;
  unsigned long *sp;

  unsigned char remcomInBuffer[BUFMAX];
  unsigned char remcomOutBuffer[BUFMAX];
  

/* First, we must force all of the windows to be spilled out */

//  if (registers[PC] == (unsigned long)breakinst)
//    {
//      registers[PC] = registers[NPC];
//      registers[NPC] += 4;
//    }

  sp = (unsigned long *)registers[SP];

  tt = (registers[TBR] >> 4) & 0xff;

  /* reply to host that an exception has occurred */
  sigval = computeSignal(tt);
  ptr = remcomOutBuffer;

  *ptr++ = 'T';
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[PC >> 4];
  *ptr++ = hexchars[PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[PC], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[FP >> 4];
  *ptr++ = hexchars[FP & 0xf];
  *ptr++ = ':';
//  ptr = mem2hex(sp + 8 + 6, ptr, 4, 0); /* FP */
   ptr = mem2hex((char *)&registers[FP],ptr,4,0);
  *ptr++ = ';';

  *ptr++ = hexchars[SP >> 4];
  *ptr++ = hexchars[SP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&sp, ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[NPC >> 4];
  *ptr++ = hexchars[NPC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[NPC], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[O7 >> 4];
  *ptr++ = hexchars[O7 & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[O7], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = 0;

//  putpacket(remcomOutBuffer);

  putgetpacket(remcomOutBuffer);
  strcpy(remcomInBuffer,remcomOutBuffer);

  while (1)
    {
        ptr=remcomInBuffer;
//      ptr = getpacket(remcomInBuffer);

      remcomOutBuffer[0] = 0;
      switch (*ptr++)
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval & 0xf];
	  remcomOutBuffer[3] = 0;
	  break;

	case 'd':		/* toggle debug flag */
	  break;

	case 'g':		/* return the value of the CPU registers */
	  {
	    ptr = remcomOutBuffer;
	    ptr = mem2hex((char *)registers, ptr, 16 * 4, 0); /* G & O regs */
//	    ptr = mem2hex(sp + 0, ptr, 16 * 4, 0); /* L & I regs */
            ptr = mem2hex(&registers[L0], ptr, 16 * 4, 0); /* L & I regs */
	    memset(ptr, '0', 32 * 8); /* Floating point */
	    mem2hex((char *)&registers[Y],
		    ptr + 32 * 4 * 2,
		    8 * 4,
		    0);		/* Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR */
	  }
	  break;

	case 'G':	   /* set the value of the CPU registers - return OK */
	  {
	    unsigned long *newsp, psr;

	    psr = registers[PSR];

	    hex2mem(ptr, (char *)registers, 16 * 4, 0); /* G & O regs */
//	    hex2mem(ptr + 16 * 4 * 2, sp + 0, 16 * 4, 0); /* L & I regs */
            hex2mem(ptr + 16 * 4 * 2, &registers[L0], 16 * 4, 0);
	    hex2mem(ptr + 64 * 4 * 2, (char *)&registers[Y],
		    8 * 4, 0);	/* Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR */

	    /* See if the stack pointer has moved.  If so, then copy the saved
	       locals and ins to the new location.  This keeps the window
	       overflow and underflow routines happy.  */

#ifdef HUH
	    newsp = (unsigned long *)registers[SP];
	    if (sp != newsp)
	      sp = memcpy(newsp, sp, 16 * 4);
#endif

	    /* Don't allow CWP to be modified. */

	    if (psr != registers[PSR])
	      registers[PSR] = (psr & 0x1f) | (registers[PSR] & ~0x1f);

	    strcpy(remcomOutBuffer,"OK");
	  }
	  break;

	case 'm':	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	  /* Try to read %x,%x.  */

	  if (hexToInt(&ptr, &addr)
	      && *ptr++ == ','
	      && hexToInt(&ptr, &length))
	    {
              addr += hismemory;  // handle mapping offset

	      if (mem2hex1((char *)addr, remcomOutBuffer, length, 1))
		break;

	      strcpy (remcomOutBuffer, "E03");
	    }
	  else
	    strcpy(remcomOutBuffer,"E01");
	  break;

	case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	  /* Try to read '%x,%x:'.  */

	  if (hexToInt(&ptr, &addr)
	      && *ptr++ == ','
	      && hexToInt(&ptr, &length)
	      && *ptr++ == ':')
	    {
              addr += hismemory;

	      if (hex2mem1(ptr, (char *)addr, length, 1))
		strcpy(remcomOutBuffer, "OK");
	      else
		strcpy(remcomOutBuffer, "E03");
	    }
	  else
	    strcpy(remcomOutBuffer, "E02");
	  break;

	case 'c':    /* cAA..AA    Continue at address AA..AA(optional) */
	  /* try to read optional parameter, pc unchanged if no parm */

	  if (hexToInt(&ptr, &addr))
	    {
	      registers[PC] = addr;
	      registers[NPC] = addr + 4;
	    }

/* Need to flush the instruction cache here, as we may have deposited a
   breakpoint, and the icache probably has no way of knowing that a data ref to
   some location may have changed something that is in the instruction cache.
 */

/*  YES WE HAVE TO HANDLE THIS SOMETIME There is an instruction to do this.  Is it privileged? */
/*  No, it isn't but the FLUSH instruction only works on a doubleword */

//	  flush_i_cache();
	  return 0;

	  /* kill the program */
	case 'k' :		/* do nothing */
	  return 1;  /* flag the fact that the debugger should get out of the way */

	case 'r':		/* Reset */
	  break;
	}			/* switch */

      /* reply to the request */
//      putpacket(remcomOutBuffer);
      putgetpacket(remcomOutBuffer);  // will read into the out buffer 
      strcpy(remcomInBuffer,remcomOutBuffer);
    }
}
