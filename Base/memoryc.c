/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "kktypes.h"
#include "sysdefs.h"
#include "types.h"
#include "cvt.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "prepkeyh.h"
#include "queuesh.h"
#include "dependh.h"
#include "locksh.h"
#include "gateh.h"
#include "geteh.h"
#include "kschedh.h"
#include "cpumemh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "kermap.h"
#include "memutil.h"

static uint64 segkeeperaddr=0; /* Address for the segment keeper */
static uint64 savedsegkeeperaddr=0;  /* KLUDGE for gate logging */
static uint64 vaddr=0;         /* The current virtual address */

struct Key *segkeeperslot=0;   /* Pointer to the node slot with segment keeper
                                slot key, OR 0 if keeper unknown, OR
                                1 if no keeper */

static NODE *segkeepnode=0;   /* If segkeeperslot isn't 0 or 1, pointer to the
                                 node with segment keeper key */
 
static int memneedwrite=0;            /* 0==r/o, 1==r/w */
int memispage=0;               /* 0==memitem is node, */
                                    /* 1==memitem is cte */
Producer *memitem=0;   /* Object of current mem key */
static struct Key *memkey=0;          /* Current memory key */
csid chargesetid=0;   /* The charge set id in effect */
static int memlss=0;           /* lss for current place in tree */
int seapformat=0;              /* ro+nc plus the lss */
NODE *bgnode=0;      /* Node with the background key in effect */
int error_code=0;          /* The memory fault code */

void savesegkeeperaddr() {savedsegkeeperaddr=segkeeperaddr;}
void restoresegkeeperaddr() {segkeeperaddr=savedsegkeeperaddr;}
 
int args_overlap_with_page(   /* Check for overlap with page */
   CTE *p)                            /* The page to check */
/* Returns 1 if arg and passed pages overlap, else 0 */
{  return cpumempg[0] == p || cpumempg[1] == p;
}
 
int memory_args_overlap()       /* Check for arg/parm overlap */
/* Returns 1 if arg and parm pages overlap, else 0 */
{  return cpumempg[0] == cpumempg[2] ||
      (cpumempg[1] && cpumempg[1] == cpumempg[3]);
}
 
void detpage(CTE * cte)      /* Reclaim map entries which reference page */
              /* Coretable entry of page to be reclaimed */
/*
  Output -
    All of the page keys to the page are changed to uninvolved.
      (The order of the backchain of page keys is unchanged.)
    All ASBs generated from this page will be invalidated.
 
  The following actions implement the above:
  Call slotzap for slots with involved page keys to our page,
  For all domains which have page keys in the memory root which
  designate this page, invalidate their map.
*/
{
   union Item *k;

   /* Do over all involved keys to the page */
   for (k = cte->use.page.rightchain;
        k != (union Item *)cte && k->key.type & (involvedr+involvedw);
        k = k->item.rightchain) {
      NODE *n;         /* The node which holds the page key */
      k->key.type &= ~(involvedr | involvedw);
      slotzap(&k->key);
      n = keytonode(&k->key);
      switch (n->prepcode) {
       default:
         crash("MEMORY001 Key not in segment or domain root");
       case prepasdomain:
         zap_dib_map(n->pf.dib);
         break;
       case prepassegment: ;
      }
   }
} /* End detpage */
 
void unprseg(
   NODE *node)
