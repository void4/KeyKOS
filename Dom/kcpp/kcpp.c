/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#ifdef IBM
#include <stat.h>
#endif
#include "defs.h"
#include "eval.h"
#include "codegen.h"
 
#define BUFSIZE 512   /* max record size */
 
jmp_buf err_jmp;             /* for error reset */
int noecho;
 
int kjump;     /* key cache or not            */
int assembler; /* really assembler bound */
char *xbtext, *nbtext, *krtext;
 
int cx;                      /* fileid for output */
int nopen;     /* number of include files open */
int stripseq;  /* whether to strip sequence numbers from
                  top level file */
static int istack[16], *idp=istack;  /* input fileid stack */
int errstack[16], *errline = errstack; /* line # stack */
char recbuf[BUFSIZE], *rbp;    /* Buffer & pointer for records */
extern char tokbuf[], *tbp;
int outline; /* line number in output file */

   char prefix[512];
   char prefixes[16][512];
   char ilib[32][256];
   int  nilib = 0;
 
/* newrec() reads a new record from the input file into a
 *     buffer and synchronizes the current line # for error
 *     and in the output file
 * If top level file and stripseq, strips columns 73 - 80
 */
void newrec()
{
   int len;

   if ( (len = myread(*idp, recbuf, sizeof(recbuf))) == 0 ) {
      /* end of file */
      recbuf[0] = TEOF;
   }
   else {
      if (len < 0) fatal("I/O error reading some file");
 
      if (nopen == 0 && stripseq == TRUE)
         len = 72; /* strip final '\n' and sequence number field */
      else len--;  /* strip final '\n' */
      /* flush trailing blanks */
      while (--len>=0 && recbuf[len] == ' ') ;
      recbuf[++len] = '\n'; /* restore newline */
   }
   recbuf[++len] = '\0';
   rbp = recbuf;
   (*errline)++;
   return;
}
 
/* ipush() opens a file and pushes it on the input stack */
#ifdef IBM
ipush(fn,ft,fm)
char *fn, *ft, *fm;
{
   char name[23];
   int len, l;
   char message[256];
 
   nopen++;
   name[0] = '\0';
   strcat(name,fn);
   strcat(name," ");
   strcat(name,ft);
   strcat(name," ");
   strcat(name,fm);
   strcat(name," (raw");
   if((*++idp = open(name,O_RDONLY)) == -1) {
      message[0] = '\0';
      strcat(message,"Could not open file for reading - ");
      fatal(strcat(message,name));
   }
   if (nopen == 0) { /* top level file */
      if ((len = fstat(*idp, &stbuf)) == -1) {
         message[0] = '\0';
         strcat(message,"Could not get stat info for file to read - ");
         fatal(strcat(message,name));
      }
      if ((stbuf.st_recfm == 'F') && (stbuf.st_lrecl == 80))
         stripseq = TRUE;
      else stripseq = FALSE;
   }
   *++errline = 0;
   newrec();
}
#else IBM
ipush(fn)
   char *fn;
{
   int len, l;
   int fd;
   char message[512];
  
   if((fd=open(fn,O_RDONLY)) < 0) {
    /*  strcpy(message,"Could not open file for reading - ");
      strcat(message,fn);
      fatal(message);  */
    return 0;  /* did not work */
   }
   *++idp=fd;
   getpre(fn);
   nopen++;
   strcpy(prefixes[nopen],prefix);
   stripseq=FALSE;
   *++errline = 0;
   newrec(0);
   return 1;  /* worked */
}
#endif IBM
getpre(fn)
   char *fn;
{
   int i;

   strcpy(prefix,fn);
   i=strlen(prefix);
   while(i) {
     if(prefix[i] == '/') {
        prefix[i+1]='\0';
        return 0;
     }
     i--;
   }
   prefix[0]=0;
   return 0;
}
 
/* ipop() pops a file from the input stack, returning 1
 *     normally, but 0 if there are no more files stacked
 */
ipop()
{
    close(*idp--);
 if (nopen == 0) debug("done: %d lines\n",*errline);
    errline--;                           /* pop line # stack */
    noecho--;                 /* possibly restore echoing */
    if (nopen > 0) nopen--;
#ifndef IBM
    strcpy(prefix,prefixes[nopen]);
#endif
    if(idp == istack) return(0);
    tbp = tokbuf; *tbp = '\0';           /* clear the token buffer */
    newrec();
    return(1);
}
#ifdef IBM 
#define NLEN 23
#else
#define NLEN 512
#endif
/* includefile() handles checking local (#include "foo") include *
 *    files in the hopes of finding constant key declarations    */
