/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*                                                                      
  BITMAP - Bit Map support routines                                     
*/                                                                      
/******************************************************************/    
/* Data structure and routines for Bit Map support                */    
/* The caller fills in the first two elements of the structure.   */    
/* The remaining elements are reserved for the bitmap routines.   */    
/******************************************************************/    
                                                                        
struct bitmapdata                                                       
       { uint32 *array;       /* pointer to bitmap array          */    
         uint32 num_bits;     /* number of bits in bitmap array   */    
         uint32 array_size;   /* upper bound of array index       */    
         uint32 next_index;   /* index of lowest un-allocated bit */    
         uint32 low_index;    /* index of lowest allocated bit    */    
         uint32 high_index;   /* index of highest allocated bit   */    
       } ;                                                              
                                                                        
/******************************************************************/    
/* INIT initializes internal variables, and array if instructed   */    
/* GET  returns a new number, or 0 if none are available          */    
/* FREE returns 0 and deallocates, or 1 if number wasnt allocated */    
/* LOW  returns the lowest allocated, or 0 if none allocated      */    
/* HIGH returns the highest allocated, or 0 if none allocated     */    
/* VALUE is used internally by the above routines. Parameters are */    
/*       array index, array value, bit position                   */    
/* TEST is not implemented                                        */    
/* SET  is not implemented                                        */    
/******************************************************************/    

#ifndef _H_bitmap                                                       
#define _H_bitmap                                                       
                                                                        
#define INITIALIZE_TO_ZERO  0                                           
#define DO_NOT_INITIALIZE   1                                           
                                                                        
void   bminit(struct bitmapdata *, int);    /* initialize bit map */    
uint32 bmget(struct bitmapdata *);          /* allocate a number  */    
uint32 bmfree(struct bitmapdata *, uint32); /* free a number      */    
uint32 bmlow(struct bitmapdata *);          /* return lowest num  */    
uint32 bmhigh(struct bitmapdata *);         /* return highest num */    
                                                                        
uint32 bmvalue(int, uint32, uint32 *);      /* calc value of bit  */    
uint32 bmtest(struct bitmapdata *, uint32); /* test a bit     */    
uint32 bmset(struct bitmapdata *, uint32);  /* set a bit      */    
uint32 bmreset(struct bitmapdata *, uint32);  /* reset a bit      */    
#endif
