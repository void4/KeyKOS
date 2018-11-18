#include "types.h"
#include "keyh.h"
#include "cpujumph.h"

extern struct DIB *cpudibp;
extern NODE *cpuactor;
extern NODE *cpujenode;
extern NODE *cpup3node;
extern struct exitblock cpuexitblock;
extern struct entryblock cpuentryblock;
extern ulong_t cpuordercode;
extern ulong_t cpuarglength;
extern ulong_t ticks;
extern ulong_t domain_started_at;
extern ulong_t end_of_slice;
extern ulong_t cpuparmlength;
extern long cpubackupamount;
extern int cputrapcode;
extern char *cpuargaddr;
extern char *cpuargpage;
extern char *cpuparmaddr;
extern char cpup3switch;
extern char cpu_current_prio;
extern uchar_t cpuinvokedkeytype;
extern uchar_t cpuinvokeddatabyte;
extern struct Key cpup1key;
extern struct Key cpup2key;
extern struct Key cpup3key;
extern struct Key cpup4key;
extern struct Key cpustore3key;
