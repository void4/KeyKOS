/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************/
/* SIAC - PROGRAM TO CREATE A SMALL INTEGER ALLOCATOR DOMAIN          */
/*                                                                    */
/*        SIAF(0;SB,M=>c,SIA)                                         */
/*            c = 0  -  It was allocated                              */
/*            c = 1  -  SB not "offical prompt"                       */
/*            c = 2  -  Not enough space in SB                        */
/*        SIAF(kt=>X'126')                                            */
/*                                                                    */
/*        SIA(0=>nsi)                                                 */
/*           nsi = smallest unallocated integer > 0 or -1 if none can */
/*                 be allocated                                       */
/*        SIA(si=>c)                                                  */
/*            si = integer > 0, si is deallocated and c = 0 if it was */
/*                 previously allocated and 1 if it was not.          */
/*        SIA(-1=>c)                                                  */
/*            c = smallest allocated integer or zero if none are      */
/*                alllocated                                          */
/*        SIA(kt=>X'26')                                              */
/*        SIA(kt+4=>0)                                                */
/*            destroys the SIA domain                                 */
/*                                                                    */
/* Implementation:                                                    */
/*    Integers are maintained in a bit array of unsigned long ints    */
/*    Bits in each unsigned long int have values 1-32, left to right  */
/*    A small integer = its word index times 32 plus its bit value    */
/**********************************************************************/
                     
#include "keykos.h"
#include "sia.h"
#include "domain.h"
#include "node.h"
#include "sb.h"
#include "sbt.h"
#include "bitmap.h"
                  
int exit();
          
/* Components node and its contents */
KEY COMPONENTSNODE  = 0; /* fetch key to components node containing: */
#define CFSF 2           /* slot number of FSF component */           
                                                                     
/* Domain key slots */                                              
KEY CALLER = 2;  /* Exit to CALLER */                              
KEY FDOM   = 3;  /* A domain key to this domain */                
KEY FSB    = 4;  /* The offical prompt space bank from CALLER */ 
KEY FM     = 5;  /* The meter from the CALLER */                
KEY FDC    = 6;  /* The factory's domain creator */            
                                                              
/* additional slots used */                                  
KEY K0     = 7;  /* Scratch slot */                         
KEY K1     = 8;  /* Scratch slot */                        
KEY MN    = 11;  /* Node key to top of running memory tree */          
KEY ENTRY = 12;  /* an entry key to this domain (for CALLER) */       
                                                                     
/* Address Space - created by this module */                        
/*  000000-0FFFFF  (HEX) - R/O Program */                          
/*  100000-10FFFF - stack                                         
/*  200000-FFFFFF - Workarea for bitmap of allocated integers */ 
                                                                
#define ARRAYSTART     0x00200000                              
#define ARRAYEND       0x01000000                             
#define BITS_PER_BYTE  8                                     

    char title[]="SIAC    ";
    int stacksiz=4096;
                                                            
factory()               /* called by the FACTORY command */
{                                                         
   JUMPBUF;
                                                        
/* local variable declaration and initialization */    
uint32 oc, rc;                                        
static struct Node_KeyValues window_keys =           
              {3,15,{WindowM(0,0x00000100,2,0,0),   
                     WindowM(0,0x00000200,2,0,0),  
                     WindowM(0,0x00000300,2,0,0), 
                     WindowM(0,0x00000400,2,0,0),                      
                     WindowM(0,0x00000500,2,0,0),                     
                     WindowM(0,0x00000600,2,0,0),                    
                     WindowM(0,0x00000700,2,0,0),                   
                     WindowM(0,0x00000800,2,0,0),                  
                     WindowM(0,0x00000900,2,0,0),                 
                     WindowM(0,0x00000A00,2,0,0),                
                     WindowM(0,0x00000B00,2,0,0),               
                     WindowM(0,0x00000C00,2,0,0),              
                     WindowM(0,0x00000D00,2,0,0)}             
              };                                             
struct bitmapdata bm;                                       
bm.array    = (uint32 *)ARRAYSTART;                        
bm.num_bits = (ARRAYEND-ARRAYSTART) * BITS_PER_BYTE;      
                                                         
 /* Get Domain's current memory tree key */             
 KALL(FDOM, Domain_GetMemory) KEYSTO (MN);             
                                                      
/* Build Fresh Segment for working storage */        
 KALL(COMPONENTSNODE, Node_Fetch+CFSF) KEYSTO (K1); 
 KALL(K1, 0) KEYSFROM (FSB, FM, FSB) RCTO(rc) KEYSTO (K1);             
                                                                      
 /* and install in (slot 2 of) memory tree */                        
 KALL(MN, Node_Swap+2) KEYSFROM (K1);                               
                                                                   
/* Fill in rest of LSS5 node of the memory tree     */            
/* slots 3 thru 15 have local window keys on slot 2 */           
 KALL(MN, Node_WriteData) STRUCTFROM(window_keys);              
                                                               
/* Initialize bit array, but do not zero it */                
 bminit(&bm,DO_NOT_INITIALIZE);                              
                                                            
/* Build and return entry key to this domain as SIA key */ 
 KALL(FDOM, Domain_MakeStart) KEYSTO (ENTRY);             
 LDEXBL (CALLER,0) KEYSFROM (ENTRY);                     
                                                        
/* Main Program Loop */                               
 for(;;) {                                           
   LDENBL OCTO(oc) KEYSTO(,,,CALLER);                                  
   RETJUMP();                                                         
                                                                     
   /* test order codes and process appropriately */                 
                                                                   
   if (oc == SIA_AllocateNewInteger ) {              /* allocate new */
      rc = bmget(&bm);                                                
      LDEXBL (CALLER,rc);                                            
   }                                                                
   else if (oc < KT) {                            /* de-allocate old */
      rc = bmfree(&bm,oc);                                            
      LDEXBL (CALLER,rc);                                            
   }                                                                
   else if (oc == SIA_ReturnLowestAllocated) {      /* return lowest */
      rc = bmlow(&bm);                                                
      LDEXBL (CALLER,rc);                                            
   }                                                                
   else if (oc == KT) {                           /* return KT value */
      LDEXBL (CALLER,0x00000026);                                     
   }                                                                 
   else if (oc == KT+4) {                          /* destroy domain */
      KALL(MN,Node_Fetch+2) KEYSTO(K0);                               
      KALL(K0, KT+4);        /* Destroy the segment */               
      exit();                /* and self */                         
   }                                                               
   else LDEXBL (CALLER,KT+2);                      /* bad order code */
 } /* end for(;;) loop */                                             
} /* end main */