/* Unprepare a node prepared as segment. */
{
   int i;
   union Item *itm;

   md_unprseg(node); /* Invalidate produced mapping tables */
/*
   Loop through the slots in the (segment) node. For each involved key,
   call SLOTZAP to clear dependent map entries.  Then un-involve the key
*/
   for (i=0; i<16; i++) {
      struct Key *key;
      key = &node->keys[i];
      if (key->type & involvedw) {
         slotzap(key);
         if (key->type == pihk)
            crash("UNPRND004 Hook found in segment node");
         uninvolve(key);
      }
   } /* End for each key in node */
/*
   All the Memory Keys in the (segment) node have been processed
   Mark the node as having been unprepared
*/
         node->prepcode = unpreparednode;
 
/*
   Now consider each involved key that designates the original node by
   following the chain to look at all involved keys that designate it.
 
   For each key uninvolve and slotzap it. If its node is prepared as a
   domain, then zap the map of the domain.
 
   When we finish the chain or run out of involved keys on the chain,
   then goto RET for successful return
*/
   for (itm = node->rightchain;
        itm != (union Item *)node
           && itm->key.type & (involvedw+involvedr);
        itm = itm->item.rightchain) {
      if (itm->key.type == pihk) {
         union Item *lk;
/*
               If it is a HOOK, first save previous key on chain
               since zaphook will remove it, then call zaphook.
*/
         lk = itm->item.leftchain;
         {NODE *n; register NODE *d =
            (NODE *)((char *)itm-((char *)&n->domhookkey-(char *)n) );
         zaphook(d);}
         itm = lk;
      }
      else {    /* Key is not a hook - Then */
         NODE *kn;

/*
               Mark the key un-involved. All backchain keys will be
               uninvolved at end of loop so there is no need to fix
               the backchain order.
*/
         itm->key.type &= ~(involvedr+involvedw); /* Set uninvolved */
/*
               Call KEYTONOD to get the node frame the key is in
*/
         kn = keytonode(&itm->key);
/*
   We are here because the original node was PREPASSEGMENT
   key --> involved key on chain of node we are un-preparing
   kn --> the node containing that involved key
   See how that node is prepared (its relationship to orig node)
*/
         switch (kn->prepcode) {
/*
   Its a Domain (the segment node is probably its Address Segment) or
   its a Segment (the segment node is part of another segment)
*/
          case prepasdomain:
            slotzap(&itm->key);
            zap_dib_map(kn->pf.dib);
            break;
          case prepassegment:
            slotzap(&itm->key);
            break;
         }
      } /* End key is not a hook key */
   } /* End consideration of each node that designates original */
   return;
} /* end of unprseg */

static char cvc[2][3]={{0,0,0},{0,0,0}};
static int checkmkey(
   NODE *n   /* Node to be corelocked during checkmkey. */,
   /* Either n is the node containing memkey,
      or memkey is in bgnode (which is corelocked).
      Thus we guarantee the node containing memkey is corelocked. */
   int cache  /* bool to indicate key should become involved
                 and depend called fot it. */)
/*
   Input -
      memkey - Pointer to key to check.
      seapformat is set
      memneedwrite is set
      cpudibp has actor
 
   Output -
*/
#define checkmkey_invalid 0
          /* error_code has error code, nodes are not unlocked */
#define checkmkey_wait 1
          /* Actor queued on I/O */