void includefile()
{
    char name[NLEN];
    char fn[NLEN];
    char ft[NLEN];
    int  fnlen, ftlen;
    int  l, r, len, i, local;
    char message[256];
 
    memset(name, '\0', sizeof(name));
    nexttok(NOSPACE);                /* system or local include file */
    if (tokbuf[0] == '<')   {      /* "system" include file */
       local=0;
       nexttok(ALL);
       while (tokbuf[0] != '>') { /* accumulate name */
          if (tokbuf[0] == '\n')
             error("include file name not complete on one line");
          if (strlen(name)+strlen(toktext()) > (size_t)NLEN-1)
             error("include file name too long");
          strcat(name, toktext());
          nexttok(ALL);
       }
    }
    else {                           /* "local" include file */
       local=1;
       if ((len = strlen(toktext()+1)) > NLEN-1)
          error("include file name too long");
       strcpy(name, toktext()+1);
       name[len-1] = '\0'; /* remove the final double quote */
    }
    do {          /* skip over space till end of line */
       if (nexttok(ALL) != SPACE)
          error("Extra text in include directive");
    } while (tokbuf[0] != '\n');
    if ((memcmp(name, "keykos.h", 8) == 0) ||
        (memcmp(name, "keykos", 6)   == 0)) {
       ; /* don't include this file */
    }
    else {
       if (!noecho && local) print1nl("\n");  /* echo this line now */
       noecho++;            /* this noecho is canceled in ipop() */
#ifdef IBM
       fnlen = strcspn(name, ".");
       if (strlen(name) == fnlen)
          ipush(name, "h", "*"); /* start reading file */
       else {
          memcpy(fn, name, fnlen);
          fn[fnlen] = '\0';
          if (name[fnlen] == '.')  {
             ftlen = strlen(name+fnlen+1);
             memcpy(ft, name+fnlen+1, ftlen);
             ft[ftlen] = '\0';
             ipush(fn, ft, "*"); /* start reading file */
          }
          else  error("Wrong syntax in an include file declaration");
       }
#else
       if(!local) {   /* system file name */
#ifdef XX
         if(tokbuf[0] == '/') strcpy(fn,"");
         else strcpy(fn,"/usr/include/");  /* system prefix */
         strcat(fn,name);
         if(!ipush(fn)) noecho--;
#else
         noecho--;
#endif  
       }
       else {   /* local name "" */
         if(tokbuf[0] == '/') strcpy(fn,"");
         else  strcpy(fn,prefix);  /* current prefix */
         strcat(fn,name);
         if(!ipush(fn)) {  /* lets look through -I directories */

             for(i=0;i<nilib;i++) {
               strcpy(fn,ilib[i]);
               if(fn[strlen(fn)-1] != '/') strcat(fn,"/");
               strcat(fn,name);
               if(ipush(fn)) break;  /* worked */
	     }
	     if(i==nilib) {
               strcpy(message,"Could not open file for reading - ");
               strcat(message,name);
               fatal(message);
	     }
         }
       }
#endif
    }
}
 
/* debug () prints a debug message */
debug(frm,arg)
char *frm, *arg;
{ 
#if defined(DEBUG)
	fprintf(stderr,frm,arg); fflush(stderr);
#endif
}
 
/* error() prints a string, line number, and limited context; then
 *   tries to continue processing after the next statement
 */
error(str)
char *str;
{
 
    tokbuf[80] = '\0';    /* display first 80 bytes of info */
    fprintf(stderr,"error: line %d: %s near '%s'\n",
                    *errline,str,toktext());
    if(toktype() != SC) nexttok(RESET);
    longjmp(err_jmp,1);      /* jump to main(), inhibit compilation */
}
 
/* fatal() is similar to error(), except it does not reset */
fatal(str)
char *str;
{
    fprintf(stderr,"Fatal error! line %d: %s\n",*errline,str);
    _exit(04);
}
 
/* warn() merely prints out an error message */
warn(str)
char *str;
{
    fprintf(stderr,"warning: line %d: %s near '%s'\n",
                    *errline,str,toktext());
}
 
main(argc,argv)
int argc;
char **argv;
{
    char errflag;
 
debug("KC/PP rev 09\n");
    kjump = 0;                 /* default option - no kjump */
    assembler=0;               /* default option - not assembler bound */
    noecho = 0;
    parseargs(argc,argv);
    initsymtab();
    errflag = setjmp(err_jmp);  /* 0 once, 1 after any errors */
    do {
        while(nexttok(KEYWORD) != TEOF) {
            switch(toktype() & CLASS) {
            case CALL:
                docall(toktype() & VAL);
                break;
            case DECL:
                switch(toktype() & VAL) {
                   case DKEY: installkey();  break;
                   case _FILE: includefile(); break;
                   case PLISTR: dclstring(); break;
                }
                break;
            case PARM: error("Parameter keyword outside of key call");
            case SPARM: error("Parameter keyword outside of key call");
            case TKEY:
               /* warn("Key used outside of key call"); */
               break;
            }
            /* Correct line numbers if necessary */
            while (outline < errstack[1]) print1nl("\n");
        }
    } while(ipop());
debug("KC/PP Done\n");
    return(errflag);
}
 
