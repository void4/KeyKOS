/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h" 
#include "tssf.h"
#include "switcherf.h"
#include "domain.h"
#include "node.h"
#include "snode.h"
#include "sb.h"
#include "ocrc.h"
#include "dc.h"
#include "cck.h"

   KEY comp     = 0;
#define COMPDKC  1
#define COMPSNF  2
#define COMPTSSF 3
#define COMPCONSOLE 15
   KEY sb       = 1;
   KEY caller   = 2;
   KEY domkey   = 3;
   KEY psb      = 4;
   KEY meter    = 5;
   KEY dc       = 6;

   KEY tmmk     = 7;
   KEY b0sik    = 8;
   KEY SNODE     = 8;   /* NOTE B0SIK */
   KEY b0sok    = 9;
   KEY b0cck    =10;

   KEY bid      =11;
   KEY cck      =12;

   KEY k2       =13;
   KEY k1       =14;
   KEY k0       =15;

    char title [] = "SWITCHERF";

#define NODEREADERDOM 3
#define NODEWAITERDOM 4
#define NODECENTRALDOM 5
#define NODEDC 6

#define INTERJECTBELL 100
#define SWITCHACTIVE  101
#define WRITESTRING   102
#define SWITCHOUTPUT  103
#define SWITCHBRANCH  104
#define SHOWSTATUS    105
#define MAKEBRANCH    106
#define SENDASCII     107
#define DESTROYBRANCH 108

#define DESTROY       200

#define MAXBRANCH 256

#define SNODESLOTS 6
#define SNODESLOTCCK 0
#define SNODESLOTBID 1
#define SNODESLOTZMK 2
#define SNODESLOTTEMP 3
#define SNODESLOTTEMP1 4

struct branch {
    char name[2];  /* 2 character only */
    char active;   /* bell interjected */
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
   char buf[128];
   int actlen;


   KC (caller,KT+5) KEYSTO(b0sik,b0sok,b0cck,caller) RCTO(rc);

/* at this point b0sik, etc are the base circuit that is given to the */
/* tmmk to build a multiplexor.  They become b0sik, etc as part of the */
/* creation of the multiplexor */

   KC (comp,Node_Fetch+COMPTSSF) KEYSTO(k0);
   KC (k0,EXTEND_OC) KEYSFROM(psb,meter,sb) KEYSTO(,,,k0) RCTO(rc);
   if (rc != EXTEND_RC) {
      exit(INVALIDOC_RC);
   }
   KC (k0,EXTEND_OC) KEYSFROM(b0sik,b0sok,b0cck) KEYSTO(,,,k0) RCTO(rc);
   if (rc != EXTEND_RC) {
      exit(INVALIDOC_RC);
   }
   KC (comp,0) KEYSTO(,,k1);   /* a zero data key, the easy way */
   buf[0]=0x1b;
   KC (k0,TSSF_CreateTMMK) KEYSFROM(k1) CHARFROM(buf,1) KEYSTO(tmmk,b0sik,b0sok,b0cck) RCTO(rc);
              /* now have mux */
   if (rc) {
      exit(rc);
   }

   KC (domkey,Domain_MakeStart) KEYSTO(bid);  /* for central to call me back */

   if(!(rc=fork())) {   /* start database (central) domain */
      KC (domkey,Domain_GetKey+bid) KEYSTO(k0);  /* k0 is central domain */

      LDEXBL (k0,0) KEYSFROM(domkey);  /* send home domain key */
      FORKJUMP();

      dodb();
      exit(0);
   }
   if(rc > 1) {
      exit(rc);
   }
   LDENBL OCTO(oc) KEYSTO(bid);
   LDEXBL (comp,0);
   RETJUMP();   /* wait for central to check in */
  
   KC (domkey,Domain_GetMemory) KEYSTO(k0);
   KC (k0,Node_Swap+NODECENTRALDOM) KEYSFROM(bid);

   KC (bid,Domain_MakeStart) KEYSTO(bid);  /* now have central domain key for others */

