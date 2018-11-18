/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
    JUMPBUF;
/* ************************************************************
    Simple editor for basic system.  Modelled after CMS release 5
    EDITOR.  KC GNEDITF(531;sb,m,sb==>c;GNEDIT)
             GNEDIT(0;SIK,SOK,CCKNODE ==>c)
                CCKNODE.0  = CCK
                CCKNODE.1  = OBJECT
**************************************************************/
//#include <stdlib.h>
#include <string.h>
//#include <ctype.h>
#include "callseg.h"
#include "ocrc.h"
#include "fs.h"
#include "rc.h"

 
    KEY    comp = 0;
    KEY    sb2  = 1;
    KEY    caller=2;
    KEY    dom   =3;
    KEY    sb    =4;
    KEY    meter =5;
    KEY    domcre=6;
    KEY    sik   =7;
    KEY    sok   =8;
    KEY    ccknode  =9;
    KEY    cck   =10;
    KEY    callseg=11;
    KEY    object=12;
 
    KEY    k1    =14;
    KEY    k0    =15;
 
#define COMPCALLSEGF 1
 
    char *shortcmp();
    struct line *scan();
 
    struct line {
       struct line *next;
       struct line *prev;
       char data[256];
    };
 
#define INPUT 0
#define EDIT  1
#define RCOL  1
#define SEG   2
#define UNIXSEG 3
 
    int objtype,nrecords;
    char *segaddr;
    char rcolbuf[260];
 
    int mode = EDIT;
    char casesw = 'M';
 
    struct line base;
    struct line *current;
 
    unsigned long  rc,oc;
 
    char input[256];
    int eof;
    char toupper(char);

    struct FS_UnixMeta um;

    char title[]="GNEDIT  ";
 
