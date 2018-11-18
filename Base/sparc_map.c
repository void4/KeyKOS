/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keyh.h"
#include <string.h>
#include "sysdefs.h"
#include "sparc_mem.h"
#include "sparc_map.h"
#include "sparc_asm.h"
#include "memomdh.h"
#include "wsh.h"
#include "addrcteh.h"
#include "checkh.h"
#include "checmdh.h"
#include "realkermap.h"
#include "dependh.h"
#include "geteh.h"
#include "percpu.h"
#include "queuesh.h"
#include "gateh.h"
#include "memutil.h"

extern ulong_t cpu_mmu_addr;   /* Address of the memory fault */
extern ulong_t cpu_mmu_fsr;    /* MMU Fault Status Register at the fault */


void TLBck(void);
/* More development required to turn this switch on. This would
presumably enable reducing TLB purges by allowing more context
numbers than we have region tables. The context map would be sparse. */

#define HC 1
#define UniTabCount 1 /* Bogus ... */
#define UniCnt 1 /* Bogus ... */
#define CtxTabCount 1
#define dumbAlloc 1

       ME UniTabs[UniTabCount][1]; /* Bogus ... */
extern ME CtxTabs[1][CtxCnt];
extern ME RgnTabs[RgnTabCnt][256];
extern ME SegTabs[SegTabCnt][64];
extern ME PagTabs[PagTabCnt][64];
int ptlb[16] = {0};
int ptlb_yet;
void PTLB(void);
static void RgnStep(void); static void SegStep(void); static void PagStep(void);
static void checkm(void);

#define UniN 2
#define CtxN 2
#define RgnN 2
#define SegN 3
#define PagN 4
#define BytN 6
#define nk240 240 /* number of regions per context valid in addr segment. */

void CtxRetainTabFrame(int);
void RgnRetainTabFrame(int);
void SegRetainTabFrame(int);
void CtxFixXvalidEntry(ME *);
void RgnFixXvalidEntry(ME *);
void SegFixXvalidEntry(ME *);

uint32 disp=0; /* pa = va + disp */

extern MapHeader HeaderZero[1];
/* HeaderZero is not actually used. Makes chain index values nice. */
extern MapHeader RgnHeaders[RgnTabCnt]; /* One of these per region table */
extern MapHeader SegHeaders[SegTabCnt]; /* One of these per segment table */
extern MapHeader PagHeaders[PagTabCnt]; /* One of these per page table */
ME BytTabs[1][1]; /* Not worth an explanation!! */

void assert(int t, char * mes)
  {if(!t) crash(mes);}

/* StepRgn and its kin make headway by causing RgnFreeTabs (etc.)
  to be non null. */




uint32 UniWrapCount=0, UniTarget=0; /* Bogus ... */

uint32 RgnWrapCount=0, RgnTarget=0;
int RgnWornTabs = -1, RgnWornTabCnt=0;
int RgnFreeTabs=0;
static int Ctx2RgnTI(ME x) {
  {if((x & 3) == 1) return ((ME *) ((x & ~3)<< 4) - RgnTabs[0]) >> 8;
     if((x >> 2 & 0xFF) == RgnHeaders[x>>10].allCount) return x>>10;}
return -1;}

static void RgnWorryCountWrap(){if(RgnWornTabCnt<<4 > RgnTabCnt)
  /* Too many region table frames worn out. */
  {int j, k; for(j=0; j<CtxTabCount; ++j) for(k=0; k < CtxCnt; ++k)
     {ME * mep = &CtxTabs[j][k]; ME me = * mep;
        if(me) if(!((int)me & 3)) /* Found an Xvalid entry. */
           {if(((int)me>>2 & 0xFF) != RgnHeaders[me>>10].allCount) * mep = 0;
            else * mep = (me & ~(0xFF <<2)) | 4;}}
  for(j=0; j < RgnTabCnt; ++j) RgnHeaders[j].allCount = 1;
  RgnWornTabCnt = 0;
  {int cr = RgnWornTabs;
    while(RgnHeaders[cr].StepCount>=0) cr = RgnHeaders[cr].StepCount;
    RgnHeaders[cr].StepCount = RgnFreeTabs;
    RgnFreeTabs = RgnWornTabs; RgnWornTabs = -1;}
} }

int contextSweepCount=0;

static void RgnStep(){
  ++RgnTarget; if(RgnTarget == RgnTabCnt)
     {RgnTarget = 0; ++RgnWrapCount; RgnWorryCountWrap();}
  if(RgnTarget == kernCtx) return;
  {char state = RgnHeaders[RgnTarget].State;
   if(state == 0) return;
   if(state == 1) {int j; for(j=0; j<nk240; ++j)
/* obscure kernelsize dependency above */
      {  ME * mep = &RgnTabs[RgnTarget][j]; ME me = *mep;
         if(me) if ((me & 3) == 1)
            {int ti = ((ME *)(((me & ~3)<<4) - disp) - &SegTabs[0][0]) >> 6;
             /* ti is designated segment table index */
             * mep = ti << 10 | (SegHeaders[ti].allCount << 2);}}
       RgnHeaders[RgnTarget].State = 2;
       RgnHeaders[RgnTarget].PurgeAge = PurgeCount;
       RgnHeaders[RgnTarget].StepCount =
           contextSweepCount;
    }
    else if(state < RgnN) RgnHeaders[RgnTarget].State = state+1;
    else /* We have found our next victim. */
      {int j; if(RgnHeaders[RgnTarget].allCount)
         for(j=0; j<nk240; ++j) RgnTabs[RgnTarget][j] = 0;
       ++RgnHeaders[RgnTarget].allCount;
       RgnHeaders[RgnTarget].State = 0;
       if(RgnHeaders[RgnTarget].PurgeAge == PurgeCount) {ptlb[4]++;PTLB();}
          if(contextSweepCount == RgnHeaders[RgnTarget].StepCount)
             {int j; ++contextSweepCount;
              for(j=0; j<maxdib; ++j)
               if(firstdib[j].map == RgnTarget)
                  firstdib[j].map = NULL_MAP;}
       if(!~RgnHeaders[RgnTarget].allCount)
          {++RgnWornTabCnt;
           RgnHeaders[RgnTarget].StepCount = RgnWornTabs;
           RgnWornTabs = RgnTarget;}
       else {RgnHeaders[RgnTarget].StepCount = RgnFreeTabs;
       RgnFreeTabs = RgnTarget;}
} } }

