/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************/
/* BITMAP - A set of C routines to manage a bit array of integers     */
/*                                                                    */
/* Implementation:                                                    */
/*    Integers are maintained in a bit array of unsigned long ints    */
/*    Bits in each unsigned long int have values 0-31, left to right  */
/*    A small integer = its word index times 32 + its bit value + 1   */
/**********************************************************************/
                                                                        
#include "kktypes.h"                                                    
#include "bitmap.h"                                                     
                                                                        
#define ALL_ZERO   0x00000000                                           
#define ALL_ONES   0xFFFFFFFF                                           
#define BITS_PER_UINT32    32                                           
#define LN_BITS_PER_UINT32  5                                           
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/*   BMINIT - initialize routine variables                           */ 
/*            zero the array if flag is zero                         */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
void bminit(bm, zero_flag)                                              
     struct bitmapdata *bm;                                             
     int zero_flag;                                                     
{                                                                       
   int i;                                                               
   bm->next_index = 0;                                                  
   bm->low_index  = 0;                                                  
   bm->high_index = 0;                                                  
   bm->array_size = ((bm->num_bits-1) >> LN_BITS_PER_UINT32) + 1;       
                                                                        
   if (zero_flag != 0) return;           /* dont zero the bit array */  
                                                                        
   for (i = 0; i < bm->array_size; i++)  bm->array[i] = ALL_ZERO;       
} /* end bminit */                                                      
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/* BMVALUE- Calculate integer from array index and bit contents      */ 
/* Input  - index of the unsigned long in the array                  */ 
/*          value of the ulong to search for first zero bit          */ 
/*          address of int to return the bit number from the ulong   */ 
/* Output - Integer value of bit position                            */ 
/* Method - Tests high (leftmost) bit with a mask                    */ 
/*          shifting as necessary to find the first zero bit         */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
uint32 bmvalue(index,value,p_bitno)                                     
     int index;                                                         
     uint32 value, *p_bitno;                                            
 {                                                                      
  int i;                                                                
  for (i=0; i<BITS_PER_UINT32; i++) {                                   
     if (value & 0x80000000)   /* what is the left-most bit ?        */ 
         value <<= 1;          /* was a one, shift left for next bit */ 
     else break;               /* was a zero, stop search            */ 
  }                                                                     
  *p_bitno = i;                /* return bitno for mask calculation  */ 
  return((index << LN_BITS_PER_UINT32) + i + 1);                        
 } /* end bmvalue */                                                    
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/*   BMGET - get integer value of next un-allocated bit              */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
uint32 bmget(bm)                                                        
       struct bitmapdata *bm;                                           
{                                                                       
   int i;                                                               
   uint32 rc, mask, bitno;                                              
   rc = ALL_ONES;                         /* set none available flag */ 
   for (i = bm->next_index; i < bm->array_size; i++) {                  
      if (bm->array[i] != ALL_ONES) {           /* found one       */   
         rc = bmvalue(i, bm->array[i],&bitno);  /* get its value   */   
         mask = (uint32)0x80000000 >> bitno;      /* set mask        */ 
         bm->array[i] = bm->array[i] | mask;  /* and turn bit on */     
         if (i > bm->next_index) bm->next_index = i;   /* new high ? */ 
         if (i > bm->high_index) bm->high_index = i;   /* new high ? */ 
         break;                   /* all done, so leave the for loop */ 
      }                                                                 
   }                                                                    
   return(rc);                                                          
} /* end bmget */                                                       
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/*   BMFREE - return and de-allocate an integer from the bit map     */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
uint32 bmfree(bm,oc)                                                    
       struct bitmapdata *bm;                                           
       uint32 oc;                                                       
{                                                                       
   int i;                                                               
   uint32 rc, mask, bitno;                                              
   oc = oc - 1;                  /* bit zero maps to small integer 1 */ 
   i = oc >> LN_BITS_PER_UINT32; /* divide by 32 to get array index  */ 
   /* get rightmost 5 bits, shift (32-5) left, then right  */           
   bitno = oc    << (BITS_PER_UINT32 - LN_BITS_PER_UINT32);             
   bitno = bitno >> (BITS_PER_UINT32 - LN_BITS_PER_UINT32);             
   mask = (uint32)0x80000000 >> bitno;           /* set mask bit     */ 
   if (bm->array[i] & mask) {                  /* is it allocated? */   
      rc = 0;                                    /* yes it is        */ 
      bm->array[i] = bm->array[i] ^ mask;      /* turn it off    */     
      if (i < bm->next_index) bm->next_index = i;  /* see if new low */ 
      if (i < bm->low_index)  bm->low_index  = i;  /* see if new low */ 
   }                                                                    
   else rc = 1;                                 /* was not allocated */ 
   return(rc);                                                          
} /* end bmfree */                                                      
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*   BMSET - Set bit in bit map corresponding to given integer       */  
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
uint32 bmset(bm,oc)
       struct bitmapdata *bm;
       uint32 oc;
{      
   int i;
   uint32 rc, mask, bitno;
   oc = oc - 1;                  /* bit zero maps to small integer 1 */
   i = oc >> LN_BITS_PER_UINT32; /* divide by 32 to get array index  */
   /* get rightmost 5 bits, shift (32-5) left, then right  */
   bitno = oc    << (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   bitno = bitno >> (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   mask = (uint32)0x80000000 >> bitno;           /* set mask bit  */ 
   bm->array[i] = bm->array[i] | mask;      /* turn it on    */
} /* end bmset */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*   BMRESET - reset bit in bit map corresponding to given integer       */  
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
uint32 bmreset(bm,oc)
       struct bitmapdata *bm;
       uint32 oc;
{      
   int i;
   uint32 rc, mask, bitno;
   oc = oc - 1;                  /* bit zero maps to small integer 1 */
   i = oc >> LN_BITS_PER_UINT32; /* divide by 32 to get array index  */
   /* get rightmost 5 bits, shift (32-5) left, then right  */
   bitno = oc    << (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   bitno = bitno >> (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   mask = (uint32)0x80000000 >> bitno;           /* set mask bit  */ 
   bm->array[i] = bm->array[i] ^ mask;      /* turn it off    */
} /* end bmreset */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*   BMTEST - Test bit in bit map corresponding to given integer       */  
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
uint32 bmtest(bm,oc)
       struct bitmapdata *bm;
       uint32 oc;
{      
   int i;
   uint32 rc, mask, bitno;
   oc = oc - 1;                  /* bit zero maps to small integer 1 */
   i = oc >> LN_BITS_PER_UINT32; /* divide by 32 to get array index  */
   /* get rightmost 5 bits, shift (32-5) left, then right  */
   bitno = oc    << (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   bitno = bitno >> (BITS_PER_UINT32 - LN_BITS_PER_UINT32);
   mask = (uint32)0x80000000 >> bitno;           /* set mask bit  */ 
   if (bm->array[i] & mask) {      /* test it  */
	return (0);
   } else {
    	return (-1);
   }
}
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/*   BMLOW - return the lowest allocated integer in the bit map      */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
uint32 bmlow(bm)                                                        
       struct bitmapdata *bm;                                           
{                                                                       
   int i;                                                               
   uint32 rc, mask, bitno;                                              
   rc = 0;                                     /* set not found flag */ 
   for (i = bm->low_index; i <= bm->high_index; i++) {                  
      if (bm->array[i] != ALL_ZERO) {                 /* found one */   
         /* invert bits so allocated numbers are zero */                
         mask = bm->array[i] ^ ALL_ONES;                                
         rc = bmvalue(i,mask,&bitno);               /* get its value */ 
         break;                            /* and leave the for loop */ 
      }                                                                 
   }                                                                    
   if (rc == 0) {                     /* no ones found, reset bounds */ 
       bm->next_index = 0;                                              
       bm->low_index  = 0;                                              
       bm->high_index = 0;                                              
   }                                                                    
   return(rc);                                                          
} /* end bmlow */                                                       
                                                                        
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
/*   BMHIGH - return the highest allocated integer in the bit map    */ 
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/ 
uint32 bmhigh(bm)                                                       
       struct bitmapdata *bm;                                           
{                                                                       
   int i, j;                                                            
   uint32 rc, mask, bitno;                                              
   rc = 0;                                     /* set not found flag */ 
   for (i = bm->high_index; i >= bm->low_index; i--) {                  
      if (bm->array[i] != ALL_ZERO) {                 /* found one */   
         /* invert bits so allocated numbers are zero */                
         mask = bm->array[i] ^ ALL_ONES;                                
         for (j=0; j<BITS_PER_UINT32; j++) {    /* search from right */ 
           if (mask & 0x00000001)            /* is bit one or zero ? */ 
              mask >>= 1;           /* one, shift right for next bit */ 
           else break;              /* zero, stop search             */ 
         }                                                              
         rc = (i << LN_BITS_PER_UINT32) + (BITS_PER_UINT32 - j);        
         break;                         /* found one, leave for loop */ 
      }                                                                 
   }                                                                    
   if (rc == 0) {                     /* no ones found, reset bounds */ 
       bm->next_index = 0;                                              
       bm->low_index  = 0;                                              
       bm->high_index = 0;                                              
   }                                                                    
   return(rc);                                                          
}  /* end */
