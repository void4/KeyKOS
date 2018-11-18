/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************************
  This keeper is linked with CFSTART so that it can use FORK
  for helper domains

  The memory of this domain is actually a full 4 gigabytes with 256 meg
  slots used to map files.

  In order to support fork() there is a myfork() subroutine which temporarily
  puts the memory back to its simple form and expands the memory of the
  child
*****************************************************************************/

#include "keykos.h"
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "dc.h"
#include "ocrc.h"
#include "ukeeper.h"
#include "tdo.h"
#include "fs.h"
#include "vcs.h"
#include "factory.h"
#include "wait.h"
#include "clock.h"
#include "dc.h"
#include "kuart.h"
#include "unixdevice.h"

#include <string.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/signal.h>
#define sigmask(n)              ((unsigned int)1 << (((n) - 1) & (32 - 1)))
#define sigword(n)              (((unsigned int)((n) - 1))>>5)
#define sigaddset(s, n)         ((s)->__sigbits[sigword(n)] |= sigmask(n))
#define sigdelset(s, n)         ((s)->__sigbits[sigword(n)] &= ~sigmask(n))
#define sigismember(s, n)       (sigmask(n) & (s)->__sigbits[sigword(n)])
#include <sys/siginfo.h>
#include <sys/ucontext.h>
#include <sys/stack.h>
#include <sys/time.h>
#include <sys/lwp.h>
#include <sys/synch.h>
#include <sys/schedctl.h>
#include <sys/sysconfig.h>
#include <sys/door.h>
#include <sys/systeminfo.h>
#include <sys/resource.h>
#include <sys/unistd.h>
#include <poll.h>

/* PAGESZ at 8192 to reflect the future, this is used for inodes */

#define PAGESZ 8192

    KEY comp     = 0;
#define COMPDIRECTORY 1  /* root directory for file system */
                         /* this can be a zip segment or TDO */
#define COMPCOPY      3  /* copy key */
#define COMPFSF       4  /* fresh segment factory for new files */
#define COMPWAITF     5  /* wait object factory for timers */
#define COMPCLOCK     6  /* a clock object */
#define COMPTDOF      7  /* a record collection creator */
#define COMPCONSF     8  /* DevCONSF */
#define COMPERROR   14   /* error key, can be used for temporary keeper  */
#define COMPCONSOLE  15
    KEY sb       = 1;
    KEY caller   = 2;
    KEY domkey   = 3;
    KEY psb      = 4;
    KEY meter    = 5;
    KEY dc       = 6;

    KEY node     = 7;  /* a place for extra keys */
#define NODEMYMEM  0     /* used to hold large memory during fork */
#define NODEDIRECTORY 1  /* the run time directory */
#define NODESIK 2
#define NODESOK 3
#define NODEUDOMS  4
#define NODEALARMWAIT   5
#define NODEITIMERWAIT  6
#define NODEPOLLWAIT 7
#define NODEDEVICE 8    /* the /dev directory */
#define NODELASTDIR 9
    KEY udom     = 8;  /* process domain key */

    KEY u1       = 9;
    KEY u2       = 10;

    KEY object   = 11;
    KEY TSOK     = 12;
    KEY k2       = 13;
    KEY k1       = 14;
    KEY k0       = 15;


    char title[] ="UKEEPERF";
    int  stacksiz= 8192;

/* the process memory is always mapped at 0x10000000.  This keeper only works */
/* when the user memory is limited to 256 meg.  This is the way uwrapper works */
/* so there is consistency */

    char *memorywanterror = (char *)0x00200000;    /* shared with my keeper */
    char *memoryerror     = (char *)0x00200001;
#define MEMUMEM 1
    char *usermem = (char *)0x10000000;
    struct Node_DataByteValue ndb7 = {7};

/* 256 megabyte slots */
/* the stack is limited to 1 megabyte in this model, this should be sufficient */
/* changing the stack size means lowering the file mapping addresses.  Given   */
/* the 256 megabyte limit, this choice seems reasonable */

#define STACKTOP    0x0F000000
#define STACKBOTTOM 0x0E000000

/* This structure describes a file node when the file system is a zip file */
/* The zip file is read and a directory structure using direntry's is constructed */
/* The search routines use this structure to locate the file in the zip segment */
 struct  direntry {   /* this is a node for constructed Unix directory (from seg) */
    int type;
    int inode;               /* inode number */
    int mode;
#define TYPEDIRECTORY 1
#define TYPEFILE      2
    struct direntry *next;   /* next member */
    struct direntry *chain;  /* children including . and .. */
    unsigned long length;    /* for files */
    unsigned long offset;    /* zip file offset of actual data */
    char name[2];            /* extended for entry name */
};

/* the file table has enough information so that the file can be re-opened */
/* from a restared frozen object.  Lookup and map is done again */
/* when the file system is a zip segment, re-open is not required as the directory */
/* segment will be remapped automatically by the thaw code and all the file table */
/* entries in that file system share the same memory slot */

    struct filetablee {   /*  One entry for each open file */
        char name[1024];
        char *address;
        unsigned long  flags;   /* a shared slot means a segment for a whole file system */
#define SLOTSHARED 1
#define OPENOUT 2
#define OPENAPPEND 4
#define FILEDEVICE 8
#define DEVICEKEYINSLOT 0x80000000
        long inode;
        long mode;
        long slot;    /* a device object key if the file is a device */
        long long  position;
        long long  length;
        struct termio tio;
    };

#define DEVROOT 1
#define DEVHOME  2
#define DEVCOMP  3

    struct filehandlee {   /* One per open file.  FILET is -1 after freeze */
	struct filetablee *filet;
        door_info_t *door;
        unsigned long  flags;
#define INPUT  1
#define OUTPUT 2
#define ZERO   4
#define DOOR   8
#define DIRECTORY 16
#define PROC   32
#define FILE   64
#define DEVICE 128

#define PROCSTATUS 256
#define PROCUSAGE  512
        int  attributes;  /* this should probably reflect the mode and type of open */
        int  sequence;    /* for synchronizing device requests */
        int  inode;
        int  threadid;
    };  	

/* a description of the light weight process */
/* there is one of these for every process including the main process or only */
/* process in a non-LWP task */

    struct uthread {
       short    flags;
#define ASLWP 1
#define DETACHED 2
#define SCHED_BLOCK 4
#define SCHED_STATE 5
       short    status;
       short    waitcode;
#define WAITMUTEX 1
#define WAITCONDV 2
#define WAITSEMA  3
#define WAITSIGNAL 4
#define WAITIO     5
#define WAITPOLL   6
       unsigned long waitobject;   /* user address of object or fd when waiting on io */
       sigset_t sa_mask;
       sigset_t sa_waiting;
       void *private;   /* private data pointer */
       struct Domain_SPARCRegistersAndControl drac;
       UINT32  oc;  /* original oc for bounce pass */
       door_info_t  *door;   /* for and lwp that is bound to a door */
       sc_shared_t  schedctl;
       struct pollfd *fds;  /* filled in by poll for poll completion notification */
       int nfds;
    };

    sigset_t nullset= {0,0,0,0};
    sigset_t fullset= {0xFFFFFFFF,0xFFF00000,0,0};

    int sigtran[128] = {0,
         SIGSEGV,  /* trap 1 instruction access */
         SIGILL,   /*      2 illegal inst */
         SIGILL,   /*      3 privileged inst */
         SIGEMT,   /*      4 fp disabled */
         0,0,      /*      5,6 window over/under */
         SIGBUS,   /*      7 not aligned */
         SIGFPE,   /*      8 fp exception */
         SIGSEGV   /*      9 data access */
     };
#define MAXSIGIGNORE 10
     int sigIgnore[MAXSIGIGNORE] = {SIGCHLD,SIGPWR,SIGWINCH,SIGURG,SIGWAITING,
                                    SIGLWP,SIGFREEZE,SIGTHAW,SIGCANCEL,0};
/* DATABYTES for Helpers */

#define DBTIMER    201
#define DBITIMER   202
#define DBPTIMER   203
#define DBIO       204

/* NODEUDOMS node can be a supernode and this can be changed */
#define MAXTHREADS 16

#define THREADAVAILABLE 0
#define THREADRUNNING 1
/* THREADSUSPENDED is that special state when creating a thread.  contextp is valid */
/*                                                                drac is not valid */
#define THREADSUSPENDED 2
#define THREADDOORRETURN 3
#define THREADRESTARTED 4
#define THREADDOORCALL 5

/* THREADWAITING means Blocked (io, sigwait, etc) and affects signal disposition */

#define THREADWAITING 9

/* MAX Files limited by 8 available memory slots 2-10 */
/* MAX FileHandes limited to 11 as 8 + in,out,err     */

#define MAXFILEHANDLES 16
#define MAXFILES 8
#define MAXSLOTS 16
#define MAXMAPSLOTS 256
#define MAXDOORS 8

/* this structure holds all the state data of the keeper.  This data is saved */
/* in the user memory in the highest 16 megabyte chunk (above the stack)  */
/* with the stack at 0x0F000000 this limits the user memory to less than 256 meg */

#define MAXSIGNALS  128

    struct ukeepdata {
       struct UKeeper_Name ukn;
       unsigned long brkaddress;
       unsigned long brkhighwater;
       unsigned long mapaddress;
       unsigned long maplowwater;
       struct filetablee *slots[MAXSLOTS];
       struct filehandlee filehandles[MAXFILEHANDLES];
       struct filetablee filetable[MAXFILES];
       door_info_t doors[MAXDOORS];
       int doorunique;   /* unique door count */
       int scheddoor;    /* scheduler activation door */
       int truss;
       int freezedryhack;
       int freezerc;  /* set to 0 or ENOENT */
       int freezeid;  /* thread id of lwp that issued freeze */
       unsigned long restartaddr;
       unsigned long frozenaddr;
       int threadid;
       sigset_t sa_pending;
       struct sigaction sigacttable[MAXSIGNALS];    /* seems to be max size of sigset_t */
       struct uthread uthreads[MAXTHREADS];
       struct itimerval realtimer;                  /* interval timer value */
       short havealarmtimer,haveitimertimer;        /* indicate timers running */
       short havepolltimer;                         /* indicate poll timer runnint */
       unsigned long long microseconds;             /* alarm timer value */
       unsigned long long pollmicroseconds;         /* poll timer value */
       unsigned long long alarmtod,polltod;         /* timer tod values for out of date check */
       short multithread;                           /* if more than 1 lwp */
       short alarmthread;                           /* in multi-thread deliver here */
       short curdevice;                             /* current device from lookup */
       short lookuperror;                           /* error code from lookup */
#define LOOKNODIR 1
#define LOOKNOFILE 2
       short rootslot;                              /* when directory is segment */
                                                    /* this also signals that the Unix */
                                                    /* directory has been built */
       short currentslot;                           /* when directory is a segment as in root */
       short componentslot;                         /* when object component directory is a segment */
       unsigned char *rootaddress;                  /* directory base address if segment */
       unsigned char *currentaddress;               /* directory base address if segment */
       unsigned char *componentaddress;             /* directory base address if segment */
       struct direntry *root;                       /* when using segment as root */
       struct direntry *current;                    /* when using segment as current */
       struct direntry *component;                  /* when using segment as component */
       double pool[2];                              /* malloc pool header (4 words) */
       char lastlookupname[256];                    /* last name looked up (for create) */

       char trussbuf[512]; /* for building truss messages */
       char name[1024];   /* keep this last */
    } *ukd = (struct ukeepdata *)0x1F300000; // 0x0F300000 in user memory

#define MALLOCPOOL 0x1FC00000

/*  the args, wrapper code, and ld.so are at 0x0f000000, 0x0f100000, and 0x0f200000 */
/*  keeper data starts at 0x0f300000 and can go nearly all the way to the end */
/*  malloc starts at 0x0fc00000 for 4 megabytes */

/*********************************************************************************
  ZIP FILE DIRECTORY ROUTINES

  These routines are used to parse the zip file and build the directory
  structure used by the lookup routines.   a zlookup is included here
  to search this new structure.
**********************************************************************************/

  struct cdh {   /* central directory header */
     unsigned char zipcensig[4];    /* 02014b50 */
     unsigned char zipcver;
     unsigned char zipccos;
     unsigned char zipcvxt;
     unsigned char zipexos;
     unsigned char zipcflg[2];
     unsigned char zipcmthd[2];  /* compression method */
     unsigned char zipctim[2];
     unsigned char zipcdat[2];
     unsigned char zipcrc[4];
     unsigned char zipcsiz[4];
     unsigned char zipcunc[4];   /* size */
     unsigned char zipcfnl[2];   /* name length */
     unsigned char zipcxtl[2];   /* extra length */
     unsigned char zipccml[2];   /* comment length */
     unsigned char zipdsk[2];
     unsigned char zipint[2];
     unsigned char zipext[4];    /* file mode (first 2 bytes) */
     unsigned char zipofst[4];   /* start address of header */
     unsigned char zipcfn[1];     /* name */
     unsigned char zipcxtr[1];    /* extra */
     unsigned char zipccm[1];    /* comment */
  };

  struct lfh {   /* local file header */
     unsigned char ziplocsig[4];    /* 04034b50 */
     unsigned char zipver[2];
     unsigned char zipgenflg[2];
     unsigned char zipmthd[2];
     unsigned char ziptime[2];
     unsigned char zipdate[2];
     unsigned char zipcrc[4];
     unsigned char zipsize[4];
     unsigned char zipuncmp[4];
     unsigned char zipfnln[2];
     unsigned char zipxtraln[2];
     unsigned char zipname[1];
     unsigned char zipxtra[1];
  };

  struct ecdh {  /* end central directory header */
     unsigned char zipesig[4];     /* 06054b50 */
     unsigned char zipedsk[2];
     unsigned char zipecen[2];
     unsigned char zipenum[2];
     unsigned char zipecenn[2];
     unsigned char zipecsz[4];
     unsigned char zipeofst[4];
     unsigned char zipecoml[2];
     unsigned char zipecom[1];
  };

  struct zdirentry {
     unsigned short mode;
     unsigned long  position;
     unsigned long  length;
     char name[512];
  };

/********************************************************************************
     ZLOOKPORTION

     replacement for lookitup.  This searches a directory for an entry.

     it is also used to search directories for add functions when building
     the file system, hence its early introduction.
********************************************************************************/
struct direntry *zlookportion(de,name)
    struct direntry *de;
    char *name;
{
    struct direntry *tde;

    tde=de->chain;
    while(tde) {
       if(!strcmp(tde->name,name)) {
           return tde;
       }
       tde = tde->next;
    }

    return 0;
}

/********************************************************************************
    ZADDPORTION

    adds a directory to the file tree

    INPUT: the direntry of the directory to add to
           the name
           the inode #
********************************************************************************/

struct direntry *zaddportion(de,name,inode)
    struct direntry *de;
    char *name;
    int inode;
{
    int size;
    struct direntry *tde,*newde,*dotde,*dotdotde;

    tde=de->chain;
    while(tde->next) {
        tde = tde->next;
    }
/* now at last entry */
    size = sizeof(struct direntry) + strlen(name) + 1;
    newde = (struct direntry *)malloc(ukd->pool,size);
    memset((char *)newde,0,sizeof(struct direntry));
    newde->inode=inode;
    newde->type=TYPEDIRECTORY;
    newde->mode = 0x416b;
    newde->length=512;
    strcpy(newde->name,name);
    tde->next = newde;   /* add to end */
/* now must add . and .. to new directory entry */
    size= sizeof(struct direntry) + 3;  /* big enough for . and .. */
    dotde=(struct direntry *)malloc(ukd->pool,size);
    memset((char *)dotde,0,sizeof(struct direntry));
    dotde->inode=inode;
    dotde->mode = 0x416b;
    dotde->length=512;
    dotde->type=TYPEDIRECTORY;
    dotde->chain=dotde;  /* act like newde if located */
    strcpy(dotde->name,".");
    newde->chain=dotde;

    dotdotde=(struct direntry *)malloc(ukd->pool,size);
    memset((char *)dotdotde,0,sizeof(struct direntry));
    dotdotde->inode=de->inode;
    dotdotde->mode = 0x416b;
    dotdotde->length = 512;
    dotdotde->type = TYPEDIRECTORY;
    strcpy(dotdotde->name,"..");
    dotde->next=dotdotde;
    dotdotde->chain=de->chain;   /* act like parent if located */

    return newde;
}
/********************************************************************************
    ZADDFILE

    adds a file to the directory

    INPUT: the direntry of the directory to add to
           the name
           the position of the file in the zip file
           the length
           the mode
           the inode #

********************************************************************************/
zaddfile(de,name,position,length,mode,inode)
    struct direntry *de;
    char *name;
    unsigned long length;
    int mode;
    int inode;
{
    struct direntry *tde,*newde;
    int size;

    tde=de->chain;
    while(tde->next) {
       tde = tde->next;
    }

    size = sizeof(struct direntry) + strlen(name) + 1;
    newde = (struct direntry *)malloc(ukd->pool,size);
    memset((char *)newde,0,sizeof(struct direntry));
    newde->inode=inode;
    newde->type=TYPEFILE;
    newde->mode = mode;
    newde->length=length;
    newde->offset=position;
    strcpy(newde->name,name);

    tde->next = newde;   /* add to end */
}

/********************************************************************************
    ZADDNAME

    adds a full path name to the "root" directory

    INPUT: the direntry of the directory to add to (often root)
           the zip direntry (zip dir entries are from the central directory)
           the inode number

********************************************************************************/
/* this routine is never faced with adding a directory name (ending with /) */
/* nor is it ever faced with adding a duplicate */
zaddname(root,zde,inode)
     struct direntry *root;
     struct zdirentry *zde;
     int inode;
{
     char *name,*ptr,*portion;
     struct direntry *de,*tde;
     char buf[128];

     strcpy(ukd->name,zde->name);
     name=ukd->name;
     de=root;

     while(ptr=strchr(name,'/')) {  /* found portion */
        portion=name;
        *ptr=0;
        name=ptr+1;  /* next piece */
        if(tde=zlookportion(de,portion)) {  /* ok */
           de=tde;
           continue;
        }
        else { /* need to add */
           inode++;
           de=zaddportion(de,portion,inode);
           continue;
        }
     }
     /* now at last piece */

     inode++;
     zaddfile(de,name,zde->position,zde->length,zde->mode,inode);

     return inode;
}

/********************************************************************************
    ZGETSHORT - swaps bytes
********************************************************************************/
unsigned short zgetshort(ptr)
   unsigned char *ptr;
{
    return ((*(ptr+1))<< 8) + *ptr;
}
/********************************************************************************
    ZGETLONG - swaps bytes
********************************************************************************/
unsigned long zgetlong(ptr)
   unsigned char *ptr;
{
    return ((zgetshort(ptr+2) << 16) + zgetshort(ptr));
}

/********************************************************************************
    ZREADDIR

     reads a zip directory entry and returns the offset of the next

     fills in a zipdirentry
********************************************************************************/
unsigned long zreaddir(address,position,de)
   unsigned char *address;
   unsigned long position;
   struct zdirentry *de;
{
   struct cdh  *ch;
   struct lfh  *lh;
   unsigned long lmode;
   unsigned long namepos;
   int i;
   unsigned short xtral,comml,namel;
   unsigned short xxtral,xnamel;
   unsigned long filepos;

   ch=(struct cdh *)(address+position);
   lmode = zgetlong(ch->zipext);
   de->mode = lmode >> 16;
   de->position = zgetlong(ch->zipofst);
   namel=zgetshort(ch->zipcfnl);
   namepos = position + (ch->zipcfn-ch->zipcensig);
   memcpy(de->name,address+namepos,(int)namel);
   de->name[namel]=0;

   xtral=zgetshort(ch->zipcxtl);
   comml=zgetshort(ch->zipccml);

   lh=(struct lfh *)(address + de->position);

   xxtral=zgetshort(lh->zipxtraln);
   xnamel=zgetshort(lh->zipfnln);
   filepos=de->position+xxtral+xnamel + (lh->zipname-lh->ziplocsig);
   de->position=filepos;

   de->length = zgetlong(lh->zipuncmp);

/* now get file data position using de->position as header location */

   return namepos + namel + xtral + comml;
}

/**********************************************************************************
   MAKEZIPDIR  - makes the basic directory structure

   INPUT:   object contains the segment key of the zip segment

**********************************************************************************/

makezipdir(rdir,rslot,raddress,rname,rinode)
   struct direntry **rdir;
   short *rslot;
   unsigned char **raddress;
   char *rname;
{
   JUMPBUF;
   int i;
   char buf[256];

   *rslot=nextslot(1);   /* no filet needed here */
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+(*rslot));XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   *raddress = (unsigned char *)((*rslot) << 28);

   *rdir = (struct direntry *)calloc(ukd->pool,1,sizeof(struct direntry));
   (*rdir)->chain = (struct direntry *)calloc(ukd->pool,1,sizeof(struct direntry));
   (*rdir)->chain->next = (struct direntry *)calloc(ukd->pool,1,sizeof(struct direntry));

   (*rdir)->type=TYPEDIRECTORY;
   (*rdir)->inode=rinode;
   (*rdir)->length=512;
   (*rdir)->mode=0x416b;
   strcpy((*rdir)->name,rname);

   (*rdir)->chain->type=TYPEDIRECTORY;
   (*rdir)->chain->inode=rinode;
   (*rdir)->chain->length=512;
   (*rdir)->chain->mode=0x416b;
   strcpy((*rdir)->chain->name,".");

   (*rdir)->chain->next->type=TYPEDIRECTORY;
   (*rdir)->chain->next->inode=rinode;
   (*rdir)->chain->next->length=512;
   (*rdir)->chain->next->mode=0x416b;
   strcpy((*rdir)->chain->next->name,"..");

   i=zbuilddir(*raddress,*rdir,rinode);  /* what to do if fails ?? */

   return i;
}


