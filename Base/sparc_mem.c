/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* sparc_mem.c - Memory management for TI SuperSPARC. */

#include "sysdefs.h"
#include "string.h"
#include <stdio.h>
#include "keyh.h"
#include "scafoldh.h"
#include "kermap.h"
#include "sparc_mem.h"
#include "locksh.h"
#include "spaceh.h"
#include "dependh.h"
#include "lli.h"
#include "wsh.h"
#include "splh.h"
#include "addrcteh.h"
#include "kktypes.h"
#include "cpujumph.h"
#include "geteh.h"
#include "queuesh.h"
#include "domamdh.h"
#include "jpageh.h"
#include "checkh.h"
#include "kschedh.h"
#include "kerinith.h"
#include "gateh.h"
#include "getih.h"
#include "ioworkh.h"
#include "memomdh.h"
// #include "mmu.h"
#include "sparc_asm.h"
#include "memutil.h"

extern ulong_t cpu_mmu_addr;   /* Address of the memory fault */
extern ulong_t cpu_mmu_fsr;    /* MMU Fault Status Register at the fault */
extern ME RegnTab[2<<log_context_cnt][256]; /* File maps.s allocates these. */
extern ME SegTab[SegTabCnt][64];
#define NULL_TAB 0
extern int ptlb[16];
extern int ptlb_yet;

void markpagedirty(CTE *cte) /* mark page dirty */
{
   cte->flags |= ctchanged;
}

unsigned char * map_window(int window, CTE * cte, int rw) 
/* This changes the kernel's data map to map the specified page frame into
   the window selected by the caller with write access as specified by rw.*/
/* The page frame must not be of type MapFrame. */
{  /* Note that it may now be cached by this cpu. */
   cte->flags |= ctreferenced | (rw ? ctchanged : 0);
   return map_any_window(window, cte->busaddress, rw);
}

short currentContext = kernCtx;
static void set_context(ME userareapointer) /* was set_CMMU */
/* Set user map */
{/* Set the hardware register */
 sta04(0x200 /* = MCTX, Context Register */, userareapointer);
 currentContext = userareapointer;
 check_seg_map(userareapointer, 0 /* ... */);
}

void set_memory_management(void)
/* Sets hardware mapping to map cpudibp's address segments. */
{if(0)set_context(cpudibp->map); /* set inst CMMU */}
/* The above 'if(0)' is to defang the sta04 which is wrong for the
Sparc. */

unsigned int forsakensegtabcount = 0,
             forsakenpagtabcount = 0;
CTE *forsakensegtabhead = NULL,
    *forsakenpagtabhead = NULL;

void steal_table(CTE *cte)
{crash("I no longer use page frames to hold maps!");}

void md_unprseg(NODE *node)
/* Unprepare a segment node, machine-dependent part. */
/* Nothing to do on Sparc */
{return;}

void prepare_segment_node(
   NODE *np)  /* node to prepare. Must be unprepared. */
{
   np->prepcode = prepassegment;
   np->pf.maps = NULL;
}

unsigned short mape_hash(ME p)
/* Hash the contents of a map entry */
{  return ((long)p & ~0x44)^(long)p>>16;}


CTE *map_parm_page(uint32 addr, struct DIB *dib)
/* This attempts to locate the CTE of the page at the virtual address
  addr in the data space of a domain.
  Do not call any segment keeper. If any fault, give trap code for
     invalid parm page address.
  Returns NULL if it can't map, otherwise pointer to CTE.
  */
/* dib->rootnode must be preplocked (to prevent dib from disappearing). */
{
   switch(resolve_address(addr, dib, 1/*write*/)) {
    case mem_wait: return NULL;
    case mem_fault: 
/* Don't call any segment keeper. */
      set_trapcode(dib, 9);
      (dib -> trapcodeextension)[0] = addr;
      (dib -> trapcodeextension)[1] = error_code;
      return NULL;
    case mem_ok: return thepagecte;
    default: Panic(); return 0;
   }
}

