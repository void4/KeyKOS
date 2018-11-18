/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include <string.h>
//#include <ctype.h>
#include "kktypes.h"
#include "domain.h"
#include "node.h"
#include "tdo.h"
#include "factory.h"
#include "cck.h"
/****************************************************************
   RECEPF  The Receptionist Factory.
            RECEPF  is called once for each user validation
 
       RECEPF(kt+5;sb,m,sb,sik2,sok2,cck2 ==>)
 
            RECEPF is called by the terminal driver when a new
                   circuit (logon, poweron, etc) activates.
                   RECEPF returns (and self destructs) only if
                   the circuit goes bad during the logon validation
                   or if the validation fails.
                   SIK, SOK, CCK will be passed on to the key in
                   the LUD.
 
       On the 370 this must be compiled with the ASCIIOUT option

       On the LUNA oc=1 rather than 0 means to fetch the SIK/SOK
       from the CCK and to CALL clients rather than fork.  When the
       client returns, put up a new logo and recycle . RECEPC will
       be forked!!.  
***************************************************************/
 
     KEY comp   = 0;       /* components node */
     KEY lud    = 1;       /* lud or node of luds */
     KEY caller = 2;
     KEY dom    = 3;
     KEY sb     = 4;
     KEY meter  = 5;
     KEY domcre = 6;
 
     KEY k3     = 12;
     KEY k2     = 13;
     KEY k1     = 14;
     KEY k0     = 15;
 
/* key definitions for the "back end"  RECEP */
 
     KEY becck2    = 7;
     KEY bedir     = 8;    /* holds lud key now and then */
     KEY beencrypt = 9;    /* encrypter if present */
     KEY benode    = 10;   /* node from lud entry */
     KEY becall   = 11;
     KEY besik2   = 12;
     KEY besok2   = 13;
 
#define COMPLUD   2        /* back end lud */
#define COMPNODE  5        /* back end priv node extension */
#define COMPCONS  15
       char title[]="RECEPC  ";
 
static char hextab[]="0123456789ABCDEF";
static char flogo[] =
 
"\n\n\n\
       KKKK\n\
    KKK    KKK\n\
  KK          KK\n\
 K              K\n\
K    KeyTECH     KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n\
K     KeyNIX     KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n\
 K              K                          KKKKKKKKK\n\
  KK          KK    Key Logic              KKKKKKKKK\n\
    KKK    KKK                               KKKKK\n\
       KKKK                                KKKKKKKKK\n\
                                           KKK   KKK\n\n\n";
 
static char   chgpass[]  =  "password";
static char  grabuser[]  =  "grabuser";
static char   passmsg[]  =  "\nKeyKOS Password changing server";
static char  newpass1[]  =  "\nEnter your new password  ";
static char  newpass2[]  =  "\nEnter your new password again  ";
static char  newpass3[]  =  "\nTyped password was different, \
restart..";
static char  passcant[]  =  "\nPassword change is not authorized";
static char  pwfailed[]  =  "\nChange of password failed";
static char  pwworked[]  =  "\nPassword is changed";
static char  siezemsg[]  =  "\nSeize control of logged on user";
static char   nosieze[]  =  "\nSeize control not authorized";
static char    whostr[]  =  "\nPlease enter your username  ";
static char cantenstr[]  =  "\nPassword cannot be encrypted";
static char   upasstr[]  =  "\nEnter your password  ";
static char     nfmt1[]  ="\nInvalid terminal type for this USERID";
static char     badls[]  =  "\nBad LUD entry.. disconnecting";
static char  grabfail[]  =  "\nSeize control failed";
 
   struct bework {            /* work area for RECEPF */
     UINT32 oc,rc;     /* allocated on the stack to avoid */
     SINT32 actlen;              /* static read/write storage */
 
     UINT32 besoklim;        /* keep these together */
     char unamelen;          /* always zero for r3270 */
     char uname[256];
     char cr;             /* end restriction */
 
     char ludtype;
     SINT32  llen,plen;      /* length lud entry, password */
     char newpass[256];
     char ludstring[258];
     SINT32  passtype;
     char password[16];
     char passstring[256]; /* read in */
     char passstringencrypted[16];
     UINT32  belim;
   };
 
char toupper(char);
void dochgpass(),upperit(),zapdrain();
SINT32 outsok(),readinput(),dograbuser(),chkname(),chkpass(),golud();
SINT32 ckonepass(),readlud();
 
