/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

hexcvt(str,buf,len)
   char *str,*buf;
   int len;
{
   int i,j;
   
   for(i=0;i<len;i++) {
     j=str[i] >> 4;	
     j= j & 0x0f;
     if (j < 10) j += 0x30;
     else j = (j-10)+0x41;
     *buf=j;
     buf++;
     j=str[i] & 0x0f;
     if (j < 10) j += 0x30;
     else j = (j-10)+0x41;
     *buf=j;
     buf++;
   }
   *buf=0;
}
