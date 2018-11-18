extern unsigned long idlrenc;
void unprmet(NODE *);
void refill_cpucache(void);
struct domcache {
   uint64 cputime;
#if defined(viking)
   long long dom_instructions;
   long long dom_cycles;
   long long ker_instructions;
   long long ker_cycles;
   ...foo
#endif
};
struct domcache scavenge_meter(NODE *, int);
void retcache(struct DIB *);