/****************************************************************
  Main factory program
****************************************************************/
SINT32 factory(ordercode,ordinal)
   UINT32 ordercode,ordinal;
{
     JUMPBUF;
     struct bework w;
     SINT32    i;
     char vlogo[800],*logo,*portstr;
     UINT32 j;
     UINT16 portn;
 
     KC (caller,KT+5) CHARTO(&(w.besoklim),260,w.actlen)
          KEYSTO(besik2,besok2,becck2,caller) OCTO(w.oc);


     if (w.oc) { /* this is the luna */
        KC (becck2,0) KEYSTO(besik2,besok2,becck2);  /* cck is reall conkey */
     }
/*
   This receptionist is for R3270.  The first 6 bytes of the string
   contain screen size information.  the next 2 bytes are the binary
   port number.
 
   There may be a TCEMK key as the 4th key on the above
   invocation.  However, a bug in KC/CP does not allow receiving
   4 keys.  Fortunately it is not used by anyone.
*/
     w.besoklim=0;
     KC (becck2,CCK_EchoCRAsCRLF)  RCTO(w.rc);   /* echo CR as CRLF */
     KC (becck2,CCK_EchoLFAsLF)    RCTO(w.rc);   /* echo LF as LF   */
     KC (becck2,CCK_SetActivationMask+0x09) RCTO(w.rc);
              /* activate CR LF CTL *
 /*    KC (becck2,SetEchoMask+0xF7) RCTO(w.rc);   */
              /* echo all */
#ifdef XYX
     if (w.rc) {zapdrain(&w);return 1;} 
#endif
 
     logo=flogo;  /* nominal advertisement */
 
/* Begin R3270 only code,  for other drivers, user different code */
     if((int)strlen(flogo) < 800) {
        strcpy(vlogo,logo);
        logo=vlogo;
        if(w.actlen>6) {  /* have port number */
           if(portstr=strchr(vlogo,'@')) {
             memcpy(&portn,&(w.uname[1]),4);
             for(i=0;i<4;i++) {
               j=(portn>>(12-(4*i))) & 0x0F;
               *portstr=hextab[j];
               portstr++;
             }
           }
        }
     }
/* End R3270 only code */
 
/*
   Put up advertisement
*/
newlogo:
     if(!outsok(&w,logo)) {zapdrain(&w);return 2;}
     for (;;) {
        if(!outsok(&w,whostr)) {zapdrain(&w);return 3;}
/*
    Read the userid
*/
        i=readinput(&w,w.uname,8192);
        switch (i) {
           case 0: break;                  /* OK typed a name */
           case 1: continue;               /* Null so loop    */
           case 2: {zapdrain(&w);return 4;}  /* circuit dropped */
        }
/*
   Check for special names in either case
*/
        if(strlen(w.uname) == 8) {
           if(!stricmp(w.uname,chgpass)) {
              dochgpass(&w);
              goto newlogo;
#ifdef XYX
              zapdrain(&w);
#endif
              return 5;
           }
#ifdef XYX
           if(!stricmp(w.uname,grabuser)) {
              if(!dograbuser(&w)) zapdrain(&w);
              return 6;
           }
#endif
        }
/*
   Check for exact match (mixed case)
   and then look at upper case compare
*/
        if(!chkname(&w)) continue;
/*
   Have a valid user  now check the password
*/
        if(!chkpass(&w)) {
           zapdrain(&w);
           goto newlogo;	
#ifdef XYX
           return 7;
#endif
        }
/*
   Valid logon, connect to LUD key if possible
*/
        i=golud(&w);
        switch(i) {
          case 0: break;               /* worked */
          case 1: continue;            /* bad lud type, retry */
          case 2: {
            zapdrain(&w);
            goto newlogo;
#ifdef XYX
            return 8;
#endif
          }        /* bad lud entry  */
        }
        if(w.oc) goto newlogo;   /* on luna recycle */
        return 0;         /* done with work, self destruct */
     }
}
/********************************************************************
   ZAPDRAIN  -  zap circuit and drain input
********************************************************************/
void zapdrain(w)
   struct bework *w;
{
     JUMPBUF;
     unsigned long rc;
 
