/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************
    MKEEPER - simple meter keeper
 
*************************************************************/
#include "keykos.h"
#include "kktypes.h"
#include "sb.h"
#include "node.h"
#include "domain.h"
#include "ocrc.h"
#include "mkeeper.h"

   KEY COMP      =  0;
#define COMPCONSOLE 15
   KEY SB        =  1;
   KEY CALLER    =  2;
   KEY DOMKEY    =  3;
   KEY PSB       =  4; 
   KEY M         =  5;
   KEY DC        =  6; 
 
   KEY BNODE     =  7;  /* base node */
   KEY NODE      =  8;  /* active node */
   KEY FNODE     =  9;  /* Next (forward) node */

   KEY NODEKEY   = 10;

   KEY K0        = 15;
 
   char title[]="MKEEPERF";

   int stacksiz=4096;
 
factory(factoc,factord)
   UINT32 factoc,factord;
{
     JUMPBUF;
     UINT32 oc,rc;
     int extranodes = 0;
     int numberofkeys = 0;
    
     char buf[256];

     KC (PSB,SB_CreateNode) KEYSTO(BNODE) RCTO(rc);  /* base key storage node */
     if (rc) {
         exit(NOSPACE_RC);
     }
     KC (DOMKEY,Domain_GetKey+BNODE) KEYSTO(NODE);  /* reset starter node */

     KC (DOMKEY,Domain_MakeStart) KEYSTO(K0);
     LDEXBL (CALLER,0) KEYSFROM(K0);                 /* return mkeeper key to caller */

     for(;;) {
        LDENBL OCTO(oc) KEYSTO(,,NODEKEY,CALLER);
        RETJUMP();

        if(oc == KT) {                               /* what am i */
            LDEXBL (CALLER,MKeeper_AKT);
            continue;
        }
   
        if(oc == DESTROY_OC) {                       /* go away */
            if(numberofkeys != 0) {                  /* must not be holding keys */
                 LDEXBL (CALLER,MKeeper_HoldsKeys);
                 continue;
            }
            KC (PSB,SB_DestroyNode) KEYSFROM(BNODE); /* destroy base node */
            exit(0);
        }
        if(oc == MKeeper_HowManyKeys) {
            LDEXBL (CALLER,numberofkeys);
            continue;
        }
 
        if(oc == MKeeper_RestartKeys) {              /* start all stopped domains */
            int i;
            int slot;
  
            KC (DOMKEY,Domain_GetKey+BNODE) KEYSTO(NODE);  /* start at base */
            slot=0;                                        /* slot 0 */
            for(i=0;i<numberofkeys;i++) {                  /* for number of keys held */
               if (slot == 15) {                           /* end of node .. next */
                   KC (NODE,Node_Fetch+15) KEYSTO(NODE);   /* next node in chain */
                   slot = 0;                               /* slot back to zero */
               }
               KC (NODE,Node_Fetch+slot) KEYSTO(K0);       /* get restart key */
               LDEXBL (K0,0);                              /* give a kick */
               FORKJUMP();
               slot++;
            }
            
            KC (DOMKEY,Domain_GetKey+BNODE) KEYSTO(NODE);  /* start at base */
            KC (NODE,Node_Fetch+15) KEYSTO(NODE);          /* get first extra */
            for(i=0;i<extranodes;i++) {                    /* for all extras */
                KC (DOMKEY,Domain_GetKey+NODE) KEYSTO(FNODE);  /* save node */
                KC (NODE,Node_Fetch+15) KEYSTO(NODE);      /* get next node */
                KC (PSB,SB_DestroyNode) KEYSFROM(FNODE);   /* destroy saved */
            }
            extranodes = 0;                                /* only base */
            numberofkeys = 0;
            KC (DOMKEY,Domain_GetKey+BNODE) KEYSTO(NODE);  /* reset starter node */
            LDEXBL (CALLER,OK_RC);
            continue;
        }            
        if ((int)oc < 0) {                                      /* a trap */
            int i;
            int slot;
            int nodenumber;

            slot = numberofkeys % 15;                      /* figure current slot */
            nodenumber = numberofkeys / 15;                /* how many nodes */
            if(nodenumber != extranodes) {                 /* need a new node */
                  extranodes++;                            /* bump count */
                  KC (PSB,SB_CreateNode) KEYSTO(FNODE);    /* create node */
                  KC (NODE,Node_Swap+15) KEYSFROM(FNODE);  /* store in chain */
                  KC (NODE,Node_Fetch+15) KEYSTO(NODE);    /* switch to new node */
            }
            KC (NODE,Node_Swap+slot) KEYSFROM(CALLER) KEYSTO(,,CALLER);  /* save key */
            numberofkeys++;                                /* note number */
            LDEXBL (CALLER,OK_RC);
            continue;
        }
        LDEXBL (CALLER,INVALIDOC_RC);                      /* all else is false */
      }
}
