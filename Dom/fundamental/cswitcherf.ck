/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h" 
#include "tssf.h"
#include "cswitcherf.h"
#include "domain.h"
#include "node.h"
#include "snode.h"
#include "sb.h"
#include "ocrc.h"
#include "dc.h"
#include "cck.h"
#include "tdo.h"
#include "sia.h"
#include "mkeeper.h"
#include "clock.h"
#include "consdefs.h"
#include <strings.h>


   KEY comp     = 0;
#define COMPDKC  1
#define COMPTDOF  2
#define COMPTSSF 3
#define COMPSIAF 4
#define COMPPCSF 5
#define COMPMKEEPERF 6
#define COMPCLOCK 7
#define COMPDISCRIM 8

#define COMPCONSOLE 15

   KEY sb       = 1;
   KEY caller   = 2;
   KEY domkey   = 3;
   KEY psb      = 4;
   KEY meter    = 5;
   KEY dc       = 6;

   KEY record   = 7;
   KEY cnode    = 8;
   KEY tmmk     = 9;

   KEY sik      = 10;
   KEY sok      = 11;

   KEY cons     =12;  /* for debugging... the first slot to go */
   KEY k2       =13;
   KEY k1       =14;
   KEY k0       =15;

   char title [] = "CSWITCHERF";

/***********************************************************************************

   This Context switcher is a "port" of the cswitch.s code from the 370

   The purpose is to manage multiple environments (spaces).  Each environment
   has its own spacebank, meter, local directory of capabilities, and a command system.
   The environments share the user/ directory

   At the heart of the context switcher is a multiplexor that handles multiple
   terminal paths.  This will become a window manager in a desktop version of
   Pacific.

   There are 3 domains.

       The control branch reader and command interpreter
       The activity monitor
       The command domain which also supports the Switcher Key

   All state data is kept in a Record Collection so that the actual context switcher
   can be replaced while preserving all the contexts and their objects

   In this source the key slots for each domain will be specified separately
   so that the control branch reader will redefine every slot above 6
   and the activity monitor will redefine every slot above 6

   Some seldom used keys will be stored in the Memory node of the command domain
   as it is likely to need more.

   All domains will be defined by this source.  FORK() is used to define all
   non-command domain helpers.   Helpers include a domain to zap space and one
   to create new contexts.   These latter domains help insure that the user doesn't
   panic while waiting for lengthy operations to proceed.

   This is made from the Users Bank 

   CSwitcherf(0;sb,m,sb,UserDirectory,UserContextDB => c;UserZMK)

TODO HACK  Must plan for death by KT+4 for takeover

***********************************************************************************/

/***************************** MASTER RECORD *************************/

struct rmaster {
    char lname;
    char name;
};

/* rmaster node (mnode) used to be a copy of a parameter node */

#define MNODESIK   0  /* Zapper SIK - temp */
#define MNODESOK   1  /* Zapper SOK - temp */
#define MNODECCK   2  /* Zapper CCK */
#define MNODEZMK   3  /* The USER's ZMK */
#define MNODETMMK  4  /* Users TMMK key */
#define MNODERC    5  /* The Master Record Collection */
#define MNODEUSER  6  /* user/ directory */
#define MNODESW    7  /* Entry key for Context Switcher key segments */
#define MNODESIA   8  /* SIA for bids */
#define MNODECBCCK 9  /* Control Branch CCK */
#define MNODEPCS   10 /* Comand System Factory to use */
#define MNODECENTRAL 11 /* Central domain start key */
#define MNODECURRENTCCK 12 /* current branch CCK (starts at control) */

#define MNODEDEATH 14 /* Death key for takeover */
#define MNODEOLDMNODE 15  /* for takeover */

/***************************** CONTEXT RECORD ************************/

struct rcontext {  /* 66 bytes */
    char lname;       /* LCONTEXTNAME - bname field is for searching */
#define LCONTEXTNAME 16
#define LBRANCHNAME  2
    char cname[LCONTEXTNAME];   /* context name Blank filled */
    char bname[LBRANCHNAME];    /* branch name Blank filled */
    char flags;       /* context state */
#define CRUNNING  0x80
#define CSTOPPED  0x40
    int  activity;    /* count of active branches */
    long long tod;    /* creation time */
    char reserved[34];
};

/* context node (cnode) */

#define CNODEPSB  0   /* context spacebank */
#define CNODESB   1   /* context spacebank */
#define CNODEM    2   /* context meter node */
#define CNODEDIR  3   /* context local directory */ 
#define CNODESIA  4   /* branch name sia */
#define CNODEMN   5   /* MNODE */
#define CNODESW   6   /* switcher node key for this context */
#define CNODEK1   7   /* temp need these for zmk/branch create */
#define CNODEK2   8   /* temp */
#define CNODEK3   9   /* temp */
#define CNODEK4   10  /* temp */
#define CNODEK5   11  /* temp */

/******************************* BRANCH RECORD ***********************/

struct rbranch {   /* 66 bytes */
    char lname;       /* 18 */
    char cname[LCONTEXTNAME];   /* context name Blank filled */
    char bname[LBRANCHNAME];    /* branch name Blank filled */
    char flags;       /* context state */
#define BACTIVE  0x80
    int  bnamenumber; /* sia value for branch */
    long long tod;    /* creation time */
    unsigned char bid[6];      /* bid value */
    char name[28];    /* description */
};

/* branch node (bnode) */

#define BNODECCK  0   /* branch CCK from MUX */
#define BNODEZMK  1   /* ZMK of branch zapper */
#define BNODECCKZ 2   /* CCK key of branch zapper */
#define BNODEBID  3   /* BID key */
#define BNODEK1   4
#define BNODEK2   5
#define BNODEK3   6
#define BNODEK4   7

/* Front End Node for Switcher key */

#define FENAME1   0   /* first 6 bytes of context name */
#define FENAME2   1   /* second 6 bytes of context name */
#define FENAME3   2   /* third 6 bytes of context name (trailing blanks) */

#define FEKEEPER  14
#define FEFORMAT  15

/****************************** BID RECORD *************************/

struct rbid {
    char lname;    /* length of name = 6 */
    unsigned char bid[6];   /* name is the bid */
    char cname[LCONTEXTNAME]; /* context name */
    char bname[LBRANCHNAME];  /* branch name */
};

#define SWITCHERDB 1
#define SWITCHERDEATHDB 2

/* Central domain ordercodes */

#define DBMONITOR        100   /* make active and interject bell */
#define DBCREATECONTEXT  101
#define DBSWITCHBRANCH   103
#define DBSWITCHACTIVE   104
#define DBSWITCHOUTPUT   105   /* switch output to control */
#define DBSENDASCII      106
#define DBPRINTSTRING    107
#define DBDESTROYCONTEXT 108
#define DBDESTROYBRANCH  109
#define DBDESCRIBEBRANCH 110
#define DBPRINTCONTEXT   111
#define DBPRINTCURRENT   112
#define DBSTARTCONTEXT   113
#define DBSTOPCONTEXT    114
#define DBRENAME         115
#define DBFREEZE         116
#define DBTHAW           117

    struct centraldata {
      char currentname[19];  /* name of current active branch */
      char lastname[19];     /* name of last branch active */
      unsigned char curbid[6];        /* current branch BID */
      int  masteract;        /* number of active branches */
    };

#define BADNAME 4
#define INTERNALERROR 5

    char blankC[LCONTEXTNAME+1]="                ";
    char blankCBR[LCONTEXTNAME+LBRANCHNAME+1]="                  ";
    char zeros[16] = "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

/*
   CSwitcherf(0;UserPSB,UserM,UserSB,UserDirectory,UserContextDB => ZMK)
*/

void doswitchercentral();
UINT32 doswitcher(UINT32,char *,int,int *, struct centraldata *);
UINT32 createbranch(char *,char *,struct centraldata *);
UINT32 disconnectbranch(char *,struct centraldata *);
UINT32 connectbranch(char *,char *,int *,struct centraldata *);
UINT32 destroybranch(char *,struct centraldata *);
UINT32 switchtobranch(char *,struct centraldata *);
UINT32 describebranch(char *,char *,int len,struct centraldata *);
void dowaiter();
void doreader();
void branchisactive(struct centraldata *);
UINT32 createcontext(char *,struct centraldata *);
UINT32 switchactive(struct centraldata *);
UINT32 switchoutput(struct centraldata *);
UINT32 sendascii(char *,struct centraldata *);
UINT32 printstring(char *,struct centraldata *);
UINT32 destroycontext(char *,struct centraldata *);
UINT32 printcontext(char *,struct centraldata *);
UINT32 printcurrent(struct centraldata *);
UINT32 startcontext(char *,struct centraldata *);
UINT32 stopcontext(char *,struct centraldata *);
UINT32 renamecontext(char *,struct centraldata *);
UINT32 freezecontext(char *,struct centraldata *);
UINT32 thawcontext(char *,struct centraldata *);

UINT32 getcontext(char *,struct rcontext *);
UINT32 switchactivebranch(struct rcontext *,struct centraldata *);
void putkey(char *,char *);
int makename(int,char *);
void squeezename(char *);
void tokenize(char *,char **,char **,char **);
int checkwild(struct rcontext *,char *);
int iswild(char *);
void trace(int);
void dberror(char *);
void outsok(char *); 
void mydump(char *,unsigned char *,int);


