
#ifndef DIRENT_H
#define DIRENT_H

#include "kktypes.h"
#include "keyh.h"
#include "ioreqsh.h"

union DirLoc {
	CTE *cte;            /* Core table entry of page or nodepot */
	DEVREQ *devreq;      /* Iff DIRENTDEVREQ FIRST or SECOND */
	RANGELOC swaploc;    /* Swap area location of page or node */
};

typedef struct DirEntry DIRENTRY;
struct DirEntry {
	PCFA pcfa;           /* pcfa for entry, pcfa.cda[0]==0x80-->node */
	DIRENTRY *next;      /* Next entry in the hash chain */
	union DirLoc first;  /* If NULL then page is virtual zero */
	union DirLoc second; /* If NULL then no second location */
};
extern void idirect(char *dirstart, uint32 dirsize);

#endif /* DIRENT_H */
