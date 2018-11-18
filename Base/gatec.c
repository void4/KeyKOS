/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "string.h"
#include "sysdefs.h"
#include "keyh.h"
#include "enexmdh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "prepkeyh.h"
#include "kschedh.h"
#include "locksh.h"
#include "queuesh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "kermap.h"  /* for lowcoreflags */
#include "primcomh.h"
#include "domamdh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "kernkeyh.h"
#include "cpumemh.h" /*...*/
#include "ioworkh.h"
#include "cyclecounter.h"
#include "unprndh.h"
#include "locore.h"
#include "scafoldh.h"
#include <stdio.h>
#include "jsegmenth.h"
#include "memutil.h"

#if defined(viking)
static long ucycle=0,uinst=0,kcycle=0,kinst=0;
static long long lastcycle=0,lastinst=0;
#endif 
 
void abandonj(void)
   /* Have domain redo jump (caller queued it) */
   /* Input - */
   /*   cpudibp - pointer to the dib of blocked domain */
   /*   cpuexitblock - Jumper's exit block */
   /*   cpup3switch != CPUP3_UNLOCKED - cpup3node's node is corelocked */
{
   release_parm_pages();
   if (CPUP3_UNLOCKED != cpup3switch) {
      coreunlock_node(cpup3node);
      cpup3switch = CPUP3_UNLOCKED;
   }
   back_up_jumper();
   putawaydomain();
}
 
 
void set_slot(       /* Put key into slot */
   struct Key *slot,             /* Pointer to slot to store into */
   struct Key *key)              /* Pointer to the key to store */
/*
      The key to store must be unprepared or half prepared.
      It must not be involvedr unless it is a hook.
*/
{
   if (key->type & involvedr) {        /* involvedr must be hook */
      if (key->type != pihk) crash("GATE004 involvedr and not hook");
      if (key->databyte) *slot = dk0;
      else *slot = dk1;
      return;
   }
   if (key->type & prepared) {         /* Copy prepared key */
      slot->databyte = key->databyte;
      slot->type = key->type;
      slot->nontypedata.ik.item.pk.subject =
                    key->nontypedata.ik.item.pk.subject;
      halfprep(slot);
   }
   else {
      *slot = *key;
   }
   return;
} /* End set_slot */