void RgnRetainTabFrame(int ndx) /* assert RgnHeader[ndx].State > 1 */
{int j; for(j=0; j<nk240; ++j)
  {ME * mep = &RgnTabs[ndx][j]; ME me = * mep;
   if(me) {int sbdx = me>>10;
     if(SegHeaders[sbdx].allCount == (me>>2 & 0xFF))
     if(SegHeaders[sbdx].State == 1) * mep = ((int)RgnTabs[sbdx] >> 4) + 1;
       else ;
     else * mep = 0;}}
 RgnHeaders[ndx].State = 1;}

void SegRetainTabFrame(int);
void RgnFixXvalidEntry(ME * mep)
{ME me = *mep;
 int ti = ((ME *)(((uint32)me<<4 & ~0x3FF) - disp)
                 - &RgnTabs[0][0]) >> 8;
   if(RgnHeaders[ti].State > 1) RgnRetainTabFrame(ti);
   {int tx = *mep >>10; if(SegHeaders[tx].State > 1) SegRetainTabFrame(tx);
    * mep = SegHeaders[tx].allCount == (me>>2 & 0xFF) ?
            ((int)RgnTabs[tx] >> 4) + 1 : 0;
}}

/* remProd, below, removes maps in a selected range from the product chains
   of all nodes. It was used to remove all maps at one level. */
static void remProd(MI lo, int many){
CTE *pp; NODE *np; MI hi=lo+many;
for(np = firstnode; np < anodeend; ++np) if(np->prepcode == prepassegment)
   {MI * hp = &np->pf.maps, h;
     while((h = *hp)) if(lo < h && h <= hi)
        *hp = HeaderZero[h].coProd;
        else hp = &HeaderZero[h].coProd;}
for(pp = firstcte; pp < lastcte; ++pp) if(pp->ctefmt == PageFrame)
   {MI * hp = &pp->use.page.maps, h;
     while((h = *hp)) if(lo < h && h <= hi)
        *hp = HeaderZero[h].coProd;
        else hp = &HeaderZero[h].coProd;}}

MI stealRgnTabFrame(){MI x;
   while(!~(x=RgnFreeTabs)) if(dumbAlloc)
     {/* The code below is for when we exhaust the set of region tables. */
       {int j; for(j=1; j<RgnTabCnt-1; ++j) /* Skips NULL_MAP & kernCtx. */
         {if(RgnHeaders[j].lock) crash("long locked region table");
         RgnHeaders[j].StepCount = RgnFreeTabs; RgnFreeTabs = j;}}
     remProd(0, RgnTabCnt);
     {int k; for(k=0; k<maxdib; ++k) firstdib[k].map = NULL_MAP;}}
 else RgnStep();
   memzero((char*)&RgnTabs[x][0], nk240*4);
   RgnFreeTabs = RgnHeaders[x].StepCount;
   return x;}


uint32 SegWrapCount=0, SegTarget=0;
int SegWornTabs = -1, SegWornTabCnt=0;
int SegFreeTabs=0;
static int Rgn2SegTI(ME x) {
  if((x & 3) == 1) return ((ME *) ((x & ~3)<< 4) - SegTabs[0]) >> 6;
  if((x >> 2 & 0xFF) == SegHeaders[x>>10].allCount) return x>>10;
  return -1;}

static void SegWorryCountWrap(){if(SegWornTabCnt<<4 > SegTabCnt)
  /* Too many region table frames worn out. */
  {int j, k; for(j=0; j<RgnTabCnt; ++j) for(k=0; k < RgnCnt; ++k)
     {ME * mep = &RgnTabs[j][k]; ME me = * mep;
        if(me) if(!((int)me & 3)) /* Found an Xvalid entry. */
           {if(((int)me>>2 & 0xFF) != SegHeaders[me>>10].allCount) * mep = 0;
            else * mep = (me & ~(0xFF <<2)) | 4;}}
  for(j=0; j < SegTabCnt; ++j) SegHeaders[j].allCount = 1;
  SegWornTabCnt = 0;
  {int cr = SegWornTabs;
    while(SegHeaders[cr].StepCount>=0) cr = SegHeaders[cr].StepCount;
    SegHeaders[cr].StepCount = SegFreeTabs;
    SegFreeTabs = SegWornTabs; SegWornTabs = -1;}
} }

static void SegStep(){
  ++SegTarget;
  if(SegTarget == SegTabCnt)
     {SegTarget = 0; ++SegWrapCount; SegWorryCountWrap();}
  {char state = SegHeaders[SegTarget].State;
   if(state == 0) return;
   if(state == 1) {int j; for(j=0; j<64; ++j)
      {  ME * mep = &SegTabs[SegTarget][j]; ME me = *mep;
         if(me) if ((me & 3) == 1)
            {int ti = ((ME *)(((me & ~3)<<4) - disp) - &PagTabs[0][0]) >> 6;
             /* ti is designated segment table index */
             * mep = ti << 10 | (SegHeaders[ti].allCount << 2);}}
       SegHeaders[SegTarget].State = 2;
       SegHeaders[SegTarget].PurgeAge = PurgeCount;
       SegHeaders[SegTarget].StepCount =
           RgnWrapCount*RgnTabCnt + RgnTarget;}
    else if(state < SegN) SegHeaders[SegTarget].State = state+1;
    else /* We have found our next victim. */
      {int j; if(SegHeaders[SegTarget].allCount)
         for(j=0; j<(3==2 ? nk240 : 64); ++j) SegTabs[SegTarget][j] = 0;
       ++SegHeaders[SegTarget].allCount;
       SegHeaders[SegTarget].State = 0;
       if(SegHeaders[SegTarget].PurgeAge == PurgeCount) {ptlb[6]++;PTLB();}
       while(RgnWrapCount*RgnTabCnt + RgnTarget
             < SegHeaders[SegTarget].StepCount) RgnStep();
       if(!~SegHeaders[SegTarget].allCount)
          {++SegWornTabCnt;
           SegHeaders[SegTarget].StepCount = SegWornTabs;
           SegWornTabs = SegTarget;}
       else {SegHeaders[SegTarget].StepCount = SegFreeTabs;
       SegFreeTabs = SegTarget;}
} } }