factory()    /* link with CSTART and NO -N  */
{
    char *ptr,*iptr;
 
    KC (comp,COMPCALLSEGF) KEYSTO(callseg);
    KC (callseg,0) KEYSFROM(sb,meter,sb) KEYSTO(callseg) RCTO(rc);
    KC (dom,64) KEYSTO(k0);
    LDEXBL (caller,0) KEYSFROM(k0);
  for(;;) {
    LDENBL  OCTO(oc) KEYSTO(sik,sok,ccknode,caller);
    RETJUMP();
 
    if(oc == KT) {LDEXBL (caller,531);continue;}
    if(oc == DESTROY_OC)  {
       KC(callseg,DESTROY_OC) RCTO(rc);
       return 0;
    }
    KC (ccknode,0) KEYSTO(cck);
    KC (ccknode,1) KEYSTO(object);
    KC (sok,0) KEYSTO(,,,sok) RCTO(rc);
 
    eof=openfile("r");
    if(!eof){termout("INPUT open failed",1);LDEXBL (caller,1);continue;}
 
    base.next=&base;
    base.prev=&base;
    current=&base;
    strcpy(base.data,"TOF:");
 
    readfile();
 
    rc=0;
    for(;;) {
      if(mode == EDIT)    termout("E>",0);
      if(mode == INPUT)   termout("I>",0);
      eof=termin(input,256);
      if(casesw == 'U') upperit(input);
      if(!eof) {freeall();rc=2;break;}
      if(mode == INPUT) {
          if(*input) insert(input);
          else mode=EDIT;
      }
      else if(mode == EDIT) {
/*
   Note here that SHORTCMP stops as soon as a match is found.
   Therefore put the long names first ("top" before "t") if the
   two names are different commands.   PTR is the first character
   AFTER the match or NULL if there is no match
*/
        iptr=input;
        while(*iptr == ' ') iptr++;    /* skip leading blanks */
        if(*iptr) {
          if     (ptr=shortcmp(iptr,"?"))       dohelp();
          else if(ptr=shortcmp(iptr,"q"))       {freeall();break;}
          else if(ptr=shortcmp(iptr,"top"))     dotop();
          else if(ptr=shortcmp(iptr,"t"))       dotype(ptr);
          else if(ptr=shortcmp(iptr,"b"))       dobottom();
          else if(ptr=shortcmp(iptr,"i"))       doinput(ptr);
          else if(ptr=shortcmp(iptr,"n"))       donext(ptr);
          else if(ptr=shortcmp(iptr,"+"))       donext(ptr);
          else if(ptr=shortcmp(iptr,"u"))       doprev(ptr);
          else if(ptr=shortcmp(iptr,"-"))       doprev(ptr);
          else if(ptr=shortcmp(iptr,"del"))     dodelete(ptr);
          else if(ptr=shortcmp(iptr,"dup"))     dodup(ptr);
          else if(ptr=shortcmp(iptr,"case "))   docase(ptr);
          else if(ptr=shortcmp(iptr,"l"))       dolocate(ptr);
          else if(ptr=shortcmp(iptr,"/"))       dolocate(ptr-1);
          else if(ptr=shortcmp(iptr,"file"))    {dofile();break;}
          else if(ptr=shortcmp(iptr,"f "))      dofind(ptr);
          else if(ptr=shortcmp(iptr,"c"))       dochange(ptr);
 
          else {termout("Eh??  ->",0);termout(iptr,0);termout("<-",1);}
        }
      }
    }
    LDEXBL (caller,rc);
  }
}
/*************************************************************
    Command subroutines
*************************************************************/
dohelp()          /* print command summary */
{
    termout("Commands are: ?              - help",1);
    termout("              Quit           - leave, no file",1);
    termout("              top            - go to top of file",1);
    termout("              Bot            - go to end of file",1);
    termout("              Type {n}       - type {n} lines",1);
    termout("              Input {text}   - insert text or mode",1);
    termout("              n{n}           - next",1);
    termout("              +{n}           - next",1);
    termout("              u{n}           - previous",1);
    termout("              -{n}           - previous",1);
    termout("              del {n}        - delete line",1);
    termout("              dup {n}        - duplicate line",1);
    termout("              case U|M       - set case UPPER or MiXeD",1);
    termout("              l /str/        - locate string",1);
    termout("              file           - file changes",1);
    termout("              f str          - find string (in col 1)",1);
    termout("              c /s1/s2/ {n} {n} - change string",1);
 
    return 1;
}
docase(ptr)     /* set case to U or M  */
    char *ptr;
{
    if(mytoupper(*ptr) == 'U') casesw='U';
    if(mytoupper(*ptr) == 'M') casesw='M';
    return 1;
}
dotop()         /* go to top of file */
{
    current=&base;
    termout(current->data,1);
    return 1;
}
dobottom()      /* go to bottom of file */
{
    current=base.prev;
    termout(current->data,1);
    return 1;
}
donext(ptr)     /* go to next N lines */
    char *ptr;
{
    int n;
 
    n=getnumber(ptr);
    while(n) {
      if(current==base.prev) {termout("EOF:",1);return 0;}
      else current=current->next;
      n--;
    }
    termout(current->data,1);
    return 1;
}
doprev(ptr)      /* go to previous N lines */
    char *ptr;
{
    int n;
 
    n=getnumber(ptr);
    while(n) {
      if(current==&base) {termout("TOF:",1);return 0;}
      else current=current->prev;
      n--;
    }
    termout(current->data,1);
    return 1;
}
doinput(ptr)     /* insert a line OR set mode to INPUT */
    char *ptr;
{
    if(*ptr == ' ') insert(ptr+1);
    else mode=INPUT;
    return 1;
}
dodelete(ptr)    /* delete lines */
    char *ptr;
{
    struct line *t;
    int n;
 
    n=getnumber(ptr);
    while(n) {
      if(current != &base) {
         t=current->next;
         deleteline(current);
         if(t == &base) {        /* deleted last line */
            current=base.prev;   /* set to current last line */
            return 1;            /* stop */
         }
         current=t;
      }
      else {
        termout("Cannot delete TOF:",1);
        return 0;
      }
      n--;
    }
    return 1;
}
dotype(ptr)     /* type lines */
    char *ptr;
{
    int n;
 
    n=getnumber(ptr);
    do {
       termout(current->data,1);
       n--;
       if(n) current=current->next;
    } while((current != &base) && n);
    if((current == &base) && n) {
        termout("EOF:",1);
        current=base.prev;
    }
    return 1;
}
dolocate(ptr)         /* locate string anywhere */
    char *ptr;
{
    char target[256],*p;
    struct line *t;
 
    p=target;
    while(*ptr) {if(*ptr  == '/') break;ptr++;}
    if(*ptr != '/')   {termout("No string",1);return 0;}
    ptr++;
    while(*ptr) {
      if(*ptr == '/') break;
      *p=*ptr;
      p++;
      ptr++;
    }
    *p=0;
    if(!*target)      {termout("No string",1);return 0;}
    t=scan(current,target,1);   /* scan in all positions */
    if(!t)            {termout("Not Found",1);return 0;}
    termout(t->data,1);
    current=t;
    return 1;
}
dofind(ptr)           /* locate string in col 1 */
    char *ptr;
{
    struct line *t;
 
    if(!*ptr)         {termout("No string",1);return 0;}
    t=scan(current,ptr,0);     /* scan in col 1 */
    if(!t)            {termout("Not Found",1);return 0;}
    termout(t->data,1);
    current=t;
    return 1;
}
dochange(ptr)         /* change string in current line */
    char *ptr;
{
    char target[256],replace[256],temp[256],*p;
    int len1,len2,n,nlines,occur,found,changed,linechanged;
 
    changed=0;
    p=target;
    while(*ptr) {if(*ptr  == '/') break;ptr++;}
    if(*ptr != '/')   {termout("No string",1);return 0;}
    ptr++;
    while(*ptr) {
      if(*ptr == '/') break;
      *p=*ptr;
      p++;
      ptr++;
    }
    *p=0;
    if(!*ptr)         {termout("No replacement string",1);return 0;}
    p=replace;
    ptr++;
    while(*ptr) {
      if(*ptr == '/') break;
      *p=*ptr;
      p++;
      ptr++;
    }
    *p=0;
    if(*ptr) ptr++;   /* skip the '/'  */
    nlines=getnumber(ptr);   /* get number of lines (first n) */
/* note that *ptr stays zero if there are not enough parameters */
    while(*ptr) {if(*ptr != ' ') break;ptr++;}
    while(*ptr) {if(*ptr == ' ') break;ptr++;}
    while(*ptr) {if(*ptr != ' ') break;ptr++;}
    occur=getnumber(ptr);    /* get number of occurrances each line */
 
    len1=strlen(target);
    len2=strlen(replace);
    while(nlines)  {
       if(current != &base ) {
          p=current->data;
          n=occur;
          linechanged=0;
          while(n) {
             found=0;
             while(*p) {
               if(!strncmp(p,target,len1)) {found=1;break;}
               p++;
             }
             if(found) {
               changed=1;
               linechanged=1;
               if((int)strlen(current->data)-len1+len2 > 255) {
                  termout("Line to long",1);
                  return 0;
               }
               strcpy(p,p+len1);   /* get rid of match */
               strcpy(temp,replace);
               strcat(temp,p);
               strcpy(p,temp);
               p=p+len2;
               n--;
             }
             else break;
          }
          if(linechanged) termout(current->data,1);
       }
       nlines--;
       if(current == base.prev) break;
       if(nlines) current=current->next;
    }
    if(!changed) termout("No lines changed",1);
    return 1;
}
dodup(ptr)          /* duplicate current line */
    char *ptr;
{
    int n;
 
    n=getnumber(ptr);
 
    while(n) {
       insert(current->data);
       n--;
    }
    return 1;
}
dofile()
{
    eof=openfile("w");
    if(!eof) {termout("Output open failed",1);return 0;}
    writefile();
    freeall();
    closefile();
    return 1;
}
/***********************************************************
     Subroutines
***********************************************************/
char *shortcmp(ptr,str) /* returns 0 or point to first char after */
    char *ptr,*str;     /* the located string, case insensitive */
{
    while(*str) {
       if(mytoupper(*str) != mytoupper(*ptr)) return NULL;
       ptr++;
       str++;
    }
    return ptr;
}
struct line *scan(start,str,all)
    struct line *start;
    char *str;
    int all;            /* check all positions in line */
{
    int len;
    char *p;
 
    len=strlen(str);
    do {
       p=start->data;
       while(*p) {
          if(!strncmp(p,str,len)) return start;
          if(all) p++;
          else break;
       }
       start=start->next;
    }  while (start != &base);
    return NULL;
}
readfile()              /* read entire file */
{
   struct line *t;
 
   char buf[256];
   for(;;) {
      if(!readline(buf,256)) break;
      insert(buf);
   }
   termout("EOF REACHED",1);
   t=base.prev;
   termout(t->data,1);
   closefile();
   return 1;
}
writefile()            /* write entire file */
{
   struct line *t;
 
   t=base.next;
   while(t != &base) {
      writeline(t->data);
      t=t->next;
   }
   return 1;
}
insert(ptr)         /* add a line of text */
   char *ptr;
{
   struct line *new;
 
   new=(struct line *)calloc(1,sizeof(struct line));
   strcpy(new->data,ptr);
   insertline(new,current);
   current=new;
   return 1;
}
insertline(l1,cur)           /* insert line into chains */
   struct line *l1,*cur;
{
   struct line *t;
 