   KC (domkey,Domain_MakeStart) KEYSTO(cck);

   if(!(rc=fork())) {   /* start waiter for active branches */

      /* bid is central domain */ 

       KC (domkey,Domain_GetKey+bid) KEYSTO(k0);  /* k0 is central domain */
       LDEXBL (cck,0) KEYSFROM(domkey);
       FORKJUMP();

       while(1) {
          KC (tmmk,TMMK_WaitForActiveBranch) KEYSTO(cck,bid) RCTO(rc);
          if(rc == KT+1) exit(0);
          if(rc & 1) {
             KC (k0,INTERJECTBELL) KEYSFROM(cck,bid) RCTO(rc);
             if(rc) exit(0);
          }
       }
    }
    if(rc > 1) {
       exit(rc);
    }
    
    LDENBL OCTO(oc) KEYSTO(cck);
    LDEXBL (comp,0);
    RETJUMP();  /* wait for waiter to check in */
   
    KC (domkey,Domain_GetMemory) KEYSTO(k0);
    KC (k0,Node_Swap+NODEWAITERDOM) KEYSFROM(cck);

    KC (domkey,Domain_MakeStart) KEYSTO(cck);

    if(!(rc=fork())) {  /* start reader */
       char buf[128];
       int actlen;
       int controlmode;

       /*  bid is central domain */

       KC (domkey,Domain_GetKey+bid) KEYSTO(k0);
       LDEXBL (cck,0) KEYSFROM(domkey);
       FORKJUMP();
       
       strcpy(buf,"[");   /* additional prompt */
       KC (k0,WRITESTRING) CHARFROM(buf,strlen(buf)) RCTO(rc);
       controlmode=1;

       while(1) {
          KC (b0sik,8192+128) CHARTO(buf,128,actlen) KEYSTO(,,,b0sik) RCTO(rc);  
          if(rc == KT+1) {   /* circuit went away, probably mux is gone  */
              exit(0);
          }

          if ((actlen == 1) && (*buf == 0x1b)) {  /* switch character */
              if(!controlmode) {
                  controlmode=1;
                  KC (k0,SWITCHOUTPUT) KEYSFROM(b0cck);
                  KC (k0,WRITESTRING) CHARFROM("[",1) RCTO(rc);
                  if(rc) exit(0);
                  continue;
              }
              else {   /* read an escape in control mode, send to "active branch" */
                  KC (k0,SENDASCII) CHARFROM(buf,1) RCTO(rc);  /* switches to user mode */
                  if(!rc) {  /* worked and switched back to "current" */
                      controlmode=0;
                  }  /* else still in control mode */
                  continue;
              }
          }
          if ((actlen == 1) && (buf[0] == '\r')) {
              KC (k0,SWITCHACTIVE) RCTO(rc);
              if(rc) {
                 strcpy(buf,"No Active Branch\n[");
                 KC (k0,WRITESTRING) CHARFROM(buf,strlen(buf));
              }
              else {
                 controlmode=0;
              }
              continue;
          }
          if (actlen < 4) {   /* 1 or 2 letter command */ 
              if(actlen == 2) {  /* if only 1 letter, second is null */
                  buf[1]=0;
              }  
              KC (k0,SWITCHBRANCH) CHARFROM(buf,2) RCTO(rc);
              if(rc) {
                   strcpy(buf,"Unknown Branch\n[");
                   KC (k0,WRITESTRING) CHARFROM(buf,strlen(buf));
                   continue;
              }
              else {
                 controlmode=0;
              }
              continue;
          }
          if(!strncmp(buf,"status",6)) {
              KC (k0,SHOWSTATUS);
              strcpy(buf,"[");
              KC (k0,WRITESTRING) CHARFROM(buf,strlen(buf)); 
              continue;
          }
          
          strcpy(pbuf,"Unknown Command\n[");
 
          KC (k0,WRITESTRING) CHARFROM(pbuf,strlen(pbuf));
       }
    }
    if(rc > 1) { 
       exit(rc);
    } 