void SegRetainTabFrame(int ndx) /* assert SegHeader[ndx].State > 1 */
{int j; for(j=0; j<64; ++j)
  {ME * mep = &SegTabs[ndx][j]; ME me = * mep;
   if(me) {int sbdx = me>>10;
     if(PagHeaders[sbdx].allCount == (me>>2 & 0xFF))
       if(PagHeaders[sbdx].State == 1) * mep = ((int)SegTabs[sbdx] >> 4) + 1;
       else ;
     else * mep = 0;}}
 SegHeaders[ndx].State = 1;}

void PagRetainTabFrame(int);
void SegFixXvalidEntry(ME * mep)
{ME me = *mep;
 int ti = ((ME *)(((uint32)me<<4 & ~0xFF) - disp)
                 - SegTabs[0]) >> 6;
   if(SegHeaders[ti].State > 1) SegRetainTabFrame(ti);
   {int tx = *mep >>10; if(PagHeaders[tx].State > 1) PagRetainTabFrame(tx);
    * mep = PagHeaders[tx].allCount == (me>>2 & 0xFF) ?
            ((int)SegTabs[tx] >> 4) + 1 : 0;
}}

MI stealSegTabFrame(){MI x;
   while(!~(x=SegFreeTabs)) if(dumbAlloc)
    {/* The code below is for when we exhaust the set of segment tables. */
      {int j; for(j=0; j<SegTabCnt; ++j) 
        {if(SegHeaders[j].lock) crash("long locked segment table");
         SegHeaders[j].StepCount = SegFreeTabs; SegFreeTabs = j;}}
     remProd(RgnTabCnt, SegTabCnt);
    {int j; for(j=0; j<RgnTabCnt; ++j) memzero(&RgnTabs[j][0], nk240*4);}}
  else SegStep();
   memzero((char*)&SegTabs[x][0], 64*4);
   SegFreeTabs = SegHeaders[x].StepCount;
   return x;}


uint32 PagWrapCount=0, PagTarget=0;
int PagWornTabs = -1, PagWornTabCnt=0;
int PagFreeTabs=0;
static int Seg2PagTI(ME x) {
  {if((x & 3) == 2) return (((x & ~0xFF) << 4) - first_user_page)>>12;
         if((x >> 2 & 0x7F) == (PagHeaders[x>>10].allCount & 0x7F)) return x>>10;}
return -1;}

static void PagWorryCountWrap(){if(PagWornTabCnt<<4 > PagTabCnt)
  /* Too many region table frames worn out. */
  {int j, k; for(j=0; j<SegTabCnt; ++j) for(k=0; k < SegCnt; ++k)
     {ME * mep = &SegTabs[j][k]; ME me = * mep;
        if(me) if(!((int)me & 3)) /* Found an Xvalid entry. */
           {if(((int)me>>2 & 0xFF) != PagHeaders[me>>10].allCount) * mep = 0;
            else * mep = (me & ~(0xFF <<2)) | 4;}}
  for(j=0; j < PagTabCnt; ++j) PagHeaders[j].allCount = 1;
  PagWornTabCnt = 0;
  {int cr = PagWornTabs;
    while(PagHeaders[cr].StepCount>=0) cr = PagHeaders[cr].StepCount;
    PagHeaders[cr].StepCount = PagFreeTabs;
    PagFreeTabs = PagWornTabs; PagWornTabs = -1;}
} }

/* The left 24 bits of an Xvalid page table entry are the same as the
   valid form. The right 8 bits are 0000W000 where W is the write 
   permission of the valid form. */
static void PagStep(){
  ++PagTarget;
  if(PagTarget == PagTabCnt)
     {PagTarget = 0; ++PagWrapCount; PagWorryCountWrap();}
  {char state = PagHeaders[PagTarget].State;
   if(state == 0) return;
   if(state == 1) {int j; for(j=0; j<64; ++j)
      {ME * mep = &PagTabs[PagTarget][j]; ME me = *mep;
         if(me && (me & 3) == 2) * mep = me & ~0xF7;}
       PagHeaders[PagTarget].State = 2;
       PagHeaders[PagTarget].PurgeAge = PurgeCount;
       PagHeaders[PagTarget].StepCount =
           SegWrapCount*SegTabCnt + SegTarget;}
    else if(state < PagN) PagHeaders[PagTarget].State = state+1;
    else /* We have found our next victim. */
      {int j; if(PagHeaders[PagTarget].allCount)
         for(j=0; j<64; ++j) PagTabs[PagTarget][j] = 0;
       ++PagHeaders[PagTarget].allCount;
       PagHeaders[PagTarget].State = 0;
       while(SegWrapCount*SegTabCnt + SegTarget
             < PagHeaders[PagTarget].StepCount) SegStep();
       if(!~PagHeaders[PagTarget].allCount)
          {++PagWornTabCnt;
           PagHeaders[PagTarget].StepCount = PagWornTabs;
           PagWornTabs = PagTarget;}
       else {PagHeaders[PagTarget].StepCount = PagFreeTabs;
       PagFreeTabs = PagTarget;}
} } }

