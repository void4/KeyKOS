/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Dynamic memory allocation routines */                                
#include <string.h>                                                     
#define NULL   0                                                        
#define NALLOC 128                                                      
#define min(a,b) ((a) < (b) ? (a) : (b))                                
                                                                        
typedef double ALIGN;    /* forces alignment on 8 bytes boundary */     
union header {                              /* free block header */     
      struct { union header *ptr;             /* next free block */     
               unsigned size;         /* size of this free block */     
      } s;                                                              
      ALIGN  x;                     /* force alignment of blocks */     
};                                                                      
typedef union header HEADER;                                            
                                                                        
   struct mwork {                                                       
       HEADER fsp;                          /* free space header */     
       HEADER head;                 /* head of the free list     */     
   };                                                                   
/*                                                                      
   initalloc   - initialize the malloc headers                          
                                                                        
      w    - pointer to a work area that must not be touched            
             w is 4 words  (header,header {pointer,size,pointer,size})  
      ap   - the address of the area to be used for storage allocation  
      size - the size of the area to be used for storage allocation     
*/                                                                      
void initalloc(w,ap,size)                                               
     struct mwork *w;                                                   
     void *ap;                                                          
     int size;                                                          
{                                                                       
    w->fsp.s.ptr=ap;                                                    
    w->fsp.s.size=size/sizeof(HEADER);                                  
    w->head.s.ptr=NULL;                                                 
    w->head.s.size=0;                                                   
}                                                                       
                                                                        
/*                                                                      
    malloc - allocate size bytes of memory aligned to 8 byte boundary   
                                                                        
      w    - the work area (8 words) that must be initialized           
      size - number of bytes to allocate                                
*/                                                                      
void *malloc(w,size)                                                    
      struct mwork *w;                                                  
      size_t size;                                                      
{                                                                       
   HEADER *new, *old;                                                   
   int nunits;                                                          
                                                                        
   nunits = 1 + (size+sizeof(HEADER)-1)/sizeof(HEADER);                 
                                                                        
   /* scan free list */                                                 
   for (new=&(w->head); new != NULL;                                    
        old = new, new=new->s.ptr)     {                                
      if (new->s.size >= nunits)    {                                   
         if (new->s.size == nunits)                                     
            old->s.ptr = new->s.ptr;                                    
         else {                                                         
            new->s.size -= nunits;                                      
            new +=new->s.size;                                          
            new->s.size=nunits;                                         
         }                                                              
         new->s.ptr = NULL;                                             
         return( (char *)(new+1) );                                     
      }                                                                 
   }                                                                    
                                                                        
   if (w->fsp.s.size < nunits) return(NULL);    /* not enough space */  
                                                                        
   w->fsp.s.size -= nunits;             /* decrement free space size */ 
   new = (HEADER *)w->fsp.s.ptr;     /* point to allocated space */     
   w->fsp.s.ptr += nunits;                  /* point to a free space */ 
   new->s.size = nunits;                                                
   new->s.ptr = NULL;                                                   
   return((char *)(new+1));                                             
}                                                                       
                                                                        
                                                                        
/*                                                                      
   free - dynamic memory free. Returns number of bytes freed.           
                                                                        
     w    - work area (8 words) must be initialized                     
     ap   - the area to be freed (obtained from malloc or calloc)       
*/                                                                      
void free(w,ap)                                                         
     struct mwork *w;                                                   
     void *ap;                                                          
{                                                                       
   HEADER *p, *new;                                                     
   int      size;                                                       
                                                                        
   p = (HEADER *)ap-1;                                                  
   size = (int)(p->s.size);                                             
                                                                        
   /* scan free list */                                                 
   for (new = &(w->head); new->s.ptr < p && new->s.ptr != NULL;         
                      new=new->s.ptr);                                  
                                                                        
   if (p+p->s.size == new->s.ptr) {         /* join to right side */    
      p->s.size += new->s.ptr->s.size;                                  
      p->s.ptr = new->s.ptr->s.ptr;                                     
   }                                                                    
   else p->s.ptr = new->s.ptr;                                          
                                                                        
   if (new+new->s.size == p) {              /* join to left side */     
      new->s.size += p->s.size;                                         
      new->s.ptr = p->s.ptr;                                            
   }                                                                    
   else new->s.ptr = p;                                                 
}                                                                       
                                                                        
/*                                                                      
   calloc - allocate a zeroed space for an array with                   
   n elements of size lsize;                                            
*/                                                                      
void *calloc(w,n,lsize)                                                 
    struct mwork *w;                                                    
    size_t n,lsize;                                                     
{                                                                       
                                                                        
   size_t  size;                                                        
   char *ptr;                                                           
                                                                        
   size = n * lsize;    /* total number of bytes to allocate */         
   ptr  = malloc(w,size); /* allocate space */                          
   memset(ptr, 0, size);    /* byte memory zero */                        
   return(ptr);                                                         
}                                                                       
                                                                        
/*                                                                      
   cfree - free memory allocated with calloc                            
                                                                        
      w     - the work area (8 words) must be initialized               
      ptr   - the area to be freed (obtained from calloc)               
*/                                                                      
void cfree(w,ptr)                                                       
    struct mwork w;                                                     
    void *ptr;                                                          
{                                                                       
   free(w,ptr);                                                         
}                                                                       
                                                                        
/*                                                                      
   realloc - change the size of the memory pointed to by ptr to size.   
   The contents remain unchanged to the lesser of the old and new sizes.
                                                                        
      w     - the work area (8 words) must be initialized               
      ptr   - the area obtained from malloc                             
      size  - the new size                                              
*/                                                                      
void *realloc(w,ptr,size)                                               
   struct mwork *w;                                                     
   void *ptr;                                                           
   size_t size;                                                         
{                                                                       
                                                                        
   free(w,ptr);                                                         
   return( ptr = malloc(w,size) );
}
