/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Definitions for item space */
/* Before this file, you must #define NUMBER_OF_INITIAL_NODES
     and NUMBER_OF_INITIAL_FILES . */
#include <string.h>
#include <stdio.h>
#include "sysdefs.h"
#include "cvt.h"
#include "keytypeh.h"
#include "keyh.h"
#include "disknodh.h"
#include "itemdefh.h"

#define min(x,y) ((x)<(y)?(x):(y))
 
struct symr {
   char name[15];
   int  isdef:1,   /* 0 if not visited */
        visited:1,
        ispage:1;  /* defined if visited */
   union {
      unsigned char *place; /* if not isdef */
      long value;           /* if isdef */
                 /* If ispage, has index of plist entry.
                    Otherwise has node cda. */
   } v;
   struct symr *fchain;
};
#define dclsym(symbol) static struct symr symbol = \
      {#symbol,0,0,0,{(unsigned char *)-1},NULL}
 /* super meter has cda 0 */
static struct symr supermeter = {"supermeter",1,0,0,{(unsigned char *)0},NULL};
 
unsigned long curr_node_cda = 0;
unsigned long curr_key = 16;
unsigned long curr_page_cda = 1;
unsigned long curr_plist    = 0;
struct symr *fchain=NULL;
 
struct DiskNode nodes[NUMBER_OF_INITIAL_NODES];
          /* nodes[0] isn't used */
struct plist_struct plist[NUMBER_OF_INITIAL_FILES];

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
         Sprintf(str,"conflicting use of symbol %s.",symbolp->name);
         lcrash(str);
      }
   }
}

void review()
{
   struct symr * p;
   char udf=0;
 for(p=fchain; p; p=p->fchain) {
   if(!p->isdef){
#ifndef NODEBUG
      fprintf(stderr,"Symbol %s is undefined\n", p->name);
#endif
      udf=1;
   }
#ifndef NODEBUG
   else if(p->ispage)
      printf("%s p%3X\n", p->name,
             plist[p->v.value].firstcda);
   else printf("%s n%3X\n", p->name, p->v.value);
#endif
 }
 if(udf) lcrash("Undefined symbols.");
 fflush(stdout);
}
 
static void define_sym(
   struct symr *symbolp,
   unsigned long cda)
/* Caller fills in symbolp->v.value */
{
    unsigned char *place, *tplace;

    if(symbolp->isdef) lcrash("already defined");
    place=symbolp->v.place;
    while(place != (unsigned char *)-1) {
      tplace=(unsigned char *)b2long(place,6);
      long2b(cda,place,6);
      place=tplace;
    }
    symbolp->isdef = TRUE;
}
 
static void cmsfile(
  struct symr *symbolp,
  short number,
  const char *filename,
  long first)
{
   if (curr_plist >= NUMBER_OF_INITIAL_FILES)
      lcrash("must increase number of initial files");
   plist[curr_plist].number = number;
   if (Strlen(filename) > maxfilenamelength)
      lcrash("ITEMH001 file name too long");
   check_visited(symbolp, 1);
   Strncpy(plist[curr_plist].filename, filename, maxfilenamelength);
   plist[curr_plist].first = first;
   plist[curr_plist].firstcda = curr_page_cda;
   define_sym(symbolp,curr_page_cda);
   symbolp->v.value = curr_plist;
   curr_page_cda += number;
   curr_plist++;
}
 
static void pages(
  struct symr *symbolp,
  short number,
  const char *filename)
{cmsfile(symbolp,number,filename,0);
}
 
static void def_nodenosym(
  char process)       /* TRUE iff there is a process in the node */
{
    /* take care of ending the previous node */
    if (curr_key != 16) lcrash("wrong number of keys in node");
    curr_key = 0;
    curr_node_cda++;
    if (curr_node_cda >= NUMBER_OF_INITIAL_NODES)
            lcrash("must increase number of initial nodes");
    nodes[curr_node_cda].flags = (process ? DNPROCESS : 0); /* and gratis = 0 */
    long2b(curr_node_cda, nodes[curr_node_cda].cda, 6);
    memzero(nodes[curr_node_cda].allocationid, 4);
    memzero(nodes[curr_node_cda].callid, 4);
}

static void def_node(
  struct symr *symbolp,
  char process)       /* TRUE iff there is a process in the node */
{
   check_visited(symbolp, 0);
   def_nodenosym(process);
   define_sym(symbolp,curr_node_cda);
   symbolp->v.value = curr_node_cda;
}
 
 
static void dk5(          /* define a data key from five values */
  unsigned char hi0,
  unsigned char hi1,
  unsigned char hi2,
  unsigned long midvalue,
  unsigned long lowvalue)
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
void dkpsw(unsigned long startaddr)
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

    len=Strlen(str);
    if(len>11) len=11;

    p->dkdk.keytype=datakey;
    Memcpy(&(p->dkdk.databody11[0]),str,len);
    curr_key++;
}

