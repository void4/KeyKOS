/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include <string.h>
#include "kktypes.h"
#include "node.h"
#include "tdo.h"
#include "cck.h"
#include "domain.h"
#include "auth.h"
#include "recep.h"
#include "tssf.h"
#include "ocrc.h"
/****************************************************************
   RECEPF  The Receptionist Factory.
            RECEPF  is called to produce a receptionist of 
                    the correct type.
 
       RECEPF(kt+5;sb,m,sb,lud ==> rc;RECEP)
 
       RECEP is called to listen for a login on a circuit
             of a particular type depending on the receptionist
             and when the user requests a login the receptionist
             passes the request to the correct authenticator which
             it finds in the lud.
    
***************************************************************/
 
     KEY comp   = 0;       /* components node */
#define COMPTSSF    1
     KEY sb     = 1;      
     KEY caller = 2;
     KEY dom    = 3;
     KEY psb    = 4;
     KEY meter  = 5;
     KEY domcre = 6;
     KEY lud    = 7;
 
     KEY sik    = 8;
     KEY sok    = 9;
     KEY cck    = 10;  
     KEY ludnode= 11;
 
     KEY k3     = 12;
     KEY k2     = 13;
     KEY k1     = 14;
     KEY k0     = 15;

#define ludconnection 0
#define ludpsb 1
#define ludm   2
#define ludsb  3

/* slot in memory node for COMM key */
/* slot in memory node for saved CCK2 */
#define COMMSLOT 3
#define CCK2SLOT 4
 
       char title[]="RECEPF  ";
 
static char hextab[]="0123456789ABCDEF";
static char logo[] =
 
"\n\n\n\
       KKKK\n\
    KKK    KKK\n\
  KK          KK\n\
 K              K\n\
K    Pacific     KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n\
K                KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n\
 K              K                          KKKKKKKKK\n\
  KK          KK    Agorics, Inc.          KKKKKKKKK\n\
    KKK    KKK                               KKKKK\n\
       KKKK                                KKKKKKKKK\n\
                                           KKK   KKK\n\n\n";
 
static char   chgpass[]  =  "change_password";
static char  grabuser[]  =  "grabuser";
static char  whostr[]    =  "\nEnter User Name: ";
 
char toupper(char);

void zapdrain(),upperit();

/****************************************************************
  Main factory program
****************************************************************/
int factory(ordercode,ordinal)
   UINT32 ordercode,ordinal;
{
     JUMPBUF;
     UINT32 rc,oc;
     unsigned long soklim;
     int i,j;
 
     KC (caller,EXTEND_OC) KEYSTO(lud,,,caller) OCTO(oc);
     KC (dom,Domain_MakeStart) KEYSTO(k0);
     LDEXBL (caller,0) KEYSFROM(k0);
     for(;;) {
         LDENBL OCTO(oc) KEYSTO(,,cck,caller);
         RETJUMP();

         if(oc == DESTROY_OC) {
             exit(0);
         }
   
         if(oc == KT) {
             LDEXBL (caller,Recep_AKT);
             continue;
         }
         if(oc) {
             LDEXBL (caller,INVALIDOC_RC);
             continue;
         }
         KC (dom,Domain_GetMemory) KEYSTO(k3);
         KC (k3,Node_Swap+COMMSLOT) KEYSFROM(cck);
         
         getcck2();   /* uses key in COMMSLOT and TSSF to build level 2 */      

#ifdef xx
         KC (cck,CCK_EchoCRAsCRLF)  RCTO(rc);   /* echo CR as CRLF */
         KC (cck,CCK_EchoLFAsLF)    RCTO(rc);   /* echo LF as LF   */
         KC (cck,CCK_SetActivationMask+0x09) RCTO(rc);
#endif
         KC (sok,0) RCTO(soklim) KEYSTO(,,,sok);

         promptandlisten(&soklim);
     }
}