void PagRetainTabFrame(int ndx)
{int j; assert(PagHeaders[ndx].State > 1, "Bad Age Logic");
  for(j=0; j<64; ++j)
  {ME * mep = &PagTabs[ndx][j]; ME me = * mep;
   if(me) {
       if(me & 3) crash("Valid entry in probated table");
       * mep = me | 2;
   }}
 PagHeaders[ndx].State = 1;}

MI stealPagTabFrame(){MI x;
   while(!~(x=PagFreeTabs)) if(dumbAlloc)
   {/* The code below is for when we exhaust the set of page tables. */
     {int j; for(j=0; j<PagTabCnt; ++j) 
      {if(PagHeaders[j].lock) crash("long locked segment table");
       PagHeaders[j].StepCount = PagFreeTabs; PagFreeTabs = j;}}
    remProd(RgnTabCnt+SegTabCnt, PagTabCnt);
    {int j; for(j=0; j<SegTabCnt; ++j) memzero(&SegTabs[j][0], 64*4);}}
  else PagStep();
   memzero((char*)&PagTabs[x][0], 64*4);
   PagFreeTabs = PagHeaders[x].StepCount;
   return x;}


int adjusts[3] = {1, RgnTabCnt+1, RgnTabCnt+SegTabCnt+1};

static MI fiatMap(uchar lv, uchar seapform, uchar org, uint32 addr, MI ndx){
  MI *y = memispage ? &memitem->cte.use.page.maps
                    : &memitem->node.pf.maps;
/* The next piece of code uses a kludge for identifying maps where
it is necessary to identify the map type as well as which map element.
This occurs in product chains. One node can produce maps for contexts
(called Rgn maps), regions, and segments. Elements of each of these
map kinds are normally identified by a subscript. A product chain,
rooted at a node or page, may run thru any kind of map.
There is a MapHeader table for each kind of map. These tables are
contiguous. I use this fact to define an index into the three
concatenated tables viewed as one big table. Such coded values occur
in the chain roots, cte.use.page.maps for pages & node.pf.maps
for nodes.
This index is offset by one to allow 0 to serve as end of chain.
RgnHeaders[i] is same as HeaderZero[i+1].
SegHeaders[i] is same as HeaderZero[RgnTabCnt+i+1].
PagHeaders[i] is same as HeaderZero[SegTabCnt+RgnTabCnt+i+1].
*/

  MI x=*y; 
  while(1) {
     if(x) { /* See if this map suits. */
        if ( (   (lv == 2 && x <= RgnTabCnt)
              || (lv == 3 && RgnTabCnt < x && x <= RgnTabCnt+SegTabCnt)
              || (lv == 4 && RgnTabCnt+SegTabCnt <x /* right kind of map? */))
             && HeaderZero[x].format  ==  seapform
             && HeaderZero[x].slot_origin == org
             && HeaderZero[x].bgnode == bgnode
             && HeaderZero[x].chargesetid == chargesetid) {
           if (HeaderZero[x].context != ndx || HeaderZero[x].address != addr) {
              HeaderZero[x].context = kernCtx;
           }
  break;
        }
        x = HeaderZero[x].coProd;	/* Try next map on product chain */
     } else {
        switch(lv) {
         case 2: x = stealRgnTabFrame()+1; break;
         case 3: x = stealSegTabFrame()+RgnTabCnt+1; break;
         case 4: x = stealPagTabFrame()+RgnTabCnt+SegTabCnt+1; break;
        }
        HeaderZero[x].State = 1;
        HeaderZero[x].producer = memitem;
        HeaderZero[x].bgnode = bgnode;
        HeaderZero[x].chargesetid = chargesetid;
        HeaderZero[x].format = seapform;
        HeaderZero[x].slot_origin = org;
        HeaderZero[x].producernode = !memispage;
        HeaderZero[x].checkbit = 0;
        HeaderZero[x].lock = 0;
        HeaderZero[x].coProd = *y; *y = x;
        HeaderZero[x].context = (NULL_MAP==ndx ? x-adjusts[lv-2] : ndx);
        HeaderZero[x].address = addr;
         /* Insert new product on chain. */
  break;
     }
  }
  return x - adjusts[lv-2];
}

void assert(int, char *);
void TLBck()
/* This code checks that any access granted by the current TLB is justified
 by mapping tables including Xvalid entries. This code should be run on various
 CPUs. */
{int i; for(i=0; i<64; ++i) {int ppn = lda06(i<<12 | 2<< 8);
  int vpn = lda06(i<<12 | 0<< 8), ctx = lda06(i<<12 | 1<< 8);
  if(ppn & 32) if((ppn & 3) == 3)
     {assert((ppn & 0x4F) == 0x4F, "Bad TLB entry");
      assert (ctx<CtxCnt, "Bad Context");
      {ME cme =  CtxTabs[1][ctx];
       int rgn = Ctx2RgnTI(cme);
       ME rme =  RgnTabs[rgn][vpn>>24];
       int seg = Rgn2SegTI(rme);
       ME sme =  SegTabs[seg][vpn>>18 & 0x3F];
       int pag = Seg2PagTI(sme);
       ME pme =  PagTabs[pag][vpn>>12 & 0x3F];
       assert(pme == (ppn | 3), "Bad xxx");}}
   assert(0, "bad entryxx");}}

char mods=0;
mem_result retRes(MapHeader *ul, mem_result r)
{
   ul->lock = 0;
   if(HC && !lowcoreflags.trustmem && mods) {
      checkm(); 
      mods=0;
   }
   return r;
}

static u_int
va_to_pa(u_int vaddr){uint32 pres = lda03(vaddr & ~0xfff);
    if (pres) return ((pres & ~0xff) << 4) | (vaddr & 0xfff);
	else return lda03(((vaddr & ~0xfff) | 0x100) & ~0xff) | (vaddr & 0x3ffff);
}