   l1->next=cur->next;
   cur->next=l1;
   t=l1->next;
   l1->prev=t->prev;
   t->prev=l1;
   return 1;
}
deleteline(l1)              /* remove line from chains */
   struct line *l1;         /* and free the storage */
{
   struct line *t1,*t2;
 
   t2=l1->next;
   t1=l1->prev;
   t1->next=t2;
   t2->prev=t1;
   free(l1);
   return 1;
}
freeall()                  /* free all storage */
{
   struct line *t;
 
   while(base.next != &base) {
     deleteline(base.next);
   }
   return 1;
}
getnumber(ptr)     /* scan for a number or return 1 */
   char *ptr;
{
   while(*ptr) {
     if(*ptr == '*') return 99999;
     if(isdigit(*ptr)) {
        return atoi(ptr);
     }
     ptr++;
   }
   return 1;
}
upperit(ptr)      /* translate string to UPPER case */
   char *ptr;
{
    while (*ptr) {*ptr=mytoupper(*ptr);ptr++;}
    return 1;
}
mytoupper(c)
   char c;
{
   if(islower(c)) return toupper(c);
   else return c;
}
/*******************************************************
    system dependent stuff
*******************************************************/
openfile(mode)      /* open file (read or write) */
   char *mode;                /* "r" or "w"  */
{
    KC(object,KT) RCTO(rc);
    if(rc == RC_ESAKT)  {
       objtype=RCOL;
       memcpy(rcolbuf,"\004\000\000\000\000",5);
       if(*mode == 'w') KC(object,1) RCTO(rc);  /* empty RC */
    }
    else {
      objtype=SEG;
      if(rc == FS_AKT) {  /* possibly a Unix segment */
         KC (object,FS_GetMetaData) STRUCTTO(um) RCTO(rc);
         if(!rc && (um.length != 0)) {  /* a Unix file */
             objtype=UNIXSEG;
             segaddr=(char *)0;
             KC(callseg,Callseg_ReplaceSegmentKey) KEYSFROM(object) RCTO(rc);
             if(*mode == 'w') um.length = 0;
             return 1;
         }
      }
      KC(callseg,Callseg_ReplaceSegmentKey) KEYSFROM(object) RCTO(rc);
      segaddr=(char *)4;
      if(*mode == 'r') {
        KC (callseg,Callseg_ReadSegmentData)
           CHARFROM("\000\000\000\000\000\000\000\004",8)
           CHARTO(&nrecords,4) RCTO(rc);
        if(rc) {
          nrecords=0;
          termout("Object is not ESRC or Segment",1);
        }
      }
      else nrecords=0;
    }
    return 1;
}
closefile()                  /* close file */
{
   return 1;
}
readline(buf,len)            /* read line from file */
   char *buf;                /* returns 0 on EOF */
   int  len;
{
   char  callparms[9];
   short int  trlen,rlen;
   char *ptr;
 
   if(objtype==RCOL)  {
      KC(object,13) CHARFROM(rcolbuf,5) CHARTO(rcolbuf,260) RCTO(rc);
      if(rc>1) return 0;
      else strcpy(buf,rcolbuf+5);
   }
   else if(objtype == UNIXSEG) {
      if((int)segaddr >= um.length) return 0;  /* end */
      rlen=len;
      memset(callparms,0,8);
      memcpy(callparms+2,&segaddr,4);
      memcpy(callparms+6,&rlen,2);
      KC (callseg,Callseg_ReadSegmentData) CHARFROM(callparms,8) CHARTO(buf,rlen) RCTO(rc);
      if(rc) return 0;
      /* now correct length... scan for '\n' */
      ptr=buf;
      trlen=0;
      while(trlen < rlen) {
         if( ((int)segaddr + trlen) > um.length) break;  /* in case no trailing NL */
         if(*ptr == '\n') break;  /* trlen has the length */
         ptr++;
         trlen++;
      } 
      segaddr += trlen;
      segaddr++;  /* skip '\n' */
      memset(ptr,0,(len-trlen)); 
   }
   else {
      if(nrecords) {
        segaddr=segaddr+2;
        memcpy(callparms,"\000\000",2);
        memcpy(callparms+2,&segaddr,4);
        memcpy(callparms+6,"\000\002",2);
        KC (callseg,Callseg_ReadSegmentData) CHARFROM(callparms,8) CHARTO(&trlen,2) RCTO(rc);
        if(rc) return 0;
        segaddr=segaddr+2;
        rlen=trlen;
        if(rlen>len) rlen=len;
        memcpy(callparms,"\000\000",2);
        memcpy(callparms+2,&segaddr,4);
        memcpy(callparms+6,&rlen,2);
        memset(buf,0,len);
        KC (callseg,Callseg_ReadSegmentData) CHARFROM(callparms,8) CHARTO(buf,rlen) RCTO(rc);
        if(rc) return 0;
        segaddr=segaddr+trlen;
        nrecords--;
      }
      else return 0;
   }
 
   return 1;
}
writeline(buf)
   char *buf;
{
   char callparms[280];
   short int len;
 
   if(objtype == RCOL) {
      strcpy(rcolbuf+5,buf);
      KC (object,2) CHARFROM(rcolbuf,strlen(buf)+5) RCTO(rc);
      if(rc) return 0;
   }
   else if(objtype == UNIXSEG) {
      len=strlen(buf);
      memset(callparms,0,280);
      memcpy(callparms+2,&segaddr,4);
      strcpy(callparms+6,buf);
      strcat(callparms+6,"\n");
      len++;
      KC (callseg,Callseg_WriteSegmentData) CHARFROM(callparms,len+6) RCTO(rc);
      if(rc) return 0;
      segaddr += len;
      um.length += len;
      KC (object,FS_SetMetaData) STRUCTFROM(um) RCTO(rc);
      if(rc) return 0;
   }
   else {
      len=strlen(buf);
      memcpy(callparms,"\000\000",2);
      memcpy(callparms+2,&segaddr,4);
      memcpy(callparms+6,"\000\000",2);
      memcpy(callparms+8,&len,2);
      KC (callseg,Callseg_WriteSegmentData) CHARFROM(callparms,10) RCTO(rc);
      if(rc) return 0;
      segaddr=segaddr+4;
      memcpy(callparms,"\000\000",2);
      memcpy(callparms+2,&segaddr,4);
      strcpy(callparms+6,buf);
      KC (callseg,Callseg_WriteSegmentData) CHARFROM(callparms,len+6) RCTO(rc);
      if(rc) return 0;
      segaddr=segaddr+len;
      nrecords++;
      memcpy(callparms,"\000\000\000\000\000\000",6);
      memcpy(callparms+6,&nrecords,4);
      KC (callseg,Callseg_WriteSegmentData) CHARFROM(callparms,10) RCTO(rc);
      if(rc) return 0;
   }
   return 1;
}
termin(ptr,len)      /* read from "terminal" */
   char *ptr;        /* returns 0 on EOF */
   int len;
{
   KC(sik,8192+len) CHARTO(ptr,len) KEYSTO(,,,sik) RCTO(rc);
   ptr[strlen(ptr)-1]=0;  /* get rid of activation character */
   if(rc == KT+1) return 0;
   else return 1;
}
termout(ptr,cr)         /*  write on terminal  */
   char *ptr;
   int   cr;            /* cr=1 means put out a CR */
{
   KC(sok,0) CHARFROM(ptr,strlen(ptr)) KEYSTO(,,,sok) RCTO(rc);
   if(rc == KT+1) return 0;
   if(cr) KC(sok,0) CHARFROM("\n",1) KEYSTO(,,,sok) RCTO(rc);
   return 1;
}

char toupper (char a)
{  
   if(a >= 'a' && a <= 'z') a = a & ~0x20;
   return a;
}

islower (a)
   char a;
{
   if(a >= 'a' && a <= 'z') return 1;
   return 0;
}
isdigit (a)
   char a;
{
   if(a >= '0' && a <= '9') return 1;
   return 0;
}