#define checkmkey_ok 2
          /* Key is prepared and involvedw */
          /* memispage is set based on memitem */
          /* memitem has address of CTE or NODE */
            /* If it is a node it will be prepared as segment */
          /* seapformat is updated for this key */
{int criterion = cache ? involvedw : prepared;
   corelock_node(16, n);  /* tie it down while we fiddle */
   if (cache) depend_build_entry(memkey);
   switch (memkey->type & keytypemask) {
    case datakey:
      error_code = 5;
      coreunlock_node(n);
      return checkmkey_invalid;
    case pagekey:
      if (!(memkey->type & criterion)) { /* Page key is not ready */
         if (!(cpudibp->rootnode->preplock & 0x80))
            crash("MEMORY004 Actor not preplocked");
 
         switch (cache?involvep(memkey):cvc[0][prepkey(memkey)]) {
          case involvep_ioerror:
              /*   Permanent I/O error reading node */
            crash("MEMORY005 Page key I/O error");
          case involvep_wait:
              /*   Actor enqueued for I/O */
            coreunlock_node(n);
            return checkmkey_wait;
          case involvep_obsolete:
              /*   Key was obsolete, changed to dk0 */
            error_code = 5;
            coreunlock_node(n);
            return checkmkey_invalid;
          case involvep_ok:
              /*   Key is now prepared + involvedw. */
            break;
         } /* End call involvep */
 
      } /* End key is not involvedw */
      memispage = 1;
      memitem = (Producer *)(CTE *)memkey->nontypedata.ik.item.pk.subject;
      break;
    case fetchkey:
      if (!(memkey->databyte & 15)){
         error_code = 2;         /* Key is a trojan fetch key */
         coreunlock_node(n);
         return checkmkey_invalid;
      } /* End trojan fetch key */
      /* Fall through to other node-designating segmode keys */
    case segmentkey:
    case nodekey:
    case sensekey:
      if (!(memkey->type & criterion)) { /* Key is not ready */
 
         switch (cache?
                 involven(memkey, prepassegment):
                 cvc[1][prepkey(memkey)]) {
          case involven_ioerror:
              /*   Permanent I/O error reading node */
            crash("MEMORY006 Node key I/O error");
          case involven_wait:
              /*   Actor enqueued for I/O */
            coreunlock_node(n);
            return checkmkey_wait;
          case involven_obsolete:
              /*   Key was obsolete, changed to dk0 */
            error_code = 5;
            coreunlock_node(n);
            return checkmkey_invalid;
          case involven_preplocked:
              /*   Key is now prepared + involvedw. */
            error_code = 4;
            coreunlock_node(n);
            return checkmkey_invalid;
          case involven_ok: break;
         } /* End call involven */
 
         memitem = (Producer *)(NODE *)memkey->nontypedata.ik.item.pk.subject;
 
         if (memitem->node.prepcode != prepassegment)
            prepare_segment_node(&memitem->node);
 
         unpreplock_node(&memitem->node);
      } /* End key is not involvedw */
      else {
         memitem = (Producer *)
            (NODE *)memkey->nontypedata.ik.item.pk.subject;
         if (memitem->node.prepcode != prepassegment && cache)
            crash("memory - involved key to non segment node");
      }
 
      memispage = 0;
      break;
    default:                 /* Invalid key type in memory tree */
      error_code = 2;
      coreunlock_node(n);
      return checkmkey_invalid;
   } /* End switch on memkey->type */
   seapformat &= (nocall + readonly);
   seapformat |= memkey->databyte;
   if (memneedwrite && seapformat & readonly) { /* Write to r/o */
      error_code = 1;
      coreunlock_node(n);
      return checkmkey_invalid;
   } /* End write to r/o */
   coreunlock_node(n);
   return checkmkey_ok;
}


static NODE *tounlock=0;
static int endSearch(int code, int ret)
{  if ((unsigned long)segkeeperslot > 1) coreunlock_node(segkeepnode);
   if (bgnode) coreunlock_node(bgnode);
   if (tounlock) coreunlock_node(tounlock);
   error_code = code; return ret;}

#define XCvC 0
#if XCvC
static char XCv[64]; // peek 5
static XCvx = 0;
int XCvZ[] = {64, (int)XCv, 4, (int) &XCvx, 0};
#else
int XCvZ[] = {0};
#endif