CTE * thepagecte; /* value returned from resolve_address */
/* first_user_page corresponds to CDT[0]
   resolve_address knows all about sparc maps and how to build them.
   It is a colleague of fiatMap that knows how to find suitable maps
   or allocate new empty ones. It calls on search_portion that knows all
   about memory trees. Together with search_potion, resolve_address
   instructs the "depend" module about depend's duties. resolve_address
   attempts to make a given virtual address valid for a given domain.
   It changes Xvalid entries to valid as if provoked by a fault. */

#define xCvC 0
#if xCvC
static char xCv[64]; // peek 4
static int xCvx = 0;
int xCvZ[] = {64, (int)xCv, 4, (int) &xCvx, 0};
#else
int xCvZ[] = {0};
#endif

mem_result resolve_address(uint32 addr, 
           struct DIB *Dib,
           int Write                  /* 1 iff attempting to write, else 0 */)

/* Tries to find the page at the given address in the given Dib's memory. */
/* Dib->rootnode must be preplocked (to prevent Dib from disappearing). */
/* Returns one of:
   mem_ok: thepagecte locates cte of the page.
   mem_wait: I/O has been initiated, cpuactor queued.
             Caller must call putawaydomain().
   mem_fault: the address is unresolved. error_code and
              segkeeperslot are set accordingly.
 */
{
#if xCvC
void z(char q){xCv[xCvx++&63] = q;}
#else
#define z(a) ;
#endif

z(0); z(Write);
   if (addr>>28 == 15) {z(1);
      error_code = 5; 
      segkeeperslot = 0; 
      return mem_fault; 
      // This kernel can't allow domains to use addresses fxxxxxxx.
   }
   {  MI ndx = Dib->map;  z(2);                /* Start block for address valid */
      if (ndx == NULL_MAP) {
         do {
            mem_result rc = search_trunk(8, &Dib->rootnode->dommemroot, 
                                         Dib, Write); z(3);
            if (rc != mem_ok) return rc;                  /* some problem */
            ndx = fiatMap(2, seapformat, 0, 0, ndx);
         } while (!depend_chain_entries((long)&Dib->map   /* entry in a dib */,
                                        mape_hash(ndx)) );
         Dib->map = ndx; 
         if(HC) mods=1;
      }
      /* Now ndx is a valid index into the contexts even if we had to create 
         one or locate an existing suitable one. */
      /* Now the DIB accesses the context. */
      
 while(1) 
      {
        ME *rep = &RgnTabs[ndx][addr>>24], re = *rep; z(4);
        if((re & 3) != 1) { z(5); // Does not locate a region.
           if (re) {z(6);RgnFixXvalidEntry(rep);} // rescued from oblivion!
           else {
              MapHeader * lm = &RgnHeaders[ndx]; z(7);
              if (HC && lm->lock) crash("prelocked"); 
              lm->lock = 1;                 /* Keep safe from scavenger */
              do {
                 mem_result rc = search_portion(addr, Write, lm, 6);z(8);
                 if (rc != mem_ok) {z(9);return retRes(lm, rc);} /* some problem */
                *rep = (va_to_pa((int)&SegTabs[
                     fiatMap(3, seapformat, 0, 
                             addr&0xff000000, ndx)] [0])>>4) | 1;
              } while(!depend_chain_entries((long)rep|1 /* Rgn Tbl Entry */,
                       mape_hash(*rep)) ); 
              if (HC) mods=1; 
              lm->lock = 0;
           }
//           return mem_ok;
           z(10); continue;  // fixed part of the problem
        }
        else { // Rgn locator valid. Must dig deeper.
           ME *sept = (ME *)(((re & ~3)<<4)-disp),
              *sep = sept + (0x3f & (addr>>18)),
               se = *sep; z(11);
           if((se & 3) != 1) { z(12); // Does not locate a segment.
             if (se) {z(13); SegFixXvalidEntry(sep);} // rescued from oblivion!
             else {
               MapHeader * lm = &SegHeaders[(sept-(ME*)&SegTabs[0][0])>>6]; z(14);
               if (HC && lm->lock) crash("prelocked");
               lm->lock = 1;                  /* Keep safe from scavenger */
               do {
                 mem_result rc = search_portion(addr & 0xFFFFFF, Write, lm, 5); z(15);
                 if (rc != mem_ok) {z(16); return retRes(lm, rc);} /* some problem */
                 /* retain (rescue) this table .... ??? */
                 *sep = (va_to_pa((int)&PagTabs[
                               fiatMap(4, seapformat, addr >> 18 & 3,
                                      addr&0xfffc0000, ndx)] [0]) >> 4) | 1;
                 if (depend_chain_entries( (long)sep|2 /* Seg Tbl Entry */,
                                            mape_hash(*sep) ) ) {
                    break;
                 }
              } while (1);
              if (HC) mods=1; 
              lm->lock = 0;
           }              
           z(17); continue;    // fixed part of the problem
//           return mem_ok;
        }

        else { // Seg locator valid. Must dig deeper.

             ME *pept = (ME *)(((se & ~3)<<4)-disp),
                *pep = pept + (0x3f & (addr>>12)),
                 pe = *pep; z(18);
             if ((pe & 3) != 2) { z(19); // Does not locate a page.
               if (pe) crash("This pte should have worked!");
               else 
nullMap:              {
                  MapHeader * lm = &PagHeaders[(pept-(ME*)&PagTabs[0][0])>>6];
                  z(0x14); z(lm->format);
                  if (lm->lock) crash("prelocked");
                  lm->lock = 1; /* Keep safe from scavenger */
                  do {
                        mem_result rc = search_portion((addr & 0x3FFFF) 
                                                    | (lm->slot_origin<<18),
                                            Write, lm, 3); z(21); z(rc);
                        if (rc != mem_ok) {z(22); return retRes(lm, rc);} /* problem */
                        *pep = (memitem->cte.busaddress>>4) | 
                            ((seapformat & 0x80) ? 0x2A : 0x6A);
         // It might seem strange that we give someone RO (0x6A) access
         // when we have just noticed that he has RW access.
         // Just because he has proven write access does not
         // mean that he will modify the page soon or at all.
         // By delaying write access we avoid the cost of cleaning dirty
         // pages that have not in fact been modified.
                  } while(!depend_chain_entries((long)pep|3 /* PagTblEntry*/,
                                                   mape_hash(*pep)) ); 
                  if (HC) mods=1; 
                  lm->lock = 0;
               }
               thepagecte=addr2cte(*pep << 4);
               thepagecte->flags |= ctreferenced;
//               (addr2cte(*pep << 4))->flags |= ctreferenced;
               return mem_ok;
             } else if (Write && !(pe & 4)) {
                     CTE* ct = addr2cte(pe << 4);z(23);

                     if (ct->flags & ctkernelreadonly) {
                        CTE* nct = gcleanmf(ct); z(24);
                        if (!nct) {z(25);
                           enqueuedom(cpudibp->rootnode, &kernelreadonlyqueue);
                           abandonj(); 
                           return mem_wait;
                        } else {z(26);
                           if (ct!=nct) {z(27);
                              if (pe) crash("should have been zapped!");
                              else goto nullMap;
                                 /* This goto is unused for current gcleanmf. */
                           }
                        }
                     }
                     if(!(pe & 0x40)) {error_code = 1; return mem_fault;}
 /* Really read only; not just testing for mods. */
                     ct->flags |= ctchanged;
                     *pep = pe | 4;
                     mods=1; 
                     ptlb[9]++; 
                     PTLB();
                 /*  sta03(addr & ~0xFFF, 0);  purge just one virtual address. 
                     The logic of this is wrong but we should be able to do a
                     selective purge as above and count the wierd cases of an 
                     underprivileged stale TLB entry. */

                     thepagecte=ct;
                     z(28); return mem_ok;
             }
             else { z(29);  // found a legit read only mapping in tables
                thepagecte = addr2cte(pe << 4);
                return mem_ok;
//                crash("Entire mapping valid! What happened?");
             }
        }  // end seg locater valid
    } // end block for valid address
}

        

#if 0
      {  mem_result rc; 
         MapHeader *lm=0;
// The current value in the Fault Status Register (FSR) will not be used.
// The dump variable gets the current value and clears the FSR.
         ME dump = lda04(0x300);
// Instead the probe (lda in the switch below) will produce a new value
// or leave it empty.
// We switch to the domain's context to make the probe instructions refer
// to that space.
         int faultLevel;
         sta04(0x200, ndx);                     /* switch to new context. */
         while(1) { // Each iteration fixes one level deeper.
                    // Previous versions of this code redispatched domain each level.

            {/*  Probe the map for page frame address */
             ME pte = lda03(addr & ~0xFFF);
             if ((pte&3)==2) {
               sta04(0x200, kernCtx);
               thepagecte = addr2cte((pte & ~0xff)<<4);
               if ((pte&4) || !Write) {
                 if (HC && !lowcoreflags.trustmem && mods) {
                   checkm(); 
                   mods=0;
               }
               return mem_ok;
             }
             else {error_code = 1;  // Needs write, but RO access.
                 return mem_fault;}
             }}
            {int fs = lda04(0x300/* MFSR */);
             if(fs>>10 & 0x1f) crash ("Bus error in map");
             faultLevel = fs==0?3:fs>>8 & 3; // no fault if pte available!!!
             }

             switch(faultLevel) {
             case 0: crash("Bad context table entry!");
             case 1: {                     /* Invalid entry in Region Table */
               ME *rep = &RgnTabs[ndx][addr>>24], re = *rep;

               if (re) RgnFixXvalidEntry(rep);
               else {
                  lm = &RgnHeaders[ndx]; 
                  if (HC && lm->lock) crash("prelocked"); 
                  lm->lock = 1;                 /* Keep safe from scavenger */
                  do {
                     rc = search_portion(addr, Write, lm, 6);
                     if (rc != mem_ok) return retRes(lm, rc); /* some problem */
                    *rep = (va_to_pa((int)&SegTabs[
                         fiatMap(3, seapformat, 0, 
                                 addr&0xff000000, ndx)] [0])>>4) | 1;
                  } while(!depend_chain_entries((long)rep|1 /* Rgn Tbl Entry */,
                           mape_hash(*rep)) ); 
                  if (HC) mods=1; 
                  lm->lock = 0;
               }
               break;
             }
             case 2: {		          /* Invalid entry in Segment Table */
               ME *sept = (ME *)(((lda03(addr & ~0xFFF | 0x200) &-4)<<4)-disp),
                  *sep = sept + (0x3f & (addr>>18)),
                   se = *sep;
               if (se) {
                  SegFixXvalidEntry(sep);
               } else {
                  lm = &SegHeaders[(sept-(ME*)&SegTabs[0][0])>>6];
                  if (HC && lm->lock) crash("prelocked");
                  lm->lock = 1;                  /* Keep safe from scavenger */
                  do {
                     rc = search_portion(addr & 0xFFFFFF, Write, lm, 5);
                     if (rc != mem_ok) return retRes(lm, rc); /* some problem */
                     /* retain (rescue) this table .... ??? */
                     *sep = (va_to_pa((int)&PagTabs[
                                   fiatMap(4, seapformat, addr >> 18 & 3,
                                          addr&0xfffc0000, ndx)] [0]) >> 4) | 1;
                     if (depend_chain_entries( (long)sep|2 /* Seg Tbl Entry */,
                                                mape_hash(*sep) ) ) {
                        break;
                     }
                  } while (1);
                  if (HC) mods=1; 
                  lm->lock = 0;
               }              
               break;
             }
             case 3: {                        /* Invalid entry in Page Table */
               ME *pept = (ME *)(((lda03(addr & ~0xFFF | 0x100)&-4)<<4)-disp),
                  *pep = pept + (0x3f & (addr>>12)),
                   pe = *pep;
               if (pe) {
                  if (Write && !(pe & 4)) {
                     CTE* ct = addr2cte(*pep << 4);

                     if (ct->flags & ctkernelreadonly) {
                        CTE* nct = gcleanmf(ct);
                        if (!nct) {
                           enqueuedom(cpudibp->rootnode, &kernelreadonlyqueue);
                           abandonj(); 
                           return retRes(lm, mem_wait);
                        } else {
                           if (ct!=nct) {
                              if (*pep) crash("should have been zapped!");
                              else goto nullMap;
                                 /* This goto is unused for current gcleanmf. */
                           }
                        }
                     }
                     if(!(pe & 0x40)) goto nullMap;
 /* Really read only; not just testing for mods. */
                     ct->flags |= ctchanged;
                     *pep = pe | 4;
                     mods=1; 
                     ptlb[9]++; 
                     PTLB(); 
                 /*  sta03(addr & ~0xFFF, 0);  purge just one virtual address. 
                     The logic of this is wrong but we should be able to do a
                     selective purge as above and count the wierd cases of an 
                     underprivileged stale TLB entry. */
                  } else crash("This pte should have worked!");
               } else 
nullMap:          {  lm = &PagHeaders[(pept-(ME*)&PagTabs[0][0])>>6];
                     if (lm->lock) crash("prelocked");
                     lm->lock = 1; /* Keep safe from scavenger */
                     do {
                        rc = search_portion((addr & 0x3FFFF) 
                                                    | (lm->slot_origin<<18),
                                            Write, lm, 3);
                        if (rc != mem_ok) return retRes(lm, rc); /* problem */
                        *pep = (memitem->cte.busaddress>>4) | 
                            ((seapformat & 0x80) ? 0xAA : 0xEA);
                     } while(!depend_chain_entries((long)pep|3 /* PagTblEntry*/,
                                                   mape_hash(*pep)) ); 
                     if (HC) mods=1; 
                     lm->lock = 0;
                   } 
                   (addr2cte(*pep << 4))->flags |= ctreferenced;
                   break;
             } /* End block for case 3 */
            } /* End switch */
         } /* End while(1) */
      } /* End block using valid user context */
#endif
   } /* End block for address valid */
} /* End resolve_address */