void deliver_message(struct exitblock exitblock)  /* send a message to a domain */
/*
   Input -
      cpudibp - pointer to the dib to run
 
      if readiness & TRAPPED && trapcode != 0 then the parameters
              for the domain keeper are described in the dib.
 
      The databyte has already been returned.
 
      The jump is completed as described in:
         exitblock - Describes parameters to pass to jumpee as:
            argtype - type of argument string
            keymask - keys to pass
            jumptype - type of jump, jump_return if primary key end
         cpuordercode - the order/return code to pass
         cpuargaddr - argument address in kernel space if ~arg_none
         cpuarglength - length of argument if ~arg_none
         cpuentrytblock - Describes parameters for the jumpee
         cpuparmaddr - kernel space parm addr if cpuentryblock.str
         cpuparmlength - max parm length if cpuentryblock.str
         cpup1key ... cpup4key - Set according to keymask
         N.B. The nodes designated by prepared versions of these keys
              do not have to be corelocked in a uni-processor
              version, since this routine will not
              cause get to be called until after they have been
              copied to the jumpee/returnee. In a multiprocessor
              version something (e.g. corelock) will have to be
              done to ensure their continued presence.
 
         if cpup3switch != CPUP3_UNLOCKED the cpup3node's node is corelocked
*/
{
   struct entryblock entryblock = cpuentryblock;
 
void pass1 (void) /* Pass the first key to the jumpee */
{
   struct Key *slot = &cpudibp->keysnode->keys[cpuentryblock.key1];
   if (clean(slot) == clean_dont) return;
   if (exitblock.keymask & 0x8) set_slot(slot,&cpup1key);
   else set_slot(slot,&dk0);
   /* keys node is prepaskeysnode, therefore dirty. */
} /* end pass1 */
 
 
void pass2 (void) /* Pass the second key to the jumpee */
{
   struct Key *slot = &cpudibp->keysnode->keys[cpuentryblock.key2];
   if (clean(slot) == clean_dont) return;
   if (exitblock.keymask & 0x4) set_slot(slot,&cpup2key);
   else set_slot(slot,&dk0);
} /* end pass2 */
 
 
void pass3 (void) /* Pass the third key to the jumpee */
{
   struct Key *slot = &cpudibp->keysnode->keys[cpuentryblock.key3];
   if (clean(slot) == clean_dont) return;
   if (exitblock.keymask & 0x2) set_slot(slot,&cpup3key);
   else set_slot(slot,&dk0);
} /* end pass3 */
 
 
void pass4 (void) /* Pass the fourth key to the jumpee */
{
   struct Key *slot = &cpudibp->keysnode->keys[cpuentryblock.key4];
   if (clean(slot) == clean_dont) return;
   if (exitblock.keymask & 0x1) set_slot(slot,&cpup4key);
   else set_slot(slot,&dk0);
} /* end pass4 */

   /* Start pass keys to jumpee */
 
   switch (cpuentryblock.keymask) {
    case 15: pass1();
    case 7:  pass2();
    case 3:  pass3();
    case 1:  pass4();
    case 0:  break;
    case 14: pass1();
    case 6:  pass2();
    case 2:  pass3();
             break;
    case 13: pass1();
    case 5:  pass2();
             pass4();
             break;
    case 11: pass1();
             pass3();
             pass4();
             break;
    case 12: pass1();
    case 4:  pass2();
             break;
    case 10: pass1();
             pass3();
             break;
    case 9:  pass1();
             pass4();
             break;
    case 8:  pass1();
             break;
   }
   if (CPUP3_UNLOCKED != cpup3switch) {
      if (cpup3switch & CPUP3_JUMPERKEY) {
         /* Store jumper's third key into slot of cpup3node */
         struct Key *slot = &cpup3node->keys[cpup3switch & 0xf];
         if (puninv(slot) != puninv_ok)
            set_trapcode(cpudibp, 0x600 + 72);
         else if (clean(slot) == clean_ok) {
            set_slot(slot,&cpustore3key);
            cpup3node->flags |= NFDIRTY;
         }
      }
      coreunlock_node(cpup3node);
      cpup3switch = CPUP3_UNLOCKED;
   }
 
   /* End pass keys to jumpee */
 
   /* Pass parameter string to jumpee */
   if (entryblock.str) {        /* string wanted */
      int len;
      switch (exitblock.argtype) {  /* what's offered? */
       case arg_none:
         len = 0;
         break;
       case arg_regs:
         if (cpuparmlength<cpuarglength) len=cpuparmlength;
         else len = cpuarglength;
         if (entryblock.regsparm) deliver_to_regs(cpudibp, len);
         else Memcpy(cpuparmaddr, cpuargaddr, len);
         break;
      case arg_memory:
         if (cpuparmlength<cpuarglength) len=cpuparmlength;
         else len = cpuarglength;
         if (entryblock.regsparm) deliver_to_regs(cpudibp, len);
         else if (cpumempg[0]) {
            if (memory_args_overlap()){
               Memcpy(cpuargpage, cpuargaddr, len);
               Memcpy(cpuparmaddr, cpuargpage, len);
            }
            else Memcpy(cpuparmaddr, cpuargaddr, len);
         } else {
            if (0 == movba2va(cpuparmaddr, cpuargaddr, len) ) {
               if (0 == movba2va(cpuargpage, cpuargaddr, len) ) {
                  crash("GATE005 - Overlap with cpuparmpage?");
               }
               Memcpy(cpuparmaddr, cpuargpage, len);
            }
         }
         break;
      default: crash("GATE003 - Bad exitblock argtype ");
     }
     if (entryblock.strl) put_string_length(len);

#ifdef stringlogging
     if (lowcoreflags.gatelogenable) {
         unsigned char *t;
         char str[128];
  
         if (cpumempg[0] || exitblock.argtype==arg_regs) {
             t=(unsigned char *)cpuargaddr;
         } else {
             movba2va(cpuargpage, cpuargaddr, len);
             t=(unsigned char *)cpuargpage;
         }
         if(len > 16) len=16;
         if(len) {int i;
           sprintf(str,"\n                                       ");
           for(i=0;i<len;i++) {
                char buf[8];
                sprintf(buf,"%02X",t[i]);
                Strcat(str,buf);
           } 
           logstr(str);
         }
     }
#endif

     release_parm_pages();
   }
 
   /* End pass parameter string */
 
   /* Begin copy order code */
 
   if (!entryblock.rc) {  /* If order/return code not wanted */
      /* Then if ordercode!=0 && key not restart_resume */
      if (cpuordercode && (cpuinvokedkeytype != resumekey+prepared ||
                 cpuinvokeddatabyte != restartresume)) {
         set_trapcode(cpudibp, 0x100);
         cpudibp->trapcodeextension[0] = cpuordercode;
      }
   }
   else put_ordercode();
 
   /* End copy order code */
 
{  char str[80];
   if (lowcoreflags.gatelogenable) {
#if defined(viking) 
      if(lowcoreflags.counters) {
           long long tcycle,tinst;

	   tcycle=get_cycle_count();
	   tinst=get_inst_count();
           kcycle=tcycle-lastcycle;
           kinst=tinst-lastinst;   /* calculate now before sprintf */

           if(!kcycle) kcycle=1;
           if(!ucycle) ucycle=1;
           sprintf(str, " rc=%x UC %d UI %d UIPC .%03d KC %d KI %d KIPC .%03d",
               cpuordercode,ucycle,uinst,(1000*uinst)/ucycle,
                            kcycle,kinst,(1000*kinst)/kcycle);
           logstr(str);

           lastcycle=get_cycle_count();
           lastinst=get_inst_count();   /* set for "user" code */
           return;
      }
#endif
      sprintf(str, " rc=%x", (unsigned int)cpuordercode);
      logstr(str);
   }
}  /* end logging stuff */
  return;
}
 