     return; 
#ifdef XYX
     KC (becck2,CCK_TerminateConnection) RCTO(rc);
     rc=0;
     while(rc != KT+1) {
        KC (besik2,4095) RCTO(rc);
     }
#endif
}
/********************************************************************
    OUTSOK - Write string to terminal
*********************************************************************/
SINT32 outsok(w,str)
   struct bework *w;
   char *str;
{
     JUMPBUF;
     SINT32 len,strl;
     UINT32 rc;
 
     strl=strlen(str);                  /* length to write */
     while(strl > 0) {                  /* as long as there is some */
        len=strl;
        if (len < w->besoklim) len=len;
          else len=w->besoklim;         /* send only what allowed to */
        KC (besok2,0) CHARFROM(str,len) KEYSTO(,,,besok2)
            RCTO(w->besoklim);          /* new limit */
        if(w->besoklim == KT+1) break;  /* did circuit die */
        str=str+len;
        strl=strl-len;                  /* update what sent */
     }
     if (w->besoklim == KT+1) return 0; /* circuit died */
     else return 1;
}
/**********************************************************************
    READINPUT - Read a string into STR, remove trailing CR
***********************************************************************/
SINT32 readinput(w,str,echo)
    struct bework *w;
    char *str;
    int echo;
{
    JUMPBUF;
    SINT32 inlen;
    UINT32 rc;
 
    KC (besik2,255+echo) CHARTO(str,255,inlen) KEYSTO(,,,besik2)
       RCTO(rc);                               /* read data */
    if(rc) return 2;                           /* circuit died */
    if(!inlen) return 2;                       /* illegal */
    if(str[inlen-1] != '\r') return 1;       /* no CR */
    str[inlen-1]=0;                          /* remove CR */
    return 0;
}
/********************************************************************
     DOCHGPASS - Change a user's password
*********************************************************************/
void dochgpass(w)
    struct bework *w;
{
     JUMPBUF;
 
    SINT32 i;
    UINT32 rc;
 
    if(!outsok(w,passmsg)) return;
    for (;;) {
       if(!outsok(w,whostr)) return;
       i=readinput(w,w->uname,8192);
       switch(i) {
         case 0: break;              /* continue with process */
         case 1: continue;           /* go around loop again */
         case 2: return;             /* return and zap circuit */
       }
       if(!chkname(w)) continue;    /* check name */
       if(!chkpass(w)) return;
/*
   Now see if this user authorized CHANGEPASSWORD
*/
       KC (benode,Node_Fetch+15) KEYSTO(becall) RCTO(rc);
       KC (becall,KT) RCTO(rc);
       if (rc != 0x022A) {
          outsok(w,passcant);
          return;
       }
       if(!outsok(w,newpass1)) return;
       KC (becck2,CCK_SetEchoMask+1) RCTO(rc);     /* turn of echo */
   /*    if(rc) continue; */               /* could not, try again */
       i=readinput(w,w->uname,0);
       switch(i) {
         case 0: break;
         case 1: KC (becck2,CCK_SetEchoMask+0xF7) RCTO(rc);
                 continue;
         case 2: return;               /* circuit died */
       }
       if(!outsok(w,newpass2)) return;
       i=readinput(w,w->newpass,0);
       switch(i) {
         case 0: break;
         case 1: KC (becck2,CCK_SetEchoMask+0xF7) RCTO(rc);
                 continue;
         case 2: return;               /* circuit died */
       }
       KC (becck2,CCK_SetEchoMask+0xF7) RCTO(rc);  /* echo all again */
       if(strcmp(w->uname,w->newpass)) {
         outsok(w,newpass3);
         continue;  /* loop */
       }
       w->unamelen=strlen(w->uname);
       KC (becall,2) CHARFROM(w->uname,w->unamelen) RCTO(rc);
       if(rc) {
          outsok(w,pwfailed);
          return;
       }
       outsok(w,pwworked);
       return;
    }
}
/********************************************************************
     DOGRABUSER - Perform the Sieze Control function
*********************************************************************/
SINT32 dograbuser(w)
    struct bework *w;
{
    JUMPBUF;
    SINT32 i;
    UINT32 rc;
 
    if(!outsok(w,siezemsg)) return 0;
    for (;;) {
       if(!outsok(w,whostr)) return 0;
       i=readinput(w,w->uname,8192);
       switch(i) {
         case 0: break;
         case 1: continue;
         case 2: return 0;
       }
       if(!chkname(w)) continue;
       if(!chkpass(w)) return 0;
       KC (benode,Node_Fetch+14) KEYSTO(becall) RCTO(rc);
       KC (becall,CCK_TerminateConnection) RCTO(rc);
                                  /* try disconnect */
       if(rc == KT+1) {
          outsok(w,nosieze);
          return 0;
       }
       i=golud(w);
       if(!i) return 1;
       outsok(w,grabfail);
       return 0;
    }
}
/*********************************************************************
    CHKNAME - Read name from LUD
*********************************************************************/
SINT32 chkname(w)
    struct bework *w;
{
    JUMPBUF;
    char savename[256];
    SINT32 i;
    UINT32 rc;