promptandlisten(soklim)
      unsigned long *soklim;
{
      JUMPBUF;
      UINT32 oc,rc;
      char username[256];
      int i;
/*
   Put up advertisement
*/
newlogo:
     if(!outsok(soklim,logo)) {zapdrain();return 2;}
     for (;;) {
        if(!outsok(soklim,whostr)) {zapdrain();return 3;}
/*
    Read the userid
*/
        i=readinput(username,255);         /* read with echo */
        switch (i) {
           case 0: break;                  /* OK typed a name */
           case 1: continue;               /* Null so loop    */
           case 2: {zapdrain();return Recep_Dropped;}  /* circuit dropped */
        }
/*
   Check for special names in either case
*/
#ifdef XXX
        if(!stricmp(username,chgpass)) {
              dochgpass(...);
              goto newlogo;
              zapdrain();
              return Recep_Fail;
           }
        if(!stricmp(username,grabuser)) {
              if(!dograbuser(...)) zapdrain();
              return Recep_Fail;
         }
#endif
        if(chkname(username))  {
              doconnect(username);
              goto newlogo;
        }
     }
}
/********************************************************************
   DOCONNECT - call authentication to complete connection
               ludnode contains the node with the keys
********************************************************************/
doconnect(username) 
   char *username;
{
   JUMPBUF;
   UINT32 rc;

   KC (ludnode,Node_Fetch+ludconnection) KEYSTO(k0);
   KC (ludnode,Node_Fetch+ludpsb) KEYSTO(k1);
   KC (ludnode,Node_Fetch+ludm) KEYSTO(k2);
   KC (ludnode,Node_Fetch+ludsb) KEYSTO(k3);
   KC (k0,EXTEND_OC) KEYSFROM(k1,k2,k3) KEYSTO(,,,k0) RCTO(rc);
   if (rc != EXTEND_RC) return 1;
   KC (k0,AUTH_MakeConnection) CHARFROM(username,strlen(username))
           KEYSFROM(sik,sok,cck) RCTO(rc);

   getcck2();   /* need a new circuit */

   return rc;
}
/********************************************************************
   GETCCK2  -  get a new CCK2 using comm key in commslot of memnode
********************************************************************/
getcck2()
{
     JUMPBUF;
     UINT32 rc,oc;

     KC (dom, Domain_GetMemory) KEYSTO(k3);
     KC (k3, Node_Fetch+CCK2SLOT) KEYSTO(cck);
     KC (cck,CCK_ActivateNow) RCTO(rc);  /* clear reading state */
     KC (cck,DESTROY_OC) RCTO(rc);

     KC (k3, Node_Fetch+COMMSLOT) KEYSTO(k2);
     KC (comp, Node_Fetch+COMPTSSF) KEYSTO(k1);
     KC (k1, KT+5) KEYSFROM(psb,meter,sb) KEYSTO(,,,k1) RCTO(rc);
     if (rc != KT+5) {
         crash("TSSF broken");
     }
     KC (k1, TSSF_CreateCCK2) KEYSFROM(k2) KEYSTO(sik,sok,cck);
     KC (k3, Node_Swap+CCK2SLOT) KEYSFROM(cck);
     KC (sok,0) KEYSTO(,,,sok) RCTO(rc);
}
/********************************************************************
   ZAPDRAIN  -  zap circuit and drain input
********************************************************************/
void zapdrain()
{
     JUMPBUF;
     unsigned long rc;
 
     return; 

     KC (cck,CCK_Disconnect) RCTO(rc);

}
/********************************************************************
    OUTSOK - Write string to terminal
*********************************************************************/
int  outsok(soklim,str)
   unsigned long *soklim;
   char *str;
{
     JUMPBUF;
     SINT32 len,strl;
     UINT32 rc;
 
     strl=strlen(str);                  /* length to write */
     while(strl > 0) {                  /* as long as there is some */
        len=strl;
        if (len < *soklim) len=len;
          else len=*soklim;         /* send only what allowed to */
        KC (sok,0) CHARFROM(str,len) KEYSTO(,,,sok)
            RCTO(rc);          /* new limit */
        if(rc == KT+1) break;  /* did circuit die */
        str=str+len;
        strl=strl-len;                  /* update what sent */
     }
     *soklim=rc;
     if (rc == KT+1) return 0; /* circuit died */
     else return 1;
}
/**********************************************************************
    READINPUT - Read a string into STR, remove trailing CR
***********************************************************************/
int readinput(username,len)
    char *username;
    int len;
{
    JUMPBUF;
    SINT32 inlen;
    UINT32 rc;

readagain: 
    KC (sik,len+8192) CHARTO(username,len,inlen) KEYSTO(,,,sik)
            RCTO(rc);                               /* read data */
    if(rc) return 2;                           /* circuit died */
    if(!inlen) {                                  /* activate now in past */
        goto readagain;        
    }
    if(username[inlen-1] != '\r') return 1;       /* no CR */
    username[inlen-1]=0;                          /* remove CR */
    return 0;
}
/*********************************************************************
    CHKNAME - Read name from LUD
*********************************************************************/
int chkname(username)
    char *username;
{
    JUMPBUF;
    char savename[256];
    SINT32 i;
    UINT32 rc;
    int unamelen;

    KC (lud,KT) RCTO(rc);
    if(rc == 0x17) {                 /* simple record collection */
       rc=readlud(username);
       if (rc != 1) {                /* try upper case version */
          upperit(username);
          rc=readlud(username);
          if(rc != 1) return 0;      /* failed */
       }
       return 1;                     /* found it */
    }
    else {                           /* assume a node of luds */
      return 0;            /* failed */
    }
}
/********************************************************************
   READLUD - Read lud
*********************************************************************/
int readlud(username)
    char *username;
{
    JUMPBUF;
    UINT32 rc;
    char rcname[260];

    strcpy(rcname+1,username);
    *rcname=strlen(username);
 
    KC (lud,TDO_GetEqual) STRUCTFROM(rcname) KEYSTO(ludnode) RCTO(rc);
 
    return (int)rc;
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
