/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "kktypes.h"
#include "cpujumph.h"
#include "gateh.h"
#include "primcomh.h"
#include "locksh.h"
#include "queuesh.h"
#include "domainh.h"
#include "domamdh.h"
#include "unprndh.h"
#include "enexmdh.h"
#include "kschedh.h"
#include "kernkeyh.h"
// #include "mmu.h"
#include "memomdh.h"
#include "cpumemh.h"
#include "sparc_asm.h"
#include "locore.h"
#include "memutil.h"
 

static int copytojumpee(ME map, unsigned long a, char *b, int len)
{
   unsigned long fsr, mmuctl, bafrom, bato;

   if (map == NULL_MAP) return 0;

   sta04(0x200/*RMMU_CTX_REG*/, map);
   fsr = lda04(0x300/*RMMU_FSR_REG*/);
   mmuctl = lda04(0/*RMMU_CTL_REG*/);
   sta04(0/*RMMU_CTL_REG*/, mmuctl | 2/*MCR_NF*/);
   if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
      bafrom = lda03((unsigned long)b & ~0xfff);
      bato = lda03(a & ~0xfff);
      if ( (bato&3) != 2 || (bafrom&3) != 2 || !((bato^bafrom)&~0xff)) {
         sta04(0/*RMMU_CTL_REG*/, mmuctl);
         sta04(0x200/*RMMU_CTX_REG*/, kernCtx);
         return 0;
      }
      Memcpy((char *)a, b, len);
   } else if ( 0 == movba2va((void *)a, b, len) ) {
      sta04(0/*RMMU_CTL_REG*/, mmuctl);
      sta04(0x200/*RMMU_CTX_REG*/, kernCtx);
      return 0;
   }
   sta04(0/*RMMU_CTL_REG*/, mmuctl);
   fsr = lda04(0x300/*RMMU_FSR_REG*/);
   sta04(0x200/*RMMU_CTX_REG*/, kernCtx);

   if (fsr & 0x1c/*SFSREG_FT*/) return 0;
   return 1;
}

#define resx2 1
#define resx3 3
void strangeResume(){int rc = cpuentryblock.rc;
*(uint32 *)&cpuentryblock = 0;
switch(cpuinvokeddatabyte){
  case faultresume:
  case restartresume:
       return;
  case resx2: cpuordercode |= 0x80000000; break;
  case resx3: cpuordercode = (cpuordercode & ~(1<<30)) | (1<<31); 
      break;}
  cpuentryblock.rc = rc;
}

int resumeType = ~0;

/* Handle jumps to resume keys */
void jresume(struct Key * key /* The invoked key */)         
            
