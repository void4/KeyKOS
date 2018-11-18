// void clean_windows(struct DIB *);
// static void clean_windows(struct DIB const * d){}
// For current machines the above is a nop.
// For possible machines it might be a defered save
// in case we can run compiled code without saving all the registers.
// It is called from routines that have promissed not to modify the DIB.
// Note that the prototype must not include "const".
// That will require further accomodation.
void clean_fp(struct DIB *);
int movba2va(void *, void *, int);
void nodomain(void);
void set_itr(uint32);