CTE *page_to_rescind=0; int Soft=0;
void rescind_write_access(uint32 locator, unsigned short hash)
/* Procedure to rescind write access to the page in page_to_rescind.
   Called for each map entry that might be to the page. */
{
   if (/*locator&1 &&*/ PagTabs[0] < (ME *)locator &&
                 (ME *)locator < PagTabs[PagTabCnt])
  { /* locator locates a page table entry. */
   ME * mep = (ME *)(locator-3); /* Remove flag bit. */
   ME me = *mep;   /* The map entry that the locator locates. */
   if(me)
     {if(me & 3)
        {if ((me & ~ 0xFF) == page_to_rescind->busaddress >> 4
                  /* The page table entry indeed points to this page. */
              && (me & 4)) /* and not already write protected */
                 {*mep = me &= Soft; /* Set write protect. */
                   if (!ptlb_yet) ptlb_yet=1;
		   ptlb[10]++;PTLB();}}
      else crash("Should be no Xvalid page table entries!");}
}   }

void rescind_read_access(uint32 locator, unsigned short hash)
/* Procedure to rescind write access to the page in page_to_rescind.
   Called for each map entry that might be to the page. */
{
   if (/*locator&1 &&*/ PagTabs[0] < (ME *)locator &&
                 (ME *)locator < PagTabs[PagTabCnt])
  { /* locator locates a page table entry. */
   ME * mep = (ME *)(locator-3); /* Remove flag bit. */
   ME me = *mep;   /* The map entry that the locator locates. */
   if(me) {
     if(me & 3) {
        if ((me & ~ 0xFF) == page_to_rescind->busaddress >> 4)
                  /* The page table entry indeed points to this page. */
             {*mep = 0; /* Deny all access. */
              ptlb[12]++;PTLB();}}
     else crash("Should be no Xvalid page table entries!");}
}   }