factory(factoc,factord)
   UINT32 factoc,factord;
{
   UINT32 oc,rc;
   JUMPBUF;
   struct rmaster master;
   struct rcontext context;
   struct rbranch branch;
   struct Domain_DataByte ddb;
   int takeover=0;
   char switchchar[2];
   int i;

   if(factoc != EXTEND_OC)           exit(INVALIDOC_RC);
   KC (caller,EXTEND_OC)  KEYSTO(k0,record,,caller) RCTO(rc);
                          /* k0 User/ */
   if(rc != CSwitcherF_Create)       exit(INVALIDOC_RC);

/* psb m and sb are user hard resources */
/* begin */

   KC (record,KT) RCTO(rc);
   if (rc != TDO_NSAKT)      exit(CSwitcher_InvalidRC);

   KC (psb,SB_CreateNode) KEYSTO(cnode) RCTO(rc);  /* really the masternode */
   if(rc)                    exit(NOSPACE_RC);
   KC (cnode,Node_Swap+MNODERC)  KEYSFROM(record);
   KC (cnode,Node_Swap+MNODEUSER) KEYSFROM(k0);

   ddb.Databyte = SWITCHERDB;
   KC (domkey,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(k0);
   KC (cnode,Node_Swap+MNODESW) KEYSFROM(k0);
   ddb.Databyte = SWITCHERDEATHDB;
   KC (domkey,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(k0);
   KC (cnode,Node_Swap+MNODEDEATH) KEYSFROM(k0);
   KC (domkey,Domain_MakeStart) KEYSTO(k0);
   KC (cnode,Node_Swap+MNODECENTRAL) KEYSFROM(k0);

   KC (comp,Node_Fetch+COMPPCSF) KEYSTO(k0);
   KC (cnode,Node_Swap+MNODEPCS) KEYSFROM(k0);   /* assume NEW Cswitcher */

/* find out if there is a master record (ie a takeover) */

   master.lname=1;
   master.name=0;
   KC (record,TDO_AddReplaceKey) STRUCTFROM(master) KEYSFROM(cnode) KEYSTO(k0) RCTO(rc);
   if(rc == 1) { /* taking over */
 /* all the old resources are in the node at k0 */
 /* Park this node in CNODE, set a flag, go ahead */
      KC (cnode,Node_Swap+MNODEOLDMNODE) KEYSFROM(k0);
      takeover=1;
   }

/* At point we need some facilities */
   
   KC (comp,Node_Fetch+COMPSIAF) KEYSTO(k0);
   KC (k0,SIAF_Create) KEYSFROM(psb,meter,sb) KEYSTO(k0);
          /* first small integer for BIDs will be 1 */
          /* control branch number will be zero, BID is DK(0) */
   KC (cnode,Node_Swap+MNODESIA) KEYSFROM(k0);   /* BID sia */

/* now the zapper */

   KC (comp,Node_Fetch+COMPTSSF) KEYSTO(k2);
   KC (k2,TSSF_CreateZMK) KEYSFROM(psb,meter,sb) KEYSTO(tmmk,sik,sok,k1);
   KC (cnode,Node_Swap+MNODEZMK) KEYSFROM(tmmk);
   KC (cnode,Node_Swap+MNODESIK) KEYSFROM(sik);
   KC (cnode,Node_Swap+MNODESOK) KEYSFROM(sok);
   KC (cnode,Node_Swap+MNODECCK) KEYSFROM(k1);

/* now the mux . Use a BID of DK(0) for a bid number of 0 */

   switchchar[0] = 0x1b;  /* escape */

   KC (k2,EXTEND_OC) KEYSFROM(psb,meter,sb) KEYSTO(,,,k2) RCTO(rc);
   if(rc != EXTEND_RC) crash("bad banks");
   KC (k2,EXTEND_OC) KEYSFROM(sik,sok,k1) KEYSTO(,,,k2) RCTO(rc);
   if(rc != EXTEND_RC) crash("bad TSSF");   /* next call includes BID key of DK(0) */

//KC (comp,COMPCONSOLE) KEYSTO(cons);
//KC (cons,0) KEYSTO(,,cons) RCTO(rc);
//KC (cons,CONCCK__START_LOG) RCTO(rc);

   KC (comp,0) KEYSTO(,,k1);
   KC (k2,TSSF_CreateTMMK) CHARFROM(switchchar,1)  KEYSFROM(k1) 
             KEYSTO(tmmk,sik,sok,k1);  /* control branch circuit */

   KC (cnode,Node_Swap+MNODETMMK) KEYSFROM(tmmk);
   KC (cnode,Node_Swap+MNODECBCCK) KEYSFROM(k1);
   KC (cnode,Node_Swap+MNODECURRENTCCK) KEYSFROM(k1);
   KC (cnode,Node_Swap+MNODESIK) KEYSFROM(sik);     /* pass to Reader  */
   KC (cnode,Node_Swap+MNODESOK) KEYSFROM(sok);     /* pass to Central (might be me) */

   if(takeover) {  /* now is a good time to move the branches and kill the old */

/* be sure to use the old BID SIA !! */
/* scan BID records */
/* snip off all branches from old MUX, add to new Mux, update branch records */
/* update context record */
/* SEE cswitchc.s  code */

      crash("No takeover yet");
   }

   if(!fork()) {   /* the reader domain */
      KC (comp,0) KEYSTO(,,caller);
      doreader();
      exit(0);
   }

   if(!fork()) {   /* the waiter domain */
      KC (comp,0) KEYSTO(,,caller);
      dowaiter();
      exit(0);
   }
   
   KC (cnode,Node_Fetch+MNODEZMK) KEYSTO(k0);
   LDEXBL (caller,0) KEYSFROM(k0);            /* return zapper */ 
   FORKJUMP();

   doswitchercentral();
 
/* PUT DEATH CODE HERE to reclaim all space */
/* used by takeover  TODO TODO HACK */

   exit(0);
    
}
/*****************************************************************************
    DOSWITCHERCENTRAL  - handle switcher key calls (keeper) and central DB

    INPUT -  Cnode is the master node
*****************************************************************************/
void doswitchercentral()
{
    JUMPBUF;
    UINT32 oc,rc;
    char buf[128];
    int actlen,len;
    short db;
    struct centraldata cd;

    strcpy(cd.currentname,blankC);
    strcpy(cd.lastname,blankC);
    cd.masteract=0;

    KC (cnode,Node_Fetch+MNODETMMK) KEYSTO(tmmk);
    KC (cnode,Node_Fetch+MNODESOK)  KEYSTO(sok);

    LDEXBL(comp,0);
    for(;;) {
       LDENBL OCTO(oc) KEYSTO(k0,k1,k2,caller) CHARTO(buf,128,actlen) DBTO(db);
       RETJUMP();

       len=0;
       switch(db) {
       case 0:       /* DB CENTRAL */
/***************************************************************************
          CENTRAL switch statement

          NOTE a BLANK context name means to use the current name
               but a context name of LCONTEXTNAME characters must be supplied
***************************************************************************/
         switch(oc) {
         case DBMONITOR:
            branchisactive(&cd);      /* k0,k1 or cck,bid */      
            rc=0;
            break;
         case DBCREATECONTEXT:
            if(actlen != LCONTEXTNAME) {rc=BADNAME;break;}
            rc=createcontext(buf,&cd);
            break;
         case DBSWITCHBRANCH:    /* shared with switcher routines */
            if(actlen != (LCONTEXTNAME+LBRANCHNAME)) {rc=BADNAME;break;}
            rc=switchtobranch(buf,&cd);
            break;
         case DBSWITCHACTIVE:
            rc=switchactive(&cd);
            break;
         case DBSWITCHOUTPUT:
            rc=switchoutput(&cd);
            break;
         case DBSENDASCII:
            rc=sendascii(buf,&cd);
            break;
         case DBPRINTSTRING: 
            buf[actlen]=0;
            rc=printstring(buf,&cd);
            break;
         case DBDESTROYCONTEXT:
            buf[actlen]=0;
            rc=destroycontext(buf,&cd);
            break;
         case DBDESTROYBRANCH:  
            if(actlen != (LCONTEXTNAME+LBRANCHNAME)) {rc=BADNAME;break;}
            rc=destroybranch(buf,&cd);
            break;
         case DBDESCRIBEBRANCH: /* shared with switcher routines */
            if(actlen < (LCONTEXTNAME+LBRANCHNAME)) {rc=BADNAME;break;}
            rc=describebranch(buf,buf+LCONTEXTNAME+LBRANCHNAME,actlen-18,&cd);
            break;
         case DBPRINTCONTEXT:
            if(actlen > LCONTEXTNAME) {rc=BADNAME;break;}
            buf[actlen]=0;
            rc=printcontext(buf,&cd);
            break; 
         case DBPRINTCURRENT:
            rc=printcurrent(&cd);
            break;
         case DBSTARTCONTEXT:
            if(actlen != LCONTEXTNAME) {rc=BADNAME;break;}
            rc=startcontext(buf,&cd);
            break; 
         case DBSTOPCONTEXT: 
            if(actlen != LCONTEXTNAME) {rc=BADNAME;break;}
            rc=stopcontext(buf,&cd);
            break; 
         case DBRENAME:
            if(actlen != 2*LCONTEXTNAME) {rc=BADNAME;break;}
            rc=renamecontext(buf,&cd);
            break; 
         case DBFREEZE:
            if(actlen != LCONTEXTNAME) {rc=BADNAME;break;}
            rc=freezecontext(buf,&cd);
            break; 
         case DBTHAW:
            if(actlen != LCONTEXTNAME) {rc=BADNAME;break;}
            rc=thawcontext(buf,&cd);
            break; 
         }
         break;

       case SWITCHERDB:
          rc=doswitcher(oc,buf,actlen,&len,&cd);
          break;
       case SWITCHERDEATHDB:
/* TODO TODO HACK */
          crash("Switcher Death"); 
          break;
       }
       LDEXBL (caller,rc) KEYSFROM(k0,k1,k2) CHARFROM(buf,len);
    }
}

/*****************************************************************************
    DOSWITCHER - user switcher key  k2 is the frontend node
                 The context name is in the frontend node
 
    INPUT      k2  is the frontend node
               buf,actlen
               oc is the key ordercode

    RETURN     k0,k1,k2   buf,len    len initialized to 0
*****************************************************************************/
UINT32 doswitcher(UINT32 oc,char *buf,int actlen,int *len,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    char cname[19];

/* get context name from node has 2 trailing blanks */

    if(oc == KT) return CSwitcher_AKT;

    KC (k2,Node_Fetch+FENAME1) KEYSTO(k1);
    KC (k1,0) CHARTO(cname,6) RCTO(rc);
    KC (k2,Node_Fetch+FENAME2) KEYSTO(k1);
    KC (k1,0) CHARTO(cname+6,6) RCTO(rc);
    KC (k2,Node_Fetch+FENAME3) KEYSTO(k1);
    KC (k1,0) CHARTO(cname+12,6) RCTO(rc);
    cname[LCONTEXTNAME]=0;

    switch(oc) {
    case Switcher_CreateBranch:      
       rc=createbranch(cname,buf,cd);  /* returns name */
       buf[2]=0;
       squeezename(buf);
       *len=strlen(buf); 
       return rc;
    case Switcher_DisconnectBranch:  
       if(actlen == 1) {buf[1]=buf[0];buf[0]=' ';}
       strncpy(cname+LCONTEXTNAME,buf,2);
       cname[18]=0;
       rc=disconnectbranch(cname,cd);      /* sets k0 to zmk */
       return rc;
    case Switcher_ConnectBranch:     
       rc=connectbranch(cname,buf,len,cd);  /* k0 has zmk */
       buf[2]=0;
       squeezename(buf);
       *len=strlen(buf); 
       return rc;
    case Switcher_DestroyBranch:      
       if(actlen == 1) {buf[1]=buf[0];buf[0]=' ';}
       strncpy(cname+LCONTEXTNAME,buf,2);
       cname[18]=0;
       rc=destroybranch(cname,cd);
       return rc; 
    case Switcher_SwitchToBranch:    
       if(actlen == 1) {buf[1]=buf[0];buf[0]=' ';}
       strncpy(cname+LCONTEXTNAME,buf,2);
       cname[18]=0;
       rc=switchtobranch(cname,cd);
       return rc;
    case Switcher_DescribeBranch:     
       strncpy(cname+LCONTEXTNAME,buf,2);
       cname[18]=0;
       rc=describebranch(cname,buf+3,actlen-2,cd);
       return;
    }

}

/*****************************************************************************
    CREATEBRANCH - create a new branch in the named context

    INPUT:  context name
            output buffer for branch name
            central data structure

    OUTPUT: branch name in output buffer
            k0-sik,k1-sok,k2-cck for branch
*****************************************************************************/
UINT32 createbranch(char *cname,char *buf,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    struct rcontext context;
    struct rbranch branch;
    struct rbid bidrecord;
    char pbuf[64];

    rc=getcontext(cname,&context);   /* sets cnode */
    if(rc) return BADNAME;          /* dup */

    memset(&branch,0,sizeof(branch));
    strncpy(branch.cname,context.cname,LCONTEXTNAME);
    branch.lname=LCONTEXTNAME+LBRANCHNAME;
    KC (cnode,Node_Fetch+CNODESIA) KEYSTO(k0);
    KC (k0,SIA_AllocateNewInteger) RCTO(rc);
    branch.bnamenumber = rc;
    rc=makename(rc,branch.bname);
    if (rc) {   /* number too big */
        KC (k0,SIA_FreeInteger+branch.bnamenumber);
        return BADNAME;
    }
//sprintf(pbuf,"Makename '%s' %d\n",branch.bname,branch.bnamenumber);
//outsok(pbuf);

    KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k0);
    KC (k0,Node_Fetch+MNODESIA) KEYSTO(k0);   /* SIA for BIDs */
    KC (k0,SIA_AllocateNewInteger) RCTO(rc);

    memcpy(branch.bid+2,&rc,4);  /* just convenient */
    KC (comp,COMPCLOCK) KEYSTO(k0);
    KC (k0,Clock_TOD_BINEPOC) CHARTO(&(branch.tod),8);

/* we are going to make the branch using cnode as the node for the scratch */
/* we need lots of key slots here */
/* 
   here is the plan   CNODEK1 will hold the real meter for a while
                      CNODEK2 will hold the SIK  for branch
                      CNODEK3 will hold the SOK
                      CNODEK4 will hold the CCK
*/
   KC (comp,Node_Fetch+COMPDKC) KEYSTO(k0);
   KC (k0,0) CHARFROM(branch.bid,6) KEYSTO(k0);   /* bid */

//KC (k0,0) CHARTO(pbuf,6) RCTO(rc);
//mydump("New Bid",(unsigned char *)pbuf,6);

   KC (tmmk,TMMK_CreateBranch) KEYSFROM(k0) KEYSTO(k0,k1,k2);
   KC (cnode,Node_Swap+CNODEK2) KEYSFROM(k0);
   KC (cnode,Node_Swap+CNODEK3) KEYSFROM(k1);
   KC (cnode,Node_Swap+CNODEK4) KEYSFROM(k2);

   KC (cnode,Node_Swap+CNODEK1) KEYSFROM(meter);    /* use user meter for all mux stuff */
//   KC (cnode,Node_Fetch+CNODEM) KEYSTO(meter);
//   KC (meter,Node_MakeMeterKey) KEYSTO(meter);
//   KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k1);
//   KC (cnode,Node_Fetch+CNODESB)  KEYSTO(k2);
   KC (comp,Node_Fetch+COMPTSSF) KEYSTO(k0);
   KC (k0,TSSF_CreateZMK) KEYSFROM(psb,meter,sb) KEYSTO(meter,k0,k1,k2);   /* now trickery */

   KC (cnode,Node_Swap+CNODEK2) KEYSFROM(k0) KEYSTO(k0); /* swap sik */
   KC (cnode,Node_Swap+CNODEK3) KEYSFROM(k1) KEYSTO(k1); /* swap sok */
   KC (cnode,Node_Swap+CNODEK4) KEYSFROM(k2) KEYSTO(k2); /* swap cck */

 /* at this point cnode hold zsik,zsok,zcck which are given to the caller */
 /* meter holds the ZMK key which goes into the branch node */
 /* k2 holds the branch CCK from the mux which goes into the branch node */

   KC (meter,ZMK_Connect) KEYSFROM(k0,k1,k2);

   KC (sb,SB_CreateNode) KEYSTO(k0);  /* this is the branch node */
   KC (k0,Node_Swap+BNODEZMK) KEYSFROM(meter);
   KC (cnode,Node_Fetch+CNODEK1) KEYSTO(meter);  /* restore meter */
   KC (k0,Node_Swap+BNODECCK) KEYSFROM(k2);      /* branch CCK from Mux */
   KC (cnode,Node_Fetch+CNODEK4) KEYSTO(k2);     /* CCK of zapper (to user) */
   KC (k0,Node_Swap+BNODECCKZ) KEYSFROM(k2);
 /* need BID key again... easier to remake than save */
   KC (comp,Node_Fetch+COMPDKC) KEYSTO(k1);
   KC (k1,0) CHARFROM(branch.bid,6) KEYSTO(k1);
   KC (k0,Node_Swap+BNODEBID) KEYSFROM(k1);     

/* BNODE is now complete */

//mydump("New Branch Record",(unsigned char *)&branch,64);

   KC (record,TDO_AddKey) STRUCTFROM(branch) KEYSFROM(k0);  /* wow recorded */

/* Now the BID record */

   memcpy(bidrecord.bid,branch.bid,6);
   memcpy(bidrecord.cname,branch.cname,LCONTEXTNAME);
   memcpy(bidrecord.bname,branch.bname,LBRANCHNAME);
   bidrecord.lname=6;

//mydump("Bid Record",(unsigned char *)&bidrecord,sizeof(bidrecord));

   KC (record,TDO_Add) STRUCTFROM(bidrecord);

   KC (cnode,Node_Fetch+CNODEK2) KEYSTO(k0);
   KC (cnode,Node_Fetch+CNODEK3) KEYSTO(k1);
   KC (cnode,Node_Fetch+CNODEK4) KEYSTO(k2);

   memcpy(buf,branch.bname,2);

   return 0;
}

/*****************************************************************************
    DISCONNECTBRANCH - snip off named branch and return zmk

    INPUT:  context,branch name
            central data structure
    
    OUTPUT  k0-zmk
*****************************************************************************/
UINT32 disconnectbranch(char *cname,struct centraldata *cd)   
{
}

/*****************************************************************************
    CONNECTBRANCH - connect zmk to the named branch and return name

    INPUT:  context name
            output buffer for branch name
            central data structure
            k0-zmk

    OUTPUT  branch name in output buffer
            branch name length 
*****************************************************************************/
UINT32 connectbranch(char *cname,char *buf,int *len,struct centraldata *cd) 
{
}

/*****************************************************************************
    DESTROYBRANCH - destroy the named branch

    INPUT:  Context,branch name
            central data structure

    OUTPUT:  none
*****************************************************************************/
UINT32 destroybranch(char *cname,struct centraldata *cd)
{
}

/*****************************************************************************
    SWITCHTOBRANCH - switch to the named branch

    INPUT:  Context,branch name  context name of blank means current
            central data structure

    OUTPUT: none

            return 1 if can't do
*****************************************************************************/
UINT32 switchtobranch(char *cname,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc,akt;
    struct rcontext context;
    struct rbranch branch;
    char buf[64];

    memset(&branch,0,sizeof(branch));
    memcpy(context.cname,cname,LCONTEXTNAME);
    if(!memcmp(context.cname,blankC,LCONTEXTNAME)) {
        memcpy(context.cname,cd->currentname,LCONTEXTNAME);
    }
    rc=getcontext(context.cname,&context);
    if(rc) return 1;
    if(context.flags & CSTOPPED) {
       strcpy(buf,"Context Stopped\n");
       KC (sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,sok) RCTO(rc);
       return 1;
    }

    memcpy(branch.cname,context.cname,LCONTEXTNAME);
    memcpy(branch.bname,cname+LCONTEXTNAME,2);
    branch.lname = LCONTEXTNAME+LBRANCHNAME;

//mydump("TOBRANCH",(unsigned char *)&branch,64);

    KC (record,TDO_GetEqual) STRUCTFROM(branch) STRUCTTO(branch) KEYSTO(k2) RCTO(rc);

//sprintf(buf,"Branch Record rc=%d\n",rc);
//outsok(buf);

    if(rc != 1) return 1;
    if(branch.flags & BACTIVE) {  /* must clear counters */
        branch.flags &= ~BACTIVE;
        if(context.activity) context.activity--;
        if(cd->masteract) cd->masteract--;
        KC (record,TDO_AddReplaceKey) STRUCTFROM(branch) KEYSFROM(k2) RCTO(rc);
        KC (record,TDO_AddReplaceKey) STRUCTFROM(context) KEYSFROM(cnode) RCTO(rc);
    }
    memcpy(cd->lastname,cd->currentname,LCONTEXTNAME);
    memcpy(cd->currentname,context.cname,LCONTEXTNAME);
    memcpy(cd->curbid,branch.bid,6);

    KC (sok,0) CHARFROM("]\n",2) KEYSTO(,,,sok) RCTO(rc);

    KC (k2,Node_Fetch+BNODECCK) KEYSTO(k0);
    KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0) RCTO(rc);
    KC (tmmk,TMMK_SwitchInput) KEYSFROM(k0) RCTO(rc);

    KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k1);
    KC (k1,Node_Swap+MNODECURRENTCCK) KEYSFROM(k1);

    return 0;
}

