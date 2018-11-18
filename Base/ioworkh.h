#include "lli.h"
#include "kktypes.h"
#define PRIMARYCHECKPOINTHEADERLOCATION     0xfffffffe
#define SECONDARYCHECKPOINTHEADERLOCATION   0xffffffff

extern
CTE buppage1, buppage2;    /* Chain heads for current and next backup */
                           /* pages. Only page.leftchain and */
                           /* page.rightchain are used in these heads */
 
extern CTE *allocationpotchainhead;
 
extern uint64 todthismigration;      /* Time of most recent migration */
 
extern char iosystemflags;
#define PAGECLEANNEEDED              1  /* Need to clean some pages */
#define CHECKPOINTMODE               2  /* Checkpoint is in progress */
#define MIGRATIONINPROGRESS          4  /* A migration is in progress */
#define MIGRATIONURGENT              8  /* Run only external migrator */
#define CHECKPOINTATENDOFMIGRATION  16  /* Take ckpt after migration */
#define INHIBITNODECLEAN            32  /* Don't make new node pots */
#define DISPATCHINGDOMAINSINHIBITED 64  /* Don't dispatch any domains */
#define WAITFORCLEAN               128  /* GSPACE waiting on a clean */

extern bool nodecleanneeded;
 
#define OUTSTANDINGIOSIZE 512  /* Must be a power of 2 > 32 */
#define OUTSTANDINGIOMASK (OUTSTANDINGIOSIZE-1)
extern uint32 outstandingio[OUTSTANDINGIOSIZE>>5];
 
 
/*
   If continuecheckpoint is not NULL, then a checkpoint is in progress
   and calling the procedure assigned to continuecheckpoint will
   attempt to make further progress with the checkpoint.
*/
extern void (*continuecheckpoint)(void);
 
 
/* Statistical counters */
 
#define defctr(c)    extern uint32 c;
#define defctra(c,n) extern uint32 c[n];
#define deftmr(t)    extern LLI t;
#define lastcounter()
#include "counterh.h"