void MakeProdRO(CTE *cte){MI mhi = cte-> use.page.maps;
  while(mhi){
     if(mhi > RgnTabCnt+SegTabCnt && mhi<= RgnTabCnt+SegTabCnt+PagTabCnt) {
        if(0) PagTabs[mhi-1-RgnTabCnt-SegTabCnt][0] &= Soft;
        else {int j = mhi-1-RgnTabCnt-SegTabCnt; PagTabs[j][0]&= Soft;}}
     mhi = HeaderZero[mhi].coProd;}}

extern ME kRgnT[256];
static void verf(int j){if (va_to_pa(j) != j + disp)
   crash("Need another kernel.");}
/* The code above verifies that kernel virtual memory is displaced
   from real memory by a constant called disp. If disp were 0
   some kernel code would a little simpler and faster. */

static void minit(){
 {int j; disp = va_to_pa((int)&RgnTabs[0][0]) - (uint32)&RgnTabs[0][0];
  for(j=(int)&RgnTabs[0][0]; j < (int)&RgnTabs[RgnTabCnt-1][256-1];
     j += 4096) verf(j); verf((int)&RgnTabs[RgnTabCnt-1][256-1]);
  for(j=(int)&SegTabs[0][0]; j < (int)&SegTabs[SegTabCnt-1][64-1];
     j += 4096) verf(j); verf((int)&SegTabs[SegTabCnt-1][64-1]);
  for(j=(int)&PagTabs[0][0]; j < (int)&PagTabs[PagTabCnt-1][64-1];
     j += 4096) verf(j); verf((int)&PagTabs[PagTabCnt-1][64-1]);
  }
  {int j; for(j=0; j<=RgnTabCnt+SegTabCnt+PagTabCnt; ++j) 
      HeaderZero[j].lock=0;}
  {int i, j; for(i=0; i<RgnTabCnt; ++i) for(j=0xf0; j<0x100; ++j)
           RgnTabs[i][j] = kRgnT[j];}
     /* Those two lines permanently map the kernel into all domains */
{int j; for(j=0; j<RgnTabCnt-1; ++j) // Skips kernCtx
   CtxTabs[0][j] = (va_to_pa((int)&RgnTabs[j][0])>>4) | 1;}
{if(sizeof(MapHeader) > 32) crash("Fix sizeofMapHeader in sparc_asm.s");}}

