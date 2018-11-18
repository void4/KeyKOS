/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
 
    Page Defines
 
   KC (Page,Page_MakeReadOnlyKey)  KEYSTO(PageRO)        - Weaken
   KC (Page,Page_Clear)                                  - Clear
   KC (Page,Page_WriteData+offset) CHARFROM (char,len)   - Write
   KC (Page,Page_TestForZeros)
 
****************************************************************/
#ifndef _H_page
#define _H_page

#define Page_AKT              0x202
#define Page_ROAKT           0x1202
 
#define Page_MakeReadOnlyKey      0
#define Page_Clear               39
#define Page_TestForZeroes       41
#define Page_WriteData         4096
 
#endif