    LDEXBL (comp,0);
    LDENBL OCTO(oc) KEYSTO(cck);
    RETJUMP();
  
    KC (domkey,Domain_GetMemory) KEYSTO(k0);
    KC (k0,Node_Swap+NODEREADERDOM) KEYSFROM(cck);
    KC (k0,Node_Swap+NODEDC) KEYSFROM(dc);


/* become switcher key */

/* BID is the central domain */

    KC (domkey,Domain_MakeStart) KEYSTO(k0);
    LDEXBL (caller,0) KEYSFROM(k0);

    while(1) {
       LDENBL OCTO(oc) KEYSTO(k0,k1,k2,caller) CHARTO(buf,128,actlen);
       RETJUMP();

// sprintf(pbuf,"Switcher oc=%d\n",oc);
// outsok(pbuf);

       if(oc == DESTROY_OC) {

/* must destroy snode, reader, waiter, and central domains */

/* NOTE: does not return the original sik,sok,cck keys.  It is up to the */
/* creator to recover the sik/sok keys from the original cck key */

          KC (domkey,Domain_GetMemory) KEYSTO(cck);
          KC (cck,Node_Fetch+NODEREADERDOM) KEYSTO(k0);
          KC (cck,Node_Fetch+NODEWAITERDOM) KEYSTO(k2);
          KC (cck,Node_Fetch+NODEDC) KEYSTO(dc);

          KC (k0,Domain_MakeBusy) RCTO(rc);  /* READER Busy */
          KC (k2,Domain_MakeBusy) RCTO(rc);  /* WAITER Busy */

          KC (bid,DESTROY) RCTO(rc);    /* gets rid of all branches and tmmk */

          KC (cck,Node_Fetch+NODECENTRALDOM) KEYSTO(k1);
          KC (k1,Domain_MakeBusy) RCTO(rc);  /* CENTRAL Busy */

          KC (k1,Domain_GetKey+SNODE) KEYSTO(SNODE);
          KC (SNODE,DESTROY_OC) RCTO(rc);    /* destroy Central Snode */

          KC (k0,Domain_GetMemory) KEYSTO(cck);
          KC (cck,Node_Fetch+1) KEYSTO(bid);
          KC (psb,SB_DestroyPage) KEYSFROM(bid);
          KC (psb,SB_DestroyNode) KEYSFROM(cck);
          KC (dc,DC_DestroyDomain) KEYSFROM(k0,psb);

          KC (k1,Domain_GetMemory) KEYSTO(cck);
          KC (cck,Node_Fetch+1) KEYSTO(bid);
          KC (psb,SB_DestroyPage) KEYSFROM(bid);
          KC (psb,SB_DestroyNode) KEYSFROM(cck);
          KC (dc,DC_DestroyDomain) KEYSFROM(k1,psb);

          KC (k2,Domain_GetMemory) KEYSTO(cck);
          KC (cck,Node_Fetch+1) KEYSTO(bid);
          KC (psb,SB_DestroyPage) KEYSFROM(bid);
          KC (psb,SB_DestroyNode) KEYSFROM(cck);
          KC (dc,DC_DestroyDomain) KEYSFROM(k2,psb);

          exit(0);
       }

       switch(oc) {
       case Switcher_CreateBranch:
//outsok("CreateBranch\n");
           KC (bid,MAKEBRANCH) CHARTO(buf,2) KEYSTO(k0,k1,k2) RCTO(rc);
           LDEXBL (caller,rc) CHARFROM(buf,2) KEYSFROM(k0,k1,k2);
           break;
       case Switcher_DisconnectBranch:
           LDEXBL (caller,INVALIDOC_RC);
           break;
       case Switcher_ConnectBranch:
           LDEXBL (caller,INVALIDOC_RC);
           break;
       case Switcher_DestroyBranch:
           KC (bid,DESTROYBRANCH) CHARFROM(buf,actlen) RCTO(rc);  /* pass name */
           LDEXBL (caller,0);
           break;
       } 
    }
 
/* main, wait for instructions and do work */
}