CTE *map_arg_page(uint32 addr)
/* This attempts to locate the CTE of the page at the virtual address
  addr in the data space of the domain in cpudibp.
  Also tries to fill in the mapping tables.
  If it can't map, backs up jumper and returns NULL,
  otherwise returns pointer to CTE.
  Calls a segment keeper if necessary.
  Faults the domain if no segment keeper.
  */
{
startover:
   switch(resolve_address(addr, cpudibp, 0)) {
    case mem_wait:
      abandonj();
      return NULL;
    case mem_fault: 
      switch(memfault(addr, cpudibp, &cpudibp->rootnode->dommemroot)) {
       case memfault_nokeep:  /* No seg keeper. Call domain keeper. */
         cpudibp->trapcodeextension[0] = addr;
         set_trapcode(cpudibp, 0x200 + 4*error_code);
         break;
       case memfault_redispatch:
         goto startover;
       case memfault_keeper: /* There is a segment keeper. */
         call_seg_keep(faultresume);
      }
      back_up_jumper();
      return NULL;
    case mem_ok: return thepagecte;
    default: Panic(); return 0;
   }
}

const char *find_program_name(void)
/* Find the name of the running program by looking in its memory.
   But, if it can't be found, don't call any keepers.
   Used for debugging.
   If NULL returned, I/O has been initiated, cpuactor queued,
             caller must call putawaydomain().
   Otherwise returns name (may be in clear_win).
 */
{
   const char *name;
   uint32 offset;

   switch (resolve_address(0L, cpudibp, 0)) {
    case mem_wait:
      return NULL;
    case mem_fault:
      switch(resolve_address(0x10000L, cpudibp, 00)) {
         case mem_wait:
           return NULL;
         case mem_fault:
           return "Unknown";
         case mem_ok:
           goto resname; 
      }     
      return "Unknown";
    case mem_ok:
resname:
      name = (char *)map_window(clear_win, thepagecte, 0);  /* look at it */
      if(!Strncmp(name+0x9C,"LSFSIM",6))
         return "LSFSIM";
      else {
         offset=0;
         while(offset < 200) {
           if(!Strncmp(name+offset,"FACTORY",7)) {
              uint32 vaddr
                 = (*(uint32 *)(name+offset+12)); /* get virt addr of name */
              switch (resolve_address(vaddr, cpudibp, 0)) {

               case mem_wait:
                 return NULL;
               case mem_fault:
                 return "Unknown";
               case mem_ok: {
		/* The name may straddle 2 pages. May need to read
		 * an additional page. In either case, store the
		 * name in a local static buffer.
		 */
		static char buf[80];
		const char *cp;
		char straddle = 0; /* 1 if name straddles pages */

                 name = (char *)map_window(clear_win, thepagecte, 0)
                      + (vaddr & 0xFFF);
		 cp = name;
		 while (*cp != '\0') {
			if (((uint_t)++cp & 0xfff) == 0)  {
				/* we've crossed a page boundary */
				straddle = 1;
				break;
			}
		 }
		 Strncpy(buf, name, cp - name);
		 *(buf + (int)(cp - name)) = '\0';
		 if (straddle) { /* get the second page */
		      switch (resolve_address(vaddr+0x1000, cpudibp, 0)) {
		       case mem_wait:
			 return NULL;
		       case mem_fault:
			 return "Unknown";
		       case mem_ok:
			 /* assume name is NULL terminated */
			 name = (char *)map_window(clear_win, thepagecte, 0);
			 Strcat(buf, name);
		       } /* end switch */
		 }
                 return buf;
              }
	      } /* end switch */
           }
           offset++;
         }
         return "Unknown";
      }
      default: Panic(); return 0;
   }
}

void zap_dib_map(struct DIB *dib)
/* This modifies the dib to sever the address segments.
   The dib is still valid. */
{  dib->map=NULL_MAP;
}

