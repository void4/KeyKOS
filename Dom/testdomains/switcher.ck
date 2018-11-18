/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h" 
#include "tssf.h"
#include "domain.h"
#include "node.h"
#include "sb.h"

   KEY comp     = 0;
#define COMPDKC  1
   KEY sb       = 1;
   KEY caller   = 2;
   KEY domkey   = 3;
   KEY psb      = 4;
   KEY meter    = 5;
   KEY dc       = 6;

   KEY tmmk     = 7;
   KEY b0sik    = 8;
   KEY NODE     = 8;   /* NOTE REUSE in MAIN */
   KEY b0sok    = 9;
   KEY b0cck    =10;

   KEY bid      =11;
   KEY cck      =12;

   KEY k2       =13;
   KEY k1       =14;
   KEY k0       =15;

    char title [] = "SWITCHER";

#define INTERJECTBELL 100
#define SWITCHACTIVE  101
#define WRITESTRING   102
#define SWITCHOUTPUT  103
#define SWITCHBRANCH  104
#define SHOWSTATUS    105
#define MAKEBRANCH    106
#define SENDASCII     107

#define MAXBRANCH 8
#define TESTBRANCH 4
struct branch {
    char name;   /* 1 character only */
    char active; /* bell interjected */
    char current;  /* set if current output branch */
};


/**********************************************************************************
   Test program for multiplexor

   This switcher forks 4 simple terminal programs on 4 branches.

   The terminal programs have 1 command "output".  Any other command
   is responded with "command '' unknown".  The output command prints
   20 lines of output with the branch identification

   The control branch understands three "commands".   Status shows the
   status of the 4 branches.  <cr> switches to the first active branch.
   <char> switches to the named branch.  All else returns "command '' unknown"
 
   There is no termination.
**********************************************************************************/