void return_message(void)  /* Return a message from a primary key */
/*
   Input -
      cpudibp - pointer to the dib to run
 
      if readiness & TRAPPED && trapcode != 0 then the parameters
              for the domain keeper are described in the dib.
 
      The jump is completed as described in:
         cpuexitblock - Describes parameters to pass to jumpee as:
            argtype - type of argument string
            keymask - keys to pass
         cpuordercode - the order/return code to pass
         cpuargaddr - argument address in kernel space if ~arg_none
         cpuarglength - length of argument if ~arg_none
         cpuentrytblock - Describes parameters for the jumpee
         cpuparmaddr - kernel space parm addr if cpuentryblock.str
         cpuparmlength - max parm length if cpuentryblock.str
         cpup1key ... cpup4key - Set according to keymask
         N.B. The nodes designated by prepared versions of these keys
              do not have to be corelocked in a uni-processor
              version, since this routine will not
              cause get to be called until after they have been
              copied to the jumpee/returnee. In a multiprocessor
              version something (e.g. corelock) will have to be
              done to ensure their continued presence.
 
         if cpup3switch != CPUP3_UNLOCKED the cpup3node's node is corelocked
*/
{
   cpuexitblock.jumptype = jump_return;  /* Primary keys return */
   if (cpuentryblock.db) put_data_byte(0,cpudibp);
      /* Primary keys return via a resume key which delivers data byte 0
       */
   deliver_message(cpuexitblock);
}
 
