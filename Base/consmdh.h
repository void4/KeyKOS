/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************
   These routines must be called from interrupt level 4
**********************************************************************/
 
 
extern void console_interrupt(void);
   /* Handle interrupt from the console */
 
 
 
/**********************************************************************
   These routines must be called from interrupt level 0
     N.B. These routines may goto interrupt level 4 for serialization.
**********************************************************************/
 
 
extern void jromconsole(       /* Device key calls for rom console device */
   struct Key *key               /* The key being invoked */
   );
 
 
extern void jromconinit(void); /* Console device initialization */

void consprint(const char *p);       /* Debugging output */