/*****************************************************************************
    DESCRIBEBRANCH - add a description to a branch

    INPUT:  context,branch name
            description
            description length
            central data structure
             
    OUTPUT: none
*****************************************************************************/
UINT32 describebranch(char *cname,char *buf,int len,struct centraldata *cd)
{
}

/*****************************************************************************
    BRANCHISACTIVE - a branch has become active

    INPUT:  central data structure
            k0-cck, k1-bid

    OUTPUT: none
*****************************************************************************/
void branchisactive(struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    struct rbid bidblk;
    struct rcontext context;
    struct rbranch branch;

    char buf[64];

    KC (tmmk,TMMK_BranchStatus) KEYSFROM(k0) RCTO(rc);
//sprintf(buf,"ACTIVEBR status %x\n",rc);
//outsok(buf);
//    if (!(rc & 0x04)) return;  /* not waiting for output */

    memset(&bidblk,0,sizeof(bidblk));
    KC (k1,0) CHARTO(bidblk.bid,6) RCTO(rc);  /* get bid */
    bidblk.lname=6;

//mydump("ACTIVEBIDBLK",(unsigned char *)&bidblk,sizeof(bidblk));
//sprintf(buf,"BID rc %X\n",rc);
//outsok(buf);

 /* if the current branch reports active it is because we switched away from */
 /* it to interject a bell and we should not get caught in a bell loop       */

 /* THIS IS WRONG.. */

//mydump("CentralData",(unsigned char *)cd,sizeof(*cd));

//    if(!memcmp(cd->curbid,bidblk.bid,6)) return; /* don't do for current */
//    if(!memcmp(bidblk.bid,zeros,6)) return; /* don't do for control */


    KC (record,TDO_GetEqual) STRUCTFROM(bidblk) STRUCTTO(bidblk) RCTO(rc);
//sprintf(buf,"GetBid Record RC %d\n",rc);
//outsok(buf);
    if(rc) return;   /* false alarm, PROBABLY INTERNAL ERROR */

//mydump("ACTIVEBIDBLK1",(unsigned char *)&bidblk,sizeof(bidblk));

    memcpy(context.cname,bidblk.cname,LCONTEXTNAME);
    context.lname=LCONTEXTNAME;
    KC (record,TDO_GetEqual) STRUCTFROM(context) STRUCTTO(context) KEYSTO(cnode) RCTO(rc);
    if (rc != 1) return;   /* INTERNAL ERROR */

    memcpy(branch.cname,bidblk.cname,LCONTEXTNAME);
    memcpy(branch.bname,bidblk.bname,LBRANCHNAME);
    branch.lname=LCONTEXTNAME+LBRANCHNAME;

    KC (record,TDO_GetEqual) STRUCTFROM(branch) STRUCTTO(branch) KEYSTO(k2) RCTO(rc);
    if (rc != 1) return;   /* INTERNAL ERROR */

    /* Mark branch as active */
    branch.flags |= BACTIVE;
    context.activity++;
    cd->masteract++;

    KC (record,TDO_AddReplaceKey) STRUCTFROM(context) KEYSFROM(cnode) RCTO(rc);
    KC (record,TDO_AddReplaceKey) STRUCTFROM(branch) KEYSFROM(k2) RCTO(rc);

 /* now interject a bell */

    KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k2);
    KC (k2,Node_Fetch+MNODECBCCK) KEYSTO(k0);  /* control branch cck */
    KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0);

    KC (sok,0) CHARFROM("\007",1) KEYSTO(,,,sok) RCTO(rc); /* a Bell */

    KC (k2,Node_Fetch+MNODECURRENTCCK) KEYSTO(k0);    
    KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0) RCTO(rc);  /* current branch cck */

    return; 
}