void midfault(void)
   /* Fault the domain in CPUDIBP */
   /* Input - */
   /*   cpudibp - pointer to the dib to trap */
   /*   cputrapcode - trap code for domain */
   /*   cpup3switch != CPUP3_UNLOCKED - cpup3node's node is corelocked */
   /*   cpubackupamount - The amount to back up the program counter */
{
   if (cpudibp->readiness & TRAPPED) {
      /* Domain is having trouble jumping to its domain keeper */
      /*   too bad - put it on the junk queue */
      enqueuedom(cpudibp->rootnode,&junkqueue);
      abandonj();   /* invoker has been queued */
      return;       /* cpudibp == NULL */
   }
   release_parm_pages();
   if (CPUP3_UNLOCKED != cpup3switch) {
      coreunlock_node(cpup3node);
      cpup3switch = CPUP3_UNLOCKED;
   }
   back_up_jumper();
   set_trapcode(cpudibp, cputrapcode);
   return;
}
 
void keyjump(struct Key *je_key)        /* Invoke a key */

/*   Note: je_key must not be involvedr. It won't be if it is in a
     prepared general keys node or in domkeeper of a prepared domain
 
      cpudibp - pointer to the dib of the actor
 
      The jump is described by:
         cpuordercode - the order/return code to pass
         cpuargaddr - argument address in kernel space if ~arg_none
         cpuarglength - length of argument or 0 if arg_none
         cpuexitblock - Describes parameters to pass to jumpee as:
            argtype - type of argument string
            keymask - keys to pass
            jumptype - type of jump
            key1, key2, key3, and key4 have the slot numbers of the
                   keys to be passed if the associated mask bit is one
 
    N.B. if cpup3switch != CPUP3_UNLOCKED then {
         cpup3node's node is corelocked.
         The third key passed is not that described by the exit block,
         but the one already in cpup3key.
         je_key->type is start, resume, misc, or data (not segment). }
*/
{ 
//  prom_printf("Gate Jump\n");   // crappy debug test

  if (lowcoreflags.gatelogenable) {
   const char *name;
   char lname[9];
   unsigned long retpoint;

#if defined(viking)
   if(lowcoreflags.counters) {
       long long tcycle,tinst;
       tcycle=get_cycle_count();
       tinst=get_inst_count();
       ucycle=tcycle-lastcycle;
       uinst=tinst-lastinst;
   }
#endif
   /* The following is a kludge to try to find the name of this
      program. But, if it can't be found, don't call any keepers. */

   savesegkeeperaddr();   /* kludge to save the fault address around memory calls */
   name = find_program_name();
   if (name==NULL) {
      abandonj();
      return;
   }
   restoresegkeeperaddr(); /* kludge to restore the fault address around the resolve_address */

/* if(Strcmp(name,"EMIGC")
   && Strcmp(name, "ERESYNCC")
   && Strcmp(name, "CKPTDVRC")) */
   {
       Strncpy(lname,name,8);
       lname[8]=0;
       retpoint=cpudibp->regs[7]; 
       {char buf[256];
        sprintf(buf,"\nDIB=%8X,R=%6lX/%6lX,oc=%8lX,EX=%08lX EN=%08lX Kt=%02X %-8s",
         (int)cpudibp, retpoint, (cpudibp->pc)-4, cpuordercode ,
         *((unsigned long *)(&cpuexitblock)), cpudibp->regs[10],
         je_key->type, lname);
       logstr(buf); /* log the information */}
#ifdef stringlogging
     if(cpuexitblock.argtype && (cpudibp->regs[10] & 0x02000000)) {char buf[256];
 sprintf(buf,"\n                             ST=%8lX,LN=%4X,RST=%8lX,RLN=%4X",
         cpudibp->regs[11],cpudibp->regs[12],
         cpudibp->regs[13],cpudibp->regs[1]);
       logstr(buf);  /* log second line */

       if(mem_ok==resolve_address(cpudibp->regs[11],cpudibp,0)) {
           t=(unsigned char *)map_window(clear_win, thepagecte,0);
           t=t+(cpudibp->regs[11] & 0xFFF);
           len=cpudibp->regs[12];
           if((4096 - (cpudibp->regs[11] & 0xFFF)) < len)
                len=4096- (cpudibp->regs[11] & 0xFFF);
           if(len) {
              if(len > 16) len=16;
              sprintf(buf,"\n ");
              for(i=0;i<len;i++) {       
                 sprintf(str,"%02X",t[i]);
                 Strcat(buf,str);
              }
              logstr(buf);  /* log third line */
           }
       }

     }
     else if (cpuexitblock.argtype) {char buf[256];
 sprintf(buf,"\n                             ST=%8lX,LN=%4X                 ",
          cpudibp->regs[11],cpudibp->regs[12]);
       logstr(buf);  /* log second line */

       if(mem_ok==resolve_address(cpudibp->regs[11],cpudibp,0)) {
           t=(unsigned char *)map_window(clear_win, thepagecte,0);
           t=t+(cpudibp->regs[11] & 0xFFF);
           len=cpudibp->regs[12];
           if((4096 - (cpudibp->regs[11] & 0xFFF)) < len)
                len=4096- (cpudibp->regs[11] & 0xFFF);
           if(len) {
              if(len > 16) len=16;
              sprintf(buf,"\n ");
              for(i=0;i<len;i++) {       
                 sprintf(str,"%02X",t[i]);
                 Strcat(buf,str);
              }
              logstr(buf);  /* log third line */
           }
       }

     }
     else if (cpudibp->regs[10] & 0x02000000) {char buf[256];
 sprintf(buf,"\n                                                 RST=%8lX,RLN=%4X",
          cpudibp->regs[13],cpudibp->regs[1]);
       logstr(buf);  /* log second line */
     }
#endif
   }
#if defined(viking)
   if(lowcoreflags.counters) {
       lastcycle=get_cycle_count();
       lastinst =get_inst_count();   /* repeat to eliminate cost of logging */
   }
#endif
 }  /* end of gate logging if */

/*
   Dispatch on invoked key type
   Prepare the key if it must be prepared and
   Then: call key type dependent routine for the key type
*/
 
numberkeyinvstarted[je_key->type & keytypemask]++; /* keep statistics */
switch (je_key->type & keytypemask) {
 case datakey:
   jdata(je_key);
   return;
 case pagekey:
   jpage(je_key);
   return;
 case segmentkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jsegment(je_key);
   return;
 case frontendkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jfrontend(je_key);
   return;
 case nodekey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jnode(je_key);
   return;
 case meterkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jmeter(je_key);
   return;
 case fetchkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jfetch(je_key);
   return;
 case startkey:          /* Start and resume keys both in jresume */
 case resumekey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jresume(je_key);
   return;
 case domainkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jdomain(je_key);
   return;
 case hookkey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jhook(je_key);
   return;
 case misckey:
   jmisc(je_key);
   return;
 case copykey:
   jcopy(je_key);
   return;
 case nrangekey:
   jnrange(je_key);
   return;
 case prangekey:
   jprange(je_key);
   return;
 case chargesetkey:
   jchargeset(je_key);
   return;
 case sensekey:
   if (!(je_key->type & prepared)) {
      switch (prepkey(je_key)) {
       case prepkey_wait:
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
       case prepkey_prepared:
          break;
       case prepkey_notobj:
          jdata(je_key);
          return;
      }
   }
   jsense(je_key);
   return;
 case devicekey:
   jdevice(je_key);
   return;
 default: crash("GATE002 Key->type out of range");
}
} /* End keyjump */
 
 
void keepjump(struct Key * je_key, int rt)      /* Perform implicit jump to a keeper */
   /* je_key is Key to the keeper, rt is resume key type */
{
   /* N.B. Other inputs are the same as keyjump above */
 
   /* Since keys other than gates and the error key seem not */
   /* to provide useful keeper functions and since such keys */
   /* lead to non time-sliceable kernel loops we invoke */
   /* the kernel's discretion to "execute the processes in */
   /* some order ". (i.e. not now.) */
   /*  CPUTIMERSTORED IS 0. */

   cpubackupamount = 0;
   cpuexitblock.jumptype = jump_implicit;
   resumeType = rt;
   cpup3switch = CPUP3_LOCKED;
   corelock_node(2, cpup3node);
   cpup3key.nontypedata.ik.item.pk.subject = (union Item *)cpup3node;
 
   switch (je_key->type & keytypemask) {
    case startkey:
    case resumekey:
      break;
    case misckey:
      if (je_key->nontypedata.dk11.databody11[0] != errormisckey){
          /* Put domain on the junk queue */
          enqueuedom(cpudibp->rootnode,&junkqueue);
          abandonj();   /* invoker has been queued */
          return;       /* cpudibp == NULL */
      }
      break;
    case datakey:
    case nrangekey:
    case prangekey:
    case chargesetkey:
    case devicekey:
    case pagekey:
    case segmentkey:
    case nodekey:
    case meterkey:
    case fetchkey:
    case domainkey:
    case hookkey:
    case sensekey:
    case copykey:
    case frontendkey:
       /* Put domain on the junk queue */
       enqueuedom(cpudibp->rootnode,&junkqueue);
       abandonj();   /* invoker has been queued */
       return;       /* cpudibp == NULL */
    default: crash("GATE002 Key->type out of range");
   }
   keyjump(je_key);
} /* End keepjump */
 
 
void gate(void)
/* Called by assembler interface when a gate jump has been issued. */
/*                                                                 */
/* Input -                                                         */
/*    cpudibp - Pointer to jumper's DIB with MPU state             */
/*       N.B. The trap code field has been set to zero in the DIB  */
/*    cpuexitblock - Contains the jumper's exit block.             */
/*    cpuordercode - The order code                                */
/*    cpuarglength - Argument string length                        */
 
