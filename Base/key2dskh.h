#include "diskkeyh.h" 
 
extern void key2dsk(      /* Convert key in nodeframe to disk form */
   struct Key *key,          /* The key in a node frame */
   DISKKEY *diskkey          /* Space for the disk form of the key */
   );
