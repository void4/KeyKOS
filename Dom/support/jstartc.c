/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include "kktypes.h"
#include "domain.h"
#include "vcs.h"
#include "fs.h"
#include "dc.h"
#include "node.h"
#include "tdo.h"
#include "sb.h"
#include "kvm.h"
#include "factory.h"
#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "setjmp.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <strings.h>

//#define MYDEBUG 1

  KEY COMP           = 0;
  KEY SB             = 1;
  KEY KEYSNODE       = 1;
  KEY CALLER         = 2;
  KEY DOMKEY         = 3;
  KEY PSB            = 4;
  KEY METER          = 5;
  KEY DOMCRE         = 6;

  KEY CONSOLE        = 12;
  KEY K2             = 13;
  KEY K1             = 14;
  KEY K0             = 15;

#define COMPCOPY     3
#define COMPCONSOLE  5

#define KEYSNODECCK     7
#define KEYSNODECIRCUIT 7
#define KEYSNODESIK     8
#define KEYSNODESOK     9
#define KEYSNODEDIR    10

#define MAXFILES     16

   char title[] = "KVM     ";
   char **environ = 0;
   char mainname[256];
   int argc = 4;
   char *argv[] = {"kvm", "-classpath", ".", mainname, 0};

   int _lib_version = 1;

   JUMPBUF;
   jmp_buf restartbuf;

   int debugging = 0;

   extern unsigned long cursbrk;

   struct filete {
      char name[4096];
      int type;
#define FILETYPELIBRARY 1
#define FILETYPERUNTIME 2
      int compindex; /* the directory key or the segmentlookupkey */
      int slot;      /* contains the looked up segment or the same segmentlookupkey */
/* slot = -1 if the file is not currently mapped (as in after restart) */
      int begin;     /* always zero for directory types, segment offset else */
      int end;       /* segment offset of last byte (zero for directory types) */
      int position;  /* current position */
   };

   struct filehe {
      int filetindex;  /* only from CLASS and FILE */
      int mode;
      int type;

#define HANDLETYPECLASS 1
#define HANDLETYPEFILE 2

/* USE KEYSNODE for the following types */

#define HANDLETYPESOCKET 3
#define HANDLETYPECONSOLE 4
#define HANDLETYPESIK 5
#define HANDLETYPESOK 6

   };

   int haveterminal = 0;  /* must have set some type of terminal */
   int havedirectory = 0; /* set when a directory for non-class files installed */

   struct filete filet[MAXFILES];
   struct filehe fileh[MAXFILES];

   int oktofreeze = 0;
   int frozen = 0;   /* initial state is not frozen */

/*
   If this is a freezedry product we are running on an alternate 1 page stack
   which will change with the longjmp in the StartApplication method (when frozen)
*/

   char tracebuf[512];

jstart(foc,ord)
       UINT32 foc,ord;

