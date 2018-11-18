#ifndef _H_unprndh
#define _H_unprndh
typedef enum {
   unprnode_unprepared,  /* node has been unprepared */
   unprnode_cant         /* Can't unprepare because of preplocked node */
} unprnode_ret;

unprnode_ret unprnode(
   NODE *node
   );
 
unprnode_ret superzap(         /* Uninvolve a slot */
   struct Key *key        /* The key (and slot) to uninvolve */
   );
#endif