void memorys(){minit();
  init_mem();
  RgnFreeTabs=SegFreeTabs=PagFreeTabs=~0;}

static void check_misc(MI i){
if(HeaderZero[i].bgnode != chbgnode) crash("Bad background");
if(HeaderZero[i].format != ckseapformat) crash("Bad checked format");
if(HeaderZero[i].producernode != !memobject) crash("Bad memory type");}

static int mop;
static Producer * mwalk(Producer *np, MI k, uint32 addr, ME* where, char lv)
{   int locind = (lv==3 ? 3 : 7-lv);
    ckseapformat = HeaderZero[k].format; chbgnode = HeaderZero[k].bgnode;
    cvaddr = (uint64)addr; memobject = mop;
    entrylocator = (uint32)where|locind;
    entryhash = mape_hash(*where);
    if(HeaderZero[k].lock) crash("Locked map");
    return check_memory_tree(np, lv-1, 0);
}

void checkProdChain(MI mhi, Producer * p){while(mhi){
    if(mhi<=RgnTabCnt){MI j = mhi-1; int k;
       for(k=0; k<nk240; ++k) {ME *rep=&RgnTabs[j][k], re= *rep; if(re)
         {int si = (((re&~1)<<4)-disp-(uint32)&SegTabs[0][0])>>8; 
          if(mwalk(p, mhi, k<<24, rep, 6)
             != SegHeaders[si].producer) crash("Wrong region");
          check_misc(si+1+RgnTabCnt);
          if (cvaddr) crash("Excess address");}}}
    else if(mhi<= RgnTabCnt+SegTabCnt){MI j = mhi-1-RgnTabCnt; int k;
     for(k=0; k<64; ++k) {ME *sep=&SegTabs[j][k], se= *sep; if(se)
       {int pti = (((se&~1)<<4)-disp-(uint32)&PagTabs[0][0])>>8; 
        if(mwalk(p, mhi, k<<18, sep, 5) != PagHeaders[pti].producer)
           crash("Wrong segment");
        check_misc(pti+1+RgnTabCnt+SegTabCnt);
        if(cvaddr != PagHeaders[pti].slot_origin<<18) crash("Skew org");}}}
    else if(mhi<= RgnTabCnt+SegTabCnt+PagTabCnt)
     {MI j = mhi-1-RgnTabCnt-SegTabCnt; int k;
      for(k=0; k<64; ++k) {ME *pep=&PagTabs[j][k], pe= *pep; if(pe)
        {if((CTE*)
          mwalk(p, mhi, PagHeaders[j].slot_origin<<18 | k<<12, pep, 3) 
         != addr2cte((pe&~3)<<4)) crash("Wrong page");
         if(ckseapformat & 0x80) {if(pe & 0x44) crash("Invalid write access");}
         else if((pe & 0x7b) != 0x6A) crash("Screwy bits in pte");}}}
    else crash ("Bad product chain.");
    if(HeaderZero[mhi].producer != p) crash("Non reflexive product!");
    mhi = HeaderZero[mhi].coProd;}}
    
void check_prepassegment_md(NODE *np)
  {mop=0; checkProdChain(np->pf.maps, (Producer *)np);}

void check_mem_props_md(NODE* np){MI *mi = &np->pf.dib->map, me = *mi;
  if(me){ckseapformat = 0; chbgnode=0; cvaddr=0;
    entrylocator = (long)mi; entryhash = mape_hash(me);
    {Producer *rp = check_memory_tree(check_memory_key(&np->dommemroot), 7, 0);
     if(RgnHeaders[me].producer != rp) crash("Wrong memory root");}}}

void check_page_maps(CTE*cte) 
 {mop = 1; checkProdChain(cte->use.page.maps, (Producer *)cte);}

extern int checkmf;
static void checkm(){NODE*np; static int q = 0; CTE * cte; if(!(++q & checkmf)) {
  for(cte = firstcte; cte < lastcte; ++cte) check_page_maps(cte);
  for(np = firstnode; np < anodeend; ++np)
    if(np->prepcode == prepassegment) check_prepassegment_md(np);
    else if(np->prepcode == prepasdomain) check_mem_props_md(np);
{ int j, k; for(j=0; j<PagTabCnt; ++j) for(k=0; k<64; ++k)
  {int e = PagTabs[j][k]; if(e) {int fl = addr2cte((e & ~0xff) <<4) -> flags;
  if((4 & e) && !(fl & ctchanged))
     crash("Writable page without ctchanged set.");
  }}}}
}

ME * oKRT = 0; // a real address of kernel's region table, built by micro_loader.
void kernMap(){
    // See <http://internal/~norm/KK/Map.html#kernMap>
   { ME oldCtxTab = lda04(0x100)<<4;
      {int j; for(j=0; j<256; ++j) CtxTabs[0][j] = lda20(oldCtxTab+4*j);}
      if(0) CtxTabs[0][kernCtx] = CtxTabs[0][1];
      else {oKRT = (ME *)((lda20(oldCtxTab + 4) & ~3) << 4);
               {int j; for(j=0; j<256; ++j) kRgnT[j] =
//                      j<nk240?0:(j<254?lda20((int)(oKRT + j)):0);
                       j<nk240?0:lda20((int)(oKRT + j));
                       }
             CtxTabs[0][kernCtx] = (ME) (va_to_pa((int)kRgnT)>>4 | 1);}}
    sta04(0x100, va_to_pa((uint32)&CtxTabs[0][0])>>4);
    /* Behold we are in the new universe -- */
    sta04(0x200, kernCtx);
    sta03(0x400, 0); // purge entire TLB
    /* and now in the new map! */}