    w->unamelen=strlen(w->uname);    /* set up for RC calls */

    KC (comp,Node_Fetch+COMPLUD) KEYSTO(bedir);
    KC (bedir,KT) RCTO(rc);
    if(rc == 0x17) {                 /* simple record collection */
       rc=readlud(w);
       if (rc != 1) {                /* try upper case version */
          upperit(w->uname);
          rc=readlud(w);
          if(rc != 1) return 0;      /* failed */
       }
       return 1;                     /* found it */
    }
    else {                           /* assume a node of luds */
      for (i=0;i<16;i++) {             /* try all luds */
         KC (comp,Node_Fetch+COMPLUD) KEYSTO(bedir);
         KC (bedir,Node_Fetch+i) KEYSTO(bedir) RCTO(rc);
         if(rc) return 0;              /* not a node so return bad */
         strcpy(savename,w->uname);    /* save lower case version */
         rc=readlud(w);
         if (rc != 1) {                /* try upper case version */
            upperit(w->uname);
            rc=readlud(w);
            if(rc == 1) return 1;      /* found it */
         }
         else return 1;                /* found it */
         strcpy(w->uname,savename);
      }
      return 0;            /* failed */
    }
}
/********************************************************************
   READLUD - Read lud
*********************************************************************/
SINT32 readlud(w)
    struct bework *w;
{
    JUMPBUF;
    UINT32 rc;
 
    KC (bedir,TDO_GetEqual) CHARFROM(&(w->unamelen),w->unamelen+1)
       CHARTO(w->ludstring,258,w->llen) KEYSTO(benode) RCTO(rc);
 
    return (int)rc;
}
/********************************************************************
   CHKPASS - Check the password against value in LUD
********************************************************************/
SINT32 chkpass(w)
    struct bework *w;
{
    JUMPBUF;
    SINT32 i;
    UINT32 rc;
    char *ptr;

    char buf[256];
/*
   First parse the Lud string and determine if there is a
   password
*/
    if(w->unamelen+1 == w->llen) {       /* length of string */
       w->ludstring[w->unamelen+1]=0;  /* assume LUD byte = 0*/
       w->llen++;                        /* make standard */
    }
    ptr=w->ludstring+w->unamelen+1;      /* to LUD byte */
    w->ludtype=*ptr;
    ptr++;
    w->llen=w->llen-(w->unamelen+2);     /* length after LUD byte */
    if(w->llen == 0) return 1;            /* no password so OK */
    w->passtype=*ptr;
    ptr++;                               /* point to password */
    w->llen--;                           /* length of password */
 
    strncpy(w->password,ptr,16);

    if(w->passtype == 'E' )  {                   /* encrypted */
       KC (comp,Node_Fetch+COMPNODE) KEYSTO(beencrypt);
       KC (beencrypt,0) KEYSTO(beencrypt) RCTO(rc);
       KC (beencrypt,KT) RCTO(rc);
       if (rc == KT+1) {                 /* no encrypter provided */
          outsok(w,cantenstr);
          return 0;
       }
    }
    KC (becck2,CCK_SetEchoMask+1) RCTO(rc);  /* no echo */
#ifdef XYX
    if(rc) return 0;
#endif
    for (i=0;i<3;i++) {
      if(!outsok(w,upasstr)) return 0;   /* circuit died */
      memset(w->passstring,' ',256);
      rc=readinput(w,w->passstring,0);     /* read password */
      switch(rc) {
        case 0: break;
        case 1: continue;
        case 2: return 0;
      }
      w->plen=strlen(w->passstring);   /* length typed */
      while(w->plen < 16) {
	w->passstring[w->plen]=' ';
        w->plen++;
      }
      w->passstring[16]=0;           /* max length 16 */

      if(!ckonepass(w)) {
          upperit(w->passstring);
          if(!ckonepass(w)) continue;  /* try again */
      }
      KC (becck2,CCK_SetEchoMask+0xF7) RCTO(rc);   /* looks good */
      return 1;
    }
    return 0;                          /* failed thrice */
}
/********************************************************************
   CHECKONE - Check the password in passstring with that in password
*********************************************************************/
SINT32 ckonepass(w)
    struct bework *w;
{
    JUMPBUF;
    unsigned long rc;
 
