/* cyclecounter.h Define variables used for maintaining the instruction counts
   and cycle counts in domain's meters.
*/

#if defined(viking)
extern long long cpu_cycle_start;   /* Cycle counter at start of current mode */
extern long long cpu_inst_start;    /* ditto for instruction counter */

long long get_cycle_count(void);
long long get_inst_count(void);

#endif
