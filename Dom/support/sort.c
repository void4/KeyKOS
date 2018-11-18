/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define uchar unsigned char
#define size 14
/* "size" above is the stride between records. */
#define hunk short
/* Adjust definition of hunk above to define a scalar type that divides
   the size of the record evenly and is fast to move. (Bigger is faster.) */
static void scan(uchar * begin, uchar * end, int offset, uchar bit, int ms)
{uchar * lo = begin+offset, * hi = end+offset;
  /* Assert lo < hi */
 while(1){while(!(*lo & bit)){lo += size; if(lo > hi) goto deep;}
          while(  *hi & bit) {hi -= size; if(lo > hi) goto deep;}
    {int i; for(i=-offset; i<size-offset; i+=sizeof(hunk)) 
      {hunk t = *(hunk *)(hi + i);
      *(hunk *)(hi + i) = *(hunk *)(lo + i); *(hunk *)(lo + i) = t;}}} 
deep: if(bit == 1) {if(ms){
        if(begin < lo-size) scan(begin, lo-size-offset, offset+1, 128, ms-1);
        if(lo < end) scan(lo-offset, end, offset+1, 128, ms-1);}}
      else
       {if(begin < lo-size) scan(begin, lo-size-offset, offset, bit>>1, ms);
        if(lo < end) scan(lo-offset, end, offset, bit>>1, ms);}}

void sort14(uchar * begin, long asize, int offset, int ksize)
/* 'asize' is size, in bytes, of array to be sorted. */
/* 'offset' is location of sort key in each record.
   'ksize' is size, in bytes, of key. */
  {scan(begin, begin + asize-size, offset, 128, ksize);} 
#if 0
#include <stdlib.h>
#define tlen 67777
void main(){unsigned short ar[tlen][size/2]; short sum[size/2]; int i, j;
   for(i=0; i<size/2; i++) for(j=0; j<tlen; j++) ar[j][i] = rand();
if(0)for(j=0; j<tlen; j++) {for(i=0; i<size/2; i++) 
   printf("%04X", (unsigned int)(unsigned short)ar[j][i]); printf("\n");}
if(0)for(j=0; j<tlen; j++) ar[j][2] = j;
   for(i=0; i<size/2; i++) {sum[i]=0; for(j=0; j<tlen; j++) sum[i] += ar[j][i];}
   if(0)scan((uchar *)&ar[0][0], (uchar *)&ar[tlen-1][0], 4, 128, 4);
   sort14((uchar *)&ar[0][0], tlen*size, 4, 4);
if(0)for(j=0; j<tlen; j++) {for(i=0; i<size/2; i++) 
   printf("%04X", (unsigned int)(unsigned short)ar[j][i]); printf("\n");}
   for(i=0; i<size/2; i++) {short s=0; for(j=0; j<tlen; j++) s += ar[j][i];
     if(s != sum[i]) printf("Error, i=%u\n", i);}
   for(j=0; j<tlen-1; j++) 
     if(ar[j][2] > ar[j+1][2] || ar[j][2]==ar[j+1][2] && ar[j][3] > ar[j+1][3])
       printf("Unsorted, j=%u\n", j);}
#endif