    if(w->passtype == 'E' ) {                 /* encrypted */
       w->plen=w->plen+7;
       w->plen=w->plen & 0xFFFFFFF8;
       KC (beencrypt,2) CHARFROM(w->passstring,w->plen)
           CHARTO(w->passstringencrypted,16) RCTO(rc);
       if(!memcmp(w->passstringencrypted,w->passstring,16)) return 1;
       return 0;
    }
    if(!memcmp(w->passstring,w->password,16)) return 1;
    return 0;
}
/*******************************************************************
   GOLUD   call the LUD key
********************************************************************/
SINT32 golud(w)
   struct bework *w;
{
     unsigned long rc;
 
    JUMPBUF;
   switch(w->ludtype) {
     case 0:
        KC (benode,Node_Fetch+2) KEYSTO(becall) RCTO(rc);
        KC (becall,0) CHARFROM(&(w->besoklim),4)
            KEYSFROM(becck2,besik2,besok2) RCTO(rc);
        return 0;
     case 1:
        outsok(w,nfmt1);
        return 1;
     case 2:
        KC (benode,Node_Fetch+2) KEYSTO(becall) RCTO(rc);
        if(w->oc) { /* luna */
           KC (becall,w->oc-1) KEYSFROM(besik2,besok2,becck2)
              CHARFROM(&(w->besoklim),4)  RCTO(rc);
        }
        else {
           LDEXBL (becall,0) CHARFROM(&(w->besoklim),4)
               KEYSFROM(besik2,besok2,becck2,caller); /* really TCEMK */
           FORKJUMP();
        }
        return 0;
     case 3:               /* key is a factory */
        KC (benode,Node_Fetch+0) KEYSTO(k0)     RCTO(rc);  /* sb */
        KC (benode,Node_Fetch+1) KEYSTO(k1)     RCTO(rc);  /* m  */
        KC (benode,Node_Fetch+2) KEYSTO(becall) RCTO(rc);  /* key */
        KC (becall,KT+5) KEYSFROM(k0,k1,k0) KEYSTO(,,,becall)
           RCTO(rc) ;
        if(w->oc) { /* luna */
           KC (becall,w->oc-1) CHARFROM(&(w->besoklim),w->actlen)
             KEYSFROM(besik2,besok2,becck2) RCTO(w->rc);
        }
        else {
          LDEXBL (becall,0) CHARFROM(&(w->besoklim),w->actlen)
             KEYSFROM(besik2,besok2,becck2);
          FORKJUMP();
        }
        return 0;
      case 4:    /* factory key with parameter */
        KC (benode,Node_Fetch+0) KEYSTO(k0)     RCTO(rc);  /* sb */
        KC (benode,Node_Fetch+1) KEYSTO(k1)     RCTO(rc);  /* m  */
        KC (benode,Node_Fetch+2) KEYSTO(becall) RCTO(rc);  /* key */
        KC (becall,KT+5) KEYSFROM(k0,k1,k0) KEYSTO(,,,becall) RCTO(rc) ;
        KC (becall,KT+5) CHARFROM(&(w->besoklim),w->actlen)
            KEYSFROM(besik2,besok2,becck2) KEYSTO(,,,becall) RCTO(rc);
        KC (benode,Node_Fetch+3) KEYSTO(k0);
        if (0) {
           LDEXBL (becall,4) KEYSFROM(k0);
           FORKJUMP();
        }
        else {
           KC (becall,4) KEYSFROM(k0);
        }
        return 0;
     default:
        outsok(w,badls);
        return 2;     /* bad entry */
   }
}
/********************************************************************
  UPPERIT - Translate string to upper case
*********************************************************************/
void upperit(str)
   char *str;
{
   while(*str) {
      if(*str >= 'a' && *str <= 'z') *str=*str-0x20;
      str++;
   }
}
int stricmp(str1,str2)
    char *str1,*str2;
{
    while(*str2) {
      if(toupper(*str1) != toupper(*str2)) return 1;
      str1++;
      str2++;
    }
    return 0;
}

char toupper(char a)
{
   if ( a >= 'a' && a <= 'z') a = a & ~0x20;
   return a;
}