/*****************************************************************************
    CREATECONTEXT - create a new context

    INPUT:  context name
            central data structure
   
    OUTPUT: none
*****************************************************************************/
UINT32 createcontext(char *cname,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    struct rcontext context;
    char datakey[16];
    static struct Node_KeyValues EXformat = {15,15,
      {Format1K(0,13,15,14,0,0)}
    };

    rc=getcontext(cname,&context);      /* fetch context node to cnode */
    if(!rc) return BADNAME;    /* name already exists */
    KC (record,TDO_GetFirst) KEYSTO(k0) RCTO(rc);  /* put master node in k0 */
    if(rc != 1) return INTERNALERROR;

    KC (sb,SB_CreateNode) KEYSTO(cnode) RCTO(rc);
    KC (cnode,Node_Swap+CNODEMN) KEYSFROM(k0);        /* put master node in cnode */
    memset(&context,0,sizeof(context));
    memcpy(context.cname,cname,LCONTEXTNAME);
    memcpy(context.bname,blankC,LBRANCHNAME);
    context.lname=LCONTEXTNAME;
    context.flags=CRUNNING;                        /* it will be soon */
    KC (comp,COMPCLOCK) KEYSTO(k0);
    KC (k0,Clock_TOD_BINEPOC) CHARTO(&(context.tod),8);
    KC (record,TDO_AddKey) STRUCTFROM(context) KEYSFROM(cnode) RCTO(rc);
    if(rc) {
       KC (sb,SB_DestroyNode) KEYSFROM(cnode);
       return INTERNALERROR;
    }
    if(!fork()) {   /* do the work, cnode has the context node */
                    /* context has the initial record          */
        /* First Fill in CNODE with private resources */
        /* Then create switcher key */
        /* Then create a local directory and populate it   */
        /* Then create a command system and send it off */
        /* Then dissolve */

        KC (psb,SB_CreateBank) KEYSTO(k0);
        KC (cnode,Node_Swap+CNODEPSB) KEYSFROM(k0);
        KC (sb,SB_CreateBank) KEYSTO(k0);
        KC (cnode,Node_Swap+CNODESB) KEYSFROM(k0);
        KC (k0,SB_CreateNode) KEYSTO(k0);          /* meter node */ 
        KC (cnode,Node_Swap+CNODEM) KEYSFROM(k0);

//        memset(datakey,0,8);
//        memset(datakey+9,0xFF,7);
//        datakey[8] = 0x7F;                          /* largest 8 byte number */

        memset(datakey,0,9);      /* largest 7 byte postive number */
        memset(datakey+10,0xFF,7);
        datakey[9] = 0x7F;                     

        KC (comp,Node_Fetch+COMPDKC) KEYSTO(k2);
        KC (k2,1) CHARFROM(datakey,16) KEYSTO(k2) RCTO(rc);
        KC (k0,Node_Swap+1) KEYSFROM(meter);
        KC (k0,Node_Swap+3) KEYSFROM(k2);
        KC (k0,Node_Swap+4) KEYSFROM(k2);
        KC (k0,Node_Swap+5) KEYSFROM(k2);
                   /* make keeper from context space, use hard meter- to k2 */ 
        KC (comp,Node_Fetch+COMPMKEEPERF) KEYSTO(k2);
        KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k1);
        KC (cnode,Node_Fetch+CNODESB)  KEYSTO(k0);
        KC (k2,MKeeperF_Create) KEYSFROM(k1,meter,k0) KEYSTO(k2);

        KC (cnode,Node_Fetch+CNODEM) KEYSTO(k1);   /* install meter keeper */
        KC (k1,Node_Swap+2) KEYSFROM(k2);
                   /* make switcher front end node- to k2 */
        KC (k0,SB_CreateNode) KEYSTO(k2);
        KC (cnode,Node_Swap+CNODESW) KEYSFROM(k2);
        KC (k2,Node_WriteData) STRUCTFROM(EXformat);
        
        KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k0);  /* master node */
        KC (k0,Node_Fetch+MNODESW) KEYSTO(k1);
        KC (k2,Node_Swap+14) KEYSFROM(k1);
        KC (comp,Node_Fetch+COMPDKC) KEYSTO(k0);

        KC (k0,0) CHARFROM(context.cname,6) KEYSTO(k1);
        KC (k2,Node_Swap+FENAME1) KEYSFROM(k1);
        KC (k0,0) CHARFROM(context.cname+6,6) KEYSTO(k1);
        KC (k2,Node_Swap+FENAME2) KEYSFROM(k1);
        KC (k0,0) CHARFROM(context.cname+12,6) KEYSTO(k1);
        KC (k2,Node_Swap+FENAME3) KEYSFROM(k1);

                  /* make sia from context space, use hard meter- to k0 */ 
        KC (comp,Node_Fetch+COMPSIAF) KEYSTO(k0);
        KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k1);
        KC (cnode,Node_Fetch+CNODESB)  KEYSTO(k2);
        KC (k0,SIAF_Create) KEYSFROM(k1,meter,k2) KEYSTO(k0);
        KC (cnode,Node_Swap+CNODESIA) KEYSFROM(k0);
                  /* make local directory from context space, user hard meter- to k0 */
        KC (comp,Node_Fetch+COMPTDOF) KEYSTO(k0);
        KC (k0,TDOF_CreateNameSequence) KEYSFROM(k1,meter,k2) KEYSTO(k0);
        KC (cnode,Node_Swap+CNODEDIR) KEYSFROM(k0);

            /* must populate local directory  putkey puts k1 into k0 with name */

        KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k1);
        putkey("psb","Prompt Spacebank");
        KC (cnode,Node_Fetch+CNODESB) KEYSTO(k1);
        putkey("sb","Spacebank");
        KC (cnode,Node_Fetch+CNODEM) KEYSTO(k1);
        KC (k1,Node_MakeMeterKey) KEYSTO(k1);
        putkey("m","Meter");
        KC (cnode,Node_Fetch+CNODESW) KEYSTO(k1);
        KC (k1,Node_MakeFrontendKey) KEYSTO(k1);
        putkey("switcher","Switcher");
        KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k1);  /* master node */
        KC (k1,Node_Fetch+MNODEUSER) KEYSTO(k1);
        putkey("user/","User Directory");
        KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k1);
        KC (k1,Node_Fetch+MNODEZMK) KEYSTO(k1);
        putkey("zmk","Zapper");

           /* now build a pcs and pass local directory */

        KC (cnode,Node_Swap+CNODEK1) KEYSFROM(meter);  /* save for a bit */
        KC (cnode,Node_Fetch+CNODEM) KEYSTO(meter);
        KC (meter,Node_MakeMeterKey) KEYSTO(meter);
        KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k1);
        KC (cnode,Node_Fetch+CNODESB) KEYSTO(k2);

        KC (comp,Node_Fetch+COMPPCSF) KEYSTO(k0);
        KC (k0,EXTEND_OC) KEYSFROM(k1,meter,k2) KEYSTO(,,,k0) RCTO(rc);
        KC (k0,EXTEND_OC) KEYSTO(,,,k0) RCTO(rc);  /* odd but no sik,sok,cck */
        KC (cnode,Node_Fetch+CNODEDIR) KEYSTO(k1);
        LDEXBL (k0,0) KEYSFROM(k1);  /* back to the old standard */
        FORKJUMP();
        KC (cnode,Node_Fetch+CNODEK1) KEYSTO(meter);
        exit(0);  /* bye bye, good luck */
    }
    return 0;
}