factory()
{
   UINT32 oc,rc;
   JUMPBUF;
   int id,i,j;
   char pbuf[128];

   struct branch branches[8];

   KC (caller,KT+5) KEYSTO(b0sik,b0sok,b0cck,tmmk) RCTO(rc);
/* we are forked */

   KC (domkey,Domain_MakeStart) KEYSTO(sb);
   if(!fork()) {   /* start waiter for active branches */

      /* sb is parent which holds sok key and current cck key */ 

       while(1) {
          KC (tmmk,TMMK_WaitForActiveBranch) KEYSTO(cck,bid) RCTO(rc);
          if(rc == KT+1) exit(0);
          if(rc & 1) {
             KC (sb,INTERJECTBELL) KEYSFROM(cck,bid) RCTO(rc);
             if(rc) exit(0);
          }
       }
    }

    if(!fork()) {  /* start reader */
       char buf[128];
       int actlen;
       int controlmode;
       
       strcpy(buf,"[Switchertest]\n[");
       KC (sb,WRITESTRING) CHARFROM(buf,strlen(buf)) RCTO(rc);
       controlmode=1;

       while(1) {
          KC (b0sik,8192+128) CHARTO(buf,128,actlen) KEYSTO(,,,b0sik) RCTO(rc);  

//  KC (comp,15) KEYSTO(k2);
//  KC (k2,0) KEYSTO(,k2) RCTO(rc);
//  sprintf(pbuf,"SIK return rc %d actlen=%d, %x %x %x %x\n",rc,actlen,buf[0],buf[1],buf[2],buf[3]);
//  KC (k2,0) CHARFROM(pbuf,strlen(pbuf)) RCTO(rc); 

//          if(rc > 1) exit(0);

          if ((actlen == 1) && (*buf == 0x1b)) {  /* switch character */
              if(!controlmode) {
                  controlmode=1;
                  KC (sb,SWITCHOUTPUT) KEYSFROM(b0cck);
                  KC (sb,WRITESTRING) CHARFROM("[",1) RCTO(rc);
                  if(rc) exit(0);
                  continue;
              }
              else {   /* read an escape in control mode, send to "active branch" */
                  KC (sb,SENDASCII) CHARFROM(buf,1) RCTO(rc);  /* switches to user mode */

//sprintf(pbuf,"GenerateASCII rc %d\n",rc);
//KC (comp,15) KEYSTO(k2);
//KC (k2,0) KEYSTO(,k2) RCTO(oc);
//KC (k2,0) CHARFROM(pbuf,strlen(pbuf)) RCTO(oc);

                  if(!rc) {  /* worked and switched back to "current" */
                      controlmode=0;
                  }  /* else still in control mode */
                  continue;
              }
          }
          if ((actlen == 1) && (buf[0] == '\r')) {
              KC (sb,SWITCHACTIVE) RCTO(rc);
              if(rc) {
                 strcpy(buf,"No Active Branch\n[");
                 KC (sb,WRITESTRING) CHARFROM(buf,strlen(buf));
              }
              else {
                 controlmode=0;
              }
              continue;
          }
          if (actlen == 2) {   /* 1 letter command */ 
              KC (sb,SWITCHBRANCH) CHARFROM(buf,1) RCTO(rc);
              if(rc) {
                   strcpy(buf,"Unknown Branch\n[");
                   KC (sb,WRITESTRING) CHARFROM(buf,strlen(buf));
                   continue;
              }
              else {
                 controlmode=0;
              }
              continue;
          }
          if(!strncmp(buf,"status",6)) {
              KC (sb,SHOWSTATUS);
              strcpy(buf,"[");
              KC (sb,WRITESTRING) CHARFROM(buf,strlen(buf)); 
              continue;
          }
          
//          sprintf(pbuf,"Unknown Command - '%s' %x %x %x %x %x %x %x\n[",
//             buf,*(buf),*(buf+1),*(buf+2),*(buf+3),*(buf+4),*(buf+5),*(buf+6));

          strcpy(pbuf,"Unknown Command\n[");
 
          KC (sb,WRITESTRING) CHARFROM(pbuf,strlen(pbuf));
       }
    }
 
    for(i=0;i<TESTBRANCH;i++) {
       if(!fork()) {  /* a terminal program */
           char buf[128];
           char name;

           name=i+0x41;
           /* we are branch with ID i+1 or i+0x41 */

           KC (sb,MAKEBRANCH) KEYSTO(k0,k1,k2) RCTO(rc);  /* sik sok cck */
           while (1) {
              sprintf(buf,"%c:",name);
              KC (k1,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,k1) RCTO(rc);
              KC (k0,8192+128) CHARTO(buf,128) KEYSTO(,,,k0) RCTO(rc);
              if(!strncmp(buf,"output",6)) {
                  for(j=0;j<100;j++) {
                    sprintf(buf,"Branch %c .............................. Line %d\n",name,j);
                    KC (k1,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,k1) RCTO(rc);
                  }
              }
              else {
                 sprintf(pbuf,"Unknown Command -  %x %x %x %x %x %x %x\n",
                    *(buf),*(buf+1),*(buf+2),*(buf+3),*(buf+4),*(buf+5),*(buf+6));
//                 strcpy(pbuf,"Command Unknown\n");
                 KC (k1,0) CHARFROM(pbuf,strlen(pbuf)) KEYSTO(,,,k1) RCTO(rc);
              }
           }
       }
    }

/* main, wait for instructions and do work */

    KC (psb,SB_CreateNode) KEYSTO(NODE);   /* save BID/CCK for branches */
    for (id=0;id<MAXBRANCH;id++) {
        branches[id].name=0;
        branches[id].active=0;
        branches[id].current=0;
    } 

    KC (domkey,Domain_GetKey+b0cck) KEYSTO(cck,bid);

    LDEXBL (comp,0);

    while(1) {
       char buf[128];
       int actlen;

/* cck and bid always contain the currently active branch */

       LDENBL OCTO(oc) KEYSTO(k0,k1,,caller) CHARTO(buf,128,actlen);
       RETJUMP();

//  KC (comp,15) KEYSTO(k2);
//  KC (k2,0) KEYSTO(,k2) RCTO(rc);
//  sprintf(pbuf,"Main %d actlen %d\n",oc,actlen);
//  KC (k2,0) CHARFROM(pbuf,strlen(pbuf)) RCTO(rc); 

       switch(oc) {
       case INTERJECTBELL:

           KC (k1,0) CHARTO(buf,6) RCTO(rc);   /* a data key */
           id = buf[5];
/********************************** opps, need this ?? ********************************/
//           if(branches[id-1].current) {  /* we don't do this for current */
//              LDEXBL (caller,0);
//              break;
//           }
           branches[id-1].active = 1;   /* 0 in table is id = 1 */

//  sprintf(pbuf,"Interject Bell id %d\n",id);
//  KC (k2,0) CHARFROM(pbuf,strlen(pbuf)) RCTO(rc);

           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(b0cck);
           KC (b0sok,0) CHARFROM("\007",1) KEYSTO(,,,b0sok) RCTO(rc);
           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);  /* might be b0cck */
           LDEXBL (caller,0);
           break;

       case SWITCHACTIVE:

           for(id=0;id<MAXBRANCH;id++) {
               KC (NODE,Node_Fetch+2*id+0) KEYSTO(cck);
               KC (tmmk,TMMK_BranchStatus) KEYSFROM(cck) RCTO(rc);

               if(branches[id].active || (rc & 0x04)) {
                   branches[id].active=0;
                   KC (NODE,Node_Fetch+2*id+1) KEYSTO(bid);
                   break;
               }
           }
           if(id == MAXBRANCH) {  /* no active branch */
               LDEXBL (caller,1); 
               break;
           } 
           buf[0] = branches[id].name; 
           buf[1] = ']';
           buf[2] = '\r';
           buf[3] = '\n';
           KC (b0sok,0) CHARFROM(buf,4) KEYSTO(,,,b0sok) RCTO(rc);
           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);
           KC (tmmk,TMMK_SwitchInput) KEYSFROM(cck);
           branches[id].current=1;
           LDEXBL (caller,0);
           break;

       case SWITCHBRANCH:  /* letter code in buf[0] */
           for(id=0;id<MAXBRANCH;id++) branches[id].current=0;
           for(id=0;id<MAXBRANCH;id++) {
              if(buf[0] == branches[id].name) {
                 branches[id].active=0;
                 KC (NODE,Node_Fetch+2*id+0) KEYSTO(cck);
                 KC (NODE,Node_Fetch+2*id+1) KEYSTO(bid);
                 KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);
                 KC (tmmk,TMMK_SwitchInput)  KEYSFROM(cck);
                 branches[id].current=1;
                 break;
              }
           }
           if(id == MAXBRANCH) {
              LDEXBL (caller,1);
              break;
           }
           LDEXBL (caller,0); 
           break;

       case SHOWSTATUS:    /* of all branches except control */
           for(id=0;id<MAXBRANCH;id++) {
               if(branches[id].name) {
                 KC (NODE,Node_Fetch+2*id+0) KEYSTO(k0);  /* cck */
                 KC (tmmk,TMMK_BranchStatus) KEYSFROM(k0) RCTO(rc);
                 sprintf(buf,"Branch %c Status %X Active %d\n",branches[id].name,rc,
                       branches[id].active);
                 KC (b0sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,b0sok) RCTO(rc);
               }
           }
           LDEXBL (caller,0);
           break;

       case MAKEBRANCH:    /* make a registered branch */
           for(id=0;id<MAXBRANCH;id++) {
              if(!branches[id].name) {  /* one available */
                  KC (comp,Node_Fetch+COMPDKC) KEYSTO(k1);
                  memset(buf,0,6);
                  buf[5]=id+1;
                  branches[id].name = buf[5]+0x60;
                  KC (k1,0) CHARFROM(buf,6) KEYSTO(k1);   /* a bid */
                  KC (NODE,Node_Swap+2*id+1) KEYSFROM(k1);
                  KC (tmmk,0) KEYSFROM(k1) KEYSTO(k0,k1,k2) RCTO(rc);
                  KC (NODE,Node_Swap+2*id+0) KEYSFROM(k2); 
                  LDEXBL (caller,rc) KEYSFROM(k0,k1,k2);
                  break;
              }
           }
           if(id == MAXBRANCH) {
              LDEXBL (caller,1);
           }
           break;

       case SWITCHOUTPUT:   /* exclusively used to switch to control branch output */
                            /* last branch is still marked as "current" */
           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0);
           KC (domkey,Domain_GetKey+k0) KEYSTO(cck,bid);
           LDEXBL (caller,0);
           break;

       case WRITESTRING:
           KC (b0sok,0) CHARFROM(buf,actlen) KEYSTO(,,,b0sok) RCTO(rc);
           LDEXBL (caller,0);
           break;
    
       case SENDASCII:      /* sends to "current" branch (the one last outputing) */
           rc=1;
           for(id=0;id<MAXBRANCH;id++) {
              if(branches[id].current) {

//  KC (comp,15) KEYSTO(k2);
//  KC (k2,0) KEYSTO(,k2) RCTO(rc);
//  sprintf(pbuf,"GenerateASCII id=%d\n",id);
//  KC (k2,0) CHARFROM(pbuf,strlen(pbuf)) RCTO(rc); 


                  KC (NODE,Node_Fetch+2*id+0) KEYSTO(cck);
                  KC (NODE,Node_Fetch+2*id+1) KEYSTO(bid);
                  KC (tmmk,TMMK_GenerateASCIIInput) KEYSFROM(cck) CHARFROM(buf,actlen) RCTO(rc);
                  if(!rc) {
                     KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);
                     KC (tmmk,TMMK_SwitchInput)  KEYSFROM(cck);
                     rc=0;
                  }
                  break;
              }
           }
           LDEXBL (caller,rc);
           break; 

       }
    }
}
