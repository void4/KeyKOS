/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "types.h"
#include "disknodh.h"
#include "itemdefh.h"

#define min(x,y) ((x)<(y)?(x):(y))
 
struct symr {
	char name[15];
	ulong_t	isdef:1;   /* 0 if not visited */
	ulong_t	visited:1;
	ulong_t	ispage:1;  /* defined if visited */
	union {
		uchar_t	*place;	/* if not isdef */
		long	value;	/* if isdef */
		/* If ispage, has index of plist entry.
		 * Otherwise has node cda. */
	} v;
	struct symr *fchain;
};

#define dclsym(symbol) static struct symr symbol = \
      {#symbol,0,0,0,{(uchar_t *)-1},NULL}

#define dclstatesym(symbol) dclsym(symbol);\
                            dclsym(symbol ## _1);\
                            dclsym(symbol ## _2);\
                            dclsym(symbol ## _3)

/* Define state as: %i6 = 0x100f00, %pc = 0xAC, %npc = 0xB0, 
 * %psr = 0x00000080, backwindows = 0, %i3 is order code
 */
#define def_statenode(symbol, ordercode, stackptr)			    \
    def_node(&symbol, 0);						    \
    nkey(nodekey,0,&symbol ## _1);				/* 0 */     \
    dk0s(5);							/* 1-5 */   \
    dk5(0,stackptr>>24,stackptr>>16&0xff,(stackptr&0xffff)<<16,0);/* 6 */   \
    dk0s(3);							/* 7-9 */   \
    dk(ordercode>>16);						/* 10 */    \
    dk5(ordercode>>8&0xff,ordercode&0xff,0,0,0);		/* 11 */    \
    dk5(0,0, 0, 0, 0xAC);					/* 12 */    \
    dk5(0, 0, 0, 0xB0000000, 0x80000000);			/* 13 */    \
    dk0s(2);							/* 14-15 */ \
\
    def_node(&symbol ## _1,0);\
    nkey(nodekey,0,&symbol ## _2);\
    dk0s(15);\
\
    def_node(&symbol ## _2,0);\
    nkey(nodekey,0,&symbol ## _3);\
    dk0s(15);\
\
    def_node(&symbol ## _3,0);\
    dk0s(16)



/* External data */
extern struct symr supermeter;
extern ulong_t curr_node_cda;
extern ulong_t curr_key;
extern ulong_t curr_page_cda;
extern ulong_t curr_plist;
extern struct symr *fchain;
extern DiskNode_t nodes[];
extern plist_t plist[];

/* External functions */
void lcrash(char *str);
void check_visited(struct symr *symbolp, int ispage);
void review(void);
extern void cmsfile(struct symr *symbolp, short number, 
	const char *filename, long first);
extern void pages(struct symr *symbolp, short number, 
	const char *filename);
extern void def_nodenosym(char process);
extern void def_node(struct symr *symbolp, char process);
extern void dk5(uchar_t hi0, uchar_t hi1, uchar_t hi2, ulong_t midvalue,
	ulong_t lowvalue);
extern void dkpsw(ulong_t startaddr);
extern void dkstr(const char *str);
extern void dk(ulong_t value);
extern void dk2(ulong_t midvalue, ulong_t lowvalue);
extern void dkfilelength(struct symr *symbolp);
extern void dk0s(int n);
extern void emptynode(struct symr *symbolp);
extern void nkeynosym(short type, short db, long cdaorplace);
extern void msckey(char type, ulong_t value);
extern void irangekey(char type, ulong_t cda, ulong_t size);
extern void jrangekey(char type, ushort_t high_cda,
	ulong_t cda, ulong_t size);
extern void devkey(char slot, char device, char type, ulong_t serial);
extern void pkey(struct symr *symbolp, int pageno, char db);
extern void genseg(struct symr *nodep, short size, struct symr *pagep);
extern void genseg256rec(ushort_t size, uint_t pageoffset,
	struct symr *pagep);
extern void genseg256(struct symr *nodep, ushort_t size,
	struct symr *pagep);
extern void genseg4096(struct symr *nodep, uint_t size,
	struct symr *pagep);
void *long2b(long i, uchar_t *str, int len);
long b2long(const uchar_t *str, int len);
int file_countpages(char *filename, int filetype);