static int searchportion(
      int lock,               /* 1 to call depend, 0 not to */
      int hoglimit,           /* limit of depth of search */
      int newmemlss)     /* value for memlss.
      memlss has lower bound on the extent of the search. searchportion
         will return when a lss/ssc less than the input memlss is found.
         (n.b. searchportion will always return when it encounters a
         page key. The output value of memlss will be 2 in this case.)
      memitem is pointer to the node or CTE designated by the key
      seapformat has format byte (read-only, no-call, and lss)
      vaddr has relative address
      memneedwrite has 1 if caller requires write access, 0 for read
      memispage has 1 if memitem points to core table entry,
                    0 if it points to a node
      segkeeperslot is NULL if keeper unknown, 1 if no keeper,
                 otherwise a pointer to the segment keeper key.
      segkeepnode If segkeeperslot points to a segment keeper key, then
                   it is a pointer to the segment keeper key's node.
      bgnode has address of node with background key or NULL
      chargesetid has the id of the charge set in effect
      cpudibp has the actor.
 
  N.B. If parameter "lock" then the caller of searchportion must
       call either depend_chain_entries or depend_dispose_entries
       before exiting the memory module.
 
  Searchportion executes the memory tree algorithm
  until an LSS smaller than "memlss" is encountered.
  "vaddr" is updated as a side effect.
  "segkeeperslot" and "segkeeperaddr" are set as segment keepers
     are noticed.
  "bgnode" is set as red nodes with background keys are passed.
  "bgnode" is used when background type windows are encountered.
  domain is trapped if vaddr >= 16**"memlss" at end.
  Unlocked nodes may be changed to unprepared.
  Nodes will be unlocked before any exit.
  Intermediate nodes and keys will be prepared or involved respectively.
 
   Output -
*/
#define seap_wait 0       /* Actor enqueued */
#define seap_fault 1      /* error_code has error code */
#define seap_ok 2         /* seapformat, memlss, and memitem updated */
 