/* parseargs() opens the input and output files and deals with kjump */
parseargs(argc,argv)
int argc;
char **argv;
{
    char fn[NLEN];
    char *ptr,*sname,message[256];
    int ipushrc=0;
 
    nopen = -1;
    kjump=0;
#ifdef IBM
    switch(argc) {
    case 6:
        /* fn ft fm ( KJUMPA */
        settext("_kp","KRK");
        kjump = 1;
        ipushrc=ipush(argv[1],argv[2],argv[3]);
        break;
    case 5:
        /* fn ft ( KJUMPA */
        settext("_kp","KRK");
        kjump = 1;
        ipushrc=ipush(argv[1],argv[2],"*");
        break;
    case 4:
        /* fn ( KJUMPA  || fn ft fm */
        if(*argv[2] == '(') {
            settext("_kp","KRK");
            kjump = 1;
            ipushrc=ipush(argv[1],"C","*");
        } else {
            settext("_jumpbuf","KRN");
            ipushrc=ipush(argv[1],argv[2],argv[3]);
        }
        break;
    case 3:
        /* fn ft */
        settext("_jumpbuf","KRN");
        ipushrc=ipush(argv[1],argv[2],"*");
        break;
    case 2:
        /* fn */
        settext("_jumpbuf","KRN");
        ipushrc=ipush(argv[1],"C","*");
        break;
    default: fatal("Usage: kcpp fn {ft} {fm} {( kjumpa}");
    }
    if(!ipushrc) {
       fatal("Could not open input file\n");
    }
    /* open fn cx for output */
    fn[0] = '\0';
    strcat(fn, argv[1]);
    strcat(fn," cx (lrecl 512");
    if((cx = open(fn,O_WRONLY|O_CREAT|O_TRUNC))== -1)
        fatal("Could not open <filename cx> for writing");
#else !IBM
    settext("_jumpbuf","KRN");
    if (argc < 2) {
       fatal("Usage kcpp fn -KJUMPA");
       return;
    }
    argc--;
    while(argc) {
       if(*argv[argc] == '-') {
         if(!strcmp(argv[argc],"-KJUMPA")) {
            kjump=1;
            settext("KPBL","KRK");
         }
         else if(argv[argc][1] == 'I') {
            strcpy(ilib[nilib],&argv[argc][2]);
            nilib++;
         }
         else if(!strcmp(argv[argc],"-ASM")) assembler=1;
       }
       else {
         if(!ipush(argv[argc])) {
            strcpy(message,"Could not open file for reading - ");
            strcat(message,argv[argc]);
            fatal(message);
	 }
         strcpy(fn,argv[argc]);
         sname=argv[argc];
         ptr=sname;
         while(*ptr) {
           if(*ptr=='/') sname=ptr+1;
           ptr++;
         }
         fn[sname-argv[argc]]=0;
         strcat(fn,"kcpp.");
         strcat(fn,sname);
       }
       argc--;
    }
    strcpy(ilib[nilib],"/usr/include");
    nilib++;
    if((cx = open(fn,O_WRONLY|O_CREAT|O_TRUNC,-1)) == -1) {
      strcpy(message,"Could not open file for writing - ");
      strcat(message,fn);
      fatal(message);
    }
#endif IBM
    outline = 1;
}
 
settext(p,r)
char *p, *r;
{
    xbtext = p; krtext = r;
}
 
/* prcomment() prints the recbuf as an assembler comment */
prcomment()
{

  char buf[BUFSIZ+10];
 
   strcpy(buf,"!\"");
   strcat(buf,recbuf);
   buf[strlen(buf)-1]=0;
   strcat(buf,"\"");	
   print1(buf);
}
/* prints string onto the output */
/* The string should not have any newline characters
   (or they should have already been counted) */
print1(s)
char *s;
{
   int len,lf;
   
   len=strlen(s);
   lf=0;

     if(assembler) {
        if(s[len-1] == '}') return 0;
        if(s[len-1] == '{')  {len=0;lf=1;}
        if(s[len-1] == ';' && len!=1) {len--;lf=1;}
     }
     write(cx, s, len);
     if(lf) write(cx,"\n",1);
}
 
/* prints strings onto the output,2 arguments for convienence only */
print(s,t)
char *s, *t;
{
     print1(s);
     print1(t);
}
 
/* Print a string onto the output.
   String may have newline characters.*/
print1nl(s)
char *s;
{
   char *p = s;
   while ((p = strchr(p,'\n')) != NULL) {
      outline++;
      p++; /* skip over the newline */
   }
   print1(s);
}
myread(fd,buf,len)
  int fd,len;
  char *buf;
{
 static  char mb[20][256];
 static  int left[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 static  char *mbp[20]={mb[0],mb[1],mb[2],mb[3],mb[4],mb[5],
          mb[6],mb[7],mb[8],mb[9],mb[10],mb[11],mb[12],mb[13],mb[14],
          mb[15],mb[16],mb[17],mb[18],mb[19]};
   int cnt;	
#ifdef IBM
  return read(fd,buf,len);
#else IBM
  cnt=0;
readit:
  while(left[fd] && len) {

    *buf = *mbp[fd];
    cnt++;
    mbp[fd]++;
    left[fd]--;
    len--;
    if(*buf == '\n') return cnt;
    buf++;
  }
  if(!left[fd]) {
    left[fd]=read(fd,mb[fd],256);
    if(!left[fd]) return cnt;  /* eof, if cnt != 0 return what have  */
    mbp[fd]=mb[fd];
    goto readit;
  }	
  else return cnt;
#endif
}