/* Output -                                                        */
/*    cpudibp is a pointer to DIB of domain to run or NULL if      */
/*    scheduler should select a domain to run.                     */
/*    (The domain may have bits on in readiness.)                  */
 
{
   register struct Key *je_key;  /* Pointer to the key to the jumpee */
   register struct DIB *dibp = cpudibp;
   register struct exitblock exitblock = cpuexitblock;
 
   /* At this point the world is valid except that:                */
   /*  cpudibp designates the DIB of a prep-locked domain with an  */
   /*     un-registered process.                                   */
   /*  Its cpu allocation is still in the real process timer and   */
   /*     the process timer is running.                            */
   /*  This domain has just issued a jump.                         */
   /*  The exit block tells the type of the jump.                  */
   /* BEGIN DRY_RUN                                                */
 
   cpubackupamount = 2;   /* Entry from a trap instruction */
 
   if (!dibp->permits & GATEJUMPSPERMITTED) {
         set_trapcode(dibp, 0x0BC);
         back_up_jumper();
         return;
   } /* end if !gatejumpspermitted */;
  
   je_key = &(dibp->keysnode->keys[exitblock.gate]);
 
   /* BEGIN ENSURE_PRESENCE_OF_ARGUMENT_PAGE */
   if(cpuarglength) {
     switch (exitblock.argtype) {
      case arg_none:
        cpuarglength = 0;
        keyjump(je_key);
        break;
      case arg_memory:   /* String argument in memory */
        {
           if (cpuarglength > 4096) {
              set_trapcode(dibp, 0x30C);
              back_up_jumper();
              return;
           }
           cpuargaddr = map_arg_string(get_arg_pointer(dibp),cpuarglength);
           if(cpuargaddr) keyjump(je_key);
           release_arg_pages(); 
        }
        break;
      case arg_regs:   /* String argument in registers */
        if (!(cpuargaddr = get_register_string(cpudibp,
                               get_arg_pointer(dibp), cpuarglength))) {
           set_trapcode(dibp, 0x314);
           back_up_jumper();
           return;
        }
        keyjump(je_key);
        break;
      default: set_trapcode(dibp, 0x304);
        back_up_jumper();
        return;
     }}
   else keyjump(je_key);
return;
}