/*
  Design notes on searchportion:
     The callers of searchportion assume that after the call:
            vaddr<16**((input value of memlss)+1)
*/
{

   int targetlss = memlss = newmemlss;

   if (memispage) tounlock = NULL;
   else {tounlock = &memitem->node; corelock_node(17, tounlock);}
   if ((unsigned long)segkeeperslot > 1) corelock_node(18, segkeepnode);
   if (bgnode) corelock_node(19, bgnode);
 
   for (;;) {          /* Search portion loop */
 
      if (!memispage) {      /* memitem is a node */
         int lastinitial;    /* Last initial slot in node */
         unsigned long fkd;  /* Format key data if node is red */
         uint64 waddr;  /* Work area for address */
         if (--hoglimit == 0) return endSearch(6, seap_fault);
 
         if ((memlss = seapformat & 15))  /* Node is black */
              {lastinitial = 15;}
         else {                        /* Node is red */
            struct Key *fk = &memitem->node.keys[15];/* format key */
            int sn;             /* slot number hold area */
            if ((fk->type & keytypemask) != datakey) {
               return endSearch(7, seap_fault); 
            } /* End fault non-data key format key */
 
            fkd = b2long(fk->nontypedata.dk7.databody+3,4);
 
            if ((fkd | 0x100ff0ff) != 0x1fffffff) {
               return endSearch(8, seap_fault);
            } /* End fault bad format key data */
 
            if (lock) {
               fk->type = datakey+involvedw;   /* Involve the format key */
               depend_build_entry(fk); /* Depend on format key */}
 
            if ((sn = (fkd & 0x0000f000)>>12) !=15/* keeper specified */
                  && !(seapformat & nocall)) {/* and nocall not seen */
               if ((unsigned long)segkeeperslot > 1)
                       coreunlock_node(segkeepnode);
                       /* unlock any old segkeepnode */
               segkeeperslot = &memitem->node.keys[sn];
               segkeepnode = &memitem->node;
               corelock_node(20, segkeepnode); /* lock new segkeepnode */
               segkeeperaddr = vaddr;    /* Save keeper and address */
            } /* End keeper specified and not nocall */
 
            lastinitial = ((fkd & 0x000000f0)>>4) -1;
            memlss = fkd & 0x0f;
         } /* End node is red */
 
         if (memlss < 3 || memlss > 12) {
            return endSearch(9, seap_fault);
         } /* End fault lss out of range */
 
         if (memlss < targetlss) {   /* Reached ending criterion */
            if (vaddr & ((-1LL)<<(targetlss<<2))) {
               return endSearch(9, seap_fault);
            } /* End fault vaddr too large */
            return endSearch(0, seap_ok);
         } /* End exit because of lss below target criterion */
 
         waddr = vaddr >> (4*memlss); // waddr = vaddr; llilsr(&waddr, 4*memlss);
         
 
         if (waddr > lastinitial) {
            return endSearch(3, seap_fault);
         } /* End fault address outside node */
 
         memkey = &memitem->node.keys[waddr];
 
         /* Reduce vaddr to be relative to new memkey */
         vaddr &= ((uint64)-1) >> (64-4*memlss);

         if (memitem->node.prepcode != prepassegment && lock)
               crash("MEMORY008 Seg node not prepared as segment");
 
         if (!(seapformat & 15)) {   /* Node is red */
            if (((fkd & 0x000f0000)>>16) !=15) {  /* background key */
               if (bgnode) coreunlock_node(bgnode);
                       /* unlock any old bgnode */
               bgnode = &memitem->node;
               corelock_node(21, bgnode); /* lock new bgnode */
            }
/*
            add code here to note a meter in the segment node and set
                   CHARGESETID
*/
         } /* End second test for red nodes */
 
         if ((memkey->type & keytypemask) == datakey
              && memkey->nontypedata.dk7.databody[6] & 2) {
            /* Key is a window key */
            unsigned char *data = memkey->nontypedata.dk11.databody11;
            seapformat |= (data[10]<<4) & (nocall + readonly);
 
            /* Fault if window address > 256**6 */
 
            if (Memcmp(data, "\0\0\0\0\0", 5)) {
               return endSearch(14, seap_fault);
            } /* End fault if window address > 256**6 */
 
            /* Check low nibbles of window key for zero */
            {
//                uint64 adr = *(uint64 *)(data+3);
//                uint64 adr;
//                Memcpy((unsigned char *)&adr,data+3,8);

                uint64 adr = ( ((uint64) *(int *)(data+3) << 32) +
                               ((uint64) *(int *)(data+7)      ) );

                if (adr &~(uint64)((data[10]&1)?15:255)
                        &~((uint64)-1 << memlss*4))
                    { return endSearch(10, seap_fault);}
                vaddr += adr &~0xfff;}
            if (lock) {depend_build_entry(memkey); /* Depend on window */
               memkey->type = involvedw+datakey;}
 
            /* End check low nibbles of window key for zero */
 
            if (data[10] & 1) {  /* Key is a background window key */
               if (bgnode == NULL) {  /* Background key is NULL */
                  return endSearch(13, seap_fault);
               } /* End background window w/no background key */
               if (bgnode->prepcode != prepassegment) {
                 if (bgnode->prepcode != unpreparednode)
                    crash("memory - bgnode strangely prepared");
                 prepare_segment_node(bgnode);
               }
               memkey = &bgnode->keys[ /* background slot */
                  ((bgnode->keys)[15].nontypedata.dk7.databody)
                       [4]
                  & 0xf ];
            } /* End background window key */
 
            else {   /* Key is a local window key */
               memkey = &memitem->node.keys[data[10]>>4];
            } /* End local window key */
 
         } /* End initial key is a data key */
 
         switch (checkmkey(&memitem->node, lock)) {
          case checkmkey_invalid:
             /* error_code has error code, nodes are not unlocked */
            return endSearch(error_code, seap_fault);
          case checkmkey_wait:
             /* Actor queued on I/O */
            return endSearch(error_code, seap_wait);
          case checkmkey_ok: ;
         } /* End switch checkmkey */
 
         /* *memkey is prepared and involvedw */
         /* memispage is set based on what memitem points to */
         /* memitem points to a node or CTE */
         /* seapformat is updated for this key */
 
      } /* End memkey key is not a page key (finally!) */
      else { /* memkey is a page key */
         if (vaddr > 0xfff) { /* vaddr too large */
            return endSearch(3, seap_fault);
         }
         memlss = 2;
         return endSearch(0, seap_ok);
      } /* End memkey is a page key */
   } /* End searchportion loop */
} /* End searchportion */