static void make_page_ro(CTE *cte, int soft)
{
   union Item *k;

if(lowcoreflags.iologenable){char str[80];
 sprintf(str,"make_page_ro cte=%x\n", (int)cte);
 logstr(str);}
   page_to_rescind = cte; /* Pass this to rescind_write_access */
   Soft = soft;
   /* Do over all involved keys to the page */
   ptlb_yet = 0;
   for (k = cte->use.page.rightchain;
        k != (union Item *)cte && k->key.type & (involvedr+involvedw);
        k = k->item.rightchain) {
      visit_depends(&k->key, rescind_write_access);
   }
   MakeProdRO(cte);
   if (ptlb_yet) {ptlb[15]++;}
}

void mark_page_clean(CTE *cte)
{
   if (cte->ctefmt != PageFrame) crash("MEMO88K376 mark non-page clean");
   make_page_ro(cte, ~4); /* so we will find out if it is changed again */

   cte->flags &= ~ctchanged;
}

void makekro(CTE *cte)
/* Make a page or node pot kernel-read-only. */
{
   switch (cte->ctefmt) {
    case NodePotFrame:
    case AlocPotFrame:
      break;
    default:
      crash("MEMO88K125 makekro of strange frame");
    case PageFrame:
      make_page_ro(cte, ~0x44);
   }

   cte->flags |= ctkernelreadonly;
}

void resetkro(CTE *cte)
/* Make a page or node pot no longer kernel-read-only. */
{
   cte->flags &= ~ctkernelreadonly;
   enqmvcpu(&kernelreadonlyqueue);
}

void mark_page_unreferenced(CTE *cte)
{
   union Item *k;

   if (cte->ctefmt != PageFrame) crash("MEMO88K377 mark non-page unreferenced");
   /* Temporarily rescind access to it
      so we will find out if it is referenced again. */
   page_to_rescind = cte; /* Pass this to rescind_read_access */
   /* Do over all involved keys to the page */
   for (k = cte->use.page.rightchain;
        k != (union Item *)cte && k->key.type & (involvedr+involvedw);
        k = k->item.rightchain) {
      visit_depends(&k->key, rescind_read_access);
   }
   cte->flags &= ~ctreferenced;
}

static int SuspectXvalid=0;
int ipte = 0;

extern MapHeader RgnHeaders[RgnTabCnt]; /* One of these per region table */
extern MapHeader SegHeaders[SegTabCnt]; /* One of these per segment table */
extern MapHeader PagHeaders[PagTabCnt]; /* One of these per page table */
extern ME RgnTabs[RgnTabCnt][256];
extern ME SegTabs[SegTabCnt][64];
extern ME PagTabs[PagTabCnt][64];

void zap_depend_entry(long locator, unsigned short hash)
/* This decodes a map entry locator and zaps that entry.
   This is designed for Depend to call as it selects victims. */
/* Format of a locator:
   If (locator & 3) then locator&~3 is the real address of an
      entry in a mapping table and (locator & 3) is the level of the table
   else locator is the address of some dib.map */
{
   if (locator&3) {/* We are zapping a map entry. */
         ME * mep = (ME *)(locator & ~3);
         ME me = *mep; /* fetch the map entry */
         if(!(me & 3)) {*mep = 0; ++ SuspectXvalid; ptlb[0]++;PTLB();}
/* The line of code above could be improved to preserve the Xvalid entry
   if it proved not to be an alias for the intended entry. On the otherhand
   it might not pay to do the test. */
         else if(mape_hash(me)==hash) {
            ME *maptable;
            MapHeader *mh;
            uint32 zaparg;

            *mep = 0;               /* invalidate table entry */
            switch (locator & 3) {		/* Find maping table header */
             case 1:                             /* Region Table Entry */
               maptable = (ME*)(locator&~(256*4-1));
               mh = &RgnHeaders[(maptable - &RgnTabs[0][0])/256];
               zaparg = (mep-maptable)<<24 | 2<<8;  /* Zap region */
               break;
             case 2:                             /* Segment Table Entry */
               maptable = (ME*)(locator&~(64*4-1));
               mh = &SegHeaders[(maptable - &SegTabs[0][0])/64];
               zaparg = mh->address | (mep-maptable)<<18 | 1<<8; /*Zap segment*/
               break;
             case 3:                             /* Page Table Entry */
               maptable = (ME*)(locator&~(64*4-1));
               mh = &PagHeaders[(maptable - &PagTabs[0][0])/64];
               zaparg = mh->address | (mep-maptable)<<12; /*Zap page*/
               break;
             default: mh = 0; zaparg = 0; Panic();
                // Above is unnecessary, but compiler worries about uninitialized variables.
            }
            if (mh->context != kernCtx) {
               sta04(0x200, mh->context);       /* switch to new context. */
               sta03(zaparg, 0);		        /* Zap part of the TLB */
               sta04(0x200, kernCtx);
            } else {
               ptlb[1]++;
               PTLB();                          /* Purge the whole TLB */
            }
         }
   }
   else /* This is a pointer to the map field of some DIB.
           It is directly in the kernel's map. */
      if (mape_hash(*(long *)locator)==hash) {
         *(long *)locator = NULL_MAP;
         /* if (cpudibp && cpudibp != &idledib)
            set_memory_management();  --  NOP on Sparc */
               /* We could avoid this in most cases. */
      }
}
 
