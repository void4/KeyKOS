/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

// I think this runs in a Unix process.
// If not I think it could and should.
#include <string.h>
#include "item.h"
#include "diskkeyh.h"
#include "itemdefh.h"
#include "sysdefs.h"
#include "types.h"
#include "cvt.h"
#include "memutil.h"
#include <stdio.h>

void exit(int);

struct symr supermeter = {"supermeter",1,0,0,{(uchar_t *)0},NULL};
 
ulong_t curr_node_cda = 0;
ulong_t curr_key = 16;
ulong_t curr_page_cda = 1;
ulong_t curr_plist = 0;
struct symr *fchain = NULL;
 
/* Make this dynamic eventually */
#define NUMBER_OF_INITIAL_NODES 1000
#define NUMBER_OF_INITIAL_FILES 100

DiskNode_t nodes[NUMBER_OF_INITIAL_NODES];
          /* nodes[0] isn't used */
plist_t plist[NUMBER_OF_INITIAL_FILES];

void lcrash(str)
   char *str;
{
   fprintf(stderr,"CRASH -> %s\n",str);
   exit(1);
}

void check_visited(
   struct symr *symbolp,
   int ispage)
{
   if(!symbolp->visited) {
      symbolp->visited=1;
      symbolp->ispage=ispage;
      symbolp->fchain=fchain;
      fchain=symbolp;
   } else {
      if (symbolp->ispage != ispage) {
         char str[80];
         sprintf(str,"conflicting use of symbol %s.",symbolp->name);
         lcrash(str);
      }
   }
}

void 
review()
{
	struct symr *p;
	char udf = 0;
	extern int verbose;

	for(p = fchain; p; p = p->fchain) {
		if (!p->isdef) {
			if (verbose)
				fprintf(stderr,"Symbol %s is undefined\n", 
					p->name);
			udf = 1;
		} else if (verbose) {
			if (p->ispage)
				printf("%16s p%3X\n", p->name, 
					(int)plist[p->v.value].firstcda);
			else
				printf("%16s n%3X\n", p->name, (int)p->v.value);
		}
	}
	if (udf) 
		fprintf(stderr, "review: WARNING: Undefined symbols.\n");
}
 
void define_sym(
   struct symr *symbolp,
   ulong_t cda)
/* Caller fills in symbolp->v.value */
{
    uchar_t *place, *tplace;

    if(symbolp->isdef) lcrash("already defined");
    place=symbolp->v.place;
    while(place != (uchar_t *)-1) {
      tplace=(uchar_t *)b2long(place,6);
      long2b(cda,place,6);
      place=tplace;
    }
    symbolp->isdef = TRUE;
}
 
void 
cmsfile(struct symr *symbolp, short number, const char *filename, long first){

	char *fullname, *getfullname();

	if (curr_plist >= NUMBER_OF_INITIAL_FILES)
		lcrash("must increase number of initial files");
	fullname = getfullname(filename);
	if (fullname) {
		if (strlen(fullname) > maxfilenamelength)
			lcrash("ITEMH001 file name too long");
	number = file_countpages(fullname, first); 
		strcpy(plist[curr_plist].filename, fullname);
	} else
		*plist[curr_plist].filename = '\0';
	check_visited(symbolp, 1);
	plist[curr_plist].lengthplace = 0;
	plist[curr_plist].first = first;
	plist[curr_plist].number = number;
	plist[curr_plist].firstcda = curr_page_cda;
	define_sym(symbolp,curr_page_cda);
	symbolp->v.value = curr_plist;
	curr_page_cda += number;
	curr_plist++;
}
 
void 
pages(struct symr *symbolp, short number, const char *filename)
{
	cmsfile(symbolp, number, filename, 0);
}
 
void def_nodenosym(
  char process)       /* TRUE iff there is a process in the node */
{
    /* we increment curr_node_cda here because it is used between
     * the end of this function and the next call to this function.
     * (i.e. when defining keys for this node.)
     */
    curr_node_cda++;
    /* take care of ending the previous node */
    if (curr_key != 16) lcrash("wrong number of keys in node");
    curr_key = 0;
    if (curr_node_cda >= NUMBER_OF_INITIAL_NODES)
            lcrash("must increase number of initial nodes");
    nodes[curr_node_cda].flags = (process ? DNPROCESS : 0); /* and gratis = 0 */
    long2b(curr_node_cda, nodes[curr_node_cda].cda, 6);
    memzero(nodes[curr_node_cda].allocationid, 4);
    memzero(nodes[curr_node_cda].callid, 4);
}

