/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include "domain.h"
#include "node.h"
#include "sb.h"
#include "fs.h"
 
    KEY comp   = 0;
    KEY concck   = 1;	
    KEY caller = 2;
    KEY dom    =  3;
    KEY sb     =  4;
    KEY meter  =  5;
    KEY domcre = 6;
    KEY segment = 7;
    KEY node    = 8;

    KEY consik     = 9;
   
    KEY master = 10;
    KEY helper = 11;

    KEY consok   = 12;

    KEY dum1 = 13;
    KEY dum2 = 15;
    KEY dk0 = 14;

#define COMPCLOCK 0
#define COMPJOURNAL 1
#define COMPSNC 2
#define COMPFSC 3

    char title[]="DEMO2   ";
    int stacksiz=4096;

struct journalp {
    long long Lastckpt;
    long long RestartchkpTOD;
    long long RestartTOD;
    long long KP_LastSetTOD;
    long long systime;
};

#define JOURNALP 0x00200000
#define TESTP 0x00300000
#define COWP  0x00400000

#define MEMNODEMETER 9

factory(foc,ord)
    int foc,ord;
{

     JUMPBUF;

     unsigned long oc,rc,rc1,rc2,rc3;
     register int i,j;
     int actlen;
     char buf[80];
     struct {
         long long align;
         char buf[256];
     } foo;
     char *str;
     int len;
     int slot;
     struct journalp *jp;
     long long start,stop;
     long long copyargs;
     char *tp;
     int *ip,*ip1;
     int warn;

     struct Domain_SPARCRegistersAndControl drac;
  
   struct Node_KeyValues nkv ={3,5,
     {{0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff}}
     }; 

     unsigned long long bdominst,bkerninst,bdomcycle,bkerncycle;
     unsigned long long edominst,ekerninst,edomcycle,ekerncycle;
     
     KC (caller,KT+5) KEYSTO(consik,consok,concck,caller) RCTO(rc);

     KC (dom,Domain_GetMemory) KEYSTO(dum1);
     KC (comp,COMPJOURNAL) KEYSTO(dum2);
     KC (dum1,Node_Swap+2) KEYSFROM(dum2);

     KC (sb,SB_CreateNode) KEYSTO(dum1);
     KC (dum1,Node_WriteData) STRUCTFROM(nkv);
     KC (dum1,Node_Swap+1) KEYSFROM(meter);
     KC (dum1,Node_MakeMeterKey) KEYSTO(meter);
//     KC (dom,Domain_Swap+10) KEYSFROM(dum1);
     KC (dom,Domain_GetMemory) KEYSTO(dum2);
     KC (dum2,Node_Swap+MEMNODEMETER) KEYSFROM(dum1);
//
     KC (dom,Domain_SwapMeter) KEYSFROM(meter);


     jp=(struct journalp *)JOURNALP;
     tp=(char *)TESTP;

     strcpy(buf,"\n\n\n\rHello from\r\n\n");
     KC (concck,0) KEYSTO(consik,consok,concck);
     KC (consok,0) KEYSTO(,,,consok) RCTO(rc);	
//     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
		    
#ifdef xx
     strcpy(buf,"LL       UU  UU  NN    NN      AA       "\
                " 88888   88888   KK  KK \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     strcpy(buf,"LL       UU  UU  NNN   NN      AA       "\
                "88   88 88   88  KK KK  \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     strcpy(buf,"LL       UU  UU  NNNN  NN     AAAA      "\
                "88   88 88   88  KKKK   \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     strcpy(buf,"LL       UU  UU  NN NN NN    AA  AA     "\
                " 88888   88888   KKK    \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

     strcpy(buf,"LL       UU  UU  NN  NNNN   AAAAAAAA    "\
                "88   88 88   88  KK KK  \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

     strcpy(buf,"LLLLLLL  UUUUUU  NN   NNN  AA      AA   "\
                "88   88 88   88  KK  KK \r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

     strcpy(buf,"LLLLLLL  UUUUUU  NN    NN AA        AA  "\
                " 88888   88888   KK   KK\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
#endif

     
     strcpy(buf,"\nWelcome to Agorics Pacific Kernel Test Program\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     strcpy(buf,"This program will demonstrate Pacific execution\r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     strcpy(buf,"and its support of object-to-object communication.\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     strcpy(buf,"Timings are adjusted for 60 Mhz Clock\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
      
//     strcpy(buf,"Two Activities can be performed:\r\n\n");
//     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

options:     
     strcpy(buf,"Enter '1' to exercise Pacific object-to-object communication\r\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     strcpy(buf,"facilities (known as 'Key Calls').\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

     strcpy(buf,"Enter '9' to Quit.\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     
     strcpy(buf,"Make your selection now and press <RETURN>. ");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     
     KC (consik,8192+80) CHARTO(buf,80,actlen) KEYSTO(,,,consik) RCTO(rc);
     buf[actlen] = '\0';
     if(!strcmp(buf,"9\r")) goto quit;
     if(!strcmp(buf,"1\r")) goto btime;
     goto options;

btime:
     KC (dom,64) KEYSTO(master);
     if(!fork()) { /* helper domain */
        KC (dom,64) KEYSTO(helper);
        LDEXBL (master,0) KEYSFROM(helper);
        for (;;) {
           LDENBL OCTO(oc) KEYSTO(,,,caller);
           RETJUMP();
           if(oc==KT+4) break;
           switch (oc) {
             case 0:   /* pass and receive nothing */
                  LDEXBL (caller,0);
                  for (;;) {
                     LDENBL OCTO(oc) KEYSTO(,,,caller);
                     RETJUMP ();
                     if(oc) break;    /* leave loop */
                     LDEXBL (caller,0);
                  }
               break;
             case 1:   /* pass and receive a 256 byte string */
                  LDEXBL (caller,0);
                  for (;;) {
                     LDENBL OCTO(oc) KEYSTO(,,,caller) CHARTO(foo.buf,256,actlen);
                     RETJUMP ();
                     if(oc) break;    /* leave loop */
                     LDEXBL (caller,0) CHARFROM(foo.buf,actlen);
                  }
               break;	
             case 2:   /* pass and receive 4 keys   */
                  LDEXBL (caller,0);
                  for (;;) {
                     LDENBL OCTO(oc) KEYSTO(dum1,dum2,dk0,caller);
                     RETJUMP ();
                     if(oc) break;    /* leave loop */
                     LDEXBL (caller,0) KEYSFROM(dum1,dum2,dk0,node);
                  }
               break;	
             case 3:   /* pass and receive 4 keys and a string */
                  LDEXBL (caller,0);
                  for (;;) {
                     LDENBL OCTO(oc) KEYSTO(dum1,dum2,dk0,caller)
                         CHARTO(foo.buf,256,actlen);
                     RETJUMP ();
                     if(oc) break;    /* leave loop */
                     LDEXBL (caller,0) CHARFROM(foo.buf,actlen) 
                         KEYSFROM(dum1,dum2,dk0,node);
                  }
               break;
             case 4:   /* pass and receive a 128 byte string */
                  LDEXBL (caller,0);
                  for (;;) {
                     LDENBL OCTO(oc) KEYSTO(,,,caller) CHARTO(foo.buf,128,actlen);
                     RETJUMP ();
                     if(oc) break;    /* leave loop */
                     LDEXBL (caller,0) CHARFROM(foo.buf,actlen);
                  }
               break;	
             default: break;
           }
        }
        return 0;
     }
     KC (comp,0) KEYSTO(,,dk0);
     LDEXBL (dk0,0);
     LDENBL OCTO(oc) KEYSTO(helper);
     RETJUMP ();  /* wait to get key to helper */

     strcpy(buf,"Begin Key Call exercise now.\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

     strcpy(buf,"Do you wish to exercise 100,000 calls to Data Key Y/N?\r\n");
     if(yorn(buf)) {
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++)  {
           KC (dk0,1) CHARTO(buf,16,actlen) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }

     strcpy(buf,"Do you wish to exercise 100,000 KT calls to domcre Y/N?\r\n");
     if(yorn(buf)) {
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (domcre,KT) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
     }

     strcpy(buf,"Do you wish to exercise 100,000 calls to Get_Register_Info Y/N?\r\n");
     if(yorn(buf)) {
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (dom,Domain_GetSPARCRegs) CHARTO(foo.buf,256,actlen) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 calls to Get_Control_Info Y/N?\r\n");
     if(yorn(buf)) {
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (dom,Domain_GetSPARCControl) CHARTO(foo.buf,256,actlen) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 calls to Node_Fetch Y/N?\r\n");
     if(yorn(buf)) {
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (comp,0) KEYSTO(dum1) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }
   
     strcpy(buf,"Do you wish to excercise 100,000 calls to Supernode 4 deep Y/N?\r\n");
     if(yorn(buf)) {
        slot=0x1234;
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        KC (comp,COMPSNC) KEYSTO(dum1);
        KC (dum1,0) KEYSFROM(sb,meter,sb) KEYSTO(dum1);
        KC (dum1,42) STRUCTFROM(slot) KEYSFROM(meter);
        start=jp->systime;
        for(i=0;i<100000;i++) {
            KC (dum1,41) STRUCTFROM(slot) KEYSTO(dum2);
#ifdef xx
            superfetch(dum1,slot,dum2);
#endif
        }
        stop=jp->systime;
        KC (dum1,KT+4) RCTO(rc);
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }

     strcpy(buf,"Do you wish to exercise 100,000 calls to MapAPage Y/N?\r\n");
     if(yorn(buf)) {
        warn=0;
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        KC (sb,SB_CreatePage) KEYSTO(dum1);
        KC (sb,SB_CreatePage) KEYSTO(dum2);
        KC (dom,Domain_GetMemory) KEYSTO(node);
        start=jp->systime;
        for(i=0;i<50000;i++) {
           KC (node,Node_Swap+3) KEYSFROM(dum1);
           if(*tp) {
             if(*tp != 'a') {
                if(!warn) {
                   warn=1;
                   sprintf(buf,"Data fetched should be 'a' is '%c'\r\n",*tp);
                   KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok)
                        RCTO(rc);
                }
             }
           }
           *tp='a';
           KC (node,Node_Swap+3) KEYSFROM(dum2);
           if(*tp) {
             if(*tp != 'b') {
                if(!warn) {
                   warn=1;
                   sprintf(buf,"Data fetched should be 'b' is '%c'\r\n",*tp);
                   KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok)
                        RCTO(rc);
                }
             }
           }
           *tp='b';
        }
        stop=jp->systime;
        KC (sb,SB_DestroyPage) KEYSFROM(dum1);
        KC (sb,SB_DestroyPage) KEYSFROM(dum2);
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,100000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,100000);
     }
     
   
     strcpy(buf,"Do you wish to exercise 100,000 NULL messages to helper Y/N?\r\n");
     if(yorn(buf)) {
        KC (helper,0);  /* signal case 0 */
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (helper,0) RCTO(rc);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (helper,1);  /* force out of loop */
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 256 byte string messages to helper Y/N?\r\n");
     if(yorn(buf)) {
        KC (helper,1);  /* signal case 1 */
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (helper,0) CHARFROM(foo.buf,256) RCTO(rc)
                CHARTO(foo.buf,256,actlen);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (helper,1);
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 128 byte string messages to helper Y/N?\r\n");
     if(yorn(buf)) {
        KC (helper,4);  /* signal case 4 (could use case 1 ) */
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (helper,0) CHARFROM(foo.buf,128) RCTO(rc)
                CHARTO(foo.buf,128,actlen);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (helper,1);
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 4 KEYS ONLY messages to helper Y/N?\r\n");
     if(yorn(buf)) {
        KC (helper,2);  /* signal case 2 */
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (helper,0) KEYSFROM(consik,consok,concck) RCTO(rc)
               KEYSTO(dum1,dum2,dk0,master);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (helper,1);
     }
   
     strcpy(buf,"Do you wish to exercise 100,000 4 KEYS+STRING messages to helper Y/N?\r\n");
     if(yorn(buf)) {
        KC (helper,3);  /* signal case 3 */
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;
        for(i=0;i<100000;i++) {
           KC (helper,0) KEYSFROM(consik,consok,concck)
              CHARFROM(foo.buf,256)  RCTO(rc)
              KEYSTO(dum1,dum2,dk0,master) CHARTO(foo.buf,256,actlen);
        }
        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (helper,1);
     }
     KC (helper,KT+4) RCTO(rc);


     strcpy(buf,"Do you wish to exercise 100,000 Domain Keeper Faults (trap) Y/N?\r\n");
     if(yorn(buf)) {
         KC(dom,Domain_GetKey+3) KEYSTO(master);  /* my domain key to master */
         if(!fork()) {  /* the keeper */
              KC (dom,Domain_MakeStart) KEYSTO(dum1);
              KC (master,Domain_SwapKeeper) KEYSFROM(dum1) KEYSTO(dum2);
              LDEXBL (comp,0);
              for (;;) {
                  LDENBL OCTO(oc) KEYSTO(,,dum1,caller) STRUCTTO(drac);
                  RETJUMP();
                  if(4 == oc) {
                       KC(master,Domain_SwapKeeper) KEYSFROM(dum2);
                       exit();
                  }
		  if (0x80000097u != oc) {
			KC (comp, 0x40000000|oc);
			crash(oc);
		  }
                  drac.Control.PC = drac.Control.NPC;
                  drac.Control.NPC = drac.Control.PC+4;
/* nothing to do for bad trap call  step over */
                  LDEXBL(dum1,Domain_ResetSPARCStuff) KEYSFROM(,,,caller)
                      STRUCTFROM(drac);
              }
        }
        readinst(&bdominst,&bkerninst,&bdomcycle,&bkerncycle);
        start=jp->systime;

        for(i=0;i<100000;i++) {
           crash(i);
        }

        stop=jp->systime;
        readinst(&edominst,&ekerninst,&edomcycle,&ekerncycle);
        prtcycles(start,stop,200000);
        prtinst(bdominst,bkerninst,bdomcycle,bkerncycle,
                edominst,ekerninst,edomcycle,ekerncycle,200000);
        KC (dom,Domain_GetKeeper) KEYSTO(dum1);
        KC (dum1,4) RCTO(rc);
     }
     strcpy(buf,"Do you wish to exercise CopyOnWrite Y/N?\r\n");
     if(yorn(buf)) {
        ip=(int *)TESTP;
        ip1=(int *)COWP;
        KC (dom,Domain_GetMemory) KEYSTO(node);
        KC (comp,COMPFSC) KEYSTO(dum1);
        KC (dum1,0) KEYSFROM(sb,meter,sb) KEYSTO(dum1);
        KC (node,Node_Swap+3) KEYSFROM(dum1);
        for(i=0;i<1024*4;i++) {ip[i]=i;} 
        copyargs=0x0000000000004000ll;
        for(j=0;j<4;j++) {
            KC (dum1,FS_CopyMe) KEYSFROM(sb) STRUCTFROM(copyargs) KEYSTO(dum2);
            KC (node,Node_Swap+4) KEYSFROM(dum2);

            for(i=0;i<1024*4;i++) {
                 if(ip1[i] != i) {
         sprintf(buf,"Checking Pass %d (Child before modify) ip1[%d] = %d should be %d\n",
                   j,i,ip[i],i);
                    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok)
                        RCTO(rc);
                    break;
                 }
            }

            for(i=0;i<1024*4;i++) {ip1[i]= -i;}

            /* now check parent */
            for(i=0;i<1024*4;i++) {
                 if(ip[i] != i) {
          sprintf(buf,"Pass %d (parent) ip[%d] = %d should be %d\n",j,i,ip[i],i);
                    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok)
                        RCTO(rc);
                    break;
                 }
            }
            /* now check child */ 
            for(i=0;i<1024*4;i++) {
                 if(ip1[i] != -i) {
          sprintf(buf,"Pass %d (child) ip1[%d] = %d should be %d\n",j,i,ip[i],-i);
                    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok)
                        RCTO(rc);
                    break;
                 }
            }

            KC (dum2,KT+4);
        }
     }

     KC (comp,0) KEYSTO(,,dk0);
     strcpy(buf,"End Key Call tests.\r\n\n");
     KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
     goto options;

quit:
     KC (helper,KT+4) RCTO(rc);  /* make sure child is dead */
                                 /* the above should stall if child living */
//     KC (dom,Domain_Get+10) KEYSTO(dum1);
     KC (dom,Domain_GetMemory) KEYSTO(dum1);
     KC (dum1,Node_Fetch+MEMNODEMETER) KEYSTO(dum1);
//
     KC (dum1,Node_Fetch+1) KEYSTO(meter);
     KC (dom,Domain_SwapMeter) KEYSFROM(meter);
     KC (sb,SB_DestroyNode) KEYSFROM(dum1);

     return 0;
}
prompt(buf)
    char *buf;
{
    JUMPBUF;

    unsigned long rc;

    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
    KC (consik,8192+1) KEYSTO(,,,consik) RCTO(rc);
}
yorn(buf)
    char *buf;
{
    JUMPBUF;
    unsigned long rc;
    int actlen;

    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
    KC (consik,8192+80) CHARTO(buf,80,actlen) KEYSTO(,,,consik) RCTO(rc);
    buf[actlen] = '\0';
    if(!strcmp(buf,"y\r")) return 1;
    if(!strcmp(buf,"Y\r")) return 1;
    return 0;
}
prtcycles(start,stop,njumps)
    long long start;
    long long stop;
    int njumps;
{
    JUMPBUF;
    char buf[80];
    long long diff;
    unsigned long rc;
    int len;

    diff=stop-start;
    diff=diff/4096;
    diff=diff*60;
    diff=diff/njumps;
    len=diff;

    sprintf(buf,"%d cycles per jump\r\n",len);
    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
}
readinst(dominst,kerninst,domcycle,kerncycle)
   unsigned long long *dominst,*kerninst,*domcycle,*kerncycle;
{
   JUMPBUF;
   unsigned long rc;
   char data[16];

//   KC (dom,Domain_Get+10) KEYSTO(dum1);
   KC (dom,Domain_GetMemory) KEYSTO(dum1);
   KC (dum1,Node_Fetch+MEMNODEMETER) KEYSTO(dum1);
//
   KC (dum1,Node_Fetch+6) KEYSTO(dum2);
   KC (dum2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)dominst,&data[8],8);
   KC (dum1,Node_Fetch+10) KEYSTO(dum2);
   KC (dum2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)domcycle,&data[8],8);
   KC (dum1,Node_Fetch+11) KEYSTO(dum2);
   KC (dum2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)kerninst,&data[8],8);
   KC (dum1,Node_Fetch+12) KEYSTO(dum2);
   KC (dum2,1) CHARTO(data,16) RCTO(rc);
   memcpy((char *)kerncycle,&data[8],8);

   return 0; 
}
prtinst(bdi,bki,bdc,bkc,edi,eki,edc,ekc,jumps)
   unsigned long long bdi,bki,bdc,bkc,edi,eki,edc,ekc;
   int jumps;
{
   JUMPBUF;
   unsigned long long dif;
   unsigned long secs,micros; 
   char buf[100];
   unsigned long long dominst,domcycle,kerninst,kerncycle;
   unsigned long rc;
   unsigned long long j;

   dominst=edi-bdi;
   domcycle=edc-bdc;
   kerninst=eki-bki;
   kerncycle=ekc-bkc;
   j=jumps;

 sprintf(buf,"Domain Instructions/Cycles %lld/%lld\n",dominst/j,domcycle/j);
    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
   if(domcycle) {
     dominst *= 100;
     dif = dominst/domcycle;
     secs = dif/100;
     micros = dif -(secs*100);
     sprintf(buf,"    IPC %d.%02d\n",secs,micros);
    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
   }
   sprintf(buf,"Kernel Instructions/Cycles %lld/%lld\n",kerninst/j,kerncycle/j);
    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
   if(kerncycle) {
     kerninst *= 100;
     dif = kerninst/kerncycle;
     secs = dif/100;
     micros = dif -(secs*100);
     sprintf(buf,"    IPC %d.%02d\n",secs,micros);
    KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);
   }

}
dump(f)
   unsigned char *f;
{
   JUMPBUF;
   char buf[80];
   unsigned long rc;

   sprintf(buf,"%2X%2X%2X%2X%2X%2X%2X%2x\r\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]); 
   KC (consok,0) CHARFROM(buf,strlen(buf)) KEYSTO(,,,consok) RCTO(rc);

}
