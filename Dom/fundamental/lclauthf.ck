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
/****************************************************************

***************************************************************/
 
     KEY comp   = 0;       /* components node */
#define comptdof  0
     KEY psb    = 1;       
     KEY caller = 2;
     KEY dom    = 3;
     KEY sb     = 4;
     KEY meter  = 5;
     KEY domcre = 6;
     KEY directory    = 7;
 
     KEY sik    = 8;
     KEY sok    = 9;
     KEY cck    = 10;

     KEY connection = 11;
     KEY encryption = 12; 
  
     KEY k2     = 13;
     KEY k1     = 14;
     KEY k0     = 15;

       char title[]="AUTHF   ";
 
char toupper(char);
static char passprompt[] = "\nEnter your password:  ";

/****************************************************************
  Main factory program
****************************************************************/
int factory(ordercode,ordinal)
   UINT32 ordercode,ordinal;
{
     JUMPBUF;
     UINT32 rc,oc;
     struct Domain_DataByte dbmaint= {1};
     short db; 
     char param[257],dirname[256];
     char password[256];
     int actlen,len;
     char connectiontype;
     unsigned long soklim;

     KC (dom,Domain_GetMemory) KEYSTO(k0);
     KC (k0,Node_Swap+3) KEYSFROM(psb);
     KC (k0,Node_Swap+4) KEYSFROM(meter);
     KC (k0,Node_Swap+5) KEYSFROM(sb);

     KC (comp,Node_Fetch+comptdof) KEYSTO(k0);
     KC (k0,TDOF_CreateNameSequence) KEYSFROM(psb,meter,sb) KEYSTO(directory) RCTO(rc);
     if(rc) {
         exit(rc);
     }
 
     KC (dom,Domain_MakeStart) KEYSTO(k0);
     KC (dom,Domain_MakeStart) STRUCTFROM(dbmaint) KEYSTO(k1);

     LDEXBL (caller,0) KEYSFROM(k0,k1);
     for(;;) {
         LDENBL OCTO(oc) CHARTO(param,256,actlen) KEYSTO(psb,meter,sb,caller) DBTO(db);
         RETJUMP();
         
         switch(db) {
         case 1:   // AUTHM
           if(oc == KT+4) {
               KC (dom,Domain_GetMemory) KEYSTO(k0);
               KC (k0,Node_Fetch+3) KEYSTO(psb);
               KC (k0,Node_Fetch+4) KEYSTO(meter);
               KC (k0,Node_Fetch+5) KEYSTO(sb);
               KC (directory, KT+4) RCTO(rc);
               exit(0);
           }
           if(actlen > 256) actlen=256;
           param[actlen]=0;

           switch(oc) {
           case AUTHM_ReturnAUTH:
               KC (dom,Domain_MakeStart) KEYSTO(k0);
               LDEXBL (caller,0) KEYSFROM(k0);
               continue;

           case AUTHM_AddDirectoryKey:
               strcpy(dirname+1,param);
               *dirname=strlen(dirname+1);
               KC (directory,TDO_AddReplaceKey) CHARFROM(dirname,(*dirname)+1) 
                  KEYSFROM(psb) RCTO(rc);
               if(rc>2) {
                  LDEXBL(caller,rc);
               }
               else {
                  LDEXBL(caller,0);
               }
               continue;

           case AUTHM_GetFirstKey:
                KC (directory,TDO_GetFirst) KEYSTO(k0) CHARTO(dirname,256) RCTO(rc);
                if(rc < 2) {  /* ok */
                    len=*dirname;
                    strncpy(param,dirname+1,len);               
                    param[len]=0;
                    LDEXBL (caller,0) KEYSFROM(k0) CHARFROM(param,len);
                }
                else {
                    LDEXBL (caller,rc);
                }
                continue;
                   
           case AUTHM_GetNextKey:
               strcpy(dirname+1,param);
               *dirname=strlen(dirname+1);
               KC (directory,TDO_GetGreaterThan) CHARFROM(dirname,(*dirname)+1) 
                     CHARTO(dirname,256) KEYSTO(k0) RCTO(rc);
                if(rc < 2) {  /* ok */
                    len=*dirname;
                    strncpy(param,dirname+1,len);               
                    param[len]=0;
                    LDEXBL (caller,0) KEYSFROM(k0) CHARFROM(param,len);
                }
                else {
                    LDEXBL (caller,rc);
                }
                continue; 

           case AUTHM_PutConnection:
                connectiontype = *param;
                KC (dom,Domain_SwapKey+connection) KEYSFROM(psb);
                LDEXBL(caller,0);
                continue;

           case AUTHM_PutEncryptionService:
                KC (dom,Domain_SwapKey+encryption) KEYSFROM(psb);
                LDEXBL(caller,0);
                continue;

           case AUTHM_PutPassword:
                strcpy(password,param);
                LDEXBL(caller,0);
                continue;

           default:
                LDEXBL(caller,KT+2);
                break; 
           }  /* switch on AUTHM oc */
           continue; 

         case 0:  // AUTH

           if(oc != KT+5) {
               LDEXBL(caller,KT+2);
               continue;
           }
           KC (caller,KT+5) CHARTO(param,256,actlen) KEYSTO(sik,sok,cck,caller) RCTO(oc);
           if(actlen > 256) actlen = 256;
           param[actlen]=0;

           switch(oc) {
           case AUTH_MakeConnection:
                rc=checkpassword(param,password);
                if(rc) {
                    LDEXBL(caller,1);
                    continue;
                }
                rc=doconnection(param,connectiontype);  /* returns when user breaks connection */
                if(rc) { /* connection failed */
                    LDEXBL (caller,rc);
                    continue;
                }
                LDEXBL(comp,0);  // must go ready
                continue;

           case AUTH_ChangePass:
                LDEXBL(caller,KT+2);  // NOT DONE
                continue;
           default:
                LDEXBL(caller,KT+2);
                break;
           }  /* switch on AUTH oc */

           continue;
         }  /* switch on db */
     }
}