void checkSegMapFrame(
   CTE * cte)
{
   if (cte->iocount) crash("MEMO88K442checkMapFrame iocount");
   if (cte->flags & ctchanged) crash("MEMO88K443checkMapFrame ctchanged");
}

#if LATER 
void checkPagMapFrame(
   CTE * cte)
{
   int n, k, i; char j;
   ME *map = (ME *)map_map_window(check_win, cte, MAP_WINDOW_RO);

   if (cte->iocount) crash("MEMO88K542checkMapFrame iocount");
   if (cte->flags & ctchanged) crash("MEMO88K543checkMapFrame ctchanged");

   /* Check all entries. */
   for (n = 0; n<4; n++)
    for (j = ((char *)&cte->use.map.zones)[n], k=0; j; j+=j, k++)
     if (j<0)
        for(i = 256*n + 32*k; i <  256*n + 32*k + 32; i++) {
           ME me = map[i];
           if (me) {
              CTE *pagecte = addr2cte(me);
              if(me>=endmemory && !(me & PTE_CI))
                 crash("Cached display access.");
              if(pagecte->flags & ctkernelreadonly && !(me&PTE_WP))
                 crash("MEMO88K544 Kernel Read Only but not protected.");
              ckseapformat = cte->use.map.format;
              memobject = !cte->use.map.producernode;
              chbgnode = cte->use.map.bgnode;
              entrylocator = cte->busaddress + i*4 + 1;
              entryhash = mape_hash(me);
              cvaddr.low = i*pagesize;
              check_memory_tree(cte->use.map.producer, 2,
                                cte->use.map.slot_origin);
}       }  }
void memorys(void){}/* Initialization for memory. */