/*****************************************************************************
    SWITCHACTIVE  - switch to the first branch with active output

    INPUT:  central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 switchactive(struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc,crc;
    struct rcontext context;
    struct rbid bidblk;
    int i;

/* first check in the current context */

    if(!cd->masteract) return 1;  /* nothing to do */

    if(memcmp(cd->curbid,zeros,6)) {  /* there is a current branch, check its context */
        memcpy(bidblk.bid,cd->curbid,6);
        bidblk.lname=6;
        KC(record,TDO_GetEqual) STRUCTFROM(bidblk) STRUCTTO(bidblk) RCTO(rc);
        if(!rc) {  /* search current context first */
           rc=getcontext(bidblk.cname,&context);
           if(!rc) {  /* have a record */
               if(context.activity) {  /* an alleged active branch */
                   if(!(context.flags & CSTOPPED)) {
                      rc=switchactivebranch(&context,cd);
                      if (!rc) return 0;  /* switched OK */
                   }
               }
           }
        }
    }
    /* current context did not contain an active branch */

    /* search all contexts, first context record is after last bid record */
    
    crc = 1;
    memset(&context,0,sizeof(context));
    for(i=0;i<4;i++) context.cname[i+2]=0xff;  /* maximum BID */
    context.lname=6;
    while (crc == 1) {  /* loop till run out of contexts */

//mydump("ScanContext",(unsigned char *)&context,64);

       KC (record,TDO_GetGreaterThan) STRUCTFROM(context) STRUCTTO(context)
              KEYSTO(cnode) RCTO(crc);

//mydump("ActiveContext",(unsigned char *)&context,64);

       if (crc != 1) break;   /* no more */
       if(context.activity) {  /* allegedly some activity */
           if(!(context.flags & CSTOPPED)) {
              rc=switchactivebranch(&context,cd);
              if(!rc) return 0;  /* switched ok */ 
           }
       }
       context.bname[0]=0xff;
       context.bname[1]=0xff;  /* max branch name */
       context.lname=LCONTEXTNAME+LBRANCHNAME;
    }
    return 1;  /* no active branch */
}

/*****************************************************************************
    SWITCHOUTPUT   - switch output to the control branch

    INPUT:  central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 switchoutput(struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    char buf[64];
    int actlen;

    memset(buf,0,64);
    KC (record,TDO_GetFirst) CHARTO(buf,64,actlen) KEYSTO(cnode) RCTO(rc);
//mydump("SwitchOUTPUT Master record",buf,actlen);
    if(rc != 1) return 1;
//outsok("SwitchOutput\n");
    KC (cnode,Node_Fetch+MNODECBCCK) KEYSTO(k0) RCTO(rc);
    KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0) RCTO(rc);
    KC (cnode,Node_Swap+MNODECURRENTCCK) KEYSFROM(k0);
    return 0;
}

/*****************************************************************************
    SENDASCII - make ascii string appear as input on current branch

    INPUT:  data buffer
            central data structure

    OUTPUT: none   rc=1 didn't work (no current branch)
*****************************************************************************/
UINT32 sendascii(char *buf,struct centraldata *cd)
{
    return 1;
}