/* Input:
   cpudibp - has the jumper's DIB
   cpuordercode - has order code.
   The invoked key type is one of the following:
      resumekey+prepared,
      startkey+prepared,
      or startkey+prepared+involvedw
*/
{
   register NODE *jumper = cpudibp->rootnode;
   register NODE *jumpee =
               (NODE *)key->nontypedata.ik.item.pk.subject;
   register struct DIB *jedib;
      /* Initialized after jumpee has been prepared */
 
   cpuinvokeddatabyte = key->databyte;
   cpuinvokedkeytype = key->type;
 
   if (preplock_node(jumpee,lockedby_jresume)) {
      if (jumper != jumpee)
         crash("JRESUME001 - Node other than jumper preplocked");
      cputrapcode = 0x300;    /* Same node -> jumping to self */
      midfault();
      return;
   }
/*
   At this point we have a jumper and a jumpee. The jumper is prepared
   as a domain. The source string is set up. The domain roots of both
   the jumper and jumpee are preplocked.
 
   Begin ensure jumpee's domain prepared
*/
   if (jumpee->prepcode != prepasdomain) { /* Jumpee not prep as dom */
      if (jumpee->prepcode != unpreparednode) {
         if (unprnode(jumpee) != unprnode_unprepared) {
            if (jumpee != cpudibp->keysnode
                && !node_overlaps_statestore(jumpee) )
               crash("JRESUMEC002 Node left locked");
            cputrapcode = 0x604;
            unpreplock_node(jumpee);
            midfault();
            return;
         }
      }
      /* Jumpee is now unprepared */
      switch (prepdom(jumpee)) {
       case prepdom_prepared:
         break;
       case prepdom_overlap:
         cputrapcode = 0x600;
         unpreplock_node(jumpee);
         midfault();
         return;
       case prepdom_wait:
         unpreplock_node(jumpee);
         abandonj();
         return;
       case prepdom_malformed:
         unpreplock_node(jumpee);
         if (cpuexitblock.jumptype == 0) {
            enqueuedom(jumper, &junkqueue);
            abandonj();
            return;
         }
         jdata(key);
         return;
      }
   }
/*
   At this point we have the jumper and the jumpee prepared as domains.
   Both the jumper and the jumpee's domain roots are preplocked.
 
   Begin Check gate type vs. busyness
*/
   jedib = jumpee->pf.dib;
   if (cpuinvokedkeytype == resumekey+prepared) {
      if (!(jedib->readiness & BUSY)) {
        /* Resume key but jumpee not busy - jumper sees normal action */
         /* but jumpee is abandoned */
         unpreplock_node(jumpee);
         handlejumper();
         return;
      }
      if (cpuinvokeddatabyte == returnresume) {
         int rc = 0;
         unsigned long len;

         get_entry_block(jedib);     /* get the jumpee's entry block */
         if (cpuentryblock.str && cpuentryblock.regsparm==0) {
            unsigned long a = get_parm_pointer(jedib);

            len = get_parm_length(jedib);
            switch (cpuexitblock.argtype) {  /* what's offered? */
             case arg_none:
               len = 0;
               rc = 1;
               break;
             case arg_regs:
             case arg_memory:
               if (len>cpuarglength) len = cpuarglength;
               rc = copytojumpee(jedib->map, a, cpuargaddr, len);
               break;
             default: crash("JRESUMEC004 - Bad exitblock argtype ");
            }
         }
         if (rc) {
            cpuentryblock.str = 0;  /* Already copied */
            if (cpuentryblock.strl) jedib->regs[8+4] = cpuarglength;
         } else if (setupdestpage(jedib)) {
            unpreplock_node(jumpee);
            abandonj();
            return;
         }
      }
      else strangeResume();
 
/* End dry run - (but hold handle jumper until common code below) */
 
      zapresumes(jedib);
      if (cpuentryblock.db) put_data_byte(0,jedib);
   }
   else {              /* Start key invoked */
      int rc = 0;
      unsigned long len;

      if (jedib->readiness & BUSY) { /* Must stall jumper */
 
         register struct Key *hook = &jumper->domhookkey;
 
         if (jumper->flags & EXTERNALQUEUE)
            crash("JRESUME003 External queue not yet implemented");
         hook->type = pihk;
         hook->databyte = 1;
         hook->nontypedata.ik.item.pk.subject =
               (union Item *)jumpee;
         hook->nontypedata.ik.item.pk.leftchain =
               jedib->lastinvolved;
         hook->nontypedata.ik.item.pk.rightchain =
               jedib->lastinvolved->item.rightchain;
         jedib->lastinvolved->item.rightchain =
               (union Item *)hook;
         hook->nontypedata.ik.item.pk.rightchain->item.leftchain =
               (union Item *)hook;
         jedib->lastinvolved = (union Item *)hook;
         jumpee->flags |= NFREJECT;
         cpudibp->readiness |= HOOKED;
         ksstall(jumper,jumpee);
         unpreplock_node(jumpee);
         abandonj();
         return;
      }
      /* get entry block & pages */

      get_entry_block(jedib);     /* get the jumpee's entry block */
      if (cpuentryblock.str && cpuentryblock.regsparm==0) {
         unsigned long a = get_parm_pointer(jedib);

         len = get_parm_length(jedib);
         switch (cpuexitblock.argtype) {  /* what's offered? */
          case arg_none:
            len = 0;
            rc = 1;
            break;
          case arg_regs:
            if (len>cpuarglength) len = cpuarglength;
            rc = copytojumpee(jedib->map, a, cpuargaddr, len);
            break;
          case arg_memory:
            if (len>cpuarglength) len = cpuarglength;
            rc = copytojumpee(jedib->map, a, cpuargaddr, len);
            break;
          default: crash("JRESUMEC004 - Bad exitblock argtype ");
         }
      }
      if (rc) {
         cpuentryblock.str = 0;  /* Already copied */
         if (cpuentryblock.strl) jedib->regs[8+4] = cpuarglength;
      } else if (setupdestpage(jedib)) {
         unpreplock_node(jumpee);
         abandonj();
         return;
      }
 
/* End dry run - (but hold handle jumper until common code below) */
 
      if (cpuentryblock.db) put_data_byte(cpuinvokeddatabyte,jedib);
   }
/*
   We now have verified that the busyness state of the jumpee will
   allow the jump to proceed.  The destination page has been set up.
 
   Set up key parameters
*/
   {
      register int mask = cpuexitblock.keymask & cpuentryblock.keymask;
 
      if (mask & 8)
         cpup1key = cpudibp->keysnode->keys[cpuexitblock.key1];
      if (mask & 4)
         cpup2key = cpudibp->keysnode->keys[cpuexitblock.key2];
       /* Don't overlay implicit key */
      if (mask & 2 && CPUP3_UNLOCKED == cpup3switch)
         cpup3key = cpudibp->keysnode->keys[cpuexitblock.key3];
      switch (cpuexitblock.jumptype) {
       case jump_return:
       case jump_fork:
         if (mask & 1)
            cpup4key = cpudibp->keysnode->keys[cpuexitblock.key4];
         break;
       case jump_implicit:
         if(!~resumeType) crash("No resume type");
         cpup4key.databyte = resumeType;
         resumeType = ~0;
      /* if (cpuexitblock.keymask & 1)
              cpup4key.databyte = faultresume;
         else cpup4key.databyte = restartresume; */
         cpup4key.type = resumekey+prepared;
         cpup4key.nontypedata.ik.item.pk.subject = (union Item *)jumper;
         cpuexitblock.keymask |= 1;
         break;
       case jump_call:
         cpup4key.databyte = returnresume;
         cpup4key.type = resumekey+prepared;
         cpup4key.nontypedata.ik.item.pk.subject = (union Item *)jumper;
         cpuexitblock.keymask |= 1;
         break;
      }
   }
 
   handlejumper();
   jedib->readiness |= BUSY; /* Mark jumpee as busy */
   cpuactor = jumpee;
   startdom(jedib);          /* make returnee the running domain */
   deliver_message(cpuexitblock);
   return;
}
