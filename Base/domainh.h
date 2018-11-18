unsigned long stealcache(struct DIB *dib);
extern int prepdom(
   NODE *node);
 /* Changes a domain to its prepared form. */
 /* Input: */
 /*   Pointer to node to prepare */
 /* Returns int as follows: */
#define prepdom_prepared  0 /* The key has been prepared */
#define prepdom_overlap   1 /* The domain overlaps with a
                               preplocked node */
#define prepdom_wait      2 /* An object must be fetched, actor queued */
#define prepdom_malformed 3 /* The domain is malformed */

void unprdr(
   NODE *rootnode);  /* Unprepare a domain root node */