/*****************************************************************************
    PRINTSTRING - print string on control branch output

    INPUT:  data buffer
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 printstring(char *buf,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;

    KC (sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,sok) RCTO(rc);
    return 0;
}

/*****************************************************************************
    DESTROYCONTEXT - reclaim branches and space for named context
 
    INPUT:  context name
            central data structure

    OUTPUT: none  
*****************************************************************************/
UINT32 destroycontext(char *cname,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    struct rcontext context;
    struct rbranch branch;
    struct rbid bidrecord;
    long bidv;

    memcpy(context.cname,blankC,LCONTEXTNAME);
    memcpy(context.cname,cname,strlen(cname));  
 
    if(getcontext(context.cname,&context)) return 1;

    KC (cnode,Node_Fetch+CNODEM) KEYSTO(k0);
    KC (k0,Node_Swap+3);

    /* loop through branches */

    memcpy(branch.cname,context.cname,LCONTEXTNAME);
    branch.bname[0]=0;
    branch.bname[1]=0;
    branch.lname=LCONTEXTNAME+LBRANCHNAME;

    while(1) {
       KC (record,TDO_GetGreaterThan) STRUCTFROM(branch) STRUCTTO(branch) KEYSTO(k0) RCTO(rc);
       if(rc != 1) break;

       if(memcmp(branch.cname,context.cname,8)) break;  /* not same context */
       KC (k0,Node_Fetch+BNODEZMK) KEYSTO(k1);
       KC (k1,DESTROY_OC) RCTO(rc);
       KC (k0,Node_Fetch+BNODECCK) KEYSTO(k1);
       KC (tmmk,TMMK_DestroyBranch) KEYSFROM(k1);
       
       KC (sb,SB_DestroyNode) KEYSFROM(k0);
       KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k0);
       memcpy(&bidv,&(branch.bid[2]),4);
       KC (k0,MNODESIA) KEYSTO(k0);
       KC (k0,bidv) RCTO(rc);  /* free integer */

       memcpy(bidrecord.bid,branch.bid,6);
       bidrecord.lname=6;
       KC (record,TDO_Delete) STRUCTFROM(bidrecord);

       KC (record,TDO_Delete) STRUCTFROM(branch);
    }
    if(context.activity) cd->masteract -= context.activity;  /* reduce active branch count */
    memcpy(cd->currentname,blankC,LCONTEXTNAME);
    memset(cd->curbid,0,6);

    KC (record,TDO_Delete) STRUCTFROM(context);
    
    if(!(rc=fork())) {  /* get a helper to do this long task */

       KC (cnode,Node_Fetch+CNODEM) KEYSTO(k0);
       KC (k0,Node_Swap+3);   /* stop meter */

       KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k0);
       KC (k0,DESTROY_OC);
       KC (cnode,Node_Fetch+CNODESB) KEYSTO(k0);
       KC (k0,DESTROY_OC);
       KC (sb,SB_DestroyNode) KEYSFROM(cnode);

       exit(0); 
    }
    if(rc > 1) {  /* ran out of space, do the zap in line */ 
       KC (cnode,Node_Fetch+CNODEM) KEYSTO(k0);
       KC (k0,Node_Swap+3);   /* stop meter */

       KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k0);
       KC (k0,DESTROY_OC);
       KC (cnode,Node_Fetch+CNODESB) KEYSTO(k0);
       KC (k0,DESTROY_OC);
       KC (sb,SB_DestroyNode) KEYSFROM(cnode);
    }
    return 0;
}

/*****************************************************************************
    PRINTCONTEXT - print status of a context or all if name is missing
                   Use wildcards (null is equal to *)
   
                   If a single context (no wildcards) then print branch 
                   details.

    INPUT:  context name or part thereof with *
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 printcontext(char *cname,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc,crc;
    struct rcontext context;
    struct rbranch branch;
    struct SB_FullStatisticsLL sbstat;
    long long ppages,pnodes,pages,nodes;
    char obuf[128];
    int i,didheader;
    long long metervalue;
    char datakey[16];
static long long maxmeter = 0x007FFFFFFFFFFFFF;
    long long meterdif;
    long seconds,mills;
    char cstate[16];
    char activity[8];
    char startingname[LCONTEXTNAME+1];
    char name[LCONTEXTNAME+1];
    char bstate[16];
    char bname[LBRANCHNAME+1];
    char datestr[32],ymd[9],hms[13],tz[4];
    
    memcpy(context.cname,blankC,LCONTEXTNAME);
    memcpy(context.cname,cname,strlen(cname));  
    memcpy(startingname,context.cname,LCONTEXTNAME);
    context.lname=LCONTEXTNAME;
    for(i=0;i<LCONTEXTNAME;i++) {
        if (context.cname[i] == '*') context.cname[i] = ' ';
    }

/* psb and SB represent the User spacebank */

    ppages=pages=pnodes=nodes=0;
    KC (psb,SB_QueryStatistics) STRUCTTO(sbstat) RCTO(rc);
    if(!rc) {
        ppages = sbstat.PageCreates-sbstat.PageDestroys;
        pnodes = sbstat.NodeCreates-sbstat.NodeDestroys;
    }    

    KC (sb,SB_QueryStatistics) STRUCTTO(sbstat) RCTO(rc);
    if(!rc) {
        pages = sbstat.PageCreates-sbstat.PageDestroys;
        nodes = sbstat.NodeCreates-sbstat.NodeDestroys;
    }    

    sprintf(obuf," Pages: %lld/%lld  Nodes: %lld/%lld Activity %d\n",
          ppages,pages,pnodes,nodes,cd->masteract);
    KC (sok,0) CHARFROM(obuf,strlen(obuf)) KEYSTO(,,,sok) RCTO(rc);

    didheader=0; 
    crc=1;
//mydump("Starting Context",(unsigned char *)&context,sizeof(context));
    while(crc == 1) {
        KC (record,TDO_GetGreaterEqual) STRUCTFROM(context) 
              STRUCTTO(context) KEYSTO(cnode) RCTO(crc); 
//mydump("Context",(unsigned char *)&context,sizeof(context));
        if(crc != 1) break;
        if(!checkwild(&context,cname)) break;   /* doesn't conform */
        if(!didheader) {
           didheader=1;
           strcpy(obuf,
             "Context Name     Pages P/NP    Nodes P/NP        Time Activity Status  Date Created\n");
           KC (sok,0) CHARFROM(obuf,strlen(obuf)) KEYSTO(,,,sok) RCTO(rc);
        }
        ppages=pnodes=pages=nodes=0;

        KC (cnode,Node_Fetch+CNODESB) KEYSTO(k0);
        KC (k0,SB_QueryStatistics) STRUCTTO(sbstat) RCTO(rc);
        pages = sbstat.PageCreates-sbstat.PageDestroys;
        nodes = sbstat.NodeCreates-sbstat.NodeDestroys;

        KC (cnode,Node_Fetch+CNODEPSB) KEYSTO(k0);
        KC (k0,SB_QueryStatistics) STRUCTTO(sbstat) RCTO(rc);
        ppages = sbstat.PageCreates-sbstat.PageDestroys;
        pnodes = sbstat.NodeCreates-sbstat.NodeDestroys;

        KC (cnode,Node_Fetch+CNODEM) KEYSTO(k0);
        KC (k0,Node_Fetch+3) KEYSTO(k0);  /* slot 3 is CPU meter */
        KC (k0,1) CHARTO(datakey,16) RCTO(rc);
        memcpy(&metervalue,&datakey[8],8);
        meterdif = maxmeter - metervalue;
        meterdif = meterdif/16;   /* to microseconds */
        meterdif = meterdif/1000;   /* to millisconds */
        seconds = meterdif;
        seconds = seconds / 1000;
        mills = meterdif;
        mills = mills % 1000;

        strcpy(cstate,"       ");
        if(context.flags & CSTOPPED) strcpy(cstate,"STOPPED"); 

        strcpy(activity,"   ");
        if(context.activity) sprintf(activity,"%3d",context.activity);

        memcpy(name,context.cname,LCONTEXTNAME);
        name[LCONTEXTNAME]=0;

        KC (comp,Node_Fetch+COMPCLOCK) KEYSTO(k0);
        KC (k0,Clock_TOD_ASCII) STRUCTFROM(context.tod) CHARTO(datestr,43) RCTO(rc);
        memcpy(ymd,datestr,8);
        ymd[8]=0;
        memcpy(hms,datestr+8,8);
        hms[8]=0;
        memcpy(tz,datestr+19,3);
        tz[3]=0;

        sprintf(obuf,"%16s   %5lld/%5lld   %5lld/%5lld %4d.%03d      %3s %7s %8s %8s %3s\n",
           name,ppages,pages,pnodes,nodes,seconds,mills,activity,cstate,ymd,hms,tz);

        KC (sok,0) CHARFROM(obuf,strlen(obuf)) KEYSTO(,,,sok) RCTO(rc);
        
        context.bname[0]=255;
        context.bname[1]=255;
        context.lname = LCONTEXTNAME+LBRANCHNAME;
    }
    if(!iswild(cname)) { /* only one context, do branch status report */
        memcpy(branch.cname,startingname,LCONTEXTNAME);
        branch.bname[0]=0;
        branch.bname[1]=0;
        branch.lname = LCONTEXTNAME+LBRANCHNAME;
      
        strcpy(obuf,"Branch Status\n");
        KC (sok,0) CHARFROM(obuf,strlen(obuf)) KEYSTO(,,,sok) RCTO(rc);
        
        crc = 1;
        while(crc == 1) {
            KC (record,TDO_GetGreaterThan) STRUCTFROM(branch) 
                STRUCTTO(branch) KEYSTO(k0) RCTO(crc);
            if(crc != 1) break;
            if(memcmp(branch.cname,startingname,LCONTEXTNAME)) break;

            KC (k0,Node_Fetch+BNODECCK) KEYSTO(k0);
            KC (tmmk,TMMK_BranchStatus) KEYSFROM(k0) RCTO(rc);
            if(rc & 0x20) strcpy(bstate,"Input");
            if(rc & 0x04) strcpy(bstate,"Output");
            if(branch.flags & BACTIVE) strcpy(bstate,"Output");
            memcpy(bname,branch.bname,LBRANCHNAME);
            bname[LBRANCHNAME]=0;
            squeezename(bname); 
            sprintf(obuf,"%6s %6s\n",bname,bstate);
            KC (sok,0) CHARFROM(obuf,strlen(obuf)) KEYSTO(,,,sok) RCTO(rc);
        }
           
    }
    return 0; 
}

/*****************************************************************************
    ISWILD  - checks for wild card in buf
 
    INPUT:  name
 
    OUTPUT: 0 no wild card
            1 wild card
*****************************************************************************/
iswild(char *buf) 
{
    while(*buf) {
       if(*buf == '*') return 1;
       buf++;
    }
    return 0;
}