/********************************************************************************
    ZBUILDDIR

    finds the beginning and end of the zip directory
    reads directory and adds all names to root

    returns the number of entries
********************************************************************************/
int zbuilddir(address,root,startinode)
   unsigned char *address;
   struct direntry *root;
{
   int length;
   int inode;
   unsigned short entries;
   unsigned long position;
   struct zdirentry de;
   struct lfh  *lf;
   struct cdh  *cd;
   unsigned short xtral,namel;
   unsigned long filepos;

   char buf[128];

   position=0;
   while(1) {
      lf = (struct lfh *)(address+position);
      if(zgetlong(lf->ziplocsig) == 0x02014b50) {
         break;
      }
      xtral=zgetshort(lf->zipxtraln);
      namel=zgetshort(lf->zipfnln);
      filepos = position + (lf->zipname - lf->ziplocsig) + xtral +namel ;
      length = zgetlong(lf->zipuncmp);
      position = filepos + length;
   }

/* position now equals the location of the directory */

   inode=startinode;
   entries=0;
   while(1) { /* go untill reach trailer */
      cd = (struct cdh *)(address+position);
      if( (zgetlong(cd->zipcensig) & 0xFF00FFFF) == 0x06004b50) {
          break;
      }
      position=zreaddir(address,position,&de);
      if(de.mode & 0x8000) {
         inode=zaddname(root,&de,inode);
      }
      entries++;
   }
   return entries;
}

/********************************************************************************
    ZLOOKUPNAME

    given a directory and a full name
    parse the name into portions and lookup each in the directory tree

    returns a pointer to the directory entry of interest
********************************************************************************/
struct direntry *zlookupname(de,fullname)
    struct direntry *de;
    char *fullname;
{
     char *name,*ptr,*portion;
     struct direntry *tde;

     strcpy(ukd->name,fullname);
     name=ukd->name;

     while(ptr=strchr(name,'/')) {  /* found portion */
        portion=name;
        *ptr=0;
        name=ptr+1;  /* next piece */
        if(tde=zlookportion(de,portion)) {  /* ok */
           de=tde;
           continue;
        }
        ukd->lookuperror=LOOKNODIR;
        return 0;
     }
     if(!*name) {
         return de;
     }
     /* now at last piece */
     tde=zlookportion(de,name);
     if(!tde) {
        ukd->lookuperror=LOOKNOFILE;
     }
     return tde;
}

/********************************************************************************
*********************************************************************************
    End of directory lookup routines.

    BEGIN the main keeper program
*********************************************************************************
*********************************************************************************/

