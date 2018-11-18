/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "unprndh.h" /* for unprnode_ret */
void back_up_jumper(void);

void set_inst_pointer(
   struct DIB *dib,
   unsigned long ip);
   
void set_trapcode(register struct DIB *dib, unsigned short code);

void clear_trapcode(struct DIB *dib);

int trapcode_nonzero(struct DIB *dib);
   /* Returns 1 if trapcode is nonzero, otherwise 0 */

void deliver_to_regs(      /* Copy string to jumpee's registers */
   struct DIB *dib,
   int len);

char *get_register_string(  /* Returns a pointer to the string (or copy of) */
   struct DIB *dib,
   unsigned long origin,
   unsigned long length);

char *check_register_string(  /* Returns 0 iff string is invalid */
   struct DIB const *dib,
   unsigned long origin,
   unsigned long length);
  
int node_overlaps_statestore(NODE const * np);

int format_control(                 /* Format domain control info */
   struct DIB *dib,                 /* Pointer to dib of domain */
   unsigned char *buffer);          /* Place for output */

void coreunlock_statestore(void);

void corelock_statestore(struct DIB *dib);

void unpr_dom_md(NODE *rn);
unprnode_ret superzap_dom_md(NODE *hn, int slot);

int prepdom_md(
   NODE *dr,          /* Root node to be prepared */
   struct DIB *dib);   /* Dib being built */
   
void call_domain_keeper(void);
void dispatch_trapped_domain(void);

void init_idledib_md(struct DIB *dib);
