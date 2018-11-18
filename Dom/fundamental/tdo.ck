/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************************

  tod.ck

   TDO  Trusted Directory Object
 
    A simple record collection written in C
 
    This record collection uses techniques similar to the
    RC1 record collection.  There is an index segement (1 meg)
    and a data segment (13 meg).  THe program and stack are in
    the first meg.   All read write storage for the program
    is in the stack which is several pages long (at address 10000)
 
    Records are limited to 512 bytes.
 
    Index entries are in a doubly linked list with pointers to
    the name string and data string.
 
    There is room for 4096 records with keys (more without keys)
    If any limits are reached
    (4096 keys, 1 meg index, 13megs data)
    RC=11 (FULL) is returned
 
    KT statistic values are not updated.
 
    Components   -  The FSC and SNC are used
***************************************************************************/

#include "kktypes.h"
#include "keykos.h"
#include "tdo.h"
#include "domain.h"
#include "sb.h"
#include "node.h"
//#include "akt.h"
#include "ocrc.h"
 
#define COMPFSC 0
#define COMPSNC 1
 
#define NOREADU   0x40
#define NOWRITEU  0x20
#define NOREADR   0x10
#define NOWRITER  0x08
#define NOUPDATE  0x04
#define NODELETE  0x02
#define NOEMPTY   0x01
 
     int stacksiz=8192;     /* tell prolog about size of stack */
 
     KEY comp   = 0;       /* components node */
     KEY sb2    = 1;
     KEY caller = 2;
     KEY dom    = 3;
     KEY sb     = 4;
     KEY m      = 5;
     KEY domcre = 6;
     KEY meg16  = 7;
     KEY dataseg = 8;
     KEY indexseg = 9;
     KEY snode = 10;
     KEY userkey = 11;
 
     KEY k3     = 12;
     KEY k2     = 13;
     KEY k1     = 14;
     KEY k0     = 15;
 
   struct datakey {     /* string form for Node__WRITE_DATA */
     UINT32 s1;                 /* start slot number */
     UINT32 s2;                 /* end slot number */
     char body[16];         /* data key body */
   };
   struct istring {             /* input string description */
      SINT16 namelen;            /*  after parsing */
      char *name;
      SINT16 datalen;
      char *data;
   };
   struct index {
      struct index *next;
      struct index *prev;
      char *name;
      char *data;
      SINT16 namelen;
      SINT16 datalen;
      UINT32 slot;              /* 0 is never used and means no key */
   };
 
    char title[]="TDO     ";
 
 void initalloc(),*calloc(),*free();
 char *docalloc();
 struct index *locate9(),*locate10(),*locate11();
 struct index *locate12(),*locate13();
 UINT32 allocslot();
 
