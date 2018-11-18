/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Initialization of constants used in the kernel */
 
#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
#include "kerinith.h"
 
 
void wss()
{
   int i;
 
   free_dib_head = NULL;
   for (i=0;i<maxdib;i++) {
      firstdib[i].keysnode = NULL;  /* mark it free */
      firstdib[i].rootnode = (NODE *)free_dib_head;
      free_dib_head = &firstdib[i];
   }
}