void def_node(
  struct symr *symbolp,
  char process)       /* TRUE iff there is a process in the node */
{
   check_visited(symbolp, 0);
   def_nodenosym(process);
   define_sym(symbolp,curr_node_cda);
   symbolp->v.value = curr_node_cda;
}
 
 
void dk5(          /* define a data key from five values */
  uchar_t hi0,
  uchar_t hi1,
  uchar_t hi2,
  ulong_t midvalue,
  ulong_t lowvalue)
  {
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype = datakey;
    p->dkdk.databody11[0] = hi0;
    p->dkdk.databody11[1] = hi1;
    p->dkdk.databody11[2] = hi2;
    long2b(midvalue, &(p->dkdk.databody11[3]), 4);
    long2b(lowvalue, &(p->dkdk.databody11[7]), 4);
    curr_key++;
  }
void dkpsw(ulong_t startaddr)
  /* define start address (status slot 13) */
{
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype=datakey;
    p->dkdk.databody11[0]=0;
    long2b(startaddr,&(p->dkdk.databody11[1]),4);
    p->dkdk.databody11[4] |= 0x02;
    long2b(0x800003F0,&(p->dkdk.databody11[5]),4);
    p->dkdk.databody11[9]=0;
    p->dkdk.databody11[10]=0;
    curr_key++;
}
void dkstr(const char *str) /* define 11 byte datakey with string */
{ 
    int len;

    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);

    len=strlen(str);
    if(len>11) len=11;

    p->dkdk.keytype=datakey;
    memcpy(&(p->dkdk.databody11[0]),str,len);
    curr_key++;
}

void dk(  /* define a data key with a long int value */
  ulong_t value)
  {dk5(0,0,0,0,value);}
 
void dk2(  /* define a data key with two long int values */
  ulong_t midvalue,
  ulong_t lowvalue)
  {dk5(0,0,0,midvalue,lowvalue);}
 
void dkfilelength(
   struct symr *symbolp)
/* Define a data key containing the length of the given file. */
{
   if (! symbolp->isdef) lcrash("forward reference to page");
   check_visited(symbolp, 1); /* check that it is a page */
   plist[symbolp->v.value].lengthplace
      = &(nodes[curr_node_cda].keys[curr_key].dkdk.databody11[5]);
   dk(0);
}
 
void dk0s(  /* n zero data keys */
  int n)
  {int i;
   for (i=n; i>0; i--) dk(0);
  }

void emptynode(
  struct symr *symbolp)
  {
    def_node(symbolp,0);  /* node has no process */
    dk0s(16);
  }

void nkeynosym(
  short type,  /* the key type */
  short db,    /* the data byte */
  long cdaorplace)
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->ik.keytype = type;
    p->ik.databyte = db;
    memzero(p->ik.allocationid,4);
    long2b(cdaorplace, p->ik.cda,6);
    curr_key++;
}
 
void nkey(
  short type,  /* the key type */
  short db,    /* the data byte */
  struct symr *np)  /* the node referenced */
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
   nkeynosym(type, db,
             np->isdef ? np->v.value : (long)np->v.place);
   if (!np->isdef) {np->v.place=p->ik.cda;}
   check_visited(np, 0);
}
 
void msckey(
  char type,
  ulong_t value)
{
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype = misckey;
    p->dkdk.databody11[0] = type;
    long2b(value, &(p->dkdk.databody11[1]), 10);
    curr_key++;
}

void irangekey(
   char type,
   ulong_t cda,
   ulong_t size)
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
   p->rangekey.keytype = type;
   long2b(cda, p->rangekey.cda, sizeof (CDA));
   long2b(size, p->rangekey.rangesize, sizeof (p->rangekey.rangesize));
   curr_key++;
}

void jrangekey( /* For large CDAs */
   char type,
   unsigned short high_cda,
   ulong_t cda,
   ulong_t size)
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
   p->rangekey.keytype = type;
   long2b(cda, p->rangekey.cda, sizeof (CDA));
   long2b(high_cda, p->rangekey.cda, 2);
   long2b(size, p->rangekey.rangesize, sizeof (p->rangekey.rangesize));
   curr_key++;
}
 