static void dk(  /* define a data key with a long int value */
  unsigned long value)
  {dk5(0,0,0,0,value);}
 
static void dk2(  /* define a data key with two long int values */
  unsigned long midvalue,
  unsigned long lowvalue)
  {dk5(0,0,0,midvalue,lowvalue);}
 
static void dkfilelength(
   struct symr *symbolp)
/* Define a data key containing the length of the given file. */
{
   if (! symbolp->isdef) lcrash("forward reference to page");
   check_visited(symbolp, 1); /* check that it is a page */
   plist[symbolp->v.value].lengthplace
      = &(nodes[curr_node_cda].keys[curr_key].dkdk.databody11[5]);
   dk(0);
}
 
static void dk0s(  /* n zero data keys */
  int n)
  {int i;
   for (i=n; i>0; i--) dk(0);
  }

static void emptynode(
  struct symr *symbolp)
  {
    def_node(symbolp,0);  /* node has no process */
    dk0s(16);
  }

static void nkeynosym(
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
 
static void nkey(
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
 
static void msckey(
  char type,
  unsigned long value)
{
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype = misckey;
    p->dkdk.databody11[0] = type;
    long2b(value, &(p->dkdk.databody11[1]), 10);
    curr_key++;
}

void irangekey(
   char type,
   unsigned long cda,
   unsigned long size)
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
   unsigned long cda,
   unsigned long size)
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
   p->rangekey.keytype = type;
   long2b(cda, p->rangekey.cda, sizeof (CDA));
   long2b(high_cda, p->rangekey.cda, 2);
   long2b(size, p->rangekey.rangesize, sizeof (p->rangekey.rangesize));
   curr_key++;
}
 
static void devkey(
  char slot,
  char device,
  char type,
  unsigned long serial)
  {
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    p->dkdk.keytype = devicekey;
    p->dkdk.databody11[0] = slot;
    p->dkdk.databody11[1] = device;
    p->dkdk.databody11[2] = type;
    long2b(serial, &(p->dkdk.databody11[3]), 8);
    curr_key++;
  }
 
static unsigned long longone = 1l;
static void pkey(
  struct symr *symbolp,
  int pageno,
  char db)
  {
    union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
    unsigned long thiscda;
    if (! symbolp->isdef) lcrash("forward reference to page");
    thiscda = plist[symbolp->v.value].firstcda + pageno;
    p->ik.keytype = pagekey;
    p->ik.databyte = db;
    Memcpy(p->ik.allocationid,&longone,4);
    long2b(thiscda, p->ik.cda, 6);
   check_visited(symbolp, 1);
   curr_key++;
  }
 
static void genseg(
  /* generate a segment of up to 16 pages */
  struct symr *nodep,
  short size,
  struct symr *pagep)
{
    short i;
    if (size > 16) lcrash("Use genseg256");
    /* generate LSS 3 node */
    def_node(nodep,0);
    for (i=0; i<size; i++)
      pkey(pagep,i,0);
    dk0s(16-size); /* fill in rest of node */
}
 
static void genseg256rec(
  unsigned short size, /* number of pages */
  unsigned int pageoffset,
  struct symr *pagep)
/* On exit, curr_node_cda has cda of LSS 4 node. */
{
   unsigned int i,j;
   unsigned long first_node_cda = curr_node_cda;
   if (size <= 16) lcrash("Use genseg");
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

static void genseg256(
  /* generate a segment of 17 to 256 pages */
  struct symr *nodep,
  unsigned short size, /* number of pages */
  struct symr *pagep)
{
   check_visited(nodep, 0);
   genseg256rec(size, 0, pagep);
   define_sym(nodep,curr_node_cda);
   nodep->v.value = curr_node_cda;
}

static void genseg4096(
/* Generate a segment of 257 to 4096 pages. */
   struct symr *nodep, /* LSS 5 node */
   unsigned int size, /* number of pages */
   struct symr *pagep)
{
   unsigned long nodecdas[16];
   unsigned int k;
   if (size <= 256) lcrash("Use genseg256 or genseg.");
   for (k=0; k*256 < size; k++) { /* loop over LSS 4 nodes */
      genseg256rec(min(256, size-k*256), k*256, pagep);
      nodecdas[k] = curr_node_cda;
   }
   def_node(nodep, 0);  /* LSS 5 node */
   for (k=0; k*256 < size; k++)
      nkeynosym(nodekey, 4, nodecdas[k]);
   dk0s(16-k);  /* fill in rest of LSS 5 node */
}