factory(ocode,ord)
   UINT32 ocode,ord;
{
   JUMPBUF;
   double pool1[2],pool2[2]; /* work areas for memory allocation */
   struct datakey dk;
   char parmstring[512];
   char retstring[512];
   SINT32 i,actlen,rctype,retlen;
   UINT32 rc,retrc;
 
   struct ktdata {
     UCHAR fmt,resv1;
     UINT16 id,ver,resv2;
     UINT32 rights,nrec,maxlen,minlen,maxname,minname,limit,resv3;
     double tod,resv4;
     char userdata[128];
   };
   struct ktdata ktvalues={0,0,1,1,0,0,0,0,0,0,0,4057,0,0.0,0.0};
 
   struct Node_DataByteValue ndb;
   struct index headindex;
   struct index tailindex;
   struct istring is;
   struct index *ti;
   UINT32 userlen,key;
   SINT16 db;
   UCHAR databyte;
   UCHAR pool3[512];
 
   memset(pool3,0,512);
   pool3[0]=128;
   headindex.namelen=1000;
   tailindex.namelen=1000;
 
   KC (dom,Domain_GetMemory) KEYSTO(meg16);
   KC (comp,COMPFSC) KEYSTO(k1);
   KC (k1,0) KEYSFROM(sb,m,sb2) KEYSTO(indexseg);
   KC (k1,0) KEYSFROM(sb,m,sb2) KEYSTO(dataseg);
   KC (meg16,Node_Swap+2) KEYSFROM(indexseg);  /* index segment */
   KC (meg16,Node_Swap+3) KEYSFROM(dataseg);   /* data segment  */
   KC (comp,COMPSNC) KEYSTO(snode);
   KC (snode,0) KEYSFROM(sb,m,sb) KEYSTO(snode);
 
   for (dk.s1=4;dk.s1<16;dk.s1++) {
     dk.s2=dk.s1;
     for (i=0;i<16;i++) dk.body[i]='\0';
     dk.body[15]=0x32;
     dk.body[13]=16*(dk.s1-3);
     KC (meg16,45) CHARFROM(&dk,24);
   }
 
   initalloc(pool1,0x00200000,0x00100000);
   initalloc(pool2,0x00300000,0x00D00000);
 
   rctype=ocode;
   userlen=0;
   headindex.next=&tailindex;
   headindex.prev=0;
   tailindex.next=0;
   tailindex.prev=&headindex;
 
   parmstring[0]=0;
   parmstring[511]=0;
   KC (dom,Domain_MakeStart) KEYSTO(k1);
   LDEXBL (caller,0) KEYSFROM(k1);
   for (;;){
     LDENBL OCTO(ocode) KEYSTO(userkey,,,caller)
        CHARTO(parmstring,512,actlen) DBTO(db);
     RETJUMP();
     if (actlen > 512) actlen=512;
     ktvalues.rights=db;
     databyte=db;
 
     if(ocode == KT) {
        if(rctype) LDEXBL(caller,TDO_NSAKT) STRUCTFROM(ktvalues);
        else LDEXBL (caller,TDO_ESAKT) STRUCTFROM(ktvalues);
        continue;
     }
     if(ocode == DESTROY_OC) {
        if(databyte & NOEMPTY) {LDEXBL (caller,8);continue;}
        break;
     }
     key=0;
 
     switch(ocode) {
       case TDO_Weaken:      /* weaken */
 /*
   0x40 No Read User Data
   0x20 No Write User Data
   0x10 No Read records
   0x08 No Write records
   0x04 No Update Existing records
   0x02 No Delete records (NS)
   0x01 No Empty, Truncate (ES) or destroy
 */
         if(actlen != 4) {LDEXBL (caller,7);break;}
         databyte=databyte | parmstring[3];
         KC (dom,64) CHARFROM(&databyte,1) KEYSTO(k0);
         ktvalues.rights=databyte;
         LDEXBL (caller,0) CHARFROM(&ktvalues.rights,4) KEYSFROM(k0);
         break;
       case TDO_Empty:                /* empty */
         if(databyte & NOEMPTY) {LDEXBL (caller,8);break;}
         KC (indexseg,5) CHARFROM("\000\000\000\000\000\000\000\000",8);
         KC (dataseg,5)  CHARFROM("\000\000\000\000\000\000\000\000",8);
         initalloc(pool1,0x00200000,0x00100000);
         initalloc(pool2,0x00300000,0x00D00000);
         memset(pool3,0,512);
         pool3[0]=128;
         headindex.next=&tailindex;
         headindex.prev=0;
         tailindex.next=0;
         tailindex.prev=&headindex;
         ktvalues.nrec=0;
         LDEXBL (caller,0);
         break;
       case TDO_AddReplaceKey: /* add or replace with key */
         key=1;
       case TDO_AddReplace:    /* add or replace */
         if(databyte & NOWRITER) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         retrc=0;
         KC (dom,Domain_SwapKey+k3);  /* put dk0 into k3 */
         if(ti=locate11(&is,&headindex,&tailindex)) {  /* = OK */
           buildret(ti,retstring,&retlen);
           if(ti->slot) {
              KC (dom,Domain_SwapKey+k3) KEYSFROM(userkey);
              getkey(ti->slot);
              KC (dom,Domain_SwapKey+k3) KEYSFROM(userkey)
                                         KEYSTO(userkey);
              retrc=1;
           }
           else retrc=2;
           remove(ti,pool1,pool2,pool3);
           ktvalues.nrec--;
         }
         else {          /* not found */
           if(!rctype) {  /* ES */
             i=0;
             if((is.namelen != 4) || memcmp(is.name,&i,4)){
               LDEXBL (caller,7);
               break;
             }
             ti=tailindex.prev;
             if(ti != &headindex) memcpy(&i,ti->name,4);
             i++;  /* 1 or next */
             memcpy(is.name,&i,4);
           }
         }
         rc=add(&is,&headindex,&tailindex,pool1,pool2,pool3,key);
         if(!rc) {LDEXBL (caller,11);break;}
         ktvalues.nrec++;
         if(rctype) {
           if(retrc) LDEXBL (caller,retrc) CHARFROM(retstring,retlen)
                 KEYSFROM(k3);
           else LDEXBL (caller,0);
         }
         else {
           parmstring[0]=4;
           memcpy(parmstring+1,&i,4);
           LDEXBL (caller,0) CHARFROM(parmstring,5);
         }
         break;
       case TDO_AddKey:        /* add with key */
         key=1;
       case TDO_Add:           /* add */
         if(databyte & NOWRITER) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(rctype) {        /* NS */
           if(ti=locate11(&is,&headindex,&tailindex)) { /* = OK */
             LDEXBL (caller,3);
             break;
           }
 
           rc=add(&is,&headindex,&tailindex,pool1,pool2,pool3,key);
           if(!rc) {LDEXBL (caller,11);break;}
           ktvalues.nrec++;
           LDEXBL (caller,0);
           break;
         }
         else {            /* ES */
           i=0;
           if((is.namelen != 4) || memcmp(is.name,&i,4)){
             LDEXBL (caller,7);
             break;
           }
           ti=tailindex.prev;
           if(ti != &headindex) memcpy(&i,ti->name,4);
           i++;  /* 1 or next */
           memcpy(is.name,&i,4);
           rc=add(&is,&headindex,&tailindex,pool1,pool2,pool3,key);
           if(!rc) {LDEXBL (caller,11);break;}
           ktvalues.nrec++;
           parmstring[0]=4;
           memcpy(parmstring+1,&i,4);
           LDEXBL (caller,0) CHARFROM(parmstring,5);
           break;
         }
       case TDO_ReplaceKey:    /* replace with key */
         key=1;
       case TDO_Replace:       /* replace */
         if(databyte & NOWRITER) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         retrc=0;
         KC (dom,Domain_SwapKey+k3);
         if(ti=locate11(&is,&headindex,&tailindex)) { /* = OK */
           buildret(ti,retstring,&retlen);
           if(ti->slot) {
              KC (dom,Domain_SwapKey+k3) KEYSFROM(userkey);
              getkey(ti->slot);
              KC (dom,Domain_SwapKey+k3) KEYSFROM(userkey)
                                         KEYSTO(userkey);
              retrc=1;
           }
           else retrc=2;
           remove(ti,pool1,pool2,pool3);
           rc=add(&is,&headindex,&tailindex,pool1,pool2,pool3,key);
           if(!rc) {LDEXBL (caller,11);break;}
           if(retrc) LDEXBL (caller,retrc) CHARFROM(retstring,retlen)
                  KEYSFROM(k3);
           else LDEXBL (caller,0);
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetFirst:      /* get first */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         ti=headindex.next;
         if (ti->namelen != 1000) {
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetLessThan:   /* get < */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(ti=locate9(&is,&headindex,&tailindex)) { /* = OK */
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetLessEqual:  /* get <= */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(ti=locate10(&is,&headindex,&tailindex)) { /* = OK */
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetEqual:      /* get = */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(ti=locate11(&is,&headindex,&tailindex)) { /* = OK */
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetGreaterEqual:  /* get >= */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(ti=locate12(&is,&headindex,&tailindex)) { /* = OK */
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetGreaterThan:    /* get > */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(ti=locate13(&is,&headindex,&tailindex)) { /* = OK */
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_GetLast:       /* get last */
         if(databyte & NOREADR) {LDEXBL (caller,8);break;}
         ti=tailindex.prev;
         if (ti->namelen != 1000) {
            buildret(ti,parmstring,&actlen);
            LDEXBL (caller,0) CHARFROM(parmstring,actlen);
            if(ti->slot) {
                getkey(ti->slot);
                LDEXBL (caller,1) CHARFROM(parmstring,actlen)
                    KEYSFROM(userkey);
            }
         }
         else LDEXBL (caller,4);
         break;
       case TDO_Delete:       /* delete */
         if(databyte & NODELETE) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(rctype) {   /* NS */
delete: ;
           if(ti=locate11(&is,&headindex,&tailindex)) { /* = OK */
              buildret(ti,parmstring,&actlen);
              if(ti->slot) {
                 getkey(ti->slot);
                 retrc=1;
              }
              else {
                 KC (dom,Domain_SwapKey+userkey);
                 retrc=0;
              }
              remove(ti,pool1,pool2,pool3);
              ktvalues.nrec--;
              if(ocode==TDO_Delete) retrc=0;
              LDEXBL (caller,retrc) CHARFROM(parmstring,actlen)
                  KEYSFROM(userkey);
           }
           else {
             LDEXBL (caller,4);
           }
         }
         else  LDEXBL (caller,INVALIDOC_RC);
         break;
       case TDO_TruncateAtName:  /* ES truncate at name */
         if(databyte & NOEMPTY) {LDEXBL (caller,8);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(rctype) goto delete;
         if(is.namelen != 4) {LDEXBL (caller,7);break;}
         if(ti=locate11(&is,&headindex,&tailindex)) {  /* = OK */
           trunc(ti,pool1,pool2,&ktvalues.nrec,pool3);
           LDEXBL (caller,0);
         }
         else LDEXBL (caller,4);
         break;
       case TDO_TruncateAfterName: /* ES truncate after name */
         if(databyte & NOEMPTY) {LDEXBL (caller,8);break;}
         if(rctype) {LDEXBL (caller,INVALIDOC_RC);break;}
         parseinput(parmstring,actlen,&is,rctype);
         if(is.namelen == 4) {
           if(ti=locate11(&is,&headindex,&tailindex)) { /* = OK */
             ti=ti->next;
             trunc(ti,pool1,pool2,&ktvalues.nrec,pool3);
             LDEXBL (caller,0);
           }
         }
         else if(is.namelen == 0) {
           ti=headindex.next;
           trunc(ti,pool1,pool2,&ktvalues.nrec,pool3);
           LDEXBL (caller,0);
         }
         else LDEXBL (caller,4);
         break;
       case TDO_WriteUserData: /* write user data */
         if(databyte & NOWRITEU) {LDEXBL (caller,8);break;}
         if(actlen > 128) {LDEXBL (caller,6);break;}
         memcpy(ktvalues.userdata,parmstring,actlen);
         userlen=actlen;
         LDEXBL (caller,0);
         break;
       case TDO_ReadUserData:   /* read user data */
         if(databyte & NOREADU) {LDEXBL (caller,8);break;}
         LDEXBL (caller,0) CHARFROM(ktvalues.userdata,userlen);
         break;
       default:
         LDEXBL (caller,INVALIDOC_RC);
         break;
     }
   }
 
   KC (indexseg,DESTROY_OC)  RCTO(rc);
   KC (dataseg,DESTROY_OC)   RCTO(rc);
   KC (snode,DESTROY_OC)     RCTO(rc);
 
   return 0;
}
parseinput(ptr,len,ides,rctype)
   char *ptr;
   SINT32 len;
   struct istring *ides;
   SINT32 rctype;
{
   if(!len) {
     ides->namelen=0;
     ides->name=0;
     ides->datalen=0;
     ides->data=0;
     return 0;
   }
   ides->namelen=*ptr;
   ides->name=ptr+1;
   if(len < ides->namelen+1) {
     ides->namelen=len-1;
     ides->datalen=0;
     ides->data=0;
     return 0;
   }
   ides->datalen=len-1-ides->namelen;
   ides->data=ptr+1+ides->namelen;
   return 0;
}
trunc(sptr,pool1,pool2,nrec,pool3)
   struct index *sptr;
   double pool1[2],pool2[2];
   SINT32 *nrec;
   char *pool3;
{
   struct index *ti;
 
   while(sptr->namelen != 1000) {   /* till end */
     ti=sptr->next;                 /* get next */
     remove(sptr,pool1,pool2,pool3);      /* remove this one */
     *nrec--;
     sptr=ti;                       /* rotate hips */
   }
   return 0;
}
remove(iptr,pool1,pool2,pool3)
   struct index *iptr;
   double pool1[2],pool2[2];
   char *pool3;
{
   struct index *tp,*tn;
 
   if(iptr->slot) freeslot(iptr->slot,pool3);
   tp=iptr->prev;
   tn=iptr->next;
   tp->next=tn;
   tn->prev=tp;
   free(pool1,iptr->name);
   free(pool2,iptr->data);
   free(pool1,iptr);
   return 1;
}
add(ides,headptr,tailptr,pool1,pool2,pool3,key)
   struct istring *ides;
   struct index *headptr,*tailptr;
   double pool1[2],pool2[2];
   SINT32 key;
   char *pool3;
{
   struct index *in,*tp,*tn,*newindex;
   SINT32 len,i;
   char buf[256];
 
   newindex=(struct index *)docalloc(pool1,1,sizeof(struct index));
   if(!newindex) return 0;
 
   newindex->namelen=ides->namelen;
   newindex->datalen=ides->datalen;
   newindex->name=(char *)docalloc(pool1,1,ides->namelen);
   if(!newindex->name) {
      free(pool1,newindex);
      return 0;
   }
   newindex->data=(char *)docalloc(pool2,1,ides->datalen);
   if(!newindex->data) {
      free(pool1,newindex->name);
      free(pool1,newindex);
      return 0;
   }
   memcpy(newindex->name,ides->name,ides->namelen);
   memcpy(newindex->data,ides->data,ides->datalen);
   if(key) {
      newindex->slot=allocslot(pool3);
      if(!newindex) {
         free(pool1,newindex->name);
         free(pool2,newindex->data);
         free(pool1,newindex);
         return 0;
      }
      putkey(newindex->slot);
   }
   in=headptr->next;
   while(in) {
     if( (in->namelen == 1000) ) {  /* end, put it in */
        tn=in->prev;
        tn->next=newindex;
        in->prev=newindex;
        newindex->prev=tn;
        newindex->next=in;
        return 1;
     }
     len=in->namelen;
     if (ides->namelen < in->namelen) len=ides->namelen;
     i=memcmp(ides->name,in->name,len);
     if(i<0 || (i==0 && ides->namelen < in->namelen)) {  /* in front */
        tn=in->prev;
        tn->next=newindex;
        in->prev=newindex;
        newindex->prev=tn;
        newindex->next=in;
        return 1;
     }
     in=in->next;
   }
   if(key) freeslot(newindex->slot,pool3);
   free(pool1,newindex->name);
   free(pool2,newindex->data);
   free(pool1,newindex);
   return 0;
}
struct index *locate9(ides,headptr,tailptr)
   struct istring *ides;
   struct index *headptr,*tailptr;
{
   struct index *in;
   SINT32 i,len;
 
   in=tailptr->prev;   /* start at end   */
   while(in) {
     if(in->namelen == 1000) return 0;
     len=in->namelen;
     if(ides->namelen < in->namelen) len=ides->namelen;
     i=memcmp(in->name,ides->name,len);
     if(i<0) return in;
     if( (i==0) && (in->namelen < ides->namelen)) return in;
     in=in->prev;
   }
   return 0;
}
struct index *locate10(ides,headptr,tailptr)
   struct istring *ides;
   struct index *headptr,*tailptr;
{
   struct index *in;
   SINT32 i,len;
 
   in=tailptr->prev;   /* start at end   */
   while(in) {
     if(in->namelen == 1000) return 0;
     len=in->namelen;
     if(ides->namelen < in->namelen) len=ides->namelen;
     i=memcmp(in->name,ides->name,len);
     if(i<0) return in;
     if( (i==0) && (in->namelen <= ides->namelen)) return in;
     in=in->prev;
   }
   return 0;
}
struct index *locate11(ides,headptr,tailptr)
   struct istring *ides;
   struct index *headptr,*tailptr;
{
   struct index *in;
 
   in=headptr->next;    /* start at beginning */
   while(in) {
     if(in->namelen == 1000) return 0;
     if(in->namelen == ides->namelen) {
       if(!memcmp(in->name,ides->name,in->namelen)) return in;
     }
     in=in->next;
   }
   return 0;
}
struct index *locate12(ides,headptr,tailptr)
   struct istring *ides;
   struct index *headptr,*tailptr;
{
   struct index *in;
   SINT32 i,len;
 
   in=headptr->next;   /* start at beginning */
   while(in) {
     if(in->namelen == 1000) return 0;
     if(ides->namelen == 0) return in;
     len=in->namelen;
     if(ides->namelen < in->namelen) len=ides->namelen;
     i=memcmp(in->name,ides->name,len);
     if(i>0) return in;
     if( (i==0) && (in->namelen >= ides->namelen)) return in;
     in=in->next;
   }
   return 0;
}
struct index *locate13(ides,headptr,tailptr)
   struct istring *ides;
   struct index *headptr,*tailptr;
{
   struct index *in;
   SINT32 i,len;
 
   in=headptr->next;      /* start at beginning */
   while(in) {
     if(in->namelen == 1000) return 0;
     if(ides->namelen == 0) return in;
     len=in->namelen;
     if(ides->namelen < in->namelen) len=ides->namelen;
     i=memcmp(in->name,ides->name,len);
     if(i>0) return in;
     if( (i==0) && (in->namelen > ides->namelen)) return in;
     in=in->next;
   }
   return 0;
}
buildret(in,parmstring,actlen)
  struct index *in;
  char *parmstring;
  SINT32 *actlen;
{
   *parmstring=in->namelen;
   memcpy(parmstring+1,in->name,in->namelen);
   memcpy(parmstring+1+in->namelen,in->data,in->datalen);
   *actlen=in->namelen+in->datalen+1;
 
   return 0;
}
UINT32 allocslot(pool)
    UCHAR *pool;
{
    SINT32 i;
    for(i=0;i<512;i++) {
       if(pool[i] != 255) {
         if( !(pool[i]&0x80) ) {pool[i] |= 0x80; return i*8;}
         if( !(pool[i]&0x40) ) {pool[i] |= 0x40; return i*8+1;}
         if( !(pool[i]&0x20) ) {pool[i] |= 0x20; return i*8+2;}
         if( !(pool[i]&0x10) ) {pool[i] |= 0x10; return i*8+3;}
         if( !(pool[i]&0x08) ) {pool[i] |= 0x08; return i*8+4;}
         if( !(pool[i]&0x04) ) {pool[i] |= 0x04; return i*8+5;}
         if( !(pool[i]&0x02) ) {pool[i] |= 0x02; return i*8+6;}
         if( !(pool[i]&0x01) ) {pool[i] |= 0x01; return i*8+7;}
       }
    }
    return 0;
}
freeslot(slot,pool)
    UINT32 slot;
    UCHAR *pool;
{
    UINT32 i,j;
    i=slot/8;
    j=slot%8;
    pool[i] = pool[i] & ~((UCHAR)0x80>>j);
    return 1;
}
getkey(slot)
    UINT32 slot;
{
    UINT32 rc;
    JUMPBUF;
 
    KC (snode,41) CHARFROM(&slot,4) KEYSTO(userkey) RCTO(rc);
    return (int)rc;
}
putkey(slot)
    UINT32 slot;
{
    UINT32 rc;
    JUMPBUF;
 
    KC (snode,42) CHARFROM(&slot,4) KEYSFROM(userkey) RCTO(rc);
    return (int)rc;
}
char *docalloc(pool,num,size)
    double pool;
    SINT32 num,size;
{
    char *str;
    str=calloc(pool,num,size);
    return str;
}