void kldge(int w){
    memneedwrite = w;
    segkeeperslot = NULL;}

memfault_result memfault(
   unsigned long addr,    /* Failing virtual address */
   struct DIB *dib,       /* Domain's DIB */
   struct Key *memroot)   /* Key defining memory tree root */
   /* memneedwrite is set */
   /* segkeeperslot is set */
{
   if (addr >= 0xf0000000UL) return memfault_nokeep;
   if (segkeeperslot) return memfault_keeper;
 
   /* Must re-traverse the tree; we don't know the segment keeper */
   segkeeperslot = (struct Key *)1;  /* Mark no keeper */
   seapformat = 0;
   memkey = memroot;
 
   switch (checkmkey(dib->rootnode, 0)) {
    case checkmkey_invalid:
       /* error_code has error code, nodes are not unlocked */
      break; /* Fall out bottom to exit memfault */
    case checkmkey_wait:
       /* Actor queued on I/O */
      crash("MEMORY011 Node missing in MEMFAULT");
    case checkmkey_ok: ;
      vaddr = addr;
      bgnode = NULL;
      chargesetid = dib->chargesetid;
 
      switch (searchportion(0,60,2)) {
       case seap_wait:     /* Actor enqueued */
         /* This situation can occur when the stem call to
            searchportion displaces a node that held an involved
            key that was germain to the segment table entry that
            was used to locate the page table whose invalid
            entry provoked the stem call to searchportion. */
         return memfault_redispatch;
       case seap_fault:    /* error_code has error code */
         break; /* Fall out bottom to exit memfault */
       case seap_ok:       /* seapformat, memlss, and memitem updated */
         if(addr >= 0xf0000000u) {error_code = 21; break;}
         crash("MEMORYnnn what happened to the fault?");
      } /* End switch searchportion */
 
   } /* End switch checkmkey */
 
   if (segkeeperslot == 0 )
      crash("MEMORY013 Keeper unknown after memfault");
   if (segkeeperslot == (struct Key *)1) return memfault_nokeep;
   return memfault_keeper;
} /* End memfault */


void call_seg_keep(int rt)
/* Static input:
   segkeepnode
   segkeeperaddr
 */
{
   cpup3key.databyte = 0;
   cpup3key.type = nodekey+prepared;
   cpup3node = segkeepnode;
   cpuordercode = -1;
   cpuexitblock.argtype = arg_regs;
   cpuexitblock.keymask = 2;
   cpuargaddr = 2+(char*)&segkeeperaddr;
   cpuarglength = 6;
   keepjump(segkeeperslot, rt);
}


void fault2(         /* Set up trap to seg or dom keeper */
   int code,                /* Error code from access */
   int rw)
{
   segkeeperslot = NULL;     /* Indicate unknown keeper */
   memneedwrite = rw;
   error_code = code;
}  /* End fault2 */


mem_result search_trunk(  /* search memory tree from root */
   int targetlss,     /* stop at this lss */
   struct Key *root,  /* the root */
   struct DIB *dib,           /* DIB of domain to resolve space in */
   int rw)                    /* rw=1 if R/W access needed, else 0 */