checkpassword(username,password) 
     char *username;
     char *password;
{
     JUMPBUF;
     UINT32 rc;
     UINT32 soklim;
     char hispass[257];
     int i,returnrc;

     KC (sok,0) KEYSTO(,,,sok) RCTO(soklim);
#ifdef xx
     KC (cck,CCK_SetEchoMask+1) RCTO(rc);
#endif

     returnrc=3;
     for(i=0;i<3;i++) {
         if(!outsok(&soklim,passprompt)) {
             returnrc = 1;   
             break;
         }
         rc=readinput(hispass,256);
         if(rc) {
             returnrc = 2;
             break;
         }
         if(!strcmp(hispass,password)) {
             returnrc = 0;
             break;
         }
     }
#ifdef xx
     KC (cck,CCK_SetEchoMask+0xF7) RCTO(rc);
#endif
     outsok(&soklim,"\n");
     return returnrc;
}

doconnection(username,type)
     char *username;
     char type;
{
     JUMPBUF;
     UINT32 rc;

     switch(type) {
     case CONNECT_ZAPPER:   /* connection is a ZMK key, directory is the user directory */
          KC (connection,ZMK_Connect) KEYSFROM(sik,sok,cck) RCTO(rc);

          if(rc) {   /* connection failed  CALLER has the return key */
              return 2;
          }    

      /* make the receptionist the disconnect waiter for local connections */

          LDEXBL (connection, ZMK_WaitForDisconnect) KEYSFROM(,,,caller);
          FORKJUMP();

          KC (comp,0) KEYSTO(,,caller);

          return 0;     /* with rc = 0 mainline will leave caller alone */

     case CONNECT_FACTORY:
          KC (connection,KT+5) KEYSFROM(psb,meter,sb) KEYSTO(,,,k0) RCTO(rc);
          if(rc != KT+5) return KT+3;
          KC (k0,KT+5) KEYSFROM(sik,sok,cck) KEYSTO(,,,k0) RCTO(rc);
          if(rc != KT+5) return KT+3;

//          KC (k0,0) KEYSFROM(directory) RCTO(rc); 
          LDEXBL (k0,4) KEYSFROM(directory,,,caller);  // return to receptionist 
          FORKJUMP();
 
          KC (comp,0) KEYSTO(,,caller);
          return 0;

     case CONNECT_TRANSACTION:
          return KT+2;  /* not done */
  
     case CONNECT_NULL:
          return 0;  /* normal return */

     case CONNECT_FACTORY_NO_PARAM:
          KC (connection,KT+5) KEYSFROM(psb,meter,sb) KEYSTO(,,,k0) RCTO(rc);
          if(rc != KT+5) return KT+3;

//          KC (k0,0) KEYSFROM(sik,sok,cck) RCTO(rc);        
          LDEXBL (k0,0) KEYSFROM(sik,sok,cck,caller) RCTO(rc);
          FORKJUMP();
          return 0; 
     }

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
                NO Echo
***********************************************************************/
int readinput(str,len)
    char *str;
    int len;
{
    JUMPBUF;
    SINT32 inlen;
    UINT32 rc;
 
    KC (sik,len) CHARTO(str,len,inlen) KEYSTO(,,,sik)
            RCTO(rc);                               /* read data */
    if(rc) return 2;                           /* circuit died */
    if(!inlen) return 2;                       /* illegal */
    if(str[inlen-1] != '\r') return 1;       /* no CR */
    str[inlen-1]=0;                          /* remove CR */
    return 0;
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
