/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "types.h"
// #include "scb.h"
// #include "bootconf.h"
/* #include "comvec.h" */
#include "cpujumph.h"
#include "realkermap.h"
#include "sysdefs.h"

struct bootops *bootops;
union sunromvec *romp;

/* to satify a compiler generated reference */
int __cg92_used;

/* Sparc specific variables */
int nwindows = 0;
int nwin_minus_one = 0;

int mbus_mode = 0; /* 1 if we are on a cpu connected directly to 
		    * mbus. Set early on in initialization by
		    * reading the mmu control register.
		    */
/* Switches for debugging */
struct logenableflags lowcoreflags = {0};
int checkmf = 0x3ff; /* Frequency of private memory check.
  must be power or 2 -1 */ 

/* Variables replicated for each CPU in the system */

	/* Sparc Specific */

ulong_t cpu_mmu_addr = 0;	/* Address of the memory fault */
ulong_t cpu_mmu_fsr = 0;	/* MMU Fault Status Register at the fault */
ulong_t cpu_int_pc = 0;		/* PC when domain issued a jump */
ulong_t cpu_int_npc = 0;	/* nPC when domain issued a jump */

//#if defined(viking)
long long cpu_cycle_start = 0;	/* Cycle counter at start of interval */
long long cpu_inst_start = 0;	/* Instruction counter at start of interval */
long long cpu_cycle_count = 0;	/* Cycle count since startup */
long long cpu_inst_count = 0;	/* Instruction count since startup */
//#endif

        /* Sparc floating point */

struct DIB *cpufpowner = NULL; /* DIB whose FP regs are in the hardware */
ulong_t cpu_fpa_map[4]; /* User virtual addresses of deffered FP instrutions */
int xlsCnt = -0x1000000;
 

	/* Machine independent */
struct DIB *cpudibp = NULL;
NODE *cpuactor = NULL;
NODE *cpujenode = NULL;
NODE *cpup3node = NULL;
struct exitblock cpuexitblock = {0};
struct entryblock cpuentryblock = {0};
ulong_t cpuordercode = 0;
ulong_t cpuarglength = 0;
ulong_t ticks = 0;
ulong_t domain_started_at = 0;
ulong_t end_of_slice = 0;
ulong_t cpuparmlength = 0;
long cpubackupamount = 0;
int cputrapcode = 0;
char *cpuargaddr = NULL;
char *cpuparmaddr = NULL;
char cpup3switch = 0;
char cpu_current_prio = 0;
uchar_t cpuinvokedkeytype = 0;
uchar_t cpuinvokeddatabyte = 0;
// The next five lines were once initialized as " = {0}".
// That is not necessary and not quite legal.
struct Key cpup1key;
struct Key cpup2key;
struct Key cpup3key;
struct Key cpup4key;
struct Key cpustore3key;