/*****************************************************************************
    CHECKWILD - checks a context structure for compliance with name
    
    INPUT:  context pointer
            name pointer

    OUTPUT: 1 if conforms
            0 if doesn't
*****************************************************************************/
checkwild(struct rcontext *context,char *buf)
{
    char *ptr;

    ptr=context->cname;
    while(*buf) {
        if(*buf == '*') return 1;  /* yep conforms */
        if(*buf != *ptr) return 0; /* nope */
        ptr++;
        buf++;   /* next */
    }
    if(*ptr != ' ') return 0;     /* context does not match */
    return 1;    /* must conform */
}

/*****************************************************************************
    PRINTCURRENT - print the name of the current and last branches

    INPUT:  central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 printcurrent(struct centraldata *cd)
{
}

/*****************************************************************************
    STARTCONTEXT - restart a stopped context

    INPUT:  context name
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 startcontext(char *cname,struct centraldata *cd)
{
}

/*****************************************************************************
    STOPCONTEXT - stop a context

    INPUT:  context name
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 stopcontext(char *cname,struct centraldata *cd)
{
}

/*****************************************************************************
    RENAMECONTEXT - change the name of a context

    INPUT:  context name,context name
            central data structure

    OUTPUT: none     
*****************************************************************************/
UINT32 renamecontext(char *buf,struct centraldata *cd)
{
}

/*****************************************************************************
    FREEZECONTEXT - mark context as non-deletable
 
    INPUT:  context name
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 freezecontext(char *buf,struct centraldata *cd)
{
}

/*****************************************************************************
    THAWCONTEXT - unmake frozen state

    INPUT:  context name
            central data structure

    OUTPUT: none
*****************************************************************************/
UINT32 thawcontext(char *buf,struct centraldata *cd)
{
}

/*****************************************************************************
    DOWAITER  - wait for active branch
 
    INPUT -  Cnode is the master node
*****************************************************************************/
void dowaiter()
{
    JUMPBUF;
    UINT32 oc,rc;
    char buf[64];

    KC (cnode,Node_Fetch+MNODETMMK) KEYSTO(tmmk);
    KC (cnode,Node_Fetch+MNODECENTRAL) KEYSTO(k2);

    while(1) {
       KC (tmmk,TMMK_WaitForActiveBranch) KEYSTO(k0,k1) RCTO(rc);

// memset(buf,0,6);
// KC (k1,0) CHARTO(buf,6) RCTO(oc);
// mydump("WAKEUP BID",buf,6);
// KC (k1,KT) RCTO(oc);
// sprintf(buf,"BID KT = %x\n",oc);
// outsok(buf);

       if(rc == KT+1) exit(0);
       if(rc & 1) {
          KC (k2,DBMONITOR) KEYSFROM(k0,k1) RCTO(rc);
          if(rc) exit(0);
       }
    }
}

/*****************************************************************************
    DOREADER  - Cswitcher command system

    INPUT - Cnode is the master node
*****************************************************************************/
void doreader()
{
    JUMPBUF;
    UINT32 rc;
    char input[256];
    char buf[128];
    int actlen;
    int mode;
#define COMMAND 1
#define USER 0
    char *t1,*t2,*t3,*ptr;
    int len;

static char welcome[]="\nWelcome to Pacific\n";
static char noactivebranch[] = "No active branch\n";
static char nosuchbranch[] = "No such branch\n";
static char nosuchcontext[] = "No such context\n";
static char what[] = "?\n";
static char noti[] = "Not Implemented Yet\n";

static char hcreate[] = "create <contextname>   - create a new context\n";
static char hlogoff[] = "logoff                 - disconnect from system\n";
static char hstatus[] = "status [<contextname>] - show status of context or contexts\n";
static char hzap[] =    "zap <contextname>      - reclaim all context space\n";
static char hstart[] =  "start <contextname>    - start context meter\n";
static char hstop[] =   "stop <contextname>     - stop context meter\n";
static char hswitch[] = "<contextname.branch>   - switch to branch\n";
static char hbswitch[] ="<branch>               - switch to branch in current context\n";
static char haswitch[] ="<CR>                   - switch to active branch\n";

static char *helpp[] = {hcreate,hlogoff,hstatus,hstart,hstop,hzap,hswitch,hbswitch,haswitch,0};

    KC (cnode,Node_Fetch+MNODECENTRAL) KEYSTO(k2);   /* we can keep this here */
    KC (cnode,Node_Fetch+MNODESIK) KEYSTO(sik);
    mode=COMMAND;

    KC (k2,DBPRINTSTRING) CHARFROM(welcome,strlen(welcome)) RCTO(rc);

    KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
    while(1) {
       KC (sik,8192+255) CHARTO(input,255,actlen) KEYSTO(,,,sik) RCTO(rc);
       if(actlen == 1) {  /* very limited command set here */
          if(*input == 0x1B) {  /* escape */
             if(mode == USER) {   /* this is a switch to us */
                KC (k2,DBSWITCHOUTPUT) RCTO(rc);
                KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
                mode = COMMAND;
                continue;
             }
             else {  /* command mode */
                KC (k2,DBSENDASCII) CHARFROM(input,1) RCTO(rc);  /* send to current branch */
                if(!rc) mode=USER;
                continue;
             }
          }
/* must be in command mode */
          if(*input == '\r') {   /* switch active */
             KC (k2,DBSWITCHACTIVE) RCTO(rc);
             if(rc) {  /* no active branch */
                KC (k2,DBPRINTSTRING) CHARFROM(noactivebranch,strlen(noactivebranch)) RCTO(rc);
                KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
                continue;
             }
             mode=USER;
             continue;
          }
       }
/* a command of some form  "b" "c.b" "?" "somecommand somevalue" */ 
/*  if under 3 characters it must be a branch/context */

       tokenize(input,&t1,&t2,&t3);  /* get tokens */

       if(strlen(t1) < 3) {      /* must be a current switch request cannot contain . */
                                 /* first look for help */
          if(!strcmp(t1,"?")) goto dohelp;

          if(strlen(t1) == 1) {    /* 1 or 2 */
              *(t1+1) = *t1;
              *t1=' ';
              *(t1+2) = 0;
          }
          strcpy(buf,blankC);
          strcat(buf,t1);
          KC (k2,DBSWITCHBRANCH) CHARFROM(buf,18) RCTO(rc);
          if(rc) {
             KC (k2,DBPRINTSTRING) CHARFROM(nosuchbranch,strlen(nosuchbranch)) RCTO(rc);
             KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          }
          else {
             mode=USER;
          }
          continue;   /* more reading of commands */ 
       }
/* still could be a context.branch */
       if(ptr=strchr(t1,'.')) {  /* assume c.b */
          *ptr=0;  /* end context name */
          ptr++;   /* branch name */ 
          strcpy(buf,blankC);
          len=strlen(t1);
          if(len > 16) {
              KC (k2,DBPRINTSTRING) CHARFROM(what,strlen(what)) RCTO(rc);
              KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
              continue;
          }
          memcpy(buf,t1,len);
          if(strlen(ptr) == 1) {
              strcat(buf," ");
              strcat(buf,ptr);
          }
          else if(strlen(ptr) == 2) {
              strcat(buf,ptr);
          }
          else {
              KC (k2,DBPRINTSTRING) CHARFROM(what,strlen(what)) RCTO(rc);
              KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
              continue;
          }

//mydump("SWITCHBRANCH",(unsigned char *)buf,18);

          KC (k2,DBSWITCHBRANCH) CHARFROM(buf,18) RCTO(rc);
          if(rc) {
             KC (k2,DBPRINTSTRING) CHARFROM(nosuchbranch,strlen(nosuchbranch)) RCTO(rc);
             KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          }
          else {
             mode=USER;
          }
          continue;   /* more reading of commands */ 
       }  /* end . test */ 


/*************************************************************************************
   BEGIN COMMAND PROCESSING
*************************************************************************************/

/* LOGOFF */
       if(!strcmp(t1,"logoff")) {  /* disconnect zmk */
          KC (cnode,Node_Fetch+MNODEZMK) KEYSTO(k0);
          KC (k0,ZMK_Disconnect) RCTO(rc);
          KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          continue;   /* go read now disconnected circuit */ 
       }
/* CREATE */
       if(!strcmp(t1,"create")) {  /* create a context */
          if(!*t2) goto what;   /* best be there */
          if(strlen(t2) > 16) goto what;
          
          memcpy(buf,blankC,LCONTEXTNAME);
          memcpy(buf,t2,strlen(t2));

          KC (k2,DBCREATECONTEXT) CHARFROM(buf,LCONTEXTNAME) RCTO(rc);
          if(rc) {
             dberror("Create failed\n");
          }
          KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          continue;
       }
/* HELP */
       if(!strcmp(t1,"help")) {
          int i;
dohelp:

          i=0;
          while(helpp[i]) {
             KC (k2,DBPRINTSTRING) CHARFROM(helpp[i],strlen(helpp[i])) RCTO(rc);
             i++;
          }
          KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          continue;
       }
/* STATUS */
       if(!strcmp(t1,"status")) {
          if(t2) {
             strcpy(buf,t2);
          }
          else {
             strcpy(buf,"*");
          }
          KC (k2,DBPRINTCONTEXT) CHARFROM(buf,strlen(buf)) RCTO(rc);
          if(rc) goto notimplemented;
          KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          continue;
       }
/* ZAP */
       if(!strcmp(t1,"zap")) {
          if(t2) {
             strcpy(buf,t2);
          }
          else goto what;

          KC (k2,DBDESTROYCONTEXT) CHARFROM(buf,strlen(buf)) RCTO(rc);
          if(rc) {
              KC (k2,DBPRINTSTRING) CHARFROM(nosuchcontext,strlen(nosuchcontext)) RCTO(rc);          
          }
          KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
          continue;
       }
          
what:
       KC (k2,DBPRINTSTRING) CHARFROM(what,strlen(what)) RCTO(rc);
       KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
       continue;
notimplemented:
       KC (k2,DBPRINTSTRING) CHARFROM(noti,strlen(noti)) RCTO(rc);
       KC (k2,DBPRINTSTRING) CHARFROM("[",1) RCTO(rc);
       continue; 
    }  /* end while */
    exit(0);
}