/* search_trunk tries to ensure that the keys of the trunk of
   the memory tree are involved. When sucessful "memkey" locates
   the segment node or page that must produce the data map for
   the domain "*dib". depend_chain_entries must be called upon mem_ok.
   "*root" must designate one of the address slots of that domain. 
   "targetlss" varies only between implementations and should not thus
   be a parameter. Perhaps it should be #defined.
   "rw" is whether write access is required.
   Upon the mem_error return error_code holds the reason.*/ 
{
   memkey = root;
   seapformat = 0;
   memneedwrite = rw;        /* Set flag for kind of access needed */
   switch (checkmkey(dib->rootnode, 1)) {
    case checkmkey_invalid:
       /* error_code has error code, nodes are not unlocked */
      return mem_fault;
    case checkmkey_wait:
       /* Actor queued on I/O */
      return mem_wait;
    case checkmkey_ok: ;
   } /* End switch checkmkey */
   /* *memkey is prepared and involvedw */
   /* memispage is set based on what memitem points to */
   /* memitem points to a node or CTE */
   /* seapformat is updated for this key */

   segkeeperslot = (struct Key *)1;
   vaddr = 0;   /* vaddr has relative address */
   bgnode = NULL;
   chargesetid = dib->chargesetid;
   switch (searchportion(1,20,targetlss)) {
    case seap_ok:       /* seapformat, memlss, and memitem updated */
      return mem_ok;
    case seap_wait:     /* Actor enqueued */
      depend_dispose_entries();
      return mem_wait;
    case seap_fault:    /* error_code has error code */
      depend_dispose_entries();
      return mem_fault;
    default: crash("vnoire"); // don't know whether this path is valid.
   }
}  /* End search_trunk */

#define xCVC 0
#if xCVC
static char xCV[64]; // peek 6
static xCVx = 0;
int xCVZ[] = {64, (int)xCV, 4, (int) &xCVx, 0};
#else
int xCVZ[] = {0};
#endif

mem_result search_portion(
   uint32 addr,
   int rw,                 /* rw=1 if R/W access needed, else 0 */
   MapHeader *asb,  /* pointer to a structure with the following fields:
        producer (non-NULL)
        bgnode
        chargesetid
        format
        producernode */
   int newmemlss)     /* target lss */
/* If returned value is mem_ok, values are returned in
       memitem, memispage, chargesetid, seapformat, and bgnode, and
            caller must call depend_xxx_entries
 */
{

   /* Set up searchportion parameters from the asb */
   vaddr = addr;
   memitem = asb->producer;
   memispage = !asb->producernode;
   bgnode = asb->bgnode;
   chargesetid = asb->chargesetid;
   seapformat = asb->format;
   memneedwrite = rw;        /* Set flag for kind of access needed */
   segkeeperslot = NULL;
 
   switch (searchportion(1, 20, newmemlss)) {
    case seap_ok:      /* seapformat, memlss, and memitem updated */
      return mem_ok;
    case seap_wait:    /* Actor enqueued */
      depend_dispose_entries();
      return mem_wait;
    case seap_fault:   /* error_code has error code */
      depend_dispose_entries();
      return mem_fault;
    default: crash("mbtoip"); // don't know whether this path is valid.
   } /* End switch searchportion */
}  /* End search_portion */


char *map_arg_string(
   /* Maps cpudibp page(s) at "addr" for "len" into windows 0 (and 1) */
   unsigned long addr,    /* Domain's virtual address */
   int len)               /* Length of string to map */
/*
    Returns NULL if it can't map, otherwise pointer (within window) to
    the first byte selected by user's address "addr".
    If NULL is returned, jumper is backed up and either
    (1) the actor's map situation is better
    and its dib remains in cpudibp,
    OR (2) the domain root referenced by
    pointer "actor" is on a wait queue and cpudibp == NULL,
    OR (3) the domain referenced by the dib pointer "dib"
    has a non-zero trap code set in its DIB,
    OR (4) a segment keeper is needed and it
    has been invoked with keepjump(  ).
*/
{
   int window = arg_win;          /* First window for arg strings */
   CTE *cte;
 
   for (;;) {      /* Loop for all pages needed */
      cte = map_arg_page(addr);
      if (cte == NULL) return NULL;
      corelock_page(cte);
      cpumempg[window] = cte;
      map_window(window, cte, 0);
      if ((addr & (pagesize-1)) + len <= pagesize)
   break;
      window++;
      addr += pagesize;
      len = 0;
   } /* End loop for all pages needed */
 
   return (char *)window_address(0) + (addr & (pagesize-1));
} /* End map_arg_string */
 
 
char *map_parm_string(
    /* Maps user "dib"'s page(s) "addr" for "len" bytes */
   unsigned long addr,    /* Domain's virtual address */
   register struct DIB *dib,
               /* DIB for domain whose page is to be mapped */
/* dib->rootnode must be preplocked (to prevent dib from disappearing). */
   int len)               /* max length of string */
   /* Actor is cpudibp->rootnode */