static const char batab[2][16] = {{0,3,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
{0,0,1,0,2,0,0,0,3,0,0,0,2,0,0,0}};

void handle_data_obstacle()
{ 
   pipe_stage *st = &cpudibp->data_access[0];
   while (st->VALID) {
      /* Handle data pipe stage 0 */
      if (st->DAS) crash("DAS bit!");
startover:
      switch (resolve_address(st->DMA, cpudibp,
                              st->WRITE|st->L)) {
         unsigned char *kp;
       case mem_wait: putawaydomain(); return;
       case mem_fault:
         switch (memfault(st->DMA, cpudibp,
                 &cpudibp->rootnode->dommemroot)) {
          case memfault_nokeep:  /* No seg keeper. Call domain keeper. */
            cpuordercode = 0x80010000 + 4*error_code;
            call_domain_keeper();
            break;
          case memfault_redispatch:
            goto startover;
          case memfault_keeper: /* There is a segment keeper. */
            call_seg_keep(faultresume);
         }
         return;
       case mem_ok: /* complete the user's reference */
         kp = map_window(0, thepagecte, st->WRITE|st->L)
                    + (st->DMA & (pagesize-1))
                    + batab[st->BO][st->EN];
            /* All that byte addressing and ordering stuff */
         if (st->L)
            if (st->EN == 15) /* xmem */
               cpudibp->regs[st->DREG] = xmem(st->DMD, (unsigned long *)kp);
            else  /* xmem.b */
               cpudibp->regs[st->DREG] = xmemb(st->DMD, kp);
         else { /* do non xmem stuff */
            if (st->WRITE)
               switch (st->EN) {
                case 15: *(long *)kp = st->DMD; break;
                case 12: case 3: *(uint16 *)kp = st->DMD; break;
                case 1: case 2: case 4: case 8: * kp = st->DMD; break;
               }
            else {
               switch (st->EN) {
                case 15: {
                  cpudibp->regs[st->DREG] = *(uint32 *)kp;
                  /* cpudibp->dibSSRB &= ~(1<<st->DREG); */
                  if (st->DOUB1) {
                     int w=st->DREG+1 & 31;
                     cpudibp->regs[w] = *(uint32 *)(4^(uint32)kp);
                       /*   cpudibp->dibSSRB &= ~(1<<w);*/
                  }
                  break;
                }
                case 12: case 3:
                  cpudibp->regs[st->DREG] =
                       st->SD ? *(sint16 *)kp :*(uint16 *)kp;
                  break;
                case 1: case 2: case 4: case 8:
                  cpudibp->regs[st->DREG] =
                     st->SD ? *(char *)kp :*(unsigned char *)kp; break;
               }
               cpudibp->regs[0] = 0 /* easier than testing above */;
            }
         }
      }

      if(cpudibp->data_access[1].VALID) {
         cpudibp->data_access[0]=cpudibp->data_access[1];
         cpudibp->data_access[1]=cpudibp->data_access[2]; 
      }
      else cpudibp->data_access[0]=cpudibp->data_access[2];
      cpudibp->data_access[2].VALID=0;
   }
}
#endif

#define xcvC 0
#if xcvC
static char xcv[64]; // peek 3
static xcvx = 0;
int xcvZ[] = {64, (int)xcv, 4, (int) &xcvx, 0};
#else
int xcvZ[] = {0};
#endif

void handle_data_obstacle()
// Some domain has reached the point where it cannot proceed further due
// to a user mode instruction that is blocked my the MMU.
// The reason may be an invalid address or a store instruction
// for a read-only address.
// cpu_mmu_addr and cpu_mmu_fsr have just been filled from the
// hardware MMU unit. cpu_mmu_addr is irrelevant for invalid instruction
// fetches.
{
#if xcvC
void z(char q){xcv[xcvx++&63] = q;}
#else
void z(char q){}
#endif
z(0);
startover: z(1); {int write = (cpu_mmu_fsr >> 7) & 1;
 switch (resolve_address(cpu_mmu_addr, cpudibp, write)) {
       case mem_wait: z(2); putawaydomain(); return;
       case mem_fault: z(3);
         kldge(write);
         switch (memfault(cpu_mmu_addr, cpudibp,
                 &cpudibp->rootnode->dommemroot)) {
          case memfault_nokeep:  z(4); /* No seg keeper. Call domain keeper. */
/* trap is either 9 (data) or 21 (instruction) */
            if( cpu_mmu_fsr & 0x40/*MMU_SFSR_AT_INSTR*/) { z(5); /* inst */
                set_trapcode(cpudibp,0x21);
                cpuordercode = 0x80010021;
            }
            else { z(6);  /* data */
                set_trapcode(cpudibp,0x09);
                cpuordercode = 0x80010009;
            }
            cpudibp->trapcodeextension[0]=cpu_mmu_addr;
            cpudibp->trapcodeextension[1]=error_code;
            call_domain_keeper();
            break;
          case memfault_redispatch: z(7);
            goto startover;
          case memfault_keeper: z(8); /* There is a segment keeper. */
            call_seg_keep(faultresume);
         }
         return;
       case mem_ok: z(9); return;
}}   }

int PurgeCount;
void PTLB(){sta03(4<<8, 0); /* Purge eitire TLB, or DeMap, as Sun would say. */
            ++PurgeCount;}