/*****************************************************************************
    MAKENAME - make bid number into a 1 or 2 letter name from a - zz

    INPUT:  branch number starting with 1
            output buffer

    OUTPUT: name in buffer
*****************************************************************************/
makename(int bnum,char *buf)
{
    int snum;

    if(bnum > 26*26) return 1;
    bnum--;  /* make this zero based for ease of calculation */
    if(bnum < 26) { /* single character */
       *(buf+1) = 'a' + bnum;
       *buf = ' ';     /* done this way for sort ordering */
       return 0;
    }
    snum = bnum % 26;   /* the second letter */
    bnum = bnum / 26;   /* the first letter */
    *buf = 'a'+(bnum-1);  /* 26 becomes aa */
    *(buf+1) = 'a'+snum;
    return 0;
}

/*****************************************************************************
    PUTKEY - puts key into context local directory

    INPUT:  name of key
            k0 - the directory,
            k1 - the key

    OUTPUT: none
*****************************************************************************/
void putkey(char *name,char *desc)
{
    JUMPBUF;
    char rcname[512];
    int len,dlen;

    len=strlen(name);
    if(len > 255) return;   /* don't do anything, this won't happen */
    dlen=strlen(desc);
    if(dlen > 255) return;  /* won't happen */
    strcpy(rcname+1,name);
    strcpy(rcname+1+len,desc);
    *rcname=len;
    KC (k0,TDO_AddKey) CHARFROM(rcname,len+dlen+1) KEYSFROM(k1);
    return;
}

/*****************************************************************************
    GETCONTEXT - reads context record

    INPUT:  context name
            output context record structure

    OUTPUT: context record structure
            cnode-contextnode
*****************************************************************************/
UINT32 getcontext(char *cname,struct rcontext *context)
{
    JUMPBUF;
    UINT32 rc;

    memcpy(context->cname,cname,LCONTEXTNAME);
    context->lname=LCONTEXTNAME;
//mydump("GetContext",(unsigned char *)context,sizeof(*context));
    KC (record,TDO_GetEqual) STRUCTFROM(*context) 
           STRUCTTO(*context) KEYSTO(cnode) RCTO(rc);
    if(rc == 1) return 0;     /* got key with record */
    return 1;                 /* no context by this name */
}

/*****************************************************************************
    SWITCHACTIVEBRANCH - find an acitive branch in the context and switch to it

    INPUT:  context structure of a context with a active branch
            central data structure
            cnode - contextnode

    OUTPUT: 
              records are updated
              branch is switched to

    USES k2,k1
   
*****************************************************************************/
UINT32 switchactivebranch(struct rcontext *context,struct centraldata *cd)
{
    JUMPBUF;
    UINT32 rc;
    struct rbranch branch;
    char buf[32];

    rc = 1;
    memcpy(branch.cname,context->cname,LCONTEXTNAME);
    memcpy(branch.bname,blankC,LBRANCHNAME);
    branch.lname=LCONTEXTNAME+LBRANCHNAME;
    while(rc == 1) {
       KC (record,TDO_GetGreaterThan) STRUCTFROM(branch)
                   STRUCTTO(branch) KEYSTO(k2) RCTO(rc);
//mydump("LOOKCONTEXT",(unsigned char *)&branch,sizeof(branch));

       if(rc == 1) {
            if(memcmp(context->cname,branch.cname,LCONTEXTNAME)) return 1;
            if(branch.flags & BACTIVE) {  /* got one */
                 KC (k2,Node_Fetch+BNODECCK) KEYSTO(k1);
                 memcpy(buf,branch.cname,LCONTEXTNAME);
                 buf[LCONTEXTNAME]='.';
                 memcpy(buf+LCONTEXTNAME+1,branch.bname,2);
                 buf[LCONTEXTNAME+LBRANCHNAME+1]=0;
                 strcat(buf,"]\n"); 
                 squeezename(buf);
                 KC (sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,sok) RCTO(rc); 

                 KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k1) RCTO(rc);
                 KC (tmmk,TMMK_SwitchInput) KEYSFROM(k1) RCTO(rc); 
                 memcpy(cd->curbid,branch.bid,6);
                 strcpy(cd->lastname,cd->currentname);
                 memcpy(cd->currentname,branch.cname,LCONTEXTNAME+LBRANCHNAME);
                 cd->currentname[LCONTEXTNAME+LCONTEXTNAME]=0;

                 KC (cnode,Node_Fetch+CNODEMN) KEYSTO(k0);
                 KC (k0,Node_Swap+MNODECURRENTCCK) KEYSFROM(k1);

                 branch.flags &= ~BACTIVE;
                 if(context->activity) context->activity--;
                 cd->masteract--;
                 KC (record,TDO_AddReplaceKey) STRUCTFROM(*context) KEYSFROM(cnode) RCTO(rc);
                 KC (record,TDO_AddReplaceKey) STRUCTFROM(branch) KEYSFROM(k2) RCTO(rc);
                 return 0;
            }
        }
    }    
    return 1;
}
/*****************************************************************************
    SQEEZENAME - remove blanks
*****************************************************************************/
void squeezename(char *buf)
{
    char *iptr,*optr;
    
    iptr=buf;
    optr=buf;
    while (*iptr) {
       if(*iptr != ' ') {
          *optr=*iptr;
          optr++;
       }
       iptr++;
    } 
    *optr=0;
}
/*****************************************************************************
    TOKENIZE - set pointers to 2 tokens
*****************************************************************************/
void tokenize(char *in,char **t1,char **t2, char **t3)
{
    int needtoken;

    *t1=0;
    *t2=0;
    *t3=0;
    needtoken=1;
    while(*in) {
       if(*in == ' ') {*in=0;needtoken=1;in++;continue;}
       if(*in == '\r') {*in=0;return;}
       if(!(*t1) && needtoken) {*t1=in;needtoken=0;in++;continue;}
       if(!(*t2) && needtoken) {*t2=in;needtoken=0;in++;continue;}
       if(!(*t3) && needtoken) {*t3=in;needtoken=0;in++;continue;}
       in++;
    }
    return;
}
/*****************************************************************************
    dberror - print error message using K2
*****************************************************************************/
void dberror(char *str)
{
    JUMPBUF;
    UINT32 rc;

    KC (k2,DBPRINTSTRING) CHARFROM(str,strlen(str)) RCTO(rc);
}

/*****************************************************************************
    OUTSOK - used for debugging.  writes on console key
*****************************************************************************/
void outsok(str)
   char *str;
{
   JUMPBUF;
   UINT32 oc,rc;

   KC (comp,Node_Fetch+COMPCONSOLE) KEYSTO(cons);
   KC (cons,0) KEYSTO(,cons) RCTO(rc); 
   KC (cons,0) CHARFROM(str,strlen(str)) RCTO(rc);
}

/****************************************************************************
    TRACE - used for debugging.  Turns logging on/off
****************************************************************************/
void trace(int onoff)
{
    JUMPBUF;
    UINT32 rc;

    KC (comp,Node_Fetch+COMPCONSOLE) KEYSTO(cons);
    KC (cons,0) KEYSTO(,,cons) RCTO(rc);
    if(onoff) KC (cons,CONCCK__START_LOG) RCTO(rc);
    else KC (cons,CONCCK__STOP_LOG) RCTO(rc);
}

/****************************************************************************
    DUMP - hex dump of some data 
****************************************************************************/
void mydump(char *mytitle,unsigned char *buf,int len)
{
     char out[128];
     char *ptr,*row;
     int groups,bytes;
     int i,j;

 static char hextab[16] = "0123456789ABCDEF";

     outsok(mytitle);
     outsok("\n");
     groups=0;
     bytes=0;
     ptr=out;
     row=buf;
     for(i=0;i<len;i++) {
         *ptr = hextab[buf[i] >> 4];
         ptr++;
         *ptr = hextab[buf[i] & 0x0F];
         ptr++;
         bytes++;
         if(!(bytes % 4)) {
             *ptr = ' ';
             ptr++;
             groups++;
             if(!(groups % 4)) {
                 *ptr = 0;
                 outsok(out);
                 outsok(" *");
                 memcpy(out,row,16);
                 for(j=0;j<16;j++) if(out[j] < 32) out[j]='.';
                 out[16]='*';
                 out[17]='\n';
                 out[18]=0;
                 outsok(out); 
                 ptr=out;
                 row += 16;
             }
          }
      }
      if(ptr != out) {
         while((ptr-out) < 37) {*ptr = ' ';ptr++;}
         *ptr = '*';
         ptr++;
         *ptr=0;
         outsok(out);
         for(j=0;j<16;j++) out[j]=' ';
         memcpy(out,row,(len % 16));
         for(j=0;j<(len % 16);j++) if(out[j] < 32) out[j]='.';
         out[16]='*';
         out[17]='\n';
         out[18]=0;
         outsok(out); 
      }
}