/*
    Returns NULL if it can't map, otherwise pointer (within window) to
    the first byte selected by user's address "addr".
 
    If NULL is returned, either the domain root referenced by the
    pointer "actor" is on a wait queue (for the page), or the domain
    referenced by the dib pointer "dib" has a non-zero trap code set in
    its DIB.
*/
{
   int window = parm_win;          /* First window for parm strings */
   CTE *cte;
 
   for (;;) {      /* Loop for all pages needed */
      cte = map_parm_page(addr, dib);
      if (cte == NULL) return NULL;
 
      corelock_page(cte);
      cpumempg[window] = cte;
      map_window(window, cte, 1);
 
      if ((addr & (pagesize-1)) + len <= pagesize)
   break;
      window++;
      addr += pagesize;
      len = 0;
   } /* End loop for all pages needed */
 
   return (char *)window_address(2) + (addr & (pagesize-1));
} /* End map_parm_string */


void release_parm_pages()
             /* Releases the pages mapped into windows 3 and 4 */
             /* N.B. jsimplecall assumes this routine will is correct */
             /*      even if map_parm_string was not called first */
{
   if (cpumempg[2]) {
      coreunlock_page(14,cpumempg[2]);
      cpumempg[2] = NULL;
   }
   if (cpumempg[3]) {
      coreunlock_page(15,cpumempg[3]);
      cpumempg[3] = NULL;
   }
} /* End release_parm_pages */
 
 
void release_arg_pages()
             /* Releases the pages mapped into windows 1 and 2 */
{
   if (cpumempg[0]) {
      coreunlock_page(16, cpumempg[0]);
      cpumempg[0] = NULL;
   }
   if (cpumempg[1]) {
      coreunlock_page(17, cpumempg[1]);
      cpumempg[1] = NULL;
   }
} /* End release_arg_pages */

#define P 4096
extern CTE * ptl[2];
uchar * accessSeg(struct Key * k, int w, uint64 offset, char f, int wn)
   {memkey = k;
    memneedwrite = w; seapformat = 0;
    switch (checkmkey(cpudibp -> keysnode, 0))
    { case checkmkey_wait: error_code = 0;
      case checkmkey_invalid: return 0;
      case checkmkey_ok: vaddr = offset;
        segkeeperslot = (struct Key *) 0;
        bgnode = (NODE *)0;
        chargesetid = 0;
        memlss = 3;
        switch (searchportion(0, 40, 2)) {
          case seap_wait: error_code = 0; 
          case seap_fault: return 0;
          case seap_ok: ptl[w] = &memitem->cte;
           corelock_page(&memitem->cte);
           return (uchar *)((int)offset & ~-P |
               (int)map_window(wn, &memitem->cte, w));
          default: crash("priusp"); // don't know whether this path is valid.
     }
          default: crash("bniopdo"); // don't know whether this path is valid.
 }}

void init_mem(){
  cvc[0][prepkey_notobj] = involvep_obsolete;
  cvc[0][prepkey_prepared] = involvep_ok;
  cvc[0][prepkey_wait] = involvep_wait;
  cvc[1][prepkey_notobj] = involven_obsolete;
  cvc[1][prepkey_prepared] = involven_ok;
  cvc[1][prepkey_wait] = involven_wait;}
  /* The above kludge should be supplanted by some form of polymorphism. */