void devkey(
  char slot,
  char device,
  char type,
  ulong_t serial)
  {
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype = devicekey;
    p->dkdk.databody11[0] = slot;
    p->dkdk.databody11[1] = device;
    p->dkdk.databody11[2] = type;
    long2b(serial, &(p->dkdk.databody11[3]), 8);
    curr_key++;
  }
 
 ulong_t longone = 1l;
void pkey(
  struct symr *symbolp,
  int pageno,
  char db)
  {
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    ulong_t thiscda;
    if (! symbolp->isdef) lcrash("forward reference to page");
    thiscda = plist[symbolp->v.value].firstcda + pageno;
    p->ik.keytype = pagekey;
    p->ik.databyte = db;
    memcpy(p->ik.allocationid,&longone,4);
    long2b(thiscda, p->ik.cda, 6);
   check_visited(symbolp, 1);
   curr_key++;
  }
 
void genseg(
  /* generate a segment of up to 16 pages */
  struct symr *nodep,
  short size,
  struct symr *pagep)
{
    short i;
    int truesize;

    truesize=plist[pagep->v.value].number;
    if(size > truesize) size = truesize;

    if (size > 16) lcrash("Use genseg256");
    /* generate LSS 3 node */
    def_node(nodep,0);
    for (i=0; i<size; i++)
      pkey(pagep,i,0);
    dk0s(16-size); /* fill in rest of node */
}
 
void genseg256rec(
  unsigned short size, /* number of pages */
  uint_t pageoffset,
  struct symr *pagep)
/* On exit, curr_node_cda has cda of LSS 4 node. */
{
   uint_t i,j;
   ulong_t first_node_cda = curr_node_cda;

//   if (size <= 16) lcrash("Use genseg");
   /* generate LSS 3 nodes */
   for (i=0; i*16 < size; i++) { /* loop over LSS 3 nodes */
      def_nodenosym(0);
      for (j=0; j<16 && i*16+j < size; j++)
         pkey(pagep,pageoffset+i*16+j,0);
      dk0s(16-j);  /* fill in rest of node if necessary */
   }
   /* generate LSS 4 node */
   def_nodenosym(0);
   for (i=0; i*16 < size; i++)
      nkeynosym(nodekey,3,++first_node_cda);
   dk0s(16-i);  /* fill in rest of LSS 4 node */
}

void genseg256(
  /* generate a segment of 17 to 256 pages */
  struct symr *nodep,
  unsigned short size, /* number of pages */
  struct symr *pagep)
{
   int truesize;

    truesize=plist[pagep->v.value].number;
    if(size > truesize) size = truesize;

   check_visited(nodep, 0);
   genseg256rec(size, 0, pagep);
   define_sym(nodep,curr_node_cda);
   nodep->v.value = curr_node_cda;
}

void genseg4096(
/* Generate a segment of 257 to 4096 pages. */
   struct symr *nodep, /* LSS 5 node */
   uint_t size, /* number of pages */
   struct symr *pagep)
{
   ulong_t nodecdas[16];
   uint_t k;
   int truesize;

    truesize=plist[pagep->v.value].number;
    if(size > truesize) size = truesize;

   if (size <= 256) {
       if(size <= 16) {
           lcrash("Use genseg instead of genseg4096.");
       }
//         lcrash("Use genseg256 or genseg.");
        genseg256rec(size,0,pagep);
        nodecdas[0] = curr_node_cda;
   }
   else {
      for (k=0; k*256 < size; k++) { /* loop over LSS 4 nodes */
         genseg256rec(min(256, size-k*256), k*256, pagep);
         nodecdas[k] = curr_node_cda;
      }
   }
   def_node(nodep, 0);  /* LSS 5 node */
   for (k=0; k*256 < size; k++)
      nkeynosym(nodekey, 4, nodecdas[k]);
   dk0s(16-k);  /* fill in rest of LSS 5 node */
}

void *long2b(long i, uchar_t *str, int len) 
{  
	if (len > 4) {
		memset(str, 0, len-4);
		str += len-4;
		len = 4;
	}
	memcpy(str, (char *)&i+4-len, len);
	return str;
}

long b2long(const uchar_t *str, int len)
{  
	long v = 0L;
	if (len > 4) {
		str += len-4;
		len = 4;
	}
	memcpy((char *)&v+4-len, str, len);
	return v;
}