factory()
{
    JUMPBUF;
    UINT32 rc,oc;
    UINT16 db;
    int i;
    union {
        struct Domain_SPARCRegistersAndControl drac;
        struct UKeeper_Name ukn;
        unsigned long brkaddress;
        char devicename[255];
        struct DeviceIORequest dior;
    } parm;
    char buf[256];
    int havemem = 0;

    {OC(SB_CreatePage);XB(0x00400000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+2);XB(0x80E07000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }       /* page at 0x00200000  shared with keeper */
    {OC(Domain_GetKey+domkey);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* my domain key for keeper */

                    /* this fork is done before we have built up our memory tree */
    if(!fork()) {   /* I need a simple keeper */

        {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Swap+2);XB(0x80F07000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   /* shared page */
        {OC(Domain_MakeStart);XB(0x00300000);NB(0x00A0F020);cjcc(0x00000000,&_jumpbuf); }
        {OC(Domain_SwapKeeper);XB(0x8080F000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }  /* udom has old keeper */

        {OC(0);XB(0x00200000); }  /* DK(0) */
        for(;;) {  /* keeper loop */
           {RC(oc);RS2(&(parm.drac),sizeof(parm.drac));NB(0x0B3000F2); }
           {rj(0x00080000,&_jumpbuf); }

           if(oc == 4) {
               exit(0);
           }

           if(*memorywanterror) {  /* want to trap errors */
                *memoryerror = 1;  /* flag error */

                parm.drac.Control.PC=parm.drac.Control.NPC;
                parm.drac.Control.NPC=parm.drac.Control.PC+4;

                {OC(Domain_ResetSPARCStuff);PS2(&(parm.drac),sizeof(parm.drac));XB(0x14F00002); }
                continue;
           }
           else {   /* pass it on */
              {OC(oc);PS2(&(parm.drac),sizeof(parm.drac));XB(0x348000F2); }
              continue;
           }
        }
    }

    {OC(SB_CreateNode);XB(0x00400000);NB(0x00807000);cjcc(0x00000000,&_jumpbuf); }   /* the extra keys node */
    {OC(SB_CreateNode);XB(0x00400000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+NODEUDOMS);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+COMPWAITF);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(WaitF_Create);XB(0xE0F04510);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+NODEALARMWAIT);XB(0x8070E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(WaitF_Create);XB(0xE0F04510);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+NODEITIMERWAIT);XB(0x8070E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(WaitF_Create);XB(0xE0F04510);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+NODEPOLLWAIT);XB(0x8070E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_CreateNode);XB(0x00400000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }     /* my larger memory node */
    {OC(Node_MakeNodeKey);PS2(&(ndb7),sizeof(ndb7));XB(0x04F00000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+0);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }      /* code+stack in first 256 meg */
    {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);XB(0x8020F000); }

    for(;;) {
        {RC(oc);DB(db);RS2(&(parm),sizeof(parm));NB(0x0FF09A82); }
        {rj(0x00080000,&_jumpbuf); }

/* DataBytes 0 - 15 are for threads.  The main thread is 0 and lwp's are */
/* DB = 1 - 15.   At this time only 15 threads are supported, MAXTHREADS */
/* defines the maximum number of threads (including the master) */

/* Thread IDs for TRUSS output are databyte+1.  threadid is the databyte */

/* Databytes above 200 are used for the various helper domains */

        if(0) {
             sprintf(buf,"UKeeper[%d]: oc=%8lx PC %lX g1 %lX o0 %lX o1 %lX\n",
                    db+1, oc,
                    parm.drac.Control.PC,
                    parm.drac.Regs.g[1],
                    parm.drac.Regs.o[0],
                    parm.drac.Regs.o[1]);
             outsok(buf);
        }

        if(oc == KT) {
            {OC(UKeeper_AKT);XB(0x00200000); }
            continue;
        }

        if(oc == UKeeper_Destroy) {
            myexit(0,0);  // teardown memory and die
        }
/**********************************************************************************
   All calls (traps and explicit) require the domain key in slot 3
   The user memory is always fetched and saved in my memory for mapping

   At some time in the future a way to optimize this might be called for
   such as only doing it once and promising never to change it except
   after freezedry (thaw could pass it)
**********************************************************************************/

        if(db > 200) {  /* helper domain */

           if(db == DBTIMER) {
              rc=dotimer(0);  /* non-zero return code indicates timer restart */
           }
           if(db == DBITIMER) {
              unsigned long long value,interval;

              rc=dotimer(1);  /* non-zero return code means helper should exit */
              value = ukd->realtimer.it_value.tv_sec;
              value = value*1000000 + ukd->realtimer.it_value.tv_usec;
              interval = ukd->realtimer.it_interval.tv_sec;
              interval = interval*1000000 + ukd->realtimer.it_interval.tv_usec;
              if(!value) rc = 1;   /* signal no restart */
              if(!interval) rc = 1; /* signal no restart */
              if(rc) {
                  ukd->haveitimertimer=0;  /* because of non zero rc, helper will die */
              }

           }
           if(db == DBPTIMER) {
              rc=dotimer(2);  /* non-zero return code indicates timer restart */
           }

           if(db == DBIO) {
              rc=doiocomplete(&parm.dior);
           }
           {OC(rc);XB(0x00200000); }   /* signal caller with return code */
           continue;
        }
/* this is all databyte 0 stuff  */

        if(!havemem) {
            {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }    // get kept domain memory
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }  // get my memory node
            {OC(Node_Swap+MEMUMEM);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   // put into my map
            havemem=1;  /* save future work and help sidekick calls */
        }

        ukd->threadid = db;  /* thread ID is simply the databyte */

        if(oc == UKeeper_FreezeDryHack) {  /* set hack for open("Freezedry.class") */

            ukd->freezedryhack=1;

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetName) {  /* set program name  */
            ukd->ukn = parm.ukn;

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetBrk)  {  /* set brk address for sbrk() [no longer done] */

            ukd->brkaddress=parm.brkaddress;
            ukd->brkhighwater = (parm.brkaddress + 8191) && 0xFFFFE000;
            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetDirectory)  {  /* save run time directory for files */
                                           /* must be called AFTER INIT */
                                           /* or right before "thaw()" */
            if(!ukd->slots[0]) {
                {OC(1);XB(0x00200000); }
                continue;
            }
            {OC(Node_Swap+NODEDIRECTORY);XB(0x80709000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+NODEDIRECTORY);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }  /* for possible build */

            {OC(KT);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            if((rc != TDO_NSAKT) && (rc != KT+1)) {  /* must be segment */
                if(!ukd->currentslot) {   /* must be a thaw will fix later */
                   makezipdir(&ukd->current,&ukd->currentslot,&ukd->currentaddress,"home",10000);
                }
            }

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_TrussOn)  {  /* Set debug on */

            ukd->truss = 1;

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetRestartAddr)  {  /* start for application  (_start) */
                                             /* used to set start address for frozen factory */

            ukd->restartaddr=parm.brkaddress;

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetFrozenAddr)  {  /* must set frozen flag for application */

            ukd->frozenaddr=parm.brkaddress;

            {OC(0);XB(0x00200000); }
            continue;
        }
        if(oc == UKeeper_SetSikSok)  {     /* save terminal keys */
              struct DeviceIORequest dior;

              {OC(Node_Fetch+COMPCONSF);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(EXTEND_OC);XB(0xE0F04510);RC(rc);NB(0x0810000F);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0xC0F09A00);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0x00000000);NB(0x006009A0);cjcc(0x00000000,&_jumpbuf); }
              {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              dior.fh=0;
              dior.sequence=0;
              dior.flags=0;
              {OC(DeviceOpen);PS2(&(dior),sizeof(dior));XB(0x44F00E00);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

              {OC(Node_Swap+NODESIK);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+NODESOK);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

//            {OC(Node_Swap+NODESIK);XB(0x80709000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
//            {OC(Node_Swap+NODESOK);XB(0x8070A000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            {OC(0);XB(0x00200000); }
            continue;
        }

        if(oc == UKeeper_AddDevice)  {     /* add a device object to /dev */

            {OC(Node_Fetch+NODEDEVICE);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(KT);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            if(rc != TDO_NSAKT) {  /* need one, file names will be in file table for recovery */
               {OC(Node_Fetch+COMPTDOF);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
               {OC(TDOF_CreateNameSequence);XB(0xE0F04510);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
               {OC(Node_Swap+NODEDEVICE);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            }
            if(strlen(parm.devicename) > 255) {
               {OC(1);XB(0x00200000); }
               continue;
            }
            strcpy(buf+1,parm.devicename);
            *buf=strlen(parm.devicename);

            {OC(TDO_AddReplaceKey);PS2(buf,(*buf)+1);XB(0x84F09000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

            {OC(0);XB(0x00200000); }
            continue;
        }

        if(oc == UKeeper_Init)  {  /* Set up tables */

/* save the domain key for the master domain.  LWPs will save their domain keys here */
/* as well.   This may have to become a supernode */

            {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+0);XB(0x80F08000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

/* set up available memory slots for mapping files */

            ukd->slots[0]=(struct filetablee *)0xFFFFFFFF;
            ukd->slots[1]=(struct filetablee *)0xFFFFFFFF;
            for(i=11;i<MAXSLOTS;i++) {
                ukd->slots[i]=(struct filetablee *)0xFFFFFFFF;
            }
/* set up initial file handles */

            for(i=3;i<MAXFILEHANDLES;i++) {
                ukd->filehandles[i].filet=(struct filetablee *)0xFFFFFFFF;
                ukd->filehandles[i].flags=0;
                ukd->filehandles[i].attributes=0;
            }
            ukd->filehandles[0].filet=0;
            ukd->filehandles[0].flags=INPUT;
            ukd->filehandles[0].attributes=0;
            ukd->filehandles[1].filet=0;
            ukd->filehandles[1].flags=OUTPUT;
            ukd->filehandles[1].attributes=0;
            ukd->filehandles[2].filet=0;
            ukd->filehandles[2].flags=OUTPUT;
            ukd->filehandles[2].attributes=0;
/* set up initial file table */
            for(i=0;i<MAXFILES;i++) {
                ukd->filetable[i].address=(char *)0xFFFFFFFF;
                ukd->filetable[i].position=0;
                ukd->filetable[i].length=0;
                ukd->filetable[i].name[0]=0;
            }

            for(i=0;i<MAXTHREADS;i++) {
                ukd->uthreads[i].status = THREADAVAILABLE;
                ukd->uthreads[i].flags = 0;
                ukd->uthreads[i].sa_mask = nullset;
                ukd->uthreads[i].sa_waiting = nullset;
            }

            for(i=0;i<MAXSIGNALS;i++) {
                ukd->sigacttable[i].sa_handler = SIG_DFL;
                ukd->sigacttable[i].sa_mask    = nullset;
                ukd->sigacttable[i].sa_flags   = 0;
            }

            ukd->mapaddress = STACKBOTTOM-0x00010000;
                                             /* start dynamic mapping here going down  */
                                             /* leave 65K twixt here and the bottom of */
                                             /* the stack.  A stack check can catch    */
                                             /* intruders at system calls              */
            ukd->maplowwater = ukd->mapaddress;

            ukd->threadid = 0;
            ukd->uthreads[0].status = THREADRUNNING;

/*            ukd->truss = 1; */

            initalloc(ukd->pool,MALLOCPOOL,0x04000000);

            {OC(COMPDIRECTORY);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
            {OC(KT);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            if(rc != TDO_NSAKT) {  /* must be segment */
                makezipdir(&ukd->root,&ukd->rootslot,&ukd->rootaddress,"/",1);
            }

            {OC(0);XB(0x00200000); }
            continue;
        }

/* other calls on helper databytes will be handled here */

        if(oc > KT) {  /* a trap of some kind, hardware or software */

/********************************************************************************
   FIRST we must get the register windows saved if this is the old kernel

   This is done by putawaywindows() which gets the registers from the domain
   and stores them in the stack.

   In the case of FFFF the threads[threadid] must not be disurbed because
   it is probably a thaw request.   If it is FFFF then the registers are already
   saved by the trapping process.
********************************************************************************/

#ifndef NEWKERNEL
            putawaywindows(&parm.drac); /* new kernel keeps regs on stack at fault */
#endif
            handlefault(&parm.drac,oc);

            {OC(Domain_ResetSPARCStuff);PS2(&(parm.drac),sizeof(parm.drac));XB(0x14800002); }

            continue;
        }


        {OC(INVALIDOC_RC);XB(0x00200000); }
    }

}

/********************************************************************************
    PUTAWAYWINDOWS  - put back windows onto the stack
    udom has the domain key

********************************************************************************/
putawaywindows(drac)
    struct Domain_SPARCRegistersAndControl *drac;
{
    struct Domain_SPARCOldWindow windows[8];
    int i,j,actlen,nwindows;
    JUMPBUF;
    UINT32 rc;
    unsigned long *sp;
    char buf[256];

    {OC(Domain_GetSPARCOldWindows);XB(0x00800000);RS3(&(windows),64*8,actlen);NB(0x03000000);cjcc(0x00100000,&_jumpbuf); }
    nwindows = actlen/64;

    sp=(unsigned long *)(usermem + drac->Regs.o[6]);

    for(j=0;j<8;j++) {
        sp[j]=drac->Regs.l[j];
        sp[j+8]=drac->Regs.i[j];
    }

    sp=(unsigned long *)(usermem + drac->Regs.i[6]);

    for(i=nwindows-1;i>=0;i--) {
        for(j=0;j<8;j++) {
            sp[j] = windows[i].l[j];
            sp[j+8] = windows[i].i[j];
        }
        sp = (unsigned long *)(usermem + windows[i].i[6]);
    }
    {OC(Domain_ClearSPARCOldWindows);XB(0x00800000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
}

/********************************************************************************
    Actual fault handler.   Done after bounce pass for window saves
********************************************************************************/

handlefault(drac,oc)
    struct Domain_SPARCRegistersAndControl *drac;
    UINT32 oc;
{
    JUMPBUF;
    int ocm;
    char buf[256];

    ocm=oc & 0xFF;

    if(ocm == 0x88) { /* UNIX system call */

//       sprintf(buf,"g1/%8lX o0/%8lX o1/%8lX From: PC-%8lX o7-%8lX i7%8lX\n",
//            drac->Regs.g[1],
//            drac->Regs.o[0],drac->Regs.o[1],
//            drac->Control.PC,drac->Regs.o[7],drac->Regs.i[7]);
//       outsok(buf);

/* this doesn't work when the stacks are in mmap space (/dev/zero) */
#ifdef xx
         if(stackcheck(drac)) {
             sprintf(buf,"DEATH by STACK OVERFLOW\n");
             outsok(buf);
             doexit();   /* only other choice for default action */
         }
#endif
         dotrap(drac);  /* sets registers for return */

         if(ukd->uthreads[ukd->threadid].status != THREADRUNNING) {
             {OC(0);XB(0x00000000);NB(0x00200020);cjcc(0x00000000,&_jumpbuf); }   /* will re-create when needed */
         }
         return;
     }

     if( (ocm >= 0xA0) && (ocm <= 0xA7)) {  /* humm what are these */
        unsigned long vin,vout;

        vout=0;
        vin=drac->Regs.o[0];

        switch(ocm) {
        case 0xA0:  /* getCC */
        case 0xA1:  /* setCC */
           break;
        case 0xA2:  /* getPSR */
           vout = drac->Control.PSR;
           break;
        case 0xA3:  /* setPSR */
           break;
        case 0xA4:  /* GETHRTIME */
        case 0xA5:  /* GETHRVTIME */
        case 0xA6:  /* GETHRESTIME */
        case 0xA7:  /* GETTIMEOFDAY */
           dogettimeofday(drac);
           return;
        }

        if(ukd->truss) {
            sprintf(ukd->trussbuf,"%2d - ",ukd->threadid+1);
            outsok(ukd->trussbuf);
            sprintf(ukd->trussbuf,"SYS_TRAP(%X) in %X out %X\n",
                 ocm,vin,vout);
            outsok(ukd->trussbuf);
        }

        setreturn(drac,vout,0);
        return;
     }

     dohardware(drac,oc);   /* handle a hardware trap */
}


/****************************************************************************
    MYEXIT  Prelude to exit

    Puts memory back to simple form for exit()
    sells extra node

    When children are forked, this routine must track them down and
    kill them.

    When LWPs are supported this routine must track them down and
    kill them.

    exit()

    uses k0,k1
****************************************************************************/
myexit(retcode,slave)
    int retcode;
    int slave;
{
    JUMPBUF;
    UINT32 rc;

    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+0);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_SwapMemory);XB(0x8030E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SB_DestroyNode);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    if(!slave) {
       {OC(Node_Fetch+NODESIK);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* destroy the console */

       {OC(Node_Fetch+2);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(SB_DestroyPage);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* keeper shared page */
       {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(SB_DestroyNode);XB(0x8040F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Fetch+NODEALARMWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* this will cause waiter to wake up and die */
       {OC(Node_Fetch+NODEPOLLWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* this will cause waiter to wake up and die */
       {OC(Node_Fetch+NODEITIMERWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* this will cause waiter to wake up and die */
       {OC(SB_DestroyNode);XB(0x80407000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    }
    {OC(Domain_GetKeeper);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }   /* destroy keeper */

    exit(retcode);
}

/****************************************************************************
    MYFORK  Prelude to fork()

    Puts memory back to simple form at factory() for fork()
    Builds memory up for child
    Puts memory back for parent

    fork assumes domkey,dc,meter,psb and k0,k1,k2 as scratch

    The child shares the extra node "node"

    Returns 1 if worked  0 if no space
****************************************************************************/
myfork()
{
    JUMPBUF;
    UINT32 rc;

    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+NODEMYMEM);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+0);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   // put memory back for fork
    if(!(rc=fork())) {  // child, must build up memory and return 0;
        {OC(SB_CreateNode);XB(0x00400000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_MakeNodeKey);PS2(&(ndb7),sizeof(ndb7));XB(0x04F00000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Swap+0);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Fetch+NODEMYMEM);XB(0x00700000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Fetch+MEMUMEM);XB(0x00E00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Swap+MEMUMEM);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

        return 0;
    }
    // parent, put memory back and return 1

    {OC(Node_Fetch+NODEMYMEM);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

    if(rc > 1) {
       return 0;
    }

    return 1;
}

/****************************************************************************
    OUTSOK   debug print

    Uses  k0
****************************************************************************/
outsok(buf)
    char *buf;
{
    JUMPBUF;
    UINT32 rc;

    if(0) {  /* doesn't work with CONSDEV */
       {OC(Node_Fetch+NODESOK);XB(0x00700000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);PS2(buf,strlen(buf));XB(0x04C00000);RC(rc);NB(0x0810000C);cjcc(0x08000000,&_jumpbuf); }
       {OC(Node_Swap+NODESOK);XB(0x8070C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
    }
    else {
       {OC(COMPCONSOLE);XB(0x00000000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);XB(0x00C00000);RC(rc);NB(0x08400C00);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);PS2(buf,strlen(buf));XB(0x04C00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
    }
}

/****************************************************************************
    SETERROR  - set error return from syscall
****************************************************************************/
seterror(drac,o0)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 o0;
{
     char buf[256];

     if(ukd->truss) {
        if(*ukd->trussbuf) {  /* if no message, no return code */
           sprintf(buf," ERROR %lX\n",o0);
           strcat(ukd->trussbuf,buf);
        }
     }
     drac->Control.PSR |= 0x00100000;   // carry on
     drac->Regs.o[0]=o0;
     drac->Control.PC = drac->Control.NPC;
     drac->Control.NPC = drac->Control.NPC+4;
}
/****************************************************************************
    SETRETURN - set return value from syscall
****************************************************************************/
setreturn(drac,o0,o1)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 o0,o1;
{
     char buf[256];

     if(ukd->truss) {
        if(*ukd->trussbuf) {  /* if no message, no return code */
           sprintf(buf," =  %lX(%d) %lX\n",o0,o0,o1);
           strcat(ukd->trussbuf,buf);
        }
     }

     drac->Control.PSR &= ~(0x00100000);   // carry off
     drac->Regs.o[0]=o0;
     drac->Regs.o[1]=o1;
     drac->Control.PC = drac->Control.NPC;
     drac->Control.NPC = drac->Control.NPC+4;
}
/****************************************************************************
    SETRETURNQUIET - set return value from syscall
****************************************************************************/
setreturnquiet(drac,o0,o1)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 o0,o1;
{
     drac->Control.PSR &= ~(0x00100000);    /* carry off */
     drac->Regs.o[0]=o0;
     drac->Regs.o[1]=o1;
     drac->Control.PC = drac->Control.NPC;
     drac->Control.NPC = drac->Control.NPC+4;
}
/****************************************************************************
    GETUSER - get data from user

    User address is represented in user space
****************************************************************************/
getuser(from, to, len)
    char *from,*to;
    int len;
{
    memcpy(to,from+(UINT32)usermem,len);
}
/****************************************************************************
    PUTUSER - put data into user memory

    User address is represented in user space
****************************************************************************/
putuser(from, to, len)
    char *from;
    int to,len;
{
    memcpy(usermem+to,from,len);
}

/****************************************************************************
    LOOKUP   - look up file name

    If name begins with "/" use COMPONENT 1 directory (Root)
    If name does not begin with "/" first look in NODEDIRECTORY
       then look in COMPONENT 1 of udom

    names beginning with ./ are treated as if there is no prefix

    For a Unix lookup the last segment may be a directory even if
    it does not end with /.   Unix does not include the / as part of
    the name.  Pacific does.   Pacific allows x and x/ as distinct names.

    When the lookup of the last segment fails it is repeated with a
    trailing / to see if it is a directory lookup.

    Return: object
    Return: um is filled in based on object

      SIDEEFFECTS:   Sets ukd->curdevice  to DEVROOT, DEVHOME, or DEVCOMP
                     Setd ukd->lookuperror to LOOKUPNODIR or LOOKUPNOFILE

    return = 0 if not found
    return = 1 if file found
    return = 2 if directory found
    return = 3 if /proc/pid
    return = 4 if /proc/pid/usage
    return = 5 if /proc/pid/status
    return > 0x10000000 if segment file system (difference is offset in segment)
             0x10000000 if in root
             0x20000000 if in current
****************************************************************************/
lookup(name,um)
    char *name;
    struct FS_UnixMeta *um;
{
    JUMPBUF;
    UINT32 rc;
    char buf[256];
    struct direntry *de;

    if(!strncmp(name,"/proc/",6)) {
        int len;

        len=strlen(name);  /* must be 6 or more */

        if(!strcmp(&name[len-5],"usage")) {
            return 4;
        }
        if(!strcmp(&name[len-6],"status")) {
            return 5;
        }
        return 3;
    }

    if (*name ==  '/') {

       if(ukd->rootslot) {                                  /* using a zip filesystem */
           ukd->curdevice=DEVROOT;
           de=(struct direntry *)zlookupname(ukd->root,name+1);
           if(!de) {
               return 0;
           }

           um->mode=de->mode;
           um->inode=de->inode;
           um->length=de->length;
           um->userid=0;
           um->groupid=0;

           return (0x10000000 + de->offset);
       }
       {OC(Node_Fetch+COMPDIRECTORY);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
       rc=lookitup(name+1,DEVROOT,um);
       return rc;
    }
    else {
       if(ukd->currentslot) {   /* current directory is a zip filesystem */
          ukd->curdevice=DEVHOME;
          if((*name == '.') && (*(name+1) == '/')) {
             de=(struct direntry *)zlookupname(ukd->current,name+2);
          }
          else {
             de=(struct direntry *)zlookupname(ukd->current,name);
          }
          if(!de) {
             return 0;
          }

          um->mode=de->mode;
          um->inode=de->inode;
          um->length=de->length;
          um->userid=0;
          um->groupid=0;

          return (0x20000000 + de->offset);
       }

/* because ukd->currentslot is 0 this directory if it exists is not a zip segment */

       {OC(Node_Fetch+NODEDIRECTORY);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
       if((*name == '.') && (*(name+1) == '/')) {
           rc=lookitup(name+2,DEVHOME,um);
       }
       else {
           rc=lookitup(name,DEVHOME,um);
       }
       if(!rc) {
          {OC(Domain_GetKey+comp);XB(0x00800000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+COMPDIRECTORY);XB(0x00B00000);RC(rc);NB(0x0880B000);cjcc(0x00000000,&_jumpbuf); }

          {OC(KT);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
          if (rc == KT+1) {
             return 0;   /* leave LOOKUPERROR the same */
          }
          if(rc != TDO_NSAKT) { /* must be a segment */
             if(!ukd->componentslot) {  /* not yet assigned */
                 /* this directory has the same name as the current directory..ie "home" */
                 makezipdir(&ukd->component,&ukd->componentslot,&ukd->componentaddress,"home",20000);
             }
             if(!ukd->componentslot) return 0;

             ukd->curdevice=DEVCOMP;
             if((*name == '.') && (*(name+1) == '/')) {
                de=(struct direntry *)zlookupname(ukd->component,name+2);
             }
             else {
                de=(struct direntry *)zlookupname(ukd->component,name);
             }
             if(!de) {
                return 0;
             }

             um->mode=de->mode;
             um->inode=de->inode;
             um->length=de->length;
             um->userid=0;
             um->groupid=0;

             return (0x30000000 + de->offset);
          }

          if((*name == '.') && (*(name+1) == '/')) {
              rc=lookitup(name+2,DEVCOMP,um);
          }
          else {
              rc=lookitup(name,DEVCOMP,um);
          }
       }
       return rc;
    }
}
/****************************************************************************
    LOOKITUP  - look up name starting with directory in K0 (no links)
      return 0 no
      return 1 file
      return 2 directory

      Starting KEY object
      Return   KEY object

      SIDEEFFECTS:   Sets ukd->curdevice  to DEVROOT, DEVHOME, or DEVCOMP
                     Setd ukd->lookuperror to LOOKUPNODIR or LOOKUPNOFILE

      USES   K1
****************************************************************************/
lookitup(hname,device,um)
    char *hname;
    int device;
    struct FS_UnixMeta *um;
{
    JUMPBUF;
    UINT32 rc;
    char *ptr,*portion;
    char rcname[260];
    char *name;
    char buf[256];

    strcpy(ukd->name,hname);
    name=ukd->name;

    ukd->curdevice=device;
    ukd->lookuperror=0;

    if(!strcmp(name,"..") || !strcmp(name,"../")) {  /* looking for root */
       {OC(Node_Fetch+COMPDIRECTORY);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
       ukd->curdevice=DEVROOT;
       {OC(TDO_ReadUserData);XB(0x00B00000);RC(rc);RS2(&(*um),sizeof(*um));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
       return 2;
    }

    while(ptr=strchr(name,'/')) {  /* found a portion */
       portion=name;
       *ptr=0;

       name=ptr+1;
       {OC(KT);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
       if(rc != TDO_NSAKT) {
           ukd->lookuperror=LOOKNODIR;
           return 0;
       }
       if(strlen(portion) > 255) {
           ukd->lookuperror=LOOKNODIR;
           return 0;
       }
       strcpy(rcname+1,portion);
       strcat(rcname+1,"/");
       *rcname=strlen(rcname+1);
       {OC(TDO_GetEqual);PS2(rcname,*rcname+1);XB(0x04B00000);RC(rc);NB(0x0880B000);cjcc(0x08000000,&_jumpbuf); }
       if(rc != 1) {
           ukd->lookuperror=LOOKNODIR;
           return 0;
       }
    }

    if(*name) {  // looking up a file name
       if(strlen(name) > 255) {
           return 0;
       }
       strcpy(rcname+1,name);
       *rcname=strlen(name);

       {OC(Node_Swap+NODELASTDIR);XB(0x8070B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
       strcpy(ukd->lastlookupname,rcname);

       {OC(TDO_GetEqual);PS2(rcname,*rcname+1);XB(0x04B00000);RC(rc);NB(0x0880E000);cjcc(0x08000000,&_jumpbuf); }

       if(rc != 1) {  // no record or no key.   try with trailing slash
          strcat(rcname+1,"/");
          *rcname=strlen(name)+1;
          {OC(TDO_GetEqual);PS2(rcname,*rcname+1);XB(0x04B00000);RC(rc);NB(0x0880B000);cjcc(0x08000000,&_jumpbuf); }
          if(rc == 1) {  /* yep, a directory */
             {OC(TDO_ReadUserData);XB(0x00B00000);RC(rc);RS2(&(*um),sizeof(*um));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
             if(rc) {
                 ukd->lookuperror=LOOKNODIR;
                 return 0;
             }
             return 2;
          }
          ukd->lookuperror=LOOKNOFILE;
          return 0;   /* still not here */
       }
       else {   /* rc=1 record with key and no trailing slash */
          {OC(Domain_GetKey+k1);XB(0x00300000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
          {OC(FS_GetMetaData);XB(0x00B00000);RC(rc);RS2(&(*um),sizeof(*um));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
          if(rc) {
              ukd->lookuperror=LOOKNOFILE;
              return 0;
          }
          return 1;
       }
    }
    {OC(TDO_ReadUserData);XB(0x00B00000);RC(rc);RS2(&(*um),sizeof(*um));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
    if(rc) {
        ukd->lookuperror=LOOKNODIR;
        return 0;
    }
    return 2;  /* looked for a name ending in slash */
}
/****************************************************************************
    NEXTDOOR

    Allocates a door

    returns door_info_t
****************************************************************************/
door_info_t *nextdoor()
{
    int i;

    for(i=0;i<MAXDOORS;i++) {
       if(ukd->doors[i].di_target == 0) {
            return &ukd->doors[i];
       }
    }
    return 0;
}
/****************************************************************************
    NEXTFILEHANDLE

    Allocates a file handle table entry

    returns file handle index or -1
****************************************************************************/
int nextfilehandle()
{
    int i;
    for(i=0;i<MAXFILEHANDLES;i++) {
       if(ukd->filehandles[i].filet == (struct filetablee *)0xFFFFFFFFF) {
            return i;
       }
    }
    return -1;
}

/****************************************************************************
    FREEFILEHANDLE

****************************************************************************/
freefilehandle(fh)
    int fh;
{
    ukd->filehandles[fh].flags = 0;
    ukd->filehandles[fh].filet = (struct filetablee *)0xFFFFFFFF;
}

/****************************************************************************
    NEXTFILETABLE

    allocates a file table entry

    returns filetable pointer or 0
****************************************************************************/
struct filetablee *nextfiletable()
{
    int i;
    for(i=0;i<MAXFILES;i++) {
       if(ukd->filetable[i].address == (char *)0xFFFFFFFF) {
           return &ukd->filetable[i];
       }
    }
    return 0;
}
/****************************************************************************
    NEXTSLOT

    Slots are allocated for mapping files.  Each slot is 256 megabytes.

    returns next available mapping slot or 0
****************************************************************************/
int nextslot(filet)
    struct filetablee *filet;
{
    int i;
    for(i=0;i<MAXSLOTS;i++) {
       if(ukd->slots[i] == 0) {
          ukd->slots[i]=filet;
          return i;
       }
    }
    return 0;
}
/****************************************************************************
    NEXTLOADERMAP

    Loader locations are allocated in units of 0x10000 (65K) bytes.  At
    this time these locations are not reused after an unmap.  While loading
    libraries the unmap is usually used to unmap the bss portions and replace
    them with mappings of /dev/zero.   For this keeper the unmap is ignored
    as the mapping function copies files so it is not necessary to release
    one mapping before overlaying the bits with another mapping.  MEMCPY does
    a fine job.

    returns next available address or 0
****************************************************************************/
unsigned long nextloadermap(size)
    int size;
{
    unsigned long address;

    size = (size + 65535) & 0xFFFF0000;

    ukd->mapaddress = ukd->mapaddress - size;
    return ukd->mapaddress;
}
/****************************************************************************
    FREELOADERMAP

    Not currently used.  If used then NEXTLOADERMAP will have to look
    for holes of the right size

    returns next available address or 0
****************************************************************************/
freeloadermap(address,size)
    unsigned long address;
    int size;
{
}
/****************************************************************************
     FILLSTAT

     The Loader checks the inodenumber/devicenumber field to detect
     duplicate libraries under different names.   It does not want
     to hash duplicate symbols.   A better way to set the inode would
     be to use a hash of the file name.

     fills stat buffer based on size
****************************************************************************/
fillstat(st,size,inode,mode)
     struct stat *st;
     int inode;
     long long size;
     int mode;
{
     st->st_dev=ukd->curdevice;
     st->st_ino=inode;
//     st->st_mode = 0x81FF;
     st->st_mode = mode;
     st->st_nlink = 1;
     if(mode & 0x4000) {
//         st->st_mode = 0x45ED;
         st->st_nlink = 10;
     }
     st->st_size=size;
     st->st_blksize=PAGESZ;
     st->st_blocks = (st->st_size + 511) / 512;
     strcpy(st->st_fstype,"UFS");
}

/****************************************************************************
     FILLSTAT64

     same as fillstat but using the 64 bit stat buffer.

     fills stat buffer based on size
****************************************************************************/
fillstat64(st,size,inode,mode)
     struct stat64 *st;
     long long size;
     int inode;
     int mode;
{
     st->st_dev=ukd->curdevice;
     st->st_ino=inode;
//     st->st_mode = 0x81FF;
     st->st_nlink = 1;
     if(mode & 0x4000) {
//         st->st_mode = 0x45ED;
         st->st_nlink = 10;
     }
     st->st_size=size;
     st->st_blksize=PAGESZ;
     st->st_blocks = (st->st_size + 511) / 512;
     strcpy(st->st_fstype,"UFS");
}

/***************************************************************************
     CANFREEZE
***************************************************************************/
canfreeze()
{
     int i;
     JUMPBUF;
     UINT32 rc;

     for(i=0;i<MAXTHREADS;i++) {
        if((i != ukd->threadid) && (ukd->uthreads[i].status == THREADRUNNING)) {
/* lets stop this and put it away  temporarily make this thread "current" */
              {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+i);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* the domain key */
              {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* don't need the start key */
              {OC(Domain_GetSPARCStuff);XB(0x00800000);RS2(&(ukd->uthreads[i].drac),sizeof(ukd->uthreads[i].drac));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
              putawaywindows(&ukd->uthreads[i].drac);
        }
     }
     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+ukd->threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }  /* restore udom */
     return 1;   /* allow as how we can now freezedry the world */
}
/****************************************************************************
     DOFREEZETHAW

     handles the freezedry and thaw operations
     as a result of a freezedry system call or thaw system call

     This is called from dotrap only as a normal trap

     Thaw has a long sequence to complete
     Freeze uses "dothefreeze" which does all the freeze steps
     Dothefreeze is also called by the Open Freezedry.class hack

****************************************************************************/
dofreezethaw(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     unsigned long long value;
     int i,slot;
     struct filetablee *filet;
     struct FS_UnixMeta um;

     if(args[0] == 2) {                   /* thaw */

         {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Node_Swap+0);XB(0x80F08000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   /* thaw is always from lwp 0 */

/***************************************************************************
         What happens here.

         remember that we are running as thread 0

         0 remap files
         1 create lwps for all the uthread structures not AVAILABLE
         2 create timer objects if needed
         3 pick up the DRAC for the thread that did the freeze (RUNNING)
         4 abandon the resume key for lwp 0 (that is doing the thaw)
         5 re-create the resume key
         6 set the return code and return
***************************************************************************/

/* remap files */

         if(ukd->rootslot) {   /* file system component is a segment, mapped in this slot */

/* we will need to confirm that component 1 is still a segment, and even perhaps the same segment */

            {OC(COMPDIRECTORY);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+ukd->rootslot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
         }

         if(ukd->currentslot) {  /* file system for current is a segment mapped in this slot */

/* we might need to confirm that we were passed the same segment on the thaw request */

            {OC(Node_Fetch+NODEDIRECTORY);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+ukd->currentslot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
         }

         if(ukd->componentslot) {  /* file system for component is a segment mapped in this slot */
            {OC(Domain_GetKey+comp);XB(0x00800000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }   /* component node */
            {OC(Node_Fetch+COMPDIRECTORY);XB(0x00B00000);RC(rc);NB(0x0880B000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+ukd->componentslot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
         }

         for(i=0;i<MAXFILES;i++) {
            filet=&ukd->filetable[i];

            if(filet->address) {
                continue;   /* not used or no mapping */
            }

            if(filet->flags & SLOTSHARED) {
                continue;   /* single directory slot already mapped */
            }

            if(!lookup(filet->name,&um)) {   /* re-get object */
                continue;  /* leave address 0 */
            }

            if(um.mode & 0x2000) {  /* character special key must be in Device directory   */
                filet->flags &= ~DEVICEKEYINSLOT;
                continue;
            }
            slot=filet->slot;
            filet->address = (char *)(slot << 28);
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+slot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
         }

/* create lwps */

         for(i=1;i<MAXTHREADS;i++) {
            if(ukd->uthreads[i].status != THREADAVAILABLE) {
                makelwp(i);          /* get one ready to go */
            }
         }
/* create timer objects */
         if(ukd->haveitimertimer) {
             value = ukd->realtimer.it_value.tv_sec;
             value = value*1000000 + ukd->realtimer.it_value.tv_usec;
             if(value) {
                 {OC(Node_Fetch+NODEITIMERWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Wait_SetInterval);PS2(&(value),sizeof(value));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
                 makeitimertimer();
             }
             else {
                 ukd->haveitimertimer=0;
             }
         }
         if(ukd->havealarmtimer) {    /* had one at freezedry */
             if(ukd->microseconds) {
                 {OC(Node_Fetch+NODEALARMWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Wait_SetInterval);PS2(&(ukd->microseconds),sizeof(ukd->microseconds));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
                 {OC(Wait_ShowTOD);XB(0x00F00000);RC(rc);RS2(&(ukd->alarmtod),sizeof(ukd->alarmtod));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
                 makealarmtimer();
                 ukd->havealarmtimer=1;
             }
             else {
                 ukd->havealarmtimer=0;
             }
         }

/* we can't have a poll timer running at freezedry.  One may actually be running but */
/* it is not significant and will be allowed to die.  This is because we can't request */
/* a freezedry at the same time that we are waiting on a poll */

         ukd->havepolltimer = 0;   /* so indicate that we need a new one */

/* restore DRAC from lwp that did the freeze */
         *drac = ukd->uthreads[ukd->freezeid].drac;
         ukd->threadid=ukd->freezeid;

/* make new resume key, abandon lwp 0 resume key, if lwp 0 not running then some form of sleep */
         {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Node_Fetch+ukd->freezeid);XB(0x00F00000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x08802000);cjcc(0x00000000,&_jumpbuf); }
/* set the return code */
         if(ukd->freezerc) {
               seterror(drac,ukd->freezerc);
         }
         else {
            setreturn(drac,ukd->freezerc,0);
         }
/****************************************************************************
    Now we look for any other lwps that are NOT the one being restarted
    and is in the THREADRUNNING state.  These must be restarted
****************************************************************************/
         for(i=0;i<MAXTHREADS;i++) {
             if((i != ukd->threadid) && (ukd->uthreads[i].status == THREADRUNNING)) {
                 {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Node_Fetch+i);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }   /* the domain key */
                 {OC(Domain_MakeBusy);XB(0x00D00000);RC(rc);NB(0x0880E000);cjcc(0x00000000,&_jumpbuf); }
                 {OC(Domain_ResetSPARCStuff);PS2(&(ukd->uthreads[i].drac),sizeof(ukd->uthreads[i].drac));XB(0x14D0000E); }

                 {fj(0x08000000,&_jumpbuf); }
             }
         }

/* restart the lwp that did the freezedry */
         return;
     }
     else {                                /* some freeze request */
               /* args[0] = 1 means special freeze not yet defined */

         if(!canfreeze()) {   /* puts away all windows in LWPs */
             seterror(drac,EINVAL);
             return;
         }

         ukd->uthreads[ukd->threadid].drac=*drac;
         ukd->freezerc=0;
         dothefreeze(drac,args);   /* does not return */
     }
}
/****************************************************************************
     DOTHEFREEZE - the work of freezedry
****************************************************************************/
dothefreeze(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int i;
     JUMPBUF;
     UINT32 rc;

// mark files as unmapped

     if(ukd->truss) {
        outsok("DoTheFreeze\n");
     }

     ukd->freezeid=ukd->threadid;

/* slots will be remapped by thaw at same locations */
//     for(i=2;i<11;i++) { // make slots available
//        ukd->slots[i]=0;
//     }

     for(i=0;i<MAXFILES;i++) {  // show files unmapped
        if(ukd->filetable[i].address != (char *)0xFFFFFFFF) { /* only zap in use */
             ukd->filetable[i].address=0;
        }
     }

     *(unsigned long *)(usermem+ukd->frozenaddr) = 1; // mark as frozen

     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+ukd->threadid);XB(0x00F00000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* make sure we are using master domain */

     {OC(Domain_GetKey+comp);XB(0x00800000);NB(0x00809000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+COMPCOPY);XB(0x00900000);NB(0x00809000);cjcc(0x00000000,&_jumpbuf); }   // copy key
     {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }     // his prompt spacebank
     {OC(Domain_GetKey+sb);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(FactoryC_Copy);XB(0xA09040F0);RC(rc);NB(0x08809000);cjcc(0x00000000,&_jumpbuf); }  // new factory
     if(rc) {
         seterror(drac,0x0000ffff);
         return;
     }
     {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }    // vcs
     {OC(VCS_Freeze);XB(0x80F0A000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }   // new .program
     {OC(FactoryB_InstallFactory+17);PS2(&(ukd->restartaddr),sizeof(ukd->restartaddr));XB(0x8490F000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
     {OC(FactoryB_MakeRequestor);XB(0x00900000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }     // new requestor key
     {OC(Domain_GetKey+caller);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }     // return here
     {OC(1);XB(0xC0E0D900); }
     {fj(0x00000000,&_jumpbuf); }

     for(i=0;i<MAXTHREADS;i++) {   /* kill all LWPs */
        if(ukd->uthreads[i].status != THREADAVAILABLE) {  /* including caller */
            {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+i);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
            {OC(DC_DestroyDomain);XB(0xC0E08D00);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        }
     }
     myexit(0,0);
}

/****************************************************************************
     UCONTECT2DRAC - convert a context -> to a {drac, sigmask}
****************************************************************************/
ucontext2drac(uc,drac,sa_mask)
     ucontext_t *uc;
     struct Domain_SPARCRegistersAndControl *drac;
     sigset_t *sa_mask;
{
     mcontext_t *mc;
     UINT32 *sp;
     int i;
     char buf[256];

/* This code ignores flags, assuming a complete context */
/* This code ignores link, assuming (for the moment) non-stacked interrupts */

    *sa_mask = uc->uc_sigmask;

#ifdef xx
sprintf(buf,"UCONTEXT2DRAC  %X %X %X %X\n",
       uc->uc_sigmask.__sigbits[0],uc->uc_sigmask.__sigbits[1],
       uc->uc_sigmask.__sigbits[2],uc->uc_sigmask.__sigbits[3]);
outsok(buf);
#endif

/* We don't seem to care about the stack information going this way */
/* The local and i registers are on the stack after the stack is restored */
/* the flush windows at the trap may have put them there */

    mc = &uc->uc_mcontext;
/* restore from mc.gregs (PSR, PC, NPC, Y, G1 -> O7 ) */

    drac->Control.PC  = mc->gregs[REG_PC];
    drac->Control.NPC = mc->gregs[REG_nPC];
    drac->Control.PSR = mc->gregs[REG_PSR];
    drac->Regs.g[0]   = mc->gregs[REG_Y];
    for(i=1;i<8;i++) {
       drac->Regs.g[i]= mc->gregs[REG_Y+i];
    }
    for(i=0;i<8;i++) {
       drac->Regs.o[i]= mc->gregs[REG_O0+i];
    }
/* now sp = o6 the l and i registers were put here by setcontext or signal */

    sp = (UINT32 *)(usermem + drac->Regs.o[6]);
    for(i=0;i<8;i++) {
       drac->Regs.l[i] = sp[i];
       drac->Regs.i[i] = sp[i+8];
    }
/*  NEED TO DO FLOATING POINT HERE.  No current FREG fetch/store for the domain key */
}

/****************************************************************************
     DRAC2SIGINFO - generates a siginfo structure from a drac
****************************************************************************/
drac2siginfo(drac,si,signo)
     struct Domain_SPARCRegistersAndControl *drac;
     siginfo_t *si;
{
     si->si_signo = signo;
     si->si_code = 1;
     si->si_errno = 0;

     si->__data.__fault.__addr = (void *)drac->Control.TRAPEXT[0];
     si->__data.__fault.__trapno = 0;
     si->__data.__fault.__pc = 0;
}

/****************************************************************************
     CHECKPENDING
****************************************************************************/
checkpending(signo)
     int signo;
{
     if(sigismember(&ukd->sa_pending,signo) &&
            !sigismember(&ukd->uthreads[ukd->threadid].sa_mask,signo)) {
         sigdelset(&ukd->sa_pending,signo);
         return 1;
     }
     return 0;
}

/****************************************************************************
     STACKCHECK
****************************************************************************/
stackcheck(drac)
     struct Domain_SPARCRegistersAndControl *drac;
{
     if(drac->Regs.o[6] < STACKBOTTOM) {
        return 1;
     }
     return 0;
}
/****************************************************************************
     DRAC2UCONTEXT - convert a {drac, sigmask} -> to a context
****************************************************************************/
drac2ucontext(drac,uc,sa_mask)
     struct Domain_SPARCRegistersAndControl *drac;
     ucontext_t *uc;
     sigset_t *sa_mask;
{
     mcontext_t *mc;
     UINT32 *sp;
     int i;
     char buf[256];

     uc->uc_sigmask = *sa_mask;
#ifdef xx
sprintf(buf,"DRAC2UCONTEXT  %X %X %X %X\n",
       uc->uc_sigmask.__sigbits[0],uc->uc_sigmask.__sigbits[1],
       uc->uc_sigmask.__sigbits[2],uc->uc_sigmask.__sigbits[3]);
outsok(buf);
#endif
     uc->uc_flags = 0x27;
     uc->uc_link  = 0;
     i = drac->Regs.o[6];
     uc->uc_stack.ss_sp = (void *)i;
     uc->uc_stack.ss_flags = 0;
     uc->uc_stack.ss_size = (((0x0F000000 - i) + (PAGESZ-1))/PAGESZ) * PAGESZ;

     mc= &uc->uc_mcontext;

     mc->gregs[REG_PC]   = drac->Control.PC;
     mc->gregs[REG_nPC]  = drac->Control.NPC;
     mc->gregs[REG_PSR]  = drac->Control.PSR;
     mc->gregs[REG_Y]    = drac->Regs.g[0];

     for(i=1;i<8;i++) {
         mc->gregs[REG_Y+i] = drac->Regs.g[i];
     }
     for(i=0;i<8;i++) {
         mc->gregs[REG_O0+i]= drac->Regs.o[i];
     }

     sp = (UINT32 *)(usermem + drac->Regs.o[6]);
     for(i=0;i<8;i++) {
        sp[i] = drac->Regs.l[i];
        sp[i+8] = drac->Regs.i[i];
     }
/*  NEED TO DO FLOATING POINT HERE.   No current FREG fetch/store for the domain key */
}


/****************************************************************************
     DOHARDWARE - hardware fault handler
****************************************************************************/
dohardware(drac,oc)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 oc;
{
     char buf[256];
     int  signo,trapcode;
     struct sigaction *sa;
     void (*action)();
     int i;
     siginfo_t si;

     trapcode=drac->Control.TRAPCODE;
     signo = sigtran[trapcode & 0x7f];

     if(ukd->truss) {
         sprintf(buf,"FAULT: oc=%X, PC %8lX TRAPCODE %X[%X %X] signo %d\n",
             oc,drac->Control.PC,drac->Control.TRAPCODE,drac->Control.TRAPEXT[0],
             drac->Control.TRAPEXT[1],signo);
         outsok(buf);

         sprintf(buf,"REGS:G %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
             drac->Regs.g[0],drac->Regs.g[1],drac->Regs.g[2],drac->Regs.g[3],
             drac->Regs.g[4],drac->Regs.g[5],drac->Regs.g[6],drac->Regs.g[7]);
         outsok(buf);
         sprintf(buf,"REGS:I %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
             drac->Regs.i[0],drac->Regs.i[1],drac->Regs.i[2],drac->Regs.i[3],
             drac->Regs.i[4],drac->Regs.i[5],drac->Regs.i[6],drac->Regs.i[7]);
         outsok(buf);
         sprintf(buf,"REGS:L %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
             drac->Regs.l[0],drac->Regs.l[1],drac->Regs.l[2],drac->Regs.l[3],
             drac->Regs.l[4],drac->Regs.l[5],drac->Regs.l[6],drac->Regs.l[7]);
         outsok(buf);
         sprintf(buf,"REGS:O %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
             drac->Regs.o[0],drac->Regs.o[1],drac->Regs.o[2],drac->Regs.o[3],
             drac->Regs.o[4],drac->Regs.o[5],drac->Regs.o[6],drac->Regs.o[7]);
         outsok(buf);
     }
     sa = &ukd->sigacttable[signo];

     if(sigismember(&ukd->uthreads[ukd->threadid].sa_mask,signo)) {  /* masked ? */

         if((signo == SIGKILL) || (signo == SIGSTOP)) {   /* can't mask these */
             sprintf(buf,"SIGKILL or SIGSTOP %d\n",signo);
             outsok(buf);

             doexit();
             return;  /* in case an lwp */
         }
         sigaddset(&ukd->sa_pending,signo); /* add to pending */
         return;
     }

     action=sa->sa_sigaction;
     if(action == SIG_IGN) {   /* check for ignore */
         return;
     }
     if(action == SIG_DFL) {   /* check for ignore */
        for(i=0;i<MAXSIGIGNORE;i++) {
           if(sigIgnore[i] == signo) {
               return;
           }
        }
        sprintf(buf,"DEATH by SIGNAL %d\n",signo);
        outsok(buf);
        doexit();   /* only other choice for default action */
     }

     drac2siginfo(drac,&si,signo);
     dosignal(drac,signo,&si,&ukd->uthreads[ukd->threadid].sa_mask,ukd->threadid);
}

/****************************************************************************
    DOTRAP    the main trap handler

    INPUT:   The domain trap information in *drac and the domain's memory
    OUTPUT:  The response in *drac and the domain's memory
****************************************************************************/
dotrap(drac)
     struct Domain_SPARCRegistersAndControl *drac;
{
     JUMPBUF;
     UINT32 rc;
     int code,i;
     UINT32 args[6];
     char buf[256];
     struct Domain_SPARCRegData regs;

// sprintf(buf,"TRAP: g1=%X o0...o5 %X %X %X %X %X %X\n",
//       drac->Regs.g[1],drac->Regs.o[0],drac->Regs.o[1],drac->Regs.o[2],
//       drac->Regs.o[3],drac->Regs.o[4],drac->Regs.o[5]);
// outsok(buf);

     code=drac->Regs.g[1];
     if(code) {
         for(i=0;i<6;i++) args[i]=drac->Regs.o[i];
     }
     else {
         code=drac->Regs.o[0];
         for(i=0;i<5;i++) args[i]=drac->Regs.o[i+1];
     }

// put calls for SYS_function here

     if(ukd->truss) {
        strcpy(ukd->trussbuf,"");
     }

     if(ukd->uthreads[ukd->threadid].status == THREADRESTARTED) {
        ukd->uthreads[ukd->threadid].status = THREADRUNNING;
//        if(ukd->truss) {
//            sprintf(ukd->trussbuf,"SYSCALL %d RESTARTED",code);
//        }
//        seterror(drac,EINTR);
//        if(ukd->truss) {
//            outsok(ukd->trussbuf);
//        }
//        return;
     }

     switch(code) {
     case SYS_getuid:
          dogetuid(drac,args);
          break;
     case SYS_stat:
          dostat(drac,args);
          break;
     case SYS_stat64:
          dostat64(drac,args);
          break;
     case SYS_fstat:
          dofstat(drac,args);
          break;
     case SYS_mmap:
          dommap(drac,args);
          break;
     case SYS_munmap:
          domunmap(drac,args);
          break;
     case SYS_memcntl:
          domemcntl(drac,args);
          break;
     case SYS_sysconfig:
          dosysconfig(drac,args);
          break;
     case SYS_ioctl:
          doioctl(drac,args);
          break;
     case SYS_llseek:
          dollseek(drac,args);
          break;
     case SYS_lseek:
          dolseek(drac,args);
          break;
     case SYS_open:
          doopen(drac,args);
          break;
     case SYS_open64:
          doopen(drac,args);
          break;
     case SYS_fcntl:
          dofcntl(drac,args);
          break;
     case SYS_access:
          doaccess(drac,args);
          break;
     case SYS_close:
          doclose(drac,args);
          break;
     case SYS_write:
          dowrite(drac,args);
          break;
     case SYS_poll:
          dopoll(drac,args);
          break;
     case SYS_getpid:
          dogetpid(drac,args);
          break;
     case SYS_kill:
          dokill(drac,args);
          break;
     case SYS_exit:
          doexit();
          break;
     case SYS_systeminfo:
          dosysteminfo(drac,args);
          break;
     case SYS_mprotect:
          domprotect(drac,args);
          break;
     case SYS_brk:
          dobrk(drac,args);
          break;
     case SYS_sigaction:
          dosigaction(drac,args);
          break;
     case SYS_sigprocmask:
          dosigprocmask(drac,args);
          break;
     case SYS_fstat64:
          dofstat64(drac,args);
          break;
     case SYS_read:
          doread(drac,args);
          break;
     case SYS_context:
          docontext(drac,args);
          break;
     case SYS_setitimer:
          dosetitimer(drac,args);
          break;
     case SYS_getitimer:
          dogetitimer(drac,args);
          break;
     case SYS_sigsuspend:
          dosigsuspend(drac,args);
          break;
     case SYS_signotifywait:
          dosignotifywait(drac,args);
          break;
     case SYS_sigpending:
          dosigpending(drac,args);
          break;
     case SYS_time:
          dotime(drac,args);
          break;
     case SYS_alarm:
          doalarm(drac,args);
          break;
     case SYS_getrlimit:
          dogetrlimit(drac,args);
          break;
     case SYS_pathconf:
          dopathconf(drac,args);
          break;
/* Door support */
     case SYS_door:
          dodoor(drac,args);
          break;
/* LWP Support */
     case SYS_schedctl:
          dolwpschedctl(drac,args);
          break;
     case SYS_lwp_self:
          dolwpself(drac,args);
          break;
     case SYS_lwp_create:
          dolwpcreate(drac,args);
          break;
     case SYS_lwp_continue:
          dolwpcontinue(drac,args);
          break;
     case SYS_lwp_cond_wait:
          dolwpcondwait(drac,args);
          break;
     case SYS_lwp_cond_signal:
          dolwpcondsignal(drac,args);
          break;
     case SYS_lwp_cond_broadcast:
          dolwpcondbroadcast(drac,args);
          break;
     case SYS_lwp_sema_wait:
          dolwpsemawait(drac,args);
          break;
     case SYS_lwp_sema_post:
          dolwpsemapost(drac,args);
          break;
     case SYS_lwp_mutex_lock:
          dolwpmutexlock(drac,args);
          break;
     case SYS_lwp_mutex_wakeup:
          dolwpmutexwakeup(drac,args);
          break;
     case SYS_lwp_exit:
          dolwpexit(drac,args);
          break;
/* End of LWP support */

     case 0x0000FFFF:   /* freezethaw */
          dofreezethaw(drac,args);
          break;
     default:

          sprintf(buf,"Unknown Syscall: %X %X %X\n",code,args[0],args[1]);
          outsok(buf);

          seterror(drac,ENOENT);
          break;
     }

     if(ukd->truss) {
          if((ukd->uthreads[ukd->threadid].status != THREADRUNNING) &&
                (ukd->uthreads[ukd->threadid].status != THREADAVAILABLE)) {
              char *ptr;
              if(ptr=strchr(ukd->trussbuf,'\n')) {
                 sprintf(ptr,"  ... sleeping \n");
              }
              else {
                 ptr=ukd->trussbuf+strlen(ukd->trussbuf);
                 sprintf(ptr,"  ... sleeping\n");
              }
          }
          sprintf(buf,"%2d - ",ukd->threadid+1);
          outsok(buf);
          outsok(ukd->trussbuf);
     }
}
/************************************************************************
    EXIT

    The lower domain is destroyed and then myexit() called to
    zap this domain.
************************************************************************/
doexit()
{
     JUMPBUF;
     UINT32 rc;
     int i;
     struct DeviceIORequest dior;
     struct filetablee *filet;
     int slot;

/* must  get CALLER from lower domain, destroy lower domain, myexit(0); */

       if(ukd->truss) {
           sprintf(ukd->trussbuf,"%2d - EXIT\n",ukd->threadid);
           outsok(ukd->trussbuf);
       }


//     if(ukd->threadid) {  /* an lwp is special. just zap domain */
//         ukd->uthreads[ukd->threadid].status=THREADAVAILABLE;
//         {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
//         {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
//         {OC(DC_DestroyDomain);XB(0xC0F08E00);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
//         return;  /* will cause keeper to become available */
//     }

/* close all device objects, console device is specially handled in myexit() */

       for(i=0;i<MAXFILEHANDLES;i++) {

//           if(ukd->truss) {
//               sprintf(ukd->trussbuf,"FH(%d) flags %X filet %X\n",
//                     i,ukd->filehandles[i].flags,ukd->filehandles[i].filet);
//               outsok(ukd->trussbuf);
//           }
           if(ukd->filehandles[i].flags & DEVICE) {
              filet=ukd->filehandles[i].filet;
              slot=filet->slot;

              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+slot);XB(0x00E00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

              dior.fh=i;
              {OC(DeviceClose);PS2(&(dior),sizeof(dior));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
           }
       }

/* check to see if all lwps are dead */

     for(i=0;i<MAXTHREADS;i++) {
         if((ukd->threadid != i) && (ukd->uthreads[i].status != THREADAVAILABLE)) {
            {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+i);XB(0x00F00000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(DC_DestroyDomain);XB(0xC0F08E00);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
         }
     }

     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+ukd->threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }  /* recover */

     {OC(Domain_GetKey+caller);XB(0x00800000);NB(0x00802000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(DESTROY_OC);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }   /* VCS is now gone */
     {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(DC_DestroyDomain);XB(0xC0F08E00);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
     myexit(0,0);
}
/************************************************************************
    GETUID

    we are always root
************************************************************************/
dogetuid(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
        strcat(ukd->trussbuf,"GETUID: ");
     }
     setreturn(drac,0,0);
}
/************************************************************************
    GETPID

    we are always PID=1
************************************************************************/
dogetpid(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
        strcat(ukd->trussbuf,"GETPID: ");
     }
     setreturn(drac,1,0);
}
/************************************************************************
    MPROTECT

    This is supposed to set the protection of user memory pages.
    All of the user memory is Copy On Write in the VCS so this is
    a nop.

************************************************************************/
domprotect(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"MPROTECT: %X %X %X", args[0],args[1],args[2]);
     }
     setreturn(drac,0,0);
}
/************************************************************************
    KILL

    This is supposed to send a signal. sometimes to self
************************************************************************/
dokill(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
        strcat(ukd->trussbuf,"KILL: ");
     }
     setreturn(drac,0,0);
}
/************************************************************************
    FCNTL  - does nothing at the momement

************************************************************************/
dofcntl(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int fh;
     int cmd;
     unsigned long arg;

     fh=args[0];
     cmd=args[1];
     arg=args[2];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"FCNTL(%d) %X %X",fh,cmd,arg);
     }
     setreturn(drac,0,0);
}
/************************************************************************
    STAT

    Return the inode data for a file by name.

    Special cases:   1) the name is the same as the process file name
                        (ie. ukd->ukn.name as set by the wrapper)
                     2) . is a directory

************************************************************************/
dostat(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     char buf[256];
     char *fname;
     struct stat *st;
     unsigned long long segsize;
     struct FS_UnixMeta um;
     int dir;

     fname=(char *)(usermem+args[0]);
     st = (struct stat *)(usermem+args[1]);

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"STAT '%s' %lX",fname,st);
     }

     memset((char *)st,0,sizeof(struct stat));

     if(!strcmp(fname,ukd->ukn.name)) {   /* kludge for loader */
        ukd->curdevice=DEVHOME;
        segsize=ukd->ukn.length;
        fillstat(st,segsize,1,0x81FF);
        setreturn(drac,0,0);
        return;
     }

     if(dir=lookup(fname,&um)) {
        if(dir > 0x10000000) {   /* segment file system  root or current */
           segsize = um.length;
           fillstat(st,segsize,um.inode,um.mode);
           setreturn(drac,0,0);
           return;
        }
        if(dir==2) {   /* directory */
           segsize=um.length;
           fillstat(st,segsize,um.inode,um.mode);
           setreturn(drac,0,0);
           return;
        }
        if(dir==3) {  /* proc directory */
           segsize=512;
           fillstat(st,segsize,9999,0x41ed);
           setreturn(drac,0,0);
           return;
        }
        if(dir==4) {  /* proc  usage */
           segsize=512;
           fillstat(st,segsize,9998,0x81b6);
           setreturn(drac,0,0);
           return;
        }
        if(dir==5) {  /* proc  status */
           segsize=512;
           fillstat(st,segsize,9997,0x81b6);
           setreturn(drac,0,0);
           return;
        }
        /* regular file */
        segsize=um.length;
        fillstat(st,segsize,um.inode,um.mode);
        setreturn(drac,0,0);
        return;
     }
     if(ukd->lookuperror == LOOKNODIR) {
         seterror(drac,ENOTDIR);
     }
     else {
         seterror(drac,ENOENT);
     }
}
/************************************************************************
    STAT64 - 64 bit version of stat()
************************************************************************/
dostat64(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     char buf[256];
     char *fname;
     struct stat64 *st;
     unsigned long long segsize;
     struct FS_UnixMeta um;
     int dir;

     fname=(char *)(usermem+args[0]);
     st = (struct stat64 *)(usermem+args[1]);

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"STAT64 '%s' %lX",fname,st);
     }

     memset((char *)st,0,sizeof(struct stat));

     if(!strcmp(fname,ukd->ukn.name)) {   /* kludge for loader */
        ukd->curdevice=DEVHOME;
        segsize=ukd->ukn.length;
        fillstat64(st,segsize,1,0x81FF);
        setreturn(drac,0,0);
        return;
     }

     if(dir=lookup(fname,&um)) {
        if(dir > 0x10000000) {   /* segment file system root or current */
           segsize = um.length;
           fillstat64(st,segsize,um.inode,um.mode);
           setreturn(drac,0,0);
           return;
        }
        if(dir==2) {   /* directory */
           segsize=um.length;
           fillstat64(st,segsize,um.inode,um.mode);
           setreturn(drac,0,0);
           return;
        }
        if(dir==3) {  /* proc directory */
           segsize=512;
           fillstat64(st,segsize,9999,0x41ed);
           setreturn(drac,0,0);
           return;
        }
        if(dir==4) {  /* proc  usage */
           segsize=512;
           fillstat64(st,segsize,9998,0x81b6);
           setreturn(drac,0,0);
           return;
        }
        if(dir==5) {  /* proc  status */
           segsize=512;
           fillstat64(st,segsize,9997,0x81b6);
           setreturn(drac,0,0);
           return;
        }
        /* regular file */
        segsize=um.length;
        fillstat64(st,segsize,um.inode,um.mode);
        setreturn(drac,0,0);
        return;
     }
     if(ukd->lookuperror == LOOKNODIR) {
         seterror(drac,ENOTDIR);
     }
     else {
         seterror(drac,ENOENT);
     }
}
/************************************************************************
    WRITE

    Data is copied from the user memory to the mapped file memory
    OR
    Date is copied to the sok key
************************************************************************/
dowrite(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     char buf[256];
     int fh,len;
     struct filetablee *filet;
     unsigned char *output;
     unsigned char *fileaddress;
     int slot;

     output=usermem+args[1];
     fh=args[0];
     len=args[2];

//{OC(COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
//{OC(32);XB(0x00F00000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"WRITE %d(%d) '%s'",fh,len,output);
     }


     if(ukd->filehandles[fh].flags & OUTPUT) {
         {OC(Node_Fetch+NODESOK);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
         goto doasdevicewrite;
//         {OC(0);PS2(output,len);XB(0x04B00000);RC(rc);NB(0x0810000B);cjcc(0x08000000,&_jumpbuf); }
//         {OC(Node_Swap+NODESOK);XB(0x8070B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
//         setreturn(drac,len,0);
//         return;
     }

     if(ukd->filehandles[fh].flags & DEVICE) {
          struct DeviceIORequest dior;
          struct Domain_DataByte ddb = {DBIO};

          filet=ukd->filehandles[fh].filet;
          slot=filet->slot;
          {OC(Domain_GetMemory);XB(0x00300000);NB(0x00C0EF00);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+slot);XB(0x00E00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

          if(!(ukd->filehandles[fh].filet->flags & DEVICEKEYINSLOT)) {
              seterror(drac,EACCES);
              return;
          }

doasdevicewrite:;   /* for console (OUTPUT) */

          dior.fh=fh;
          dior.flags=0;

          ukd->filehandles[fh].sequence++;
          dior.sequence = ukd->filehandles[fh].sequence;

          dior.parameter=0;
          dior.address = args[1];  /* address in user space */
          dior.length = len;

          if(ukd->havealarmtimer || ukd->haveitimertimer) {
              dior.flags |= DEVASYNC;
              {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
          }
          {OC(DeviceWrite);PS2(&(dior),sizeof(dior));XB(0x84B0F000);RC(rc);RS2(&(dior),sizeof(dior));NB(0x0B000000);cjcc(0x08080000,&_jumpbuf); }
          if (rc == Device_IOStarted) {  /* put to sleep */
              ukd->uthreads[ukd->threadid].status = THREADWAITING;
              ukd->uthreads[ukd->threadid].waitcode = WAITIO;
              ukd->uthreads[ukd->threadid].waitobject = fh;
              ukd->uthreads[ukd->threadid].drac = *drac;
              ukd->filehandles[fh].threadid = ukd->threadid;
              return;
          }
          if (rc == Device_IOComplete) {
             if(ukd->truss) {
                 sprintf(ukd->trussbuf,"WRITE(%d) '%0x%0x%0x%0x%0x%0x%0x%0x' %d",
                      fh,*(output+0),*(output+1),*(output+2),*(output+3),
                      *(output+4),*(output+5),*(output+6),*(output+7),len);
             }
             if(dior.length == -1) seterror(drac,dior.error);
             else setreturn(drac,dior.length,0);

             return;
          }
          seterror(drac,EACCES);
          return;
     }

/* only available for Regular files which are separate segments */
     if(!(filet=ukd->filehandles[fh].filet)) {
         seterror(drac,ENOENT);
         return;
      }

     if(filet->flags & OPENOUT) {   /* can write to this type of file */
         if(filet->flags & OPENAPPEND) {
              filet->position=filet->length;
         }

         if(ukd->truss) {
             sprintf(ukd->trussbuf,"WRITE(%d) '%0x%0x%0x%0x%0x%0x%0x%0x' %d",
                   fh,*(output+0),*(output+1),*(output+2),*(output+3),
                      *(output+4),*(output+5),*(output+6),*(output+7),len);
         }
         if(filet->flags & OPENAPPEND) {
            filet->position = filet->length;
         }
         fileaddress=filet->address;
         fileaddress += filet->position;
         *memorywanterror=1;
         memcpy(fileaddress,output,len);
         *memorywanterror=0;
         if(*memoryerror) {  /* read only */
             seterror(drac,EACCES);
             return;
         }

         filet->position += len;
         filet->length += len;

         setreturn(drac,len,0);
         return;
     }
/* FILE will have to go here */

     seterror(drac,ENOENT);
}
/************************************************************************
    READ

    Data is copied from the mapped file memory to the user memory
    OR
    Data is copied from the sik key to user memory

    /dev/zero is a special case marked in the filehandle table (no filetable)
************************************************************************/
doread(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     char buf[256];
     unsigned char *input;
     unsigned char *fileaddress;
     struct filetablee *filet;
     int fh,len,slot;
     int actlen;

     input=(unsigned char *)usermem+args[1];
     fh=args[0];
     len=args[2];

     if(fh < 0 || fh > MAXFILEHANDLES) {
         seterror(drac,EBADF);
         return;
     }

     if(ukd->truss) {   /* will be replaced with better answer if read is done */
        sprintf(ukd->trussbuf,"READ(%d) %d",fh,len);
     }

     if(ukd->filehandles[fh].flags & INPUT) {
         {OC(Node_Fetch+NODESIK);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
         goto doasdeviceread;

//         {OC(8192+len);XB(0x00B00000);RC(rc);RS3(input,len,actlen);NB(0x0B10000B);cjcc(0x00100000,&_jumpbuf); }
//         {OC(Node_Swap+NODESIK);XB(0x8070B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
//         if(input[actlen-1] == '\r') {
//            input[actlen-1] = '\n';
//         }
//             sprintf(ukd->trussbuf,"READ(%d) '%0x%0x%0x%0x%0x%0x%0x%0x' %d",
//                   fh,*(input+0),*(input+1),*(input+2),*(input+3),
//                      *(input+4),*(input+5),*(input+6),*(input+7),len);
//         }
         setreturn(drac,actlen,0);
     }

/* DEVICES (including a real terminal) */

     else if(ukd->filehandles[fh].flags & DEVICE) {

          struct DeviceIORequest dior;
          struct Domain_DataByte ddb = {DBIO};

          filet=ukd->filehandles[fh].filet;
          slot=filet->slot;
          {OC(Domain_GetMemory);XB(0x00300000);NB(0x00C0EF00);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+slot);XB(0x00E00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

          if(!(ukd->filehandles[fh].filet->flags & DEVICEKEYINSLOT)) {
              seterror(drac,EACCES);
              return;
          }

doasdeviceread:;        /* for console (INPUT) */

          dior.fh=fh;
          dior.flags=0;

          ukd->filehandles[fh].sequence++;
          dior.sequence = ukd->filehandles[fh].sequence;

          dior.parameter=0;
          dior.address = args[1];  /* address in user space */
          dior.length = len;

          if(ukd->havealarmtimer || ukd->haveitimertimer) {
              dior.flags |= DEVASYNC;
              {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
          }
          {OC(DeviceRead);PS2(&(dior),sizeof(dior));XB(0x84B0F000);RC(rc);RS2(&(dior),sizeof(dior));NB(0x0B000000);cjcc(0x08080000,&_jumpbuf); }
          if (rc == Device_IOStarted) {  /* put to sleep */
              ukd->uthreads[ukd->threadid].status = THREADWAITING;
              ukd->uthreads[ukd->threadid].waitcode = WAITIO;
              ukd->uthreads[ukd->threadid].waitobject = fh;
              ukd->uthreads[ukd->threadid].drac = *drac;
              ukd->filehandles[fh].threadid = ukd->threadid;
              return;
          }
          if (rc == Device_IOComplete) {
             if(ukd->truss) {
                sprintf(ukd->trussbuf,"READ(%d) '%0x%0x%0x%0x%0x%0x%0x%0x' %d",
                   fh,*(input+0),*(input+1),*(input+2),*(input+3),
                      *(input+4),*(input+5),*(input+6),*(input+7),dior.length);
             }
             if(dior.length == -1) seterror(drac,dior.error);
             else setreturn(drac,dior.length,0);
             return;
          }

          seterror(drac,EACCES);
          return;
     }
     else if(ukd->filehandles[fh].flags & FILE) { /* regular file */

          filet=ukd->filehandles[fh].filet;

          if((len + filet->position) > filet->length) {
               len = filet->length - filet->position;
          }
          fileaddress=filet->address;
          fileaddress += filet->position;
          filet->position += len;
          memcpy(input,fileaddress,len);

          if(ukd->truss) {
             sprintf(ukd->trussbuf,"READ(%d) '%0x%0x%0x%0x%0x%0x%0x%0x' %d",
                   fh,*(input+0),*(input+1),*(input+2),*(input+3),
                      *(input+4),*(input+5),*(input+6),*(input+7),len);
          }
          setreturn(drac,len,0);
     }
     else if(ukd->filehandles[fh].flags & PROCUSAGE) {
          memset(input,0,len);
          setreturn(drac,len,0);
     }
     else if(ukd->filehandles[fh].flags & PROCSTATUS) {
          memset(input,0,len);
          setreturn(drac,len,0);
     }
     else  {  /* dev/zero and other special cases  DIRECTORY could be put here*/
          memset(input,0,len);
          setreturn(drac,len,0);
     }
}

/************************************************************************
    ACCESS
************************************************************************/
doaccess(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     char *fname;
     int dir;
     struct FS_UnixMeta um;

     fname=usermem+args[0];
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"ACCESS '%s'",fname);
     }
     dir=lookup(fname,&um);
     if(dir) {
         setreturn(drac,0,0);
     }
     else {
         if(ukd->lookuperror == LOOKNODIR) {
            seterror(drac,ENOTDIR);
         }
         else {
            seterror(drac,ENOENT);
         }
     }
}


/************************************************************************
    OPEN

    /dev/zero is a special case

    if the freezedryhack has been set (oc = 42 for the wrapper)
    then "Freezedry.class" is a special case

    The name is looked up and the segment is mapped into the next
    available slot.  The file will be remapped after a freezedry/thaw
    event (possibly in a different slot) so the full name is saved
    in the file table.

************************************************************************/
doopen(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;

     char buf[256];
     char *fname;
     int fh;
     int flags;
     struct filetablee *filet;
     int slot;
     unsigned long long length;
     char *ptr;
     struct FS_UnixMeta um;
     int dir;

     fname=usermem+args[0];
     flags=args[1];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"OPEN '%s'(%x)",fname,flags);
     }

     if(ukd->freezedryhack)  {   /* used oc = 42 */
       ptr=&fname[strlen(fname)-1];
       while(ptr >= fname) {
          if(*ptr == '/' || ptr == fname) {
             if(*ptr == '/') {
                ptr++;
             }
             if(!strcmp(ptr,"Freezedry.class")) {
                if(!canfreeze()) {    /* must not have LWPs running */
                    seterror(drac,ENOENT);
                    return;
                }
                ukd->freezerc=ENOENT;
                ukd->uthreads[ukd->threadid].drac=*drac;
                dothefreeze(drac,args);
                return;
             }
             break;
          }
          ptr--;
       }
     }

     if(!strcmp(fname,"/dev/zero")) {  // special
        fh=nextfilehandle();
        if(!fh) {
           seterror(drac,EMFILE);
           return;
        }
        ukd->filehandles[fh].filet=0;  // mark as in use
        ukd->filehandles[fh].flags=ZERO;  // special case
        setreturn(drac,fh,0);
        return;
     }

     if(!strncmp(fname,"/dev/",5)) { /* a special device */
        struct DeviceIORequest dior;

        fh=nextfilehandle();
        if(!fh) {
           seterror(drac,EMFILE);
           return;
        }

        strcpy(buf+1,fname+5);  /* get name */
        *buf = strlen(fname+5);
        {OC(Node_Fetch+NODEDEVICE);XB(0x00700000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
        {OC(TDO_GetEqual);PS2(buf,(*buf)+1);XB(0x04B00000);RC(rc);NB(0x0880B000);cjcc(0x08000000,&_jumpbuf); }
        if (rc != 1) {
           seterror(drac,ENOENT);
           freefilehandle(fh);
           return;
        }

        dior.fh=fh;
        dior.flags = 0;
        dior.sequence = 0;

        {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(DeviceOpen);PS2(&(dior),sizeof(dior));XB(0x44B00F00);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
        if (rc != Device_IOComplete) {
           freefilehandle(fh);
           seterror(drac,EACCES);
           return;
        }

        um.mode=0x21ff;  /* special device */
        um.inode = 1;
        um.length = 0;
        um.userid = 0;
        um.groupid = 0;

        ukd->filehandles[fh].filet=0;  // mark as in use
        ukd->filehandles[fh].flags=DEVICE;  // special case

        goto openregularfile;
     }

     if(dir=lookup(fname,&um)) {
        fh=nextfilehandle();   /* get a file handle table entry */
        if(!fh) {
           seterror(drac,EMFILE);
           return;
        }

        if(dir > 0x10000000) {   /* segment file case 10000000 = root 20000000 = current */
           char *raddress;
           short rslot;

           if((dir & 0xF0000000) == 0x10000000) {
               raddress=ukd->rootaddress;
               rslot=ukd->rootslot;
           }
           if((dir & 0xF0000000) == 0x20000000) {
               raddress=ukd->currentaddress;
               rslot=ukd->currentslot;
           }
           if((dir & 0xF0000000) == 0x30000000) {
               raddress=ukd->componentaddress;
               rslot=ukd->componentslot;
           }

           ukd->filehandles[fh].flags = FILE;

           filet=nextfiletable();
           if(!filet) {
              freefilehandle(fh);
              seterror(drac,ENFILE);
              return;
           }
           strcpy(filet->name,fname);  // for re-open after freeze

           if(ukd->truss) {
               char bb[64];
               sprintf(bb," Share Slot=%d",rslot);
               strcat(ukd->trussbuf,bb);
           }

           filet->slot = rslot;
           filet->flags |= SLOTSHARED;
           filet->address = raddress + (dir & 0x0FFFFFFF);
           filet->length=um.length;
           filet->position=0;
           filet->inode=um.inode;
           filet->mode=um.mode;

           ukd->filehandles[fh].inode=um.inode;
           ukd->filehandles[fh].filet=filet;   /* marks as in use */

           setreturn(drac,fh,0);
           return;
        }

        switch(dir) {
        case 1:  /* regular file */
           ukd->filehandles[fh].flags = FILE;
openregularfile:;

           filet=nextfiletable();
           if(!filet) {
              freefilehandle(fh);
              seterror(drac,ENFILE);
              return;
           }
           strcpy(filet->name,fname);  // for re-open after freeze

           slot=nextslot(filet);

           if(um.mode & 0x2000) { /* character special */
              filet->inode=um.inode;
              filet->mode=um.mode;
              filet->slot=slot;
              filet->flags = FILEDEVICE;
              ukd->filehandles[fh].flags = DEVICE;
              filet->flags |= DEVICEKEYINSLOT;
              filet->address = 0;  /* shows in use */

              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+slot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

              break;
           }

/* regular disk based file */

           if(ukd->truss) {
               char bb[64];
               sprintf(bb," Slot=%d FH.flags %X",slot,ukd->filehandles[fh].flags);
               strcat(ukd->trussbuf,bb);
           }
           if(flags & (O_WRONLY | O_RDWR)) {
              filet->flags |= OPENOUT;
           }
           if(filet->flags & OPENOUT) {
              if(flags & O_APPEND) {
                 filet->flags |= OPENAPPEND;
              }
              if(flags & O_TRUNC) {
                 filet->flags &= ~OPENAPPEND;
              }
              if(!(filet->flags & OPENAPPEND)) {
                 um.length=0;
                 length=0;
                 {OC(FS_TruncateSegment);PS2(&(length),sizeof(length));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
              }
           }

           filet->slot = slot;
           filet->address = (char *)(slot << 28);   /* marks it as in use */
           filet->length=um.length;
           filet->position=0;  /* if APPEND this is changed at each WRITE */
           filet->inode=um.inode;
           filet->mode=um.mode;
           {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
           {OC(Node_Swap+slot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

           ukd->filehandles[fh].inode=um.inode;

           break;
        case 2:  /* directory */
           ukd->filehandles[fh].flags = DIRECTORY;

/* must save directory key for getdents */
/* must make possible restoring after thaw */

           filet=nextfiletable();
           if(!filet) {
              freefilehandle(fh);
              seterror(drac,ENFILE);
              return;
           }
           strcpy(filet->name,fname);  // for re-open after freeze
           slot=nextslot(filet);

           if(ukd->truss) {
               char bb[64];
               sprintf(bb," Slot=%d",slot);
               strcat(ukd->trussbuf,bb);
           }

           filet->slot = slot;
           filet->address = (char *)(slot << 28);  /* marks in use, not used for directory */
           filet->length=um.length;
           filet->position=0;
           filet->inode=um.inode;
           filet->mode=um.mode;
           {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
           {OC(Node_Swap+slot);XB(0x80E0B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* record collection in memory tree */

           ukd->filehandles[fh].inode=um.inode;
           break;
        case 3:  /* proc directory */
           ukd->filehandles[fh].flags = PROC;
           filet=0;
        case 4:  /* proc usage */
           ukd->filehandles[fh].flags = PROC | PROCUSAGE;
           filet=0;
        case 5:  /* proc status */
           ukd->filehandles[fh].flags = PROC | PROCSTATUS;
           filet=0;

           break;
        }

        ukd->filehandles[fh].filet=filet;   /* marks as in use */

        setreturn(drac,fh,0);
        return;
     }
/* let's ask if O_CREATE and if so lets make one */

     if((flags & O_CREAT) && (ukd->lookuperror == LOOKNOFILE) ) {  /* create file */

        {OC(COMPFSF);XB(0x00000000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(FSF_Create);XB(0xE0E04510);RC(rc);NB(0x0880B000);cjcc(0x00000000,&_jumpbuf); }
        if(rc) {
//           seterror(drac,ENOENT);
           seterror(drac,0x1001);
           return;
        }

        fh=nextfilehandle();   /* get a file handle table entry */
        if(!fh) {
           {OC(KT+4);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
//           seterror(drac,EMFILE);
           seterror(drac,0x1002);
           return;
        }

        {OC(Node_Fetch+NODELASTDIR);XB(0x00700000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(TDO_AddReplaceKey);PS2(ukd->lastlookupname,(*ukd->lastlookupname)+1);XB(0x84E0B000);RC(rc);NB(0x0880E000);cjcc(0x08000000,&_jumpbuf); }

        if (rc == 1) { /* old key */
            {OC(KT+4);XB(0x00E00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        }
        if (rc > 2) {  /* error */
            {OC(KT+4);XB(0x00B00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            freefilehandle(fh);
//            seterror(drac,ENOENT);
            seterror(drac,0x1003);
            return;
        }

/* what will we use for the inode number. The inode number only seems important for */
/* the loader so we should be free to make up one that is the same for all files */

        um.mode=0x81ff;
        um.inode=0x9999;
        um.length=0;
        um.groupid=0;
        um.userid=0;

        {OC(FS_SetMetaData);PS2(&(um),sizeof(um));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

        ukd->filehandles[fh].flags = FILE;
        goto openregularfile;   /* continue with regular file open */
     }

     if(ukd->lookuperror == LOOKNODIR) {
         seterror(drac,ENOTDIR);
     }
     else {
        seterror(drac,ENOENT);
     }
}
/************************************************************************
    CLOSE

    /dev/zero files and sik/sok files are special cased.  The memory
    slot is freed after the tables are reset.
************************************************************************/
doclose(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     struct filetablee *filet;
     int fh;
     int slot;
     char buf[256];
     struct FS_UnixMeta um;
     JUMPBUF;
     UINT32 rc;

     fh=args[0];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"CLOSE %d flags %X",fh,ukd->filehandles[fh].flags);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
        seterror(drac,EBADF);
        return;
     }

     if(ukd->filehandles[fh].filet == (struct filetablee *)0xFFFFFFFF) {
        seterror(drac,EBADF);  /* already closed */
        return;
     }

     if(ukd->filehandles[fh].flags & DOOR) {
        freefilehandle(fh);
        setreturn(drac,0,0);
        return;
     }
     if(ukd->filehandles[fh].flags & ZERO) {
        freefilehandle(fh);
        setreturn(drac,0,0);
        return;
     }
     if(ukd->filehandles[fh].flags & DEVICE) {
         struct DeviceIORequest dior;

         filet=ukd->filehandles[fh].filet;
         filet->position=0;
         filet->length=0;
         filet->address=(char *)0xFFFFFFFF;  /* free file table entry */

         slot=filet->slot;
         ukd->slots[slot]=0;   /* free slot */

         filet->flags=0;
         filet->slot=0;
         filet->address=(char *)0xFFFFFFFF;  /* free file table entry */

         freefilehandle(fh);

         {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Node_Fetch+slot);XB(0x00E00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

         dior.fh=fh;
         {OC(DeviceClose);PS2(&(dior),sizeof(dior));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

         setreturn(drac,0,0);
         return;
     }
     if(ukd->filehandles[fh].flags & PROC) {
        freefilehandle(fh);
        setreturn(drac,0,0);
        return;
     }
     if(ukd->filehandles[fh].flags & (DIRECTORY | FILE)) {

         filet=ukd->filehandles[fh].filet;
         freefilehandle(fh);  /* free file handle */

         um.inode=filet->inode;
         um.mode=filet->mode;
         um.length=filet->length;
         um.groupid=0;
         um.userid=0;

         slot=filet->slot;

         if(!(filet->flags & SLOTSHARED) && (filet->flags & OPENOUT)) {
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+slot);XB(0x00E00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
            {OC(FS_SetMetaData);PS2(&(um),sizeof(um));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
         }

         filet->position=0;
         filet->length=0;
         filet->address=(char *)0xFFFFFFFF;  /* free file table entry */

         if(!(filet->flags & SLOTSHARED)) {
            ukd->slots[slot]=0;   /* free slot */
         }
         filet->flags=0;
         filet->slot=0;

         setreturn(drac,0,0);
         return;
     }

     if(ukd->filehandles[fh].flags & (INPUT | OUTPUT)) {
        setreturn(drac,0,0);
        return;
     }

     seterror(drac,EINVAL);
}
/************************************************************************
    POLL

************************************************************************/
dopoll(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     struct pollfd *fds;
     int nfds;
     unsigned long timeout;
     unsigned long long microseconds;
     int i,fh;
     int events;
     struct filetablee *filet;
     int slot;
     struct DeviceIORequest dior;
     struct Domain_DataByte ddb = {DBIO};


     fds = (struct pollfd *)(usermem+args[0]);
     nfds = args[1];
     timeout = args[2];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"POLL %X nfds=%d timeout=%X\n",fds,nfds,timeout);
     }

     /* loop through fds setting the poll status of each device */

     {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }
     events=0;
     for (i=0;i<nfds;i++) {
        fh=fds[i].fd;
        fds[i].revents = 0;
        if(fh < 0 || fh > MAXFILEHANDLES) {
           continue;
        }

        if(ukd->filehandles[fh].flags & (INPUT+OUTPUT)) {   /* polling console */
           {OC(Node_Fetch+NODESIK);XB(0x00700000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        }

        else if(!(ukd->filehandles[fh].flags & DEVICE)) {
           fds[i].revents = POLLNVAL;
           events++;
           continue;
        }
        else {   /* a device */
           filet=ukd->filehandles[fh].filet;
           slot=filet->slot;
           {OC(Node_Fetch+slot);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        }

        dior.fh=fh;
        dior.flags=DEVPOLL;
        dior.parameter=fds[i].events;
        dior.sequence = ++ukd->filehandles[fh].sequence;
        dior.address = 0;
        dior.length = 0;

        {OC(DevicePoll);PS2(&(dior),sizeof(dior));XB(0x84E0D000);RC(rc);RS2(&(dior),sizeof(dior));NB(0x0B000000);cjcc(0x08080000,&_jumpbuf); }
        if (rc == Device_IOComplete) {  /* immediate response */
            fds[i].revents = dior.parameter;
            events++;
        }
        else ukd->filehandles[i].threadid = ukd->threadid;  /* thread that is polling this fd */
     }
     if(events) {
        setreturn(drac,events,0);
        return;
     }

     /* NOW look at timeout.  If it is zero return immediately, else set the timer and wait */

     if(!timeout) {
        setreturn(drac,0,0);
        return;
     }

     if(timeout != 0xFFFFFFFF) { /* set or reset the poll timer */
        microseconds = timeout;
        microseconds = microseconds * 1000ll;   /* to micros */
        ukd->pollmicroseconds = microseconds;

        {OC(Node_Fetch+NODEPOLLWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Wait_SetInterval);PS2(&(microseconds),sizeof(microseconds));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Wait_ShowTOD);XB(0x00F00000);RS2(&(ukd->polltod),sizeof(ukd->polltod));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }  /* save this for wakeup early test */

        if(!ukd->havepolltimer) { /* must get one */
           makepolltimer();
           ukd->havepolltimer = 1;
        }
     }
     ukd->uthreads[ukd->threadid].fds = fds;  /* in user memory */
     ukd->uthreads[ukd->threadid].nfds = nfds;
     ukd->uthreads[ukd->threadid].status = THREADWAITING;
     ukd->uthreads[ukd->threadid].waitcode = WAITPOLL;
     ukd->uthreads[ukd->threadid].waitobject = 0;
     ukd->uthreads[ukd->threadid].drac = *drac;

     return;
}

/************************************************************************
    FSTAT

    as in stat but using a file handle to locate the file
************************************************************************/
dofstat(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     struct stat *st;
     int fh;
     char buf[256];

     fh=args[0];
     st=(struct stat *)(usermem+args[1]);

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"FSTAT %d %lX",fh,args[1]);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
         seterror(drac,EBADF);
         return;
     }

     if(ukd->filehandles[fh].flags & ZERO) {  // dev/zero
         seterror(drac,EBADF);
         return;
     }
     if(ukd->filehandles[fh].flags & DIRECTORY) {
         fillstat(st,512LL,ukd->filehandles[fh].inode,0x45ed);
     }
     else if(ukd->filehandles[fh].filet) {
         fillstat(st,ukd->filehandles[fh].filet->length,ukd->filehandles[fh].filet->inode,0x81ff);
     }
     else {
         seterror(drac,EBADF);
         return;
     }


//sprintf(buf,"FSTAT(%d): size=%d blksz %X, BLKS %X\n",sizeof(struct stat),st->st_size,
//   st->st_blksize, st->st_blocks);
//outsok(buf);

     setreturn(drac,0,0);
     return;
}

/************************************************************************
    FSTAT64

    as in FSTAT but using the 64 bit stat buffer
************************************************************************/
dofstat64(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     struct stat64 *st;
     int fh;
     char buf[256];

     fh=args[0];
     st=(struct stat64 *)(usermem+args[1]);

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"FSTAT64 %d %lX",fh,args[1]);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
         seterror(drac,EBADF);
         return;
     }

     if(ukd->filehandles[fh].flags & ZERO) {  // dev/zero
         seterror(drac,EBADF);
         return;
     }
     if(ukd->filehandles[fh].flags & DIRECTORY) {
         fillstat64(st,512LL,ukd->filehandles[fh].inode,0x45ed);
     }
     else if(ukd->filehandles[fh].filet) {
         fillstat64(st,ukd->filehandles[fh].filet->length,ukd->filehandles[fh].filet->inode,0x81ff);
     }
     else {
         seterror(drac,EBADF);
         return;
     }

     setreturn(drac,0,0);
     return;
}

/************************************************************************
    MMAP

    In this implementation the bits are actually copied into the user
    memory.  This makes the freezedried user application the source
    of all library sharing between multiple instances of the same object.
    Since this is the intended use of Pacific this degree of sharing
    is probably appropriate.  Loading time for initial objects is much
    longer but loading time for frozen objects is instant.

    Note that the selected window size of the caller must be honored as
    it often extends beyond the file.  This is true for BSS sections of
    libraries.   The bit copy must stop at the file boundary.  The loader
    will likey replace the mapping of the BSS section with a mapping of
    /dev/zero
************************************************************************/
dommap(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     int fh,slot;
     struct filetablee *filet;
     unsigned char *fileaddress;
     unsigned long size,offset,mapaddress;
     char buf[256];
     int dir;

     mapaddress=args[0];
     size=args[1];
     fh=args[4];
     offset=args[5];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"MMAP %8lX, %lX, PROT, MAP, %d, %lX",
            mapaddress,size,fh,offset);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
        seterror(drac,EBADF);
        return;
     }
     if(ukd->filehandles[fh].flags & ZERO) {  // special case
        if(!mapaddress) {
           mapaddress = nextloadermap(size);
           if(mapaddress < ukd->brkaddress) {
              seterror(drac,ENOMEM);
              return;
           }
        }
        else {
           if(mapaddress > ukd->maplowwater) {   /* within some file */
               memset((char *)(usermem + mapaddress),0,size);
           }
        }
        setreturn(drac,mapaddress,0);
        return;
     }

     if(!(ukd->filehandles[fh].flags & FILE)) {  /* must be file */
         seterror(drac,EINVAL);
         return;
     }
     filet=ukd->filehandles[fh].filet;

     fileaddress=filet->address + offset;

     if(!mapaddress) {
        mapaddress = nextloadermap(size);
        ukd->maplowwater=mapaddress;

        if(mapaddress < ukd->brkaddress) {
           seterror(drac,ENOMEM);
           return;
        }
     }

     if(size > filet->length) {  /* don't overstep file */
         size=filet->length;
     }
     putuser(fileaddress,mapaddress,size);

     setreturn(drac,mapaddress,0);
     return;
}
/************************************************************************
    MUNMAP - nothing for now don't reuse space, MUST FIX

    This simply zeros the space unmapped.   This is probably
    not necessary as the loader will overlay the same areas with
    /dev/zero causing them to be zeroed again.
************************************************************************/
domunmap(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     unsigned long mapaddress;
     int size;
     char buf[256];

     mapaddress = args[0];
     size = args[1];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"MUNMAP %8lX, %X", mapaddress,size);
     }
/*     memset(usermem+mapaddress,0,size); */

     setreturn(drac,0,0);
     return;
}
/************************************************************************
    MEMCNTL

    This is used to set protection.  ignored.
************************************************************************/
domemcntl(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
        sprintf(ukd->trussbuf,"MEMCNTL:");
     }
     setreturn(drac,0,0);
     return;
}
/************************************************************************
    SYSCONFIG

    At the momement this is used for the PAGESIZE CALL
    other fields will have to be added as they are discovered
************************************************************************/
dosysconfig(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"SYSCONFIG: %X",args[0]);
     }
     switch(args[0]) {
     case _CONFIG_PAGESIZE:
        setreturn(drac,PAGESZ,0);
        break;
     case _CONFIG_STACK_PROT:
        setreturn(drac,7,0);
        break;
     case _CONFIG_SEM_VALUE_MAX:
        setreturn(drac,2147483647,0);
        break;
     case _CONFIG_NPROC_CONF:
        setreturn(drac,1,0);
        break;
     default:
        setreturn(drac,0,0);
     }
}
/************************************************************************
    SYSTEMINFO

    At the moment this is used to find out what the platform is
************************************************************************/
dosysteminfo(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     char buf[128];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"SYSINFO %x, %x, %d",args[0],args[1],args[2]);
     }

     if(args[0] == SI_PLATFORM) {
         strcpy(usermem+args[1],"SUNW,Sun_4_75");
     }
     if(args[0] == SI_ARCHITECTURE) {
         strcpy(usermem+args[1],"sparc");
     }
     setreturn(drac,strlen(usermem+args[1])+1,0);

}
/************************************************************************
    IOCTL

    This is used to determine if the file is a terminal.   It is
    probably not necessary to fill in the termio structure with these
    lies.
************************************************************************/
doioctl(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     struct termio *tio;
     char buf[128];
     int  fh;

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"IOCTL %8lX %8lX %8lX", args[0],args[1],args[2]);
     }

 // need proper test here

     if (args[0] < 0) {
        seterror(drac,EBADF);
        return;
     }
     fh=args[0];
     if ((fh < 3)) {  // terminal
          tio=(struct termio *)(usermem+args[2]);
/*****************************************************************************
   There is a termio structure in the file table that can be used to
   support the function of termio.  The read and write routines could use
   it.  It is unused at this point
*****************************************************************************/
          if(args[1] == TCGETA) {
            memset(tio,0,sizeof(struct termio));
            setreturn(drac,0,0);
            return;
          }

          if(args[1] == TCGETS) {
            memset(tio,0,sizeof(struct termios));
            setreturn(drac,0,0);
            return;
          }
     }

     if(ukd->filehandles[fh].flags & DEVICE) {   /* need to call device */
         struct DeviceIORequest dior;
         int slot;
         struct filetablee *filet;

         filet=ukd->filehandles[fh].filet;
         slot = filet->slot;

         {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Node_Fetch+slot);XB(0x00F00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }

         dior.fh=fh;
         dior.flags = 0;
         dior.sequence = 0;
         dior.parameter = args[1];
         dior.address = args[2];
         dior.length = 0;

         {OC(DeviceIOCTL);PS2(&(dior),sizeof(dior));XB(0x04B00000);RC(rc);RS2(&(dior),sizeof(dior));NB(0x0B000000);cjcc(0x08080000,&_jumpbuf); }
         if(rc != Device_IOComplete) {
            seterror(drac,EINVAL);
            return;
         }
         setreturn(drac,dior.length,0);
         return;
     }
     seterror(drac,EINVAL);
     return;
}
/************************************************************************
    LLSEEK

    Seek using 64 bit long long arguments.  Just set the position field
    in the file table
************************************************************************/
dollseek(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int fh;
     long long position;
     struct filetablee *filet;
     long whence;

     fh=args[0];
     position=args[1];
     position = position<<32;
     position = position |= args[2];
     whence = args[3];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"LLSEEK %lX %llX %x", fh,position,whence);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
         seterror(drac,EBADF);
         return;
     }

     filet=ukd->filehandles[fh].filet;
     if(!filet) {
         seterror(drac,EINVAL);
         return;
     }

     if(fh > 2 ) {
        switch(whence) {
        case SEEK_SET:
           filet->position = position;
           break;
        case SEEK_CUR:
           filet->position = filet->position + position;
           break;
        case SEEK_END:
           filet->position = filet->length + position;
           break;
        }
     }
     args[1] = filet->position >> 32;
     args[2] = filet->position & 0xFFFFFFFF;

     setreturn(drac,args[1],args[2]);
}
/************************************************************************
    LSEEK

    As in llseek but with 32 bit position
************************************************************************/
dolseek(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int fh;
     long position;
     long whence;
     struct filetablee *filet;

     fh=args[0];
     position = args[1];
     whence = args[2];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"LLSEEK %lX %lX %x", fh,position,whence);
     }

     if(fh < 0 || fh > MAXFILEHANDLES) {
         seterror(drac,EBADF);
         return;
     }

     filet=ukd->filehandles[fh].filet;
     if(!filet) {
        seterror(drac,EINVAL);
        return;
     }

     if(fh > 2 ) {
        switch(whence) {
        case SEEK_SET:
           filet->position = position;
           break;
        case SEEK_CUR:
           filet->position = filet->position + position;
           break;
        case SEEK_END:
           filet->position = filet->length + position;
           break;
        }
     }

     setreturn(drac,filet->position,0);
}
/************************************************************************
    BRK

    MALLOC seems to keep track of the brk address on its own.   It
    doesn't appear necessary to do anything but say that this worked.
************************************************************************/
dobrk(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     char buf[128];

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"BRK(%lX)",args[0]);
     }

     if(args[0] >= ukd->mapaddress) {
         seterror(drac,ENOMEM);
         return;
     }
     ukd->brkaddress=args[0];
     setreturn(drac,0,0);
}
/************************************************************************
     SIGPROCMASK
************************************************************************/
dosigprocmask(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     sigset_t *sain,*saout;
     int action;
     siginfo_t si;
     int i;
     char buf[256];

     sain = (sigset_t *)(usermem+args[1]);
     saout = (sigset_t *)(usermem+args[2]);
     action = args[0];

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"SIGPROCMASK: %x %X %X",action,args[1],args[2]);
     }

#ifdef xx
sprintf(buf,"SIGPROCMASK(%d)                     %X %X %X %X\n",args[0],sain->__sigbits[0],
               sain->__sigbits[1],sain->__sigbits[2],sain->__sigbits[3]);
outsok(buf);
#endif

     if(args[0] == SIG_SETMASK) {
        if(args[2]) {
           *saout = ukd->uthreads[ukd->threadid].sa_mask;
        }
        if(args[1]) {
            ukd->uthreads[ukd->threadid].sa_mask = *sain;
        }
     }
     else if(args[0] == SIG_UNBLOCK) {
        if(args[2]) {
           *saout = ukd->uthreads[ukd->threadid].sa_mask;
        }
        for(i=1;i<64;i++) {
           if(sigismember(sain,i)) {
               sigdelset(&(ukd->uthreads[ukd->threadid].sa_mask),i);
           }
        }
     }

     else if(args[0] == SIG_BLOCK) {
        if(args[2]) {
           *saout = ukd->uthreads[ukd->threadid].sa_mask;
        }
        for(i=1;i<64;i++) {
           if(sigismember(sain,i)) {
               sigaddset(&(ukd->uthreads[ukd->threadid].sa_mask),i);
           }
        }
     }
#ifdef xx
     for(i=0;i<4;i++) {
         sprintf(buf,"PROCMASK: Thread[%d] sa_mask %X %X %X %X\n",i+1,
              ukd->uthreads[i].sa_mask.__sigbits[0],
              ukd->uthreads[i].sa_mask.__sigbits[1],
              ukd->uthreads[i].sa_mask.__sigbits[2],
              ukd->uthreads[i].sa_mask.__sigbits[3]);
         outsok(buf);
     }
#endif
/* now check pending ALARM signals */

     if(checkpending(SIGALRM)) {   /* pending and now enabled */
         if(ukd->multithread && (ukd->alarmthread == ukd->threadid)) {
            setreturn(drac,0,0);      /* set up to continue with good return */
            if(ukd->truss) {
                outsok(ukd->trussbuf);  /* show what we just did */
            }

            drac2siginfo(drac,&si,SIGALRM);
            dosignal(drac,SIGALRM,&si,&ukd->uthreads[ukd->threadid].sa_mask,ukd->threadid);
                        /* process is ready to run */

            return;
         }
         else {
            sigaddset(&ukd->sa_pending,SIGALRM);  /* put back pending bit */
         }
     }

     setreturn(drac,0,0);
}
/************************************************************************
    SIGACTION

    Currently the only used signal setup.  The libraries appear to
    translate signal() into sigaction().   All the magic for return
    seems to be handled in the libraries as well.   The library will
    arrange for a setcontext() at the end of the signal handler if
    the signal handler doesn't exit some other way.
************************************************************************/
dosigaction(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     char buf[128];
     struct sigaction *sain,*saout;
     int signo;

     sain = (struct sigaction *)(usermem+args[1]);
     saout = (struct sigaction *)(usermem+args[2]);
     signo = args[0];

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"SIGACTION: %d %X %X",signo,args[1],args[2]);
     }

     if(signo > MAXSIGNALS) {
         seterror(drac,EINVAL);
         return;
     }
     if(args[2]) {
        *saout = ukd->sigacttable[signo];
     }
     ukd->sigacttable[signo]=*sain;

     setreturn(drac,0,0);
}

/************************************************************************
     SET|GET CONTEXT
************************************************************************/
docontext(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     ucontext_t *uc;

     uc = (ucontext_t *)(usermem+args[1]);

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"CONTEXT[%d]: %x",args[0],args[1]);
     }

     if(args[0] == 0) {  /* getcontext */
         drac2ucontext(drac,uc,&ukd->uthreads[ukd->threadid].sa_mask);
         setreturn(drac,0,0);
     }
     else if(args[0] == 1) {  /* setcontext */
         ucontext2drac(uc,drac,&ukd->uthreads[ukd->threadid].sa_mask);
 /* return to repeat the interrupted instruction */
         if(ukd->truss) {
            strcat(ukd->trussbuf," = 0 0\n");
         }
         if(ukd->uthreads[ukd->threadid].waitcode == WAITMUTEX) {
             ukd->uthreads[ukd->threadid].status = THREADWAITING;
         }
     }
     else {
         seterror(drac,EINVAL);
     }
}
/************************************************************************
     DOSIGNAL
************************************************************************/
dosignal(drac,signo,sigsi,sa_mask,threadid)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 signo;
     siginfo_t *sigsi;
     sigset_t *sa_mask;
     int threadid;
{
     char buf[256];
     struct sigaction *sa;
     ucontext_t *uc;
     siginfo_t *si;
     int sp,fp;
     int i;

     sa = &ukd->sigacttable[signo];

     sp = drac->Regs.o[6];
     si = (siginfo_t *)(usermem+(sp - sizeof(siginfo_t)));
     uc = (ucontext_t *)(usermem+(sp - sizeof(siginfo_t) - sizeof(ucontext_t)));
     fp = sp - sizeof(siginfo_t) - sizeof(ucontext_t) - SA(MINFRAME);
     sp = fp - SA(MINFRAME);

     *si = *sigsi;
     drac2ucontext(drac,uc,sa_mask);
     for(i=0;i<4;i++) {
         ukd->uthreads[threadid].sa_mask.__sigbits[i] |=
              sa->sa_mask.__sigbits[i];
     }
     if(!(sa->sa_flags & SA_NODEFER)) {
          sigdelset(&ukd->uthreads[threadid].sa_mask,signo);
     }

     drac->Regs.o[0]=signo;
     drac->Regs.o[1]=0;
     drac->Regs.o[2]=0;
//     if(sa->sa_flags & SA_SIGINFO) {
          drac->Regs.o[1]=(UINT32)((UINT32)si-(UINT32)usermem);
          drac->Regs.o[2]=(UINT32)((UINT32)uc-(UINT32)usermem);
//     }
     drac->Regs.i[6]=fp;
     drac->Regs.o[6]=sp;

     drac->Control.PC=(UINT32)sa->sa_sigaction;
     drac->Control.NPC = drac->Control.PC+4;

     if(ukd->truss) {
        sprintf(buf,"SIGNAL: %d, sigaction %X sp %X fp %X si %X uc %X\n",
                signo,drac->Control.PC,sp,fp,si,uc);
        outsok(buf);
     }
}
/************************************************************************
     DOIOCOMPLETE - asynchronous IO completed
************************************************************************/
int doiocomplete(dior)
    struct DeviceIORequest *dior;
{
    JUMPBUF;
    UINT32 rc;
    int fh,threadid;
    struct Domain_SPARCRegistersAndControl *drac;
    char buf[256];
    int i;
    struct pollfd *fds;
    int events;

    fh=dior->fh;   /* this is true for polling as well as regular I/O */
    threadid = ukd->filehandles[fh].threadid;   /* thread that is waiting */

    if(ukd->truss) {
       sprintf(buf,"%2d - IOCOMPLETE FH=%d, %X(%d)\n",threadid+1,fh,dior->address,dior->length);
       outsok(buf);
       sprintf(buf,"   - thread id %d, flags %X, parameter %X\n",threadid,dior->flags,dior->parameter);
       outsok(buf);
    }

    if (dior->sequence != ukd->filehandles[fh].sequence) return 1;   /* ignore */

    drac=&ukd->uthreads[threadid].drac;

    if(dior->flags & DEVPOLL) {  /* poll return */
        /* find the fd in the pollfd array */
        events=0;
        fds = ukd->uthreads[threadid].fds;
        for(i=0;i<ukd->uthreads[threadid].nfds;i++) {
           if(fds[i].fd == fh) {  /* this one */
               events++;
               fds[i].revents = dior->parameter;
           }
        }
        if(ukd->truss) {
           sprintf(buf,"Poll events %d\n",events);
           outsok(buf);
        }
        setreturn(drac,events,0);
    }
    else {                       /* io return */
       if(dior->length == -1) seterror(drac,dior->error);
       else setreturn(drac,dior->length,0);  /* set completion code, advance PC */
    }

    ukd->uthreads[threadid].status = THREADRUNNING;
    ukd->uthreads[threadid].waitcode = 0;

    {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Fetch+threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }        /* now can dispatch domain */
    {OC(Domain_ResetSPARCStuff);PS2(&(*drac),sizeof(*drac));XB(0x1480000F); }

    {fj(0x08000000,&_jumpbuf); }                                  /* start domain */

    return 0;                                      /* return to helper */
}

/************************************************************************
     DOTIMER - an asynchronous interrupt, oh joy

     If the return code of this function is non-zero then the timer
     object is expected to restart because the timer has been reset.

     This is particularly useful for the poll timer which will be reset
     every poll.   Reset can be determined if the current time is less
     than the expected wakeup time of the timer.

************************************************************************/
dotimer(type)
     int type;   /* 0 = alarm, 1 = itimer, 2 = poll */
{
     JUMPBUF;
     UINT32 rc;
     int i,threadid;
     struct Domain_SPARCRegistersAndControl *drac;
     siginfo_t si;
     char buf[256];
     unsigned long long nowtime;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"TIMER \n");
         outsok(ukd->trussbuf);
     }

/*********************************************************************************
  Check the appropriate timer wakeup value to see if timer has been reset

  Alarm and Poll timers are treated similarly in that they may be reset
  and must check for restart.  If they aren't restarted then they will
  self destruct and we clear the havetimer flag
*********************************************************************************/

/* find a thread to deliver the signal too */

/* TODO   best check for mask */

     if(type == 0) {  /* alarm timer */
         if (ukd->microseconds == 0x7FFFFFFFFFFFFFFF) return 1;  /* reset to infinity */

         {OC(COMPCLOCK);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Clock_TOD_BINEPOC);XB(0x00F00000);RS2(&(nowtime),sizeof(nowtime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
         if (ukd->alarmtod > nowtime) return 1;     /* indicate a restart from a reset timer */

         ukd->havealarmtimer = 0;   /* the helper will die when it gets zero for a return code */
     }
     if(type == 2) {  /* poll timer */
         if (ukd->pollmicroseconds == 0xFFFFFFFFFFFFFFFF) return 1;  /* reset to infinity */

         {OC(COMPCLOCK);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Clock_TOD_BINEPOC);XB(0x00F00000);RS2(&(nowtime),sizeof(nowtime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
         if (ukd->polltod > nowtime) return 1;     /* indicate a restart from a reset timer */

         ukd->havepolltimer = 0;   /* the helper will die when it gets zero for a return code */

         threadid=-1;
         for (i=0;i<MAXTHREADS;i++) {
            if(ukd->uthreads[i].status == THREADWAITING) {
                if(ukd->uthreads[i].waitcode == WAITPOLL) {
                     threadid=i;
                     break;
                }
            }
         }
         if(threadid == -1) return 0;

         drac=&ukd->uthreads[threadid].drac;

         ukd->uthreads[threadid].status = THREADRUNNING;
         ukd->uthreads[threadid].waitcode = 0;

         /* set return to 0 - timeout */

         setreturn(drac,0,0);  /* set completion code, advance PC */

         {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Node_Fetch+threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }        /* now can dispatch domain */
         {OC(Domain_ResetSPARCStuff);PS2(&(*drac),sizeof(*drac));XB(0x1480000F); }

         {fj(0x08000000,&_jumpbuf); }                                  /* start domain */

         return 0;
     }

     threadid=-1;

     if(ukd->multithread) {
        threadid = ukd->alarmthread;
     }

     if(threadid == -1) {
        for (i=0;i<MAXTHREADS;i++) {
           if(ukd->uthreads[i].status == THREADSUSPENDED) {
              continue;
           }
           if(ukd->uthreads[i].status == THREADDOORRETURN) {
              continue;
           }
           if(ukd->uthreads[i].status == THREADAVAILABLE) {
              continue;
           }
           if(!sigismember(&ukd->uthreads[i].sa_mask,SIGALRM)) {
              threadid=i;
              break;
           }
        }
     }

     if(threadid == -1) {
/* mark as pending and return */
         sigaddset(&ukd->sa_pending,SIGALRM);
         return 0;  /* nothing to do, no one accepting just now */
     }

     if(sigismember(&ukd->uthreads[threadid].sa_mask,SIGALRM)) {
         sigaddset(&ukd->sa_pending,SIGALRM);
         return 0;
     }
#ifdef xx
     for(i=0;i<4;i++) {
         sprintf(buf,"TIMER: Thread[%d] sa_mask %X %X %X %X\n",i+1,
              ukd->uthreads[i].sa_mask.__sigbits[0],
              ukd->uthreads[i].sa_mask.__sigbits[1],
              ukd->uthreads[i].sa_mask.__sigbits[2],
              ukd->uthreads[i].sa_mask.__sigbits[3]);
         outsok(buf);
     }
#endif
#ifdef xx
     for(i=0;i<32;i++) {
         sprintf(buf,"TIMER: Sigaction[%d] %X, flags %X mask %X %X %X %X\n",
             i,
             ukd->sigacttable[i]._funcptr._handler,
             ukd->sigacttable[i].sa_flags,
             ukd->sigacttable[i].sa_mask.__sigbits[0],
             ukd->sigacttable[i].sa_mask.__sigbits[1],
             ukd->sigacttable[i].sa_mask.__sigbits[2],
             ukd->sigacttable[i].sa_mask.__sigbits[3]);
         outsok(buf);
     }
#endif

/* if the thread is not suspended it might be in a system call                   */
/* if it is running then we must use its domain key to stop it and collect       */
/* the DRAC information.   If it is suspended then the thread structure has the  */
/* DRAC information and we can use it to complete the signal process             */

    if(ukd->truss) {
         sprintf(buf,"DOTIMER: threadid = %d, threadPC = %X\n",
            threadid+1, ukd->uthreads[threadid].drac.Control.PC);
         outsok(buf);
    }

     if(ukd->uthreads[threadid].status == THREADWAITING) {  /* only one we handle just now */
          int fh;
          int slot;
          struct DeviceIORequest dior;
          struct filetablee *filet;

          if(ukd->uthreads[threadid].waitcode == WAITIO) {   /* must cancel IO */
              fh=ukd->uthreads[threadid].waitobject;
              filet = ukd->filehandles[fh].filet;
              slot = filet->slot;
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+slot);XB(0x00F00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
              dior.fh = fh;
              {OC(DeviceCancel);PS2(&(dior),sizeof(dior));XB(0x04B00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
              ukd->filehandles[fh].sequence++;    /* make sure any pending io notification is ignored */
          }
          drac=&ukd->uthreads[threadid].drac;
//          if((ukd->uthreads[threadid].waitcode != WAITMUTEX) &&
//                (ukd->uthreads[threadid].waitcode != WAITCONDV)) {
             seterror(drac,EINTR);                        /* set up the original sys call */
             ukd->uthreads[threadid].waitcode = 0;        /* clear as will skip on restart */
//          }

          drac2siginfo(drac,&si,SIGALRM);
          dosignal(drac,SIGALRM,&si,&ukd->uthreads[threadid].sa_mask,threadid);
                    /* process is ready to run */

          {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }        /* now can dispatch domain */
          ukd->uthreads[threadid].status = THREADRUNNING;
          {OC(Domain_ResetSPARCStuff);PS2(&(*drac),sizeof(*drac));XB(0x1480000F); }

          {fj(0x08000000,&_jumpbuf); }                                  /* start domain */
          return 0;                                      /* return to helper */
     }

     if(ukd->uthreads[threadid].status == THREADRUNNING) {  /* must stop and deliver */

          {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Node_Fetch+threadid);XB(0x00800000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
          {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }  /* stop domain */
          {OC(Domain_GetSPARCStuff);XB(0x00800000);RS2(&(ukd->uthreads[threadid].drac),sizeof(ukd->uthreads[threadid].drac));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
          putawaywindows(&ukd->uthreads[threadid].drac);

          drac = &ukd->uthreads[threadid].drac;

          drac2siginfo(drac,&si,SIGALRM);
          dosignal(drac,SIGALRM,&si,&ukd->uthreads[threadid].sa_mask,threadid);

          {OC(Domain_MakeBusy);XB(0x00800000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }  /* stop domain */
          {OC(Domain_ResetSPARCStuff);PS2(&(*drac),sizeof(*drac));XB(0x1480000F); }

          {fj(0x08000000,&_jumpbuf); }
          return 0;
     }
     return 0;
}

/************************************************************************
     ALARM   set a second timer

     TODO  THIS MIGHT HAVE TO BE A DIFFERENT DATABYTE
           MEANING MULTIPLE WAIT OBJECTS
************************************************************************/
doalarm(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     unsigned long long microseconds;

     microseconds = args[0];
     microseconds = microseconds * 1000000ll;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"ALARM %d",args[0]);
     }

     if(!microseconds) {    /* cancel timer */
        if(ukd->havealarmtimer) { /* set interval to zero forcing wakeup.  helper will die */
/*
     Races are avoided by using a heavy hand.   This is probably not common
     Intead of setting the interval to zero, we will simply destroy the wait object
     and make a new one.  The helper may, in fact, have died some time ago
*/
            {OC(Node_Fetch+NODEALARMWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+COMPWAITF);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(WaitF_Create);XB(0xE0F04510);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+NODEALARMWAIT);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
/* the timer domain will die when it discovers its wait object gone */
            ukd->havealarmtimer=0;
        }
     }
     else { /* microseconds is not zero, reset the wait object, make a timer if we don't have one */
        {OC(Node_Fetch+NODEALARMWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Wait_SetInterval);PS2(&(microseconds),sizeof(microseconds));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Wait_ShowTOD);XB(0x00F00000);RS2(&(ukd->alarmtod),sizeof(ukd->alarmtod));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }  /* save this for wakeup early test */
        ukd->microseconds=microseconds;   /* we reset the timer. it must check on return */
        if(!ukd->havealarmtimer) { /* must get one */
           makealarmtimer();
           ukd->havealarmtimer = 1;
        }
     }
     if(ukd->multithread) {
        ukd->alarmthread = ukd->threadid;
     }
     setreturn(drac,0,0);
}
/************************************************************************
     MAKEALARMTIMER
************************************************************************/
makealarmtimer()
{
    JUMPBUF;
    UINT32 rc;
    struct Domain_DataByte ddb = {DBTIMER};

   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
   {OC(Node_Fetch+NODEALARMWAIT);XB(0x00700000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
   if (!myfork()) {  /* a waiter domain */
        while(1) {
           {OC(Wait_Wait);XB(0x00A00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* wait for timer */
           if(rc) break;
           {OC(0);XB(0x00900000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }     /* notify keeper */
           if(!rc) break;      /* if non-zero, timer was reset, wait again */
        }
        myexit(0,1);
    }
}
/************************************************************************
     MAKEPOLLTIMER
************************************************************************/
makepolltimer()
{
    JUMPBUF;
    UINT32 rc;
    struct Domain_DataByte ddb = {DBPTIMER};

   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
   {OC(Node_Fetch+NODEPOLLWAIT);XB(0x00700000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
   if (!myfork()) {  /* a waiter domain */
        while(1) {
           {OC(Wait_Wait);XB(0x00A00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* wait for timer */
           if(rc) break;
           {OC(0);XB(0x00900000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }     /* notify keeper */
           if(!rc) break;      /* if non-zero, timer was reset, wait again */
        }
        myexit(0,1);
    }
}
/************************************************************************
     SETITIMER
************************************************************************/
dosetitimer(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     struct itimerval *newtime,*oldtime;
     int  timertype;
     long long value,interval;
     JUMPBUF;
     UINT32 rc;
     char buf[256];

     timertype = args[0];
     newtime = (struct itimerval *)(usermem+args[1]);
     oldtime = (struct itimerval *)(usermem+args[2]);

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"SETITIMER: %d %X %X ",timertype,args[1],args[2]);
     }
     if(timertype != ITIMER_REAL) {
         seterror(drac,EINVAL);
         return;
     }
     if(args[2]) {
         *oldtime = ukd->realtimer;
     }
     ukd->realtimer = *newtime;
     value = ukd->realtimer.it_value.tv_sec;
     value = value*1000000 + ukd->realtimer.it_value.tv_usec;
     interval = ukd->realtimer.it_interval.tv_sec;
     interval = interval*1000000 + ukd->realtimer.it_interval.tv_usec;

     if(ukd->truss) {
         sprintf(buf," v=%llX i=%llX",value,interval);
         strcat(ukd->trussbuf,buf);
     }

     if(ukd->haveitimertimer) {  /* one exists running or possibly not */
/*
     Races are avoided by using a heavy hand.   This is probably not common
     Intead of seting the interval to zero, we will simply destroy the wait object
     and make a new one
*/
            {OC(Node_Fetch+NODEITIMERWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(KT+4);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+COMPWAITF);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(WaitF_Create);XB(0xE0F04510);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+NODEITIMERWAIT);XB(0x8070F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
/* the timer domain will die when it discovers its wait object gone */
            ukd->haveitimertimer=0;
     }

     if(value)  {  /* there is a value */
        {OC(Node_Fetch+NODEITIMERWAIT);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Wait_SetInterval);PS2(&(value),sizeof(value));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
        if(!ukd->haveitimertimer) { /* must get one */
            makeitimertimer();
        }
     }
     if(ukd->multithread) {
        ukd->alarmthread = ukd->threadid;
     }
     setreturn(drac,0,0);
}
/************************************************************************
     MAKEITIMERTIMER
************************************************************************/
makeitimertimer()
{
     JUMPBUF;
     UINT32 rc;
     long long value,interval;
     struct Domain_DataByte ddb = {DBITIMER};

      {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
      {OC(Node_Fetch+NODEITIMERWAIT);XB(0x00700000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
      if (!myfork()) {  /* a waiter domain, has access to ukd->realtimer */
          while(1) {
             {OC(Wait_Wait);XB(0x00A00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* wait for timer */
             if(rc) break;
             {OC(0);XB(0x00900000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }     /* notify keeper */
             if(rc) break;
/* if value or interval is going to be zero the parent set rc=1 and haveitimer=0 */
             value = ukd->realtimer.it_value.tv_sec;
             value = value*1000000 + ukd->realtimer.it_value.tv_usec;
             interval = ukd->realtimer.it_interval.tv_sec;
             interval = interval*1000000 + ukd->realtimer.it_interval.tv_usec;
             {OC(Wait_SetInterval);PS2(&(interval),sizeof(interval));XB(0x04A00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
             if(rc) break;   /* if ok go wait again and ping keeper */
          }
          myexit(0,1);
      }
      ukd->haveitimertimer = 1;
}
/************************************************************************
     GETITIMER
************************************************************************/
dogetitimer(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
          sprintf(ukd->trussbuf,"GETITIMER %X NOTIMPLEMENTED",args[0]);
     }
     setreturn(drac,0,0);
}
/************************************************************************
     SIGSUSPEND
************************************************************************/
dosigsuspend(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     sigset_t *ss;

     if(ukd->truss) {
          sprintf(ukd->trussbuf,"SIGSUSPEND %X",args[0]);
     }
     ss = (sigset_t *)(usermem+args[0]);

     ukd->uthreads[ukd->threadid].sa_mask = *ss;  /* set the mask */

     ukd->uthreads[ukd->threadid].drac = *drac;  /* save state */
     ukd->uthreads[ukd->threadid].status = THREADWAITING;
     ukd->uthreads[ukd->threadid].waitcode = WAITSIGNAL;
}
/************************************************************************
     SIGPENDING (pending,fillset)
************************************************************************/
dosigpending(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     sigset_t *ss;
     int i;
     char buf[256];

     if(args[0] == 2) {   /* SIGFILLSET */
        if(ukd->truss) {
           sprintf(ukd->trussbuf,"SIGFILSET(%X %X)", args[0],args[1]);
        }
        ss = (sigset_t *)(usermem+args[1]);
        for(i=1;i< _SIGRTMAX;i++) {
           sigaddset(ss,i);
        }
#ifdef xx
sprintf(buf,"SIGFILLSET      %X %X\n",ss->__sigbits[0],ss->__sigbits[1]);
outsok(buf);
#endif
        setreturn(drac,0,0);
        return;
     }
     seterror(drac,EINVAL);
}
/************************************************************************
     SIGNOTIFYWAIT

     TODO     THIS IS UNDOCUMENTED.  TRUSS SHOWS NO ARGUMENTS
************************************************************************/
dosignotifywait(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"SIGNOTIFYWAIT: %X NOTIMPLEMENTED",args[0]);
     }
     /* seems we just put task to sleep */
     ukd->uthreads[ukd->threadid].status = THREADWAITING;
     ukd->uthreads[ukd->threadid].waitcode = WAITSIGNAL;
     ukd->uthreads[ukd->threadid].drac = *drac;

     return;
}
/************************************************************************
     GETTIMEOFDAY
************************************************************************/
dogettimeofday(drac)
     struct Domain_SPARCRegistersAndControl *drac;
{
     JUMPBUF;
     UINT32 rc;
     unsigned long long tod;
     unsigned long long seconds ;
     unsigned long long y70 = 2208988800ll;
     unsigned long useconds;
     unsigned long lseconds;
     char buf[256];

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"GETTIMEOFDAY: ");
     }

     {OC(COMPCLOCK);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Clock_TOD_BINEPOC+100);XB(0x00F00000);RC(rc);RS2(&(tod),sizeof(tod));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
     if(rc) {
         setreturn(drac,0,0);
         return;
     }

     seconds = tod >> 12;
     useconds = seconds % 1000000;
     seconds = seconds / 1000000;
//     seconds = seconds - y70;
     lseconds = seconds;

     seconds=useconds;
     seconds= seconds*1000000;
     seconds= seconds/1048576;
     useconds=seconds;

     useconds = useconds*1000;

     setreturn(drac,lseconds,useconds);

     if(ukd->truss) {
         sprintf(buf,"%2d - ",ukd->threadid+1);
         outsok(buf);
         outsok(ukd->trussbuf);
     }
}
/************************************************************************
     TIME
************************************************************************/
dotime(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     unsigned long long tod;
     unsigned long long seconds ;
     unsigned long long y70 = 2208988800ll;
     unsigned long useconds;
     char buf[256];

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"TIME: %X",args[0]);
     }

     {OC(COMPCLOCK);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Clock_TOD_BINEPOC+100);XB(0x00F00000);RC(rc);RS2(&(tod),sizeof(tod));NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
     if(rc) {
         seterror(drac,EINVAL);
         return;
     }

     seconds = tod >> 12;

     seconds = seconds / 1000000;
//     seconds = seconds - y70;
     useconds = seconds;

     if(args[0]) {
         putuser(&useconds,args[0],4);
     }
     setreturn(drac,useconds,0);
}

/************************************************************************
     GETRLIMIT
************************************************************************/
dogetrlimit(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     struct rlimit *rl;

     if(ukd->truss) {
        sprintf(ukd->trussbuf,"GETRLIMIT %X %X", args[0],args[1]);
     }

     if(args[0] == RLIMIT_STACK) {
        rl = (struct rlimit *)(usermem+args[1]);
        rl->rlim_max=STACKTOP-STACKBOTTOM;  /* 16 meg */
        rl->rlim_cur=STACKTOP-drac->Regs.o[6];
        setreturn(drac,0,0);
     }
     else {
        seterror(drac,EINVAL);
     }
}

/************************************************************************
     PATHCONF
************************************************************************/
dopathconf(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
        sprintf(ukd->trussbuf,"PATHCONF %X %X", args[0],args[1]);
     }

     if(args[1] == _PC_PATH_MAX) {
        setreturn(drac,256);
     }
     else {
        seterror(drac,EINVAL);
     }
}
/************************************************************************
     LWP_SELF
************************************************************************/
dolwpself(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         strcpy(ukd->trussbuf,"LWP_SELF: ");
     }
     setreturn(drac,ukd->threadid+1,0);
}
/************************************************************************
     LWP_CREATE
************************************************************************/
dolwpcreate(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;
     ucontext_t *uc;
     unsigned long flags;
     lwpid_t *new_lwp;
     int threadid,i;
     char buf[256];

     uc = (ucontext_t *)(usermem+args[0]);
     flags = args[1];
     new_lwp = (lwpid_t *)(usermem+args[2]);

     threadid=0;
     for(i=1;i<MAXTHREADS;i++) {
         if(ukd->uthreads[i].status == THREADAVAILABLE) {
              threadid=i;
              break;
         }
     }
     if(!threadid) {
         seterror(drac,EINVAL);
         return;
     }

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_CREATE %X %X %X",args[0],args[1],args[2]);
     }

/* Now create a domain and set initial conditions */
     ucontext2drac(uc, &(ukd->uthreads[threadid].drac), &(ukd->uthreads[threadid].sa_mask));

     makelwp(threadid);
     ukd->multithread=1;

     if(args[2]) {
         *new_lwp = threadid+1;
     }
     if(flags & LWP_DETACHED) {
         ukd->uthreads[threadid].flags = DETACHED;
     }
     if(flags & __LWP_ASLWP) {
         ukd->uthreads[threadid].flags = ASLWP;
     }
     if(flags & LWP_SUSPENDED)  {
          ukd->uthreads[threadid].status = THREADSUSPENDED;
          setreturn(drac,threadid+1,0);
          return;
     }
/* looks like the lwp is supposed to run now */

     ukd->uthreads[threadid].status = THREADRUNNING;

     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+threadid);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_MakeBusy);XB(0x00D00000);RC(rc);NB(0x0880E000);cjcc(0x00000000,&_jumpbuf); }


     {OC(Domain_ResetSPARCStuff);PS2(&(ukd->uthreads[threadid].drac),sizeof(ukd->uthreads[threadid].drac));XB(0x14D0000E); }

     {fj(0x08000000,&_jumpbuf); }                                  /* start domain */

     setreturn(drac,threadid+1,0);
     return;
}
/************************************************************************
     MAKELWP
************************************************************************/
makelwp(threadid)
     int threadid;
{
     JUMPBUF;
     UINT32 rc;
     struct Domain_DataByte ddb;
     int i;

     ukd->uthreads[threadid].schedctl.sc_state=SC_RUN;
     ukd->uthreads[threadid].schedctl.sc_cpu=1;
     ukd->uthreads[threadid].schedctl.sc_priority=SC_IGNORE;

     {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(DC_CreateDomain);XB(0x80F0E000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
/* we need a Domain_CopySelf like CopyCaller  OK, we only need to copy the keys */
     for(i=0;i<16;i++)  { /* copy all keys */
         {OC(Domain_GetKey+i);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
         {OC(Domain_SwapKey+i);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
     }
     {OC(Domain_SwapKey+3);XB(0x80D0D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* own domain key in 3 */

     {OC(Domain_GetMemory);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_SwapMemory);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_GetMeter);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_SwapMeter);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

     ddb.Databyte=threadid;
     {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
     {OC(Domain_SwapKeeper);XB(0x80D0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Swap+threadid);XB(0x80E0D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }  /* save in domain nodes */
}

/************************************************************************
     WAKELWP
************************************************************************/
wakelwp(threadid)
     int threadid;
{
     JUMPBUF;
     UINT32 rc;
     char buf[256];

     if(ukd->truss) {
         sprintf(buf,"WAKEUP THREAD %d\n",threadid+1);
         outsok(buf);
     }
     {OC(Node_Fetch+NODEUDOMS);XB(0x00700000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Node_Fetch+threadid);XB(0x00D00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }   /* the domain key */
     {OC(Domain_MakeBusy);XB(0x00D00000);RC(rc);NB(0x0880E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_ResetSPARCStuff);PS2(&(ukd->uthreads[threadid].drac),sizeof(ukd->uthreads[threadid].drac));XB(0x14D0000E); }

     {fj(0x08000000,&_jumpbuf); }

}
/************************************************************************
     LWP_SCHEDCTL
************************************************************************/
dolwpschedctl(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int i,fh;
     door_info_t *di;
     sc_shared_t **ptr;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_SCHEDCTL: %X %X %X NOTIMPLMENTED",args[0],args[1],args[2]);
     }

     if(args[0] & SC_DOOR) {   /* returns the schedule activation door id */
        di = ukd->filehandles[ukd->scheddoor].door;
        if(di) {
             fh = nextfilehandle();
             ukd->filehandles[fh].filet=0;  // mark as in use
             ukd->filehandles[fh].flags=DOOR;  // special case
             ukd->filehandles[fh].door=di;
             setreturn(drac,fh,0);
             return;
        }
        else {
            seterror(drac,EINVAL);
            return;
        }
     }
     else {
        if(args[1] != -1) {   /* args[1] is the door id of the scheduling door */
            ukd->scheddoor = args[1];
        }
        if(args[0] & SC_BLOCK) {
            ukd->uthreads[ukd->threadid].flags |= SCHED_BLOCK;
        }
        if(args[0] & SC_STATE) {
            ukd->uthreads[ukd->threadid].flags |= SCHED_STATE;
        }
        if(args[2] != 0) {  /* return state pointer */
            ptr=(sc_shared_t **)(usermem+args[2]);
            *ptr =(sc_shared_t *)((char *)(&(ukd->uthreads[ukd->threadid].schedctl))-usermem);
        }
     }


     setreturn(drac,0,0);
}
/************************************************************************
     LWP_CONTINUE
************************************************************************/
dolwpcontinue(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int threadid;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_CONTINUE:(%d)", args[0]);
     }

     threadid = args[0] - 1;
     if(threadid > MAXTHREADS) {
         seterror(drac,EINVAL);
         return;
     }

     if(ukd->uthreads[threadid].status != THREADSUSPENDED) {
         seterror(drac,EINVAL);
         return;
     }
     ukd->uthreads[threadid].status = THREADRUNNING;

     wakelwp(threadid);

     setreturn(drac,0,0);
}
/************************************************************************
     LWP_COND_WAIT

     TODO timed wait
************************************************************************/
dolwpcondwait(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_cond_t *cp;
     lwp_mutex_t *mp;
     int i;
     int waiters;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_COND_WAIT:  %X %X %X ",args[0],args[1],args[2]);
     }
     cp = (lwp_cond_t *)(usermem+args[0]);
     mp = (lwp_mutex_t *)(usermem+args[1]);

     waiters=mp->lock.lock64.pad[7];
     mp->lock.lock64.pad[7]=0;  /* clear waiters */
     mp->lock.lock64.pad[4]=0;  /* unlock */

     if(waiters != 0) { /* there are lock waiters */
        for(i=0;i<MAXTHREADS;i++) {
           if(ukd->uthreads[i].status == THREADWAITING) {
               if(ukd->uthreads[i].waitcode == WAITMUTEX) {
                   if(ukd->uthreads[i].waitobject == args[1]) {
                         ukd->uthreads[i].waitcode = 0;
                         ukd->uthreads[i].status = THREADRUNNING;
                         wakelwp(i);
                         break;
                   }
               }
            }
        }
     }

     ukd->uthreads[ukd->threadid].drac = *drac;
     ukd->uthreads[ukd->threadid].status = THREADWAITING;
     ukd->uthreads[ukd->threadid].waitcode = WAITCONDV;
     ukd->uthreads[ukd->threadid].waitobject = args[0];


     return;
}
/************************************************************************
     LWP_COND_SIGNAL
************************************************************************/
dolwpcondsignal(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_cond_t *cp;
     int i;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_COND_SIGNAL:  %X",args[0]);
     }
     for(i=0;i<MAXTHREADS;i++) {
        if(ukd->uthreads[i].status == THREADWAITING) {
            if(ukd->uthreads[i].waitcode == WAITCONDV) {
                 if(ukd->uthreads[i].waitobject == args[0]) {
                      ukd->uthreads[i].waitcode = 0;
                      ukd->uthreads[i].status = THREADRUNNING;
                      setreturnquiet(&(ukd->uthreads[i].drac),0,0); /* advance pc */
                      wakelwp(i);
                      break;
                 }
             }
         }
     }
     setreturn(drac,0,0);
}
/************************************************************************
     LWP_COND_BROADCAST
************************************************************************/
dolwpcondbroadcast(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_cond_t *cp;
     int i;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_COND_BROADCAST:  %X",args[0]);
     }
     for(i=0;i<MAXTHREADS;i++) {
        if(ukd->uthreads[i].status == THREADWAITING) {
            if(ukd->uthreads[i].waitcode == WAITCONDV) {
                 if(ukd->uthreads[i].waitobject == args[0]) {
                      ukd->uthreads[i].waitcode = 0;
                      ukd->uthreads[i].status = THREADRUNNING;
                      setreturnquiet(&(ukd->uthreads[i].drac),0,0); /* advance pc */
                      wakelwp(i);
                 }
             }
         }
     }
     setreturn(drac,0,0);
}
/************************************************************************
     LWP_SEMA_POST

     TODO check address for validity
************************************************************************/
dolwpsemapost(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_sema_t *sp;
     int i;
     char buf[256];

     sp = (lwp_sema_t *)(usermem+args[0]);

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_SEMA_POST: %X(%d)",args[0],sp->count);
     }

     sp->count++;
     if(sp->count > 0) {   /* see if anyone waiting */
         for(i=0;i<MAXTHREADS;i++) {
/* DIAGNOSTICS here */
            if(ukd->uthreads[i].status == THREADWAITING) {
                if(ukd->uthreads[i].waitcode == WAITSEMA) {
                    if(ukd->uthreads[i].waitobject == args[0]) {
                          ukd->uthreads[i].waitcode = 0;
                          ukd->uthreads[i].status = THREADRUNNING;
                          setreturnquiet(&(ukd->uthreads[i].drac),0,0); /* advance pc */
                          sp->count--;
                          wakelwp(i);
                          break;
                    }
                }
             }
         }
     }
     setreturn(drac,0,0);
}
/************************************************************************
     LWP_SEMA_WAIT
************************************************************************/
dolwpsemawait(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_sema_t *sp;

     sp = (lwp_sema_t *)(usermem+args[0]);
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_SEMA_WAIT: %X(%d)",args[0],sp->count);
     }

     if(sp->count > 0) {  /* wait is satisfied */
         sp->count--;
         setreturn(drac,0,0);
         return;
     }

/* put to sleep */
     ukd->uthreads[ukd->threadid].drac = *drac;
     ukd->uthreads[ukd->threadid].status = THREADWAITING;
     ukd->uthreads[ukd->threadid].waitcode = WAITSEMA;
     ukd->uthreads[ukd->threadid].waitobject = args[0];

     return;
}
/************************************************************************
     LWP_SEMA_TRYWAIT
************************************************************************/
dolwpsematrywait(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_sema_t *sp;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_SEMA_TRYWAIT: %X",args[0]);
     }
     sp = (lwp_sema_t *)(usermem+args[0]);

     if(sp->count > 0) {  /* wait is satisfied */
         sp->count--;
         setreturn(drac,0,0);
         return;
     }
     seterror(drac,EBUSY);

     return;
}
/************************************************************************
     LWP_MUTEX_LOCK
************************************************************************/
dolwpmutexlock(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_mutex_t *mp;
     int lock;
     char buf[256];
     unsigned char *ptr;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_MUTEX_LOCK: %X",args[0]);
     }
     mp = (lwp_mutex_t *)(usermem+args[0]);

#ifdef xx
 ptr=(char *)mp;
 sprintf(buf,"LOCK1: %02X%02X%02X%02x %02X%02X%02X%02x %02X%02X%02X%02x %02X%02X%02X%02x \n",
     *ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5),*(ptr+6),
     *(ptr+7),*(ptr+8),*(ptr+9),*(ptr+10),*(ptr+11),*(ptr+12),*(ptr+13),
     *(ptr+14),*(ptr+15));
 outsok(buf);
#endif

     lock = doswap(0xFF000001,&mp->lock.lock64.pad[4]);

#ifdef xx
 sprintf(buf,"LOCK2: %02X%02X%02X%02x %02X%02X%02X%02x %02X%02X%02X%02x %02X%02X%02X%02x \n",
     *ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5),*(ptr+6),
     *(ptr+7),*(ptr+8),*(ptr+9),*(ptr+10),*(ptr+11),*(ptr+12),*(ptr+13),
     *(ptr+14),*(ptr+15));
 outsok(buf);
#endif

     if(lock & 0xFF000000) {  /* still locked go to sleep */
          setreturnquiet(drac,0,0);    /* for when awakened by unlock */
          ukd->uthreads[ukd->threadid].drac = *drac;
          ukd->uthreads[ukd->threadid].status = THREADWAITING;
          ukd->uthreads[ukd->threadid].waitcode = WAITMUTEX;
          ukd->uthreads[ukd->threadid].waitobject = args[0];
          return;
     }
     mp->lock.lock64.pad[7]=0;   /* was unlocked by user, now locked by me */
     setreturn(drac,0,0);
}
/************************************************************************
     LWP_MUTEX_WAKEUP
************************************************************************/
dolwpmutexwakeup(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     lwp_mutex_t *mp;
     int i;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_MUTEX_WAKEUP: %X",args[0]);
     }
     mp = (lwp_mutex_t *)(usermem+args[0]);

     for(i=0;i<MAXTHREADS;i++) {
        if(ukd->uthreads[i].status == THREADWAITING) {
            if(ukd->uthreads[i].waitcode == WAITMUTEX) {
                if(ukd->uthreads[i].waitobject == args[0]) {
                      ukd->uthreads[i].waitcode = 0;
                      ukd->uthreads[i].status = THREADRUNNING;
                      wakelwp(i);
                      break;
                }
            }
         }
     }
     setreturn(drac,0,0);
}
/************************************************************************
     LWP_EXIT
************************************************************************/
dolwpexit(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     JUMPBUF;
     UINT32 rc;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"LWP_EXIT\n");
     }

     ukd->uthreads[ukd->threadid].status=THREADAVAILABLE;
     {OC(Domain_GetKey+dc);XB(0x00800000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
     {OC(Domain_GetKey+psb);XB(0x00800000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
     {OC(DC_DestroyDomain);XB(0xC0F08E00);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

/* caller just became a DK0, as did udom */

     return;  /* will cause keeper to become available */
}
/************************************************************************
     DOOR

     door_bind is a mystery.
************************************************************************/
dodoor(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     switch(args[5]) {
     case 0:  /* create */
          dodoorcreate(drac,args);
          break;
     case 1:  /* revoke */
          dodoorrevoke(drac,args);
          break;
     case 2:  /* info */
          dodoorinfo(drac,args);
          break;
     case 3:  /* call */
          dodoorcall(drac,args);
          break;
     case 4:  /* return */
          dodoorreturn(drac,args);
          break;
     case 6:
          dodoorbind(drac,args);
          break;
     default:
          if(ukd->truss) {
               sprintf(ukd->trussbuf,"DOOR[%d]: NOT IMPLEMENTED",args[5]);
          }
          setreturn(drac,0,0);
          break;
     }
     return;
}
/************************************************************************
     DOOR_CREATE
************************************************************************/
dodoorcreate(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int fh;
     door_info_t *di;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_CREATE: %X %X %X NOTIMPLEMENTED",args[0],
           args[1],args[2]);
     }
     fh=nextfilehandle();
     if(!fh) {
         seterror(drac,EMFILE);
         return;
     }

     di=nextdoor();
     if(!di) {
        seterror(drac,EMFILE);
        return;
     }

     di->di_target=1;  /* pid */
     di->di_proc=args[0];
     di->di_data=args[1];
     di->di_attributes = DOOR_LOCAL | args[2];
     ukd->doorunique++;
     di->di_uniquifier = ukd->doorunique;

/* TODO DOOR stuff */
     ukd->filehandles[fh].filet=0;  // mark as in use
     ukd->filehandles[fh].flags=DOOR;  // special case
     ukd->filehandles[fh].door=di;

     setreturn(drac,fh,0);
     return;
}
/************************************************************************
     DOOR_RETURN
************************************************************************/
dodoorreturn(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     door_info_t *di;
     unsigned long fp,sp;

     di=ukd->uthreads[ukd->threadid].door;  /* bound to this door */

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_RETURN: %X %X %X NOTIMPLEMENTED",args[0],
           args[1],args[2]);
     }

     ukd->uthreads[ukd->threadid].drac = *drac;
     ukd->uthreads[ukd->threadid].status = THREADDOORRETURN;

     setreturn(drac,0,0);
}
/************************************************************************
     DOOR_BIND
************************************************************************/
dodoorbind(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     int doorid;
     door_info_t *di;

     doorid=args[0];
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_BIND: %X NOTIMPLEMENTED",doorid);
     }

     if(doorid > MAXFILEHANDLES) {
         seterror(drac,EMFILE);
         return;
     }
     if(!(ukd->filehandles[doorid].flags & DOOR)) {
         seterror(drac,EINVAL);
         return;
     }
     di = ukd->filehandles[doorid].door;
     ukd->uthreads[ukd->threadid].door = di;

     setreturn(drac,0,0);
}
/************************************************************************
     DOOR_CALL
************************************************************************/
dodoorcall(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_CALL: %X %X %X NOTIMPLEMENTED",args[0],
           args[1],args[2]);
     }
     ukd->uthreads[ukd->threadid].drac = *drac;
     ukd->uthreads[ukd->threadid].status = THREADDOORCALL;

     setreturn(drac,0,0);
}
/************************************************************************
     DOOR_REVOKE
************************************************************************/
dodoorrevoke(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_REVOKE: %XNOTIMPLEMENTED",args[0]);
     }
     setreturn(drac,0,0);
}
/************************************************************************
     DOOR_INFO
************************************************************************/
dodoorinfo(drac,args)
     struct Domain_SPARCRegistersAndControl *drac;
     UINT32 *args;
{
     door_info_t *di;
     int fd;

     if(ukd->truss) {
         sprintf(ukd->trussbuf,"DOOR_INFO: %X",args[0]);
     }
     di =(door_info_t *)(usermem+args[1]);
     fd = args[0];

     *di = *(ukd->filehandles[fd].door);

     setreturn(drac,0,0);
}
/************************************************************************
      DOSWAP

      doswap(value,location)   on word boundary
************************************************************************/
asm("
	.type doswap, #function
doswap:
        retl
        swap [%o1],%o0
");
