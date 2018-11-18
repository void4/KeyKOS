
extern NODE *srchnode(      /* Search for current version of node */
   unsigned char *cda     /* Pointer to the CDA to search for */
   );
/*
   Output -
      Returns pointer to the node or NULL if node not in hash chains.
*/
 
 
extern CTE *srchpage(       /* Search for current version of page */
   const unsigned char *cda     /* Pointer to the CDA to search for */
   );
/*
   Output -
      Returns pointer to the cte or NULL if page not in hash chains.
*/
 
extern CTE *srchbvop(       /* Search for backup version of page */
   unsigned char *cda       /* Pointer to the CDA to search for */
   );
/*
   Output -
      Returns pointer to the cte or NULL if page not in hash chains.
*/
 
extern int prepkey(
   struct Key *key);
 /* Changes key to its prepared form. */
 /* Input: */
 /*   Pointer to key to prepare */
 /* Returns int as follows: */
#define prepkey_notobj 0
 /*   Obsolete key or does not designate page/node */
#define prepkey_prepared 1
 /*   The key has been prepared */
#define prepkey_wait 2
 /*   The object designated by the key must be fetched, actor queued */
 
 
extern void tryprep(       /* Prepare a key if possible without I/O. */
   struct Key *key          /* Pointer to key to prepare */
   );
/*
    Output - If key's object is in memory, then it has been prepared.
*/
 
 
extern void zaphook(NODE *); /* Remove hook key from slot */
 
extern int involven(
   struct Key *k, int c
   );
 /* Involves a key. */
 /* Input: */
 /*   "k" - pointer to key to prepare, must designate a node, */
 /*         can not be a resume key */
 /*   "c" - The prerequesite preperation code of the designated node */
 /* Returns: */
#define involven_ioerror 0
 /*   Permanent I/O error reading node */
#define involven_wait 1
 /*   Actor enqueued for I/O */
#define involven_obsolete 2
 /*   Key was obsolete, changed to dk0 */
#define involven_preplocked 3
 /*   Designated node differently prepared & can't be unprepared or */
 /*      it was already preplocked. */
#define involven_ok 4
 /*   Designated key is prepared and involvedw  */
 
 
extern int involvep(
   struct Key *k);
 /* Involves a key to a page. */
 /* Input: */
 /*   "k" - pointer to key to prepare, must designate a page. */
 /* Returns - */
#define involvep_ioerror 0
 /*   Permanent I/O error reading node */
#define involvep_wait 1
 /*   Actor enqueued for I/O */
#define involvep_obsolete 2
 /*   Key was obsolete, changed to dk0 */
#define involvep_ok 3
 /*   Key is now prepared + involvedw. */
 
 
extern void halfprep(
   struct Key *k
   );                    /* Chain key maintaining midpointer etc. */
/*
   The subject field of the key must point to the page or node, the
   type must be set, and the prepared bit in the key type must be on.
*/
 
 
extern void uninvolve(
   struct Key *key
   );        /* Uninvolve the key slot - Maintain backchain order */
 
 
extern NODE *keytonode(
   struct Key *key
   );         /* Return the node header for passed key slot */

void unprepare_key(struct Key *key);
                         /* Unprepare a prepared, uninvolved key. */