/*******************************************************************************
   GENNAME - makes branch name from ID
*******************************************************************************/
genname(str,id) 
    char *str;
    int id;    /* 1 to 256 inclusive */
{
    int fl,sl;

    if(id < 27) {  /* single letter */
        str[0]=id+0x60;
        str[1]=0;
    }
    else {
        fl = ((id-1) / 26) +1;
        sl = ((id-1) % 26) +1;
        str[0] = fl+0x60;
        str[1] = sl+0x60;
    }
}

/*******************************************************************************
  DODB  - database domain

  where the work is done 
*******************************************************************************/
dodb() {   /* the central "database" domain */
    JUMPBUF;
    int id,i;
    UINT32 rc,oc;
    int slot;
    char pbuf[128];
    int controlmode=1;

   struct branch branches[MAXBRANCH];

/* The following must be updated for larger number of branches */

    KC (comp,Node_Fetch+COMPSNF) KEYSTO(SNODE);
    KC (SNODE,SNodeF_Create) KEYSFROM(psb,meter,sb) KEYSTO(SNODE);
    for (id=0;id<MAXBRANCH;id++) {
        branches[id].name[0]=0;
        branches[id].name[1]=0;
        branches[id].active=0;
        branches[id].current=0;
    } 

    KC (domkey,Domain_GetKey+b0cck) KEYSTO(cck,bid);
    KC (b0sok,0) KEYSTO(,,,b0sok) RCTO(rc);

    LDEXBL (comp,0);  /* become ready */
    while(1) {
       char buf[128];
       int actlen;

/* cck and bid always contain the currently active branch */

       LDENBL OCTO(oc) KEYSTO(k0,k1,,caller) CHARTO(buf,128,actlen);
       RETJUMP();

// sprintf(pbuf,"Main call %d\n",oc);
// outsok(pbuf);

       switch(oc) {
       case INTERJECTBELL:

           KC (k1,0) CHARTO(buf,6) RCTO(rc);   /* a data key */
           id = buf[5];

           if(!controlmode && branches[id-1].current) {  /* this probably happened because of an */
                                         /* interject bell which required a temporary */
                                         /* switch to the control branch which may  */
                                         /* cause the current branch to go "active" */
               LDEXBL(caller,0);
               break;                    /* ignore this to avoid looping */
           }
           branches[id-1].active = 1;   /* 0 in table is id = 1 */

//  sprintf(pbuf,"Interject Bell id %d controlmode %d\n",id,controlmode);
//  outsok(pbuf);

           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(b0cck);
           KC (b0sok,0) CHARFROM("\007",1) KEYSTO(,,,b0sok) RCTO(rc);

           if(!controlmode) {  /* no need to switch again if on control branch */
              for(id=0;id<MAXBRANCH;id++) {  /* find current branch to switch back to */
                 if(branches[id].current) {  /* here it is */
                      slot=SNODESLOTS*id+SNODESLOTCCK;
                      KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);
                      KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck) RCTO(rc);  /* might be b0cck, might be dk0 */
                 }
              }
           }
           LDEXBL (caller,0);
           break;

       case SWITCHACTIVE:

           for(id=0;id<MAXBRANCH;id++) {
               slot = SNODESLOTS*id+0;
               KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);
               KC (tmmk,TMMK_BranchStatus) KEYSFROM(cck) RCTO(rc);

               if(branches[id].active || (rc & 0x04)) {
                   branches[id].active=0;
                   slot=SNODESLOTS*id+SNODESLOTBID;
                   KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(bid);
                   break;
               }
           }
           if(id == MAXBRANCH) {  /* no active branch */
               LDEXBL (caller,1); 
               break;
           } 
           buf[0] = branches[id].name[0]; 
           buf[1] = 0;
           if(branches[id].name[1]) {
              buf[1] = branches[id].name[1];
              buf[2] = 0;
           }
           strcat(buf,"]\n");

           KC (b0sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,b0sok) RCTO(rc);
           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);
           KC (tmmk,TMMK_SwitchInput) KEYSFROM(cck);

           for(i=0;i<MAXBRANCH;i++) branches[i].current=0;
           branches[id].current=1;
           controlmode=0;
           LDEXBL (caller,0);
           break;

       case SWITCHBRANCH:  /* letter code in buf[0] */
           for(id=0;id<MAXBRANCH;id++) {
              if((buf[0] == branches[id].name[0]) && buf[1] == branches[id].name[1]) {
                 branches[id].active=0;
                 slot=SNODESLOTS*id+SNODESLOTCCK;
                 KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);
                 slot=SNODESLOTS*id+SNODESLOTBID;
                 KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(bid);
                 KC (tmmk,TMMK_SwitchOutput) KEYSFROM(cck);
                 KC (tmmk,TMMK_SwitchInput)  KEYSFROM(cck);
                 for(i=0;i<MAXBRANCH;i++) branches[i].current=0;
                 controlmode=0;
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
               if(branches[id].name[0]) {
                 slot=SNODESLOTS*id+SNODESLOTCCK;
                 KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(k0);  /* cck */
                 KC (tmmk,TMMK_BranchStatus) KEYSFROM(k0) RCTO(rc);
                 sprintf(buf,"Branch %c%c Status %X Active %d\n",branches[id].name[0],
                       branches[id].name[1],rc,branches[id].active);
                 KC (b0sok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,b0sok) RCTO(rc);
               }
           }
           LDEXBL (caller,0);
           break;

       case MAKEBRANCH:    /* make a registered branch */
           for(id=0;id<MAXBRANCH;id++) {
              if(!branches[id].name[0]) {  /* one available */
                  KC (comp,Node_Fetch+COMPDKC) KEYSTO(k1);
                  memset(buf,0,6);
                  buf[5]=id+1;
                  genname(branches[id].name,buf[5]);
                  KC (k1,0) CHARFROM(buf,6) KEYSTO(k1);   /* a bid */
                  slot=SNODESLOTS*id+SNODESLOTBID;
                  KC (SNODE,SNode_Swap) STRUCTFROM(slot) KEYSFROM(k1);
                  KC (tmmk,TMMK_CreateBranch) KEYSFROM(k1) KEYSTO(k0,k1,k2) RCTO(rc);
                  if(rc) {
                      LDEXBL (caller,rc);
                      break;  /* from while loop */
                  }
                  slot=SNODESLOTS*id+SNODESLOTCCK;
                  KC (SNODE,SNode_Swap) STRUCTFROM(slot) KEYSFROM(k2); 

/* now make a zmk for the branch */
                  slot=SNODESLOTS*id+SNODESLOTTEMP;
                  KC (SNODE,SNode_Swap) STRUCTFROM(slot) KEYSFROM(b0sok);
                  slot=SNODESLOTS*id+SNODESLOTTEMP1;
                  KC (SNODE,SNode_Swap) STRUCTFROM(slot) KEYSFROM(cck);
                  KC (comp,Node_Fetch+COMPTSSF) KEYSTO(cck);
                  KC (cck,TSSF_CreateZMK) KEYSFROM(psb,meter,psb) KEYSTO(bid,dc,b0sok,cck) RCTO(rc); 
                  if(!rc) {
                      slot=SNODESLOTS*id+SNODESLOTZMK;
                      KC (SNODE,SNode_Swap) STRUCTFROM(slot) KEYSFROM(bid);   /* zmk key */
                      KC (k1,0) KEYSTO(,,,k1) RCTO(rc);  /* limit */
                      KC (bid,ZMK_Connect) STRUCTFROM(rc) KEYSFROM(k0,k1,k2) RCTO(rc);
                      KC (domkey,Domain_GetKey+b0sok) KEYSTO(k1);
                      KC (domkey,Domain_GetKey+cck) KEYSTO(k2);
                      slot=SNODESLOTS*id+SNODESLOTTEMP;
                      KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(b0sok);
                      slot=SNODESLOTS*id+SNODESLOTTEMP1;
                      KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);
                      if(!rc) {
                          LDEXBL (caller,0) CHARFROM(branches[id].name,2) KEYSFROM(dc,k1,k2);
                          break;  /* from while loop */
                      }
                      else {
                          /* zap zmk */
                      }
                  }
                  slot=SNODESLOTS*id+SNODESLOTTEMP;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(b0sok);
                  slot=SNODESLOTS*id+SNODESLOTTEMP1;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);

                  LDEXBL (caller,rc);
                  break;  /* from while loop */
              }
           }
           if(id == MAXBRANCH) {
              LDEXBL (caller,1);
           }
           break;

       case SWITCHOUTPUT:   /* exclusively used to switch to control branch output */
                            /* last branch is still marked as "current" */
                            /* so that sendascii can work */
           controlmode=1;
           KC (tmmk,TMMK_SwitchOutput) KEYSFROM(k0);
           KC (domkey,Domain_GetKey+k0) KEYSTO(cck,bid);
           LDEXBL (caller,0);
           break;

       case WRITESTRING:
           KC (b0sok,0) CHARFROM(buf,actlen) KEYSTO(,,,b0sok) RCTO(rc);
           LDEXBL (caller,0);
           break;
    
       case SENDASCII:      /* sends to the last branch that was outputting  */
           rc=1;
           for(id=0;id<MAXBRANCH;id++) {
              if(branches[id].current) {
                  slot=SNODESLOTS*id+SNODESLOTCCK;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(cck);
                  slot=SNODESLOTS*id+SNODESLOTBID;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(bid);
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

        case DESTROYBRANCH:

           for(id=0;id<MAXBRANCH;id++) {
              if((buf[0] == branches[id].name[0]) && buf[1] == branches[id].name[1]) {
                  slot=SNODESLOTS*id+SNODESLOTZMK;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(k0);
                  KC (k0,KT+4) RCTO(rc);
//sprintf(pbuf,"DestroyZMK rc %X\n",rc);
//outsok(pbuf);
                  slot=SNODESLOTS*id+SNODESLOTCCK;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(k0);
                  KC (tmmk,TMMK_DestroyBranch) KEYSFROM(k0) RCTO(rc);
//sprintf(pbuf,"TMMK_Destroybranch rc %X\n",rc);
//outsok(pbuf);
                  branches[id].name[0]=0;
                  branches[id].name[1]=0;
                  branches[id].active=0;
                  branches[id].current=0;
              }
           } 
           LDEXBL (caller,0);
           break;

        case DESTROY:  /* zap all branches and dissolve */

           for(id=0;id<MAXBRANCH;id++) {
              if(branches[id].name[0]) {
                  slot=SNODESLOTS*id+SNODESLOTZMK;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(k0);
                  KC (k0,KT+4) RCTO(rc);
                  slot=SNODESLOTS*id+SNODESLOTCCK;
                  KC (SNODE,SNode_Fetch) STRUCTFROM(slot) KEYSTO(k0);
                  KC (tmmk,TMMK_DestroyBranch) KEYSFROM(k0) RCTO(rc);
                  branches[id].name[0]=0;
                  branches[id].name[1]=0;
                  branches[id].active=0;
                  branches[id].current=0;
              }
           } 
           KC (tmmk,DESTROY_OC) RCTO(rc);
           LDEXBL (caller,0);
           break;

       }
    }
}

outsok(str)
   char *str;
{
   JUMPBUF;
   UINT32 rc;

   KC (comp,Node_Fetch+COMPCONSOLE) KEYSTO(sb);
   KC (sb,0) KEYSTO(,sb) RCTO(rc); 
   KC (sb,0) CHARFROM(str,strlen(str)) RCTO(rc);
}