{
       UINT32 oc,rc;
       int actlen;
       char name[256];
       int i;

       domem();
       if(!frozen) inittab();

       {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }

       {OC(0);XB(0x8020F000); }
       for (;;) {
           {RC(oc);RS3(name,255,actlen);NB(0x0BF0FED2); }
           {rj(0x00100000,&_jumpbuf); }

           if(oc == KT) {
               {OC(KVM_AKT);XB(0x00200000); }
               continue;
           }
           if(oc == KT+4) {
               undomem();

               {OC(Domain_GetKey+COMP);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
               {OC(SB_DestroyNode);XB(0x8040D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
               {OC(Domain_GetKeeper);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
               {OC(4);XB(0x00D00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }

               undokeys();
               return;
           }
           switch(oc) {
           case KVM_InstallClassLibrary:

                for(i=6;i<16;i++) {
                    {OC(Node_Fetch+i);XB(0x00000000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
                    {OC(KT);XB(0x00D00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
                    if(rc == KT+1)  {   /* put it here */
                        {OC(Node_Swap+i);XB(0x8000F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                        break;
                    }
                }
                if(i == 16) {
                    {OC(3);XB(0x00200000); }
                }
                else {
                    {OC(0);XB(0x00200000); }
                }
                continue;

           case KVM_InstallFileDirectory:

                {OC(Node_Swap+KEYSNODEDIR);XB(0x8010F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                havedirectory = 0;
                {OC(0);XB(0x00200000); }
                continue;

           case KVM_InstallCircuitKey:

                {OC(KT+2);XB(0x00200000); }
                continue;

           case KVM_InstallTerminalKeys:

                {OC(KT+2);XB(0x00200000); }
                continue;

           case KVM_InstallConsole:

                {OC(Node_Swap+KEYSNODECCK);XB(0x8010F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                {OC(0);XB(0x00F00000);RC(rc);NB(0x08C0ED00);cjcc(0x00000000,&_jumpbuf); }
                {OC(Node_Swap+KEYSNODESIK);XB(0x8010E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                {OC(Node_Swap+KEYSNODESOK);XB(0x8010D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

                fileh[0].filetindex=0;
                fileh[0].mode=0;
                fileh[0].type=HANDLETYPESIK;

                fileh[1].filetindex=0;
                fileh[1].mode=0;
                fileh[1].type=HANDLETYPESOK;

                fileh[2].filetindex=0;
                fileh[2].mode=0;
                fileh[2].type=HANDLETYPESOK;

                haveterminal = 1;

                {OC(0);XB(0x00200000); }
                continue;

           case KVM_SetNameOfMain:

                name[255]=0;
                strcpy(mainname,name);
                {OC(0);XB(0x00200000); }
                continue;

           case KVM_StartApplicationFreeze:
                oktofreeze=1;
           case KVM_StartApplication:
                if(!checkkeys()) {        /* must have terminal and 1 class file */
                    {OC(KT+2);XB(0x00200000); }
                    continue;
                }
                if(frozen) {
                    oktofreeze=0;
                    frozen=0; /* put back to normal for next invocation */
                    longjmp(restartbuf,1);  /* back on "main" stack */
                }
                else {
                    rc=main(argc,argv);
                }
                {OC(rc);XB(0xE020FED0); }
                continue;

           case KVM_EnableDebug:
                debugging=1;
                {OC(0);XB(0x00200000); }
                continue;

           default:
                {OC(KT+2);XB(0x00200000); }
                continue;
           }

       }
}
inittab()
{
       int i;

       for(i=0;i<MAXFILES;i++) {
           fileh[0].type=0;
           fileh[0].filetindex=-1;
           filet[0].type=0;
       }
}
/*
   This routine builds a new factory (using the copy key) and installs
   the current pseudo components as components in the new factory.

   the __freezedry assembly routine is used to freeze the memory
   install the memory and seal the factory (returning to the CALLER).

   When the new application is invoked with the StartApplication ordercode
   _restart effect the restart which will use longjump to restore the
   stack and returns to the application after the _freezedry call.
*/

_gettimeofday(tp,v)
     struct timeval *tp;
     void *v;
{
     if(tp == 0) return 0;

     tp->tv_sec=1;
     tp->tv_usec=0;
     return 0;
}

gettimeofday(tp,v)
     struct timeval *tp;
     void *v;
{
     return _gettimeofday(tp,v);
}

_freezedry()
{
    int i;
    UINT32 rc;

    if(!oktofreeze) return 0;

    frozen = 1;     /* mark as frozen */

    undomem();      /* puts memory back */
    {OC(Domain_GetKey+COMP);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }  /* pseudo component node */
    undokeys();     /* puts the keys back (including COMP) */
    clearkeys();    /* note - no circuit, no file directory */
/*
    StartApplication on a freezedry product will go to _restart which does
    a longjump to restore the world to here  but with memory fully set up

    No files will be open (Mapped) but the file table will be filled in
*/
    if(setjmp(restartbuf)) return;  /* if restarting, simply return */

/* make new factory - install new components */

    {OC(Node_Fetch+COMPCOPY);XB(0x00000000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }   /* K2 has pseudo component node */
    {OC(FactoryC_Copy);XB(0xA0E04010);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }

    for (i=6;i<16;i++) {
        {OC(Node_Fetch+i);XB(0x00D00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(FactoryB_InstallSensory+i);XB(0x80E0F000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        if(rc) {
            {OC(FactoryB_InstallHole+i);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        }
    }
    {OC(SB_DestroyNode);XB(0x8040D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(Domain_GetKeeper);XB(0x00300000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
    {OC(4);XB(0x00D00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  // destroy keeper
/*
   __freezedry must be in assembler because it makes memory readonly and
   must complete the operation and return to the user without using a
   stack
*/
   /* K1 (14) has factory builder key */
    __freezedry();    /* This freezes memory and installs new VCSF as .program */
}

domem()
{
    UINT32 oc,rc;
    int i;
    struct Node_DataByteValue ndb = {7};

    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_CreateNode);XB(0x00400000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+0);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_MakeNodeKey);PS2(&(ndb),sizeof(ndb));XB(0x04E00000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Domain_SwapMemory);XB(0x8030E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(SB_CreateNode);XB(0x00400000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    for(i=0;i<7;i++) {
       {OC(Domain_GetKey+i);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Swap+i);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    }
    {OC(Domain_SwapKey+KEYSNODE);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(SB_CreateNode);XB(0x00400000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    for(i=0;i<16;i++) {
       {OC(Node_Fetch+i);XB(0x00000000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Swap+i);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    }
    {OC(Domain_SwapKey+COMP);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
}

undomem()
{
       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+0);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Domain_SwapMemory);XB(0x8030E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
       {OC(SB_DestroyNode);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
}
undokeys()
{
       int i;

       {OC(Domain_GetKey+KEYSNODE);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       for(i=0;i<7;i++) {
           if(i != 2) {   /* real CALLER is in the real slot */
              {OC(Node_Fetch+i);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Domain_SwapKey+i);XB(0x8030E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
           }
       }
       {OC(SB_DestroyNode);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
}

clearkeys()
{
      int i;

      for(i=0;i<MAXFILES;i++) {
          filet[i].compindex=-1;   /* indicate not mapped */
      }
      haveterminal=0;
      havedirectory = 0;
}
checkkeys()
{
      UINT32 rc;
      int i;

      if(!haveterminal) return 0;
      for(i=6;i<16;i++) {
         {OC(Node_Fetch+i);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(KT);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
         if(rc != KT+1) return 1;
      }
      return 0;
}

__sigaction(sig, ptr1, ptr2)
     int sig;
     void *ptr1,*ptr2;
{
    if(debugging)  trace ("SIGACTION\n");
}

mylookup(name,nomap)
    char *name;
    int nomap;
{
    char lname[4096];
    char dname[4096];
    char *ptr;
    int i,j,l;
    UINT32 rc;
    long long seglen;

    if(strncmp(name,".",1)) { /* a non-class file */
        return -1;             /* not done yet */
    }
    if(!strcmp(name,".")) return 0;  // special lookup of . Directory

    strcpy(lname,name+2);    /* skip the directory part */

    errno=ENOENT;

    for(i=6;i<16;i++) {
        {OC(Node_Fetch+i);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }   /* get the key */
        {OC(KT);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        if(debugging) {
//          sprintf(tracebuf,"i=%d, KT = %X\n",i,rc);
//          trace(tracebuf);
        }
        if(rc == TDO_NSAKT) {  /* a directory */

/* need to walk down any directory path till no more '/' characters */
             while(ptr=strchr(lname,'/')) {
                 l=(int)(ptr-lname) + 1;
                 strncpy(dname+1,lname,l);
                 dname[l+1]=0;
                 *dname=l;
                 {OC(TDO_GetEqual);PS2(dname,(*dname)+1);XB(0x04F00000);RC(rc);NB(0x0880F000);cjcc(0x08000000,&_jumpbuf); }
                 if(rc != 1) goto nexti;
                 strcpy(lname,(ptr+1));
             }
             strcpy(dname+1,lname);
             *dname=strlen(lname);
             {OC(TDO_GetEqual);PS2(dname,(*dname)+1);XB(0x04F00000);RC(rc);NB(0x0880E000);cjcc(0x08000000,&_jumpbuf); }
             if(debugging) {
sprintf(tracebuf,"OC=%d,rc=%X,name=(%d,%s)\n",TDO_GetEqual,rc,*dname,(dname+1));
trace(tracebuf);
             }
           if(rc == 1) {

             {OC(KT);XB(0x00E00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
             if(debugging) {
//sprintf(tracebuf,"FileKT=%X\n",rc);
//trace(tracebuf);
             }
             if( (rc != FS_AKT) && (rc != Node_NODEAKT) &&
                 ((rc & Node_SEGMENTMASK) == Node_SEGMENTAKT) ) return -1;

             if(nomap) return i;  /* return the compindex, Key in K1 */

             for(j=3;j<MAXFILES;j++) {  /* find a spot for this */
                 if(filet[j].type == 0) {  /* found slot */
                     strcpy(filet[j].name,lname+1);
                     filet[j].type = FILETYPELIBRARY;
                     filet[j].compindex=i;
                     filet[j].slot=j-1;      /* 1-1 table and slots */
                     {OC(Node_Fetch+DOMKEY);XB(0x00100000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
                     {OC(Domain_GetMemory);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
                     {OC(Node_Swap+j-1);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
                     seglen=0;
                     {OC(FS_GetLimit);XB(0x00E00000);RC(rc);RS2(&(seglen),sizeof(seglen));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
                     filet[j].begin=0;
                     filet[j].end=seglen;
                     filet[j].position=0;

                     return j;
                 }
             }
           }
        }
        /* other types go here (segments) */
nexti:  ;
    }
    return -1;
}
void *_sbrk();

void *sbrk(incr)
     long incr;
{
     return _sbrk(incr);
}
void *_sbrk(incr)
     long incr;
{
     long cur,new,current;

     if(debugging) {
       sprintf(tracebuf,"SBRK %X ->",incr);
       trace(tracebuf);
     }

     current=cursbrk;

     cur=(cursbrk+4095) & 0xFFFFF000;
     new=((cursbrk+incr)+4095) & 0xFFFFF000;

     if(new > cur) {
          memset((char *)cur,0,(new-cur));
     }

     cursbrk = cursbrk+incr;

     if(debugging) {
       sprintf(tracebuf," %X\n",cursbrk);
       trace(tracebuf);
     }

     return (void *)current;
}

int brk(endds)
    unsigned long endds;
{
    return _brk(endds);
}

int _brk(endds)
    unsigned long endds;
{
     unsigned long cur,new;

     if(debugging) {
       sprintf(tracebuf,"BRK %x\n",endds);
       trace(tracebuf);
     }

     cur=(cursbrk+4095) & 0xFFFFF000;
     new=(endds+4095) & 0xFFFFF000;

     if(new > cur) {
          memset((char *)cur,0,(new-cur));
     }
     cursbrk = endds;

     return 0;
}


stat(name,buf)
   const char *name;
   struct stat *buf;
{
   return _stat(name,buf);
}

_stat(name, buf)
    const char *name;
    struct stat *buf;
{
    int ndx;

    if(debugging) {
      sprintf(tracebuf,"STAT '%s'\n",name);
      trace(tracebuf);
    }

    ndx=mylookup(name,0);   /* returns filet index, map the file */
    if(ndx == -1) {
        errno=ENOENT;
        return -1;
    }
    if(debugging) {
//    sprintf(tracebuf,"STAT- ndx=%d\n",ndx);
//    trace(tracebuf);
    }

    if(!ndx) {  /* this is a directory */
        memset(buf,0,sizeof(struct stat));
    buf->st_dev=1;
    buf->st_ino=1;
    buf->st_mode=0x1FF;
    buf->st_mode |= S_IFDIR;
    buf->st_nlink=1;
    buf->st_uid=1;
    buf->st_gid=1;
    buf->st_rdev=1;
    buf->st_size=128;
    buf->st_blksize=8192;
    buf->st_blocks = 1;
    strcpy(buf->st_fstype,"UFS");
        return 0;
    }

    fillbuf(buf,ndx);
    filet[ndx].type = 0;   /* allow table entry to be used again */

    return 0;

}

fillbuf(buf,ndx)
    struct stat *buf;
    int ndx;
{
    long long seglen;

    memset(buf,0,sizeof(struct stat));
    buf->st_dev=1;
    buf->st_ino=1;
    buf->st_mode=0x1FF;
    buf->st_nlink=1;
    buf->st_uid=1;
    buf->st_gid=1;
    buf->st_rdev=1;
    buf->st_size=filet[ndx].end;
    buf->st_blksize=8192;
    buf->st_blocks = (buf->st_size+8191)/8192;
    strcpy(buf->st_fstype,"UFS");

    if(debugging) {
//sprintf(tracebuf,"st_size %X, st_blocks %X\n",buf->st_size,buf->st_blocks);
//trace(tracebuf);
    }
}

fillbuf64(buf,ndx)
    struct stat64 *buf;
    int ndx;
{
    long long seglen;

    memset(buf,0,sizeof(struct stat64));
    buf->st_dev=1;
    buf->st_ino=1;
    buf->st_mode=0x1FF;
    buf->st_nlink=1;
    buf->st_uid=1;
    buf->st_gid=1;
    buf->st_rdev=1;
    buf->st_size=filet[ndx].end;
    buf->st_blksize=8192;
    buf->st_blocks = (buf->st_size+8191)/8192;
    strcpy(buf->st_fstype,"UFS");

    if(debugging) {
//sprintf(tracebuf,"st_size %X, st_blocks %X\n",buf->st_size,buf->st_blocks);
//trace(tracebuf);
    }
}

_fstat64(fh,buf)
    int fh;
    struct stat *buf;
{
    int ndx;

    if(debugging) {
      sprintf(tracebuf,"FSTAT64 '%d'\n",fh);
      trace(tracebuf);
    }

    ndx = fileh[fh].filetindex;
    if(ndx == -1) {
         errno=ENOENT;
         return -1;
    }

    fillbuf64(buf,ndx);
    filet[ndx].type = -1;

    return 0;
}

_open(name,mode)
    const char *name;
    int mode;
{
    int ndx;

    if(debugging) {
      sprintf(tracebuf,"OPEN '%s'\n",name);
      trace(tracebuf);
    }

    if(!strcmp(name,"./Freezedry.class")) {
        _freezedry();
        errno=ENOENT;
        return -1;
    }

    ndx=mylookup(name,0);   /* returns filet index maps the file */
    if(ndx == -1) {
        errno=ENOENT;
        return -1;
    }

    fileh[ndx].filetindex=ndx;  /* 1-1 fileh and filet for now */
    fileh[ndx].mode=mode;
    fileh[ndx].type=HANDLETYPECLASS;

    return ndx;
}
close(fh)
     int fh;
{
     return _close(fh);
}

_close(fh)
     int fh;
{

    if(debugging) {
      sprintf(tracebuf,"CLOSE '%d'\n",fh);
      trace(tracebuf);
    }

     filet[fileh[fh].filetindex].type=0;   /* available */
     fileh[fh].filetindex=-1;
     fileh[fh].type=0;
}
read(fh,buffer,count)
     int fh,count;
     unsigned char *buffer;
{
     return _read(fh,buffer,count);
}

_read(fh,buffer,count)
     int fh,count;
     unsigned char *buffer;
{
     char *ptr;
     int filetindex;
     UINT32 rc;
     int buflen;

    if(debugging) {
      sprintf(tracebuf,"READ '%d(%d) -> %X'",fh,count,buffer);
      trace(tracebuf);
    }

     if(!fileh[fh].type) {
          errno=EBADF;
     if(debugging) {
        trace("    =EBADF\n");
     }
          return -1;
     }

     if(fileh[fh].type == HANDLETYPESIK) {
          {OC(Node_Fetch+KEYSNODESIK);XB(0x00100000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
          if (count > 255) count=255;
          {OC(8192+count);XB(0x00F00000);RC(rc);RS3(buffer,count,buflen);NB(0x0B10000F);cjcc(0x00100000,&_jumpbuf); }
          {OC(Node_Swap+KEYSNODESIK);XB(0x8010F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
          if((buflen == 1)  && (*buffer == 4)) return 0;
          return buflen;
     }
     /* OTHER TYPES HERE */

     filetindex=fileh[fh].filetindex;

     if(filet[filetindex].compindex == -1) { /* not mapped, after restart */
          filet[filetindex].compindex=mylookup(filet[filetindex].name,1);   /* key in K1 */
          {OC(Node_Fetch+DOMKEY);XB(0x00100000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Domain_GetMemory);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Swap+filet[filetindex].slot);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
     }

     ptr=(char *)(filet[filetindex].slot * 0x10000000);
     if(filet[filetindex].position+count > filet[filetindex].end)
           count=filet[filetindex].end-filet[filetindex].position;
     if(!count) {
        if(debugging) {
           trace("    =0\n");
        }
         return 0;
     }
     memcpy(buffer,(unsigned char *)(ptr+filet[filetindex].position),count);

     if(debugging) {
       sprintf(tracebuf,"[%X(%d) '%x %x %x %x %x %x %x %x %x %x %x %x... %x']",
             ptr+filet[filetindex].position,count,
             buffer[0],buffer[1],buffer[2],buffer[3],
             buffer[4],buffer[5],buffer[6],buffer[7],
             buffer[8],buffer[9],buffer[10],buffer[11],buffer[count-1]);
       trace(tracebuf);
     }
     filet[filetindex].position += count;

     if(debugging) {
       sprintf(tracebuf,"    =%d\n",count);
       trace(tracebuf);
     }
     return count;
}
write(fh,buffer,count)
     int fh,count;
     unsigned char *buffer;
{
     return _write(fh,buffer,count);
}

_write(fh,buffer,count)
     int fh,count;
     unsigned char *buffer;
{
     char *ptr;
     int filetindex;
     UINT32 rc;
     long long limit;

     if(debugging) {
        sprintf(tracebuf,"WRITE '%d(%d)'\n",fh,count);
        trace(tracebuf);
     }

     if(!fileh[fh].type) {
          errno=EBADF;
          return -1;
     }

     if(fileh[fh].type == HANDLETYPESOK) {
          {OC(Node_Fetch+KEYSNODESOK);XB(0x00100000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
          if (count > 255) count=255;
          {OC(0);PS2(buffer,count);XB(0x04F00000);RC(rc);NB(0x0810000F);cjcc(0x08000000,&_jumpbuf); }
          {OC(Node_Swap+KEYSNODESOK);XB(0x8010F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
          return count;
     }
     /* OTHER TYPES HERE */

     filetindex=fileh[fh].filetindex;

     if(filet[filetindex].compindex == -1) { /* not mapped, after restart */
          filet[filetindex].compindex=mylookup(filet[filetindex].name,1);   /* key in K1 */
          {OC(Node_Fetch+DOMKEY);XB(0x00100000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Domain_GetMemory);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Swap+filet[filetindex].slot);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
     }

     ptr=(char *)(filet[filetindex].slot * 0x01000000);

     {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+filet[filetindex].slot);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     limit=0xFFFFFFFFFFFFFFFFull;
     {OC(FS_SetLimit);PS2(&(limit),sizeof(limit));XB(0x04F00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

     memcpy((unsigned char *)(ptr+filet[filetindex].position),buffer,count);

     filet[filetindex].position += count;
     filet[filetindex].end = filet[filetindex].position;

     limit=filet[filetindex].end;
     {OC(FS_SetLimit);PS2(&(limit),sizeof(limit));XB(0x04F00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

     return count;

}
long long _lseek64(fh,offset,whence)
    int fh,whence;
    long long offset;
{
    int filetindex;

    if(debugging) {
       sprintf(tracebuf,"LLSEEK '%d(%d)'\n",fh,offset);
       trace(tracebuf);
    }

     if(!fileh[fh].type) {
          errno=EBADF;
          return -1;
     }

     filetindex=fileh[fh].filetindex;

     switch(whence) {
     case SEEK_SET:
         filet[filetindex].position=offset;
         return (long long)filet[filetindex].position;

     case SEEK_CUR:
         filet[filetindex].position += offset;
         return (long long)filet[filetindex].position;

     case SEEK_END:
         filet[filetindex].position = filet[filetindex].end + offset;
         return (long long)filet[filetindex].position;
     }

}

long _lseek();

long lseek(fh, offset, whence)
    int fh,offset,whence;
{
     return _lseek(fh, offset, whence);
}

long _lseek(fh, offset, whence)
    int fh,offset,whence;
{
     int filetindex;

     if(debugging) {
        sprintf(tracebuf,"LSEEK '%d(%d)'\n",fh,offset);
        trace(tracebuf);
     }

     if(!fileh[fh].type) {
          errno=EBADF;
          return -1;
     }

     filetindex=fileh[fh].filetindex;

     switch(whence) {
     case SEEK_SET:
         filet[filetindex].position=offset;
         return filet[filetindex].position;

     case SEEK_CUR:
         filet[filetindex].position += offset;
         return filet[filetindex].position;

     case SEEK_END:
         filet[filetindex].position = filet[filetindex].end + offset;
         return filet[filetindex].position;
     }
}

long long _llseek(fh, offset, whence)
    int fh;
    long long offset;
    int whence;
{
     int filetindex;

    if(debugging) {
       filetindex=offset;
       sprintf(tracebuf,"LLSEEK '%d(%d)'\n",fh,filetindex);
       trace(tracebuf);
    }

     if(!fileh[fh].type) {
          errno=EBADF;
          return -1;
     }
     filetindex=fileh[fh].filetindex;

     switch(whence) {
     case SEEK_SET:
         filet[filetindex].position=offset;
         return filet[filetindex].position;

     case SEEK_CUR:
         filet[filetindex].position += offset;
         return filet[filetindex].position;

     case SEEK_END:
         filet[filetindex].position = filet[filetindex].end + offset;
         return filet[filetindex].position;
     }
}

long long lseek64(fh,offset,whence)
    int fh,whence;
    long long offset;
{
    return _lseek64(fh,offset,whence);
}

ioctl(fh, command, tio)
     int fh,command;
     struct termio *tio;
{
     return _ioctl(fh, command, tio);
}

_ioctl(fh, command, tio)
     int fh,command;
     struct termio *tio;
{
     if(debugging) {
       sprintf(tracebuf,"IOCTL '%d(%d)'\n",fh,command);
       trace(tracebuf);
     }

     if(command == TCGETA) {
        if((fileh[fh].type != HANDLETYPESOK) && (fileh[fh].type != HANDLETYPESIK)) {
            errno=ENOTTY;
            return -1;
        }

        tio->c_iflag = 0;
        tio->c_oflag = 0;
        tio->c_cflag = 0;
        tio->c_lflag = 0;
        tio->c_line   = 0;
        memset(tio->c_cc,0,8);

        return 0;
     }

     errno=EINVAL;
     return -1;

}

trace(str)
     char *str;
{
     UINT32 rc;

     {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
     {OC(0);XB(0x00C00000);RC(rc);NB(0x08400C00);cjcc(0x00000000,&_jumpbuf); }
     {OC(0);PS2(str,strlen(str));XB(0x04C00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
}


/************************************************************************

  These are needed to satisfy the link.  One hopes they are never called

  They are currently defined in nativeGraphics.c for compatibility with
  static linking of KVM for comparison testing.

************************************************************************/

#ifdef xx
void dlopen() {}
void dlclose() {}
void dlsym() {}
void dlerror() {}
#endif
