/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**********************************************************************/
/* CKPTDVR -                                                          */
/* EXTERNAL TIMER DRIVEN ROUTINE TO SIGNAL "TAKE CHECKPOINT".         */
/* ENSURES THAT A CHECKPOINT IS TAKEN AT LEAST EVERY 5 MINUTES.       */
/* KEEPS TRACK OF ANY DOWNTIME OBSERVABLE THROUGH THE JOURNAL PAGE.   */
/* COLLECTS DUMPS.                                                    */
/*                                                                    */
/* Address map:   (early object environment, all primordially set up) */
/*   0x00000000   Code                                                */
/*   0x00100000   Stack                                               */
/*   0x00200000   Journal Page                                        */
/*   0x00300000   Crash Table log page                                */ 
/*                                                                    */
/**********************************************************************/

#define WOMB 1

#include "keykos.h"
#include "wombdefs.h"
#include "domain.h"
#include "node.h"
#include "lli.h"
#include "kernelp.h"

#define CRASHTABLESIZE 100

 /* The following is a circular table with one entry for each restart.*/
 /* Each entry is 8 bytes:                                            */
 /*   The time of the last checkpoint before the restart.             */
 /*   The time from then until the restart (+/- 1 sec).               */
 /* Cursor is the index of the next available entry in crash table.   */
 
struct crash_table {
    LLI  last_checkpoint;
    LLI  downtime;
};
 
/* Slots in PARMNODE (set up primordially) */
#define PARMBWAITKEY    0
#define PARMCKPTKEY     1
#define PARMPRIVKEY     2
#define PARMSYSTIMER    3
#define PARMCONSOLE     4

#define DERRORLOG      14
 
/* Domain general keys node slot usage */

KEY PARMNODE   =  7;   /* initially has node with some keys we need */
KEY BWAITKEY   =  8;   /* BWAIT key for timer */
KEY CKPTKEY    =  9;   /* key to request that a checkpoint be taken */
KEY CDUMPKEY   =  10;
KEY SYSTIMER   =  11;
KEY CONSOLE    =  12;
KEY SOK        = 13;
KEY K0         = 15;
 
/* Ordercodes used with the keys */
#define BWAITWAIT      0        /* wait for time set */
#define BWAITSET       1        /* set time into BWAIT */
#define TAKECHECKPOINT 1        /* ask for a checkpoint to be taken */
 
char title[] = "CKPTDVRC";    /* program name      */
int stacksiz = 4096;           /* desired stacksize */
int bootwomb = 1;
 
/*********************************************************************/
/* Begin Program Code                                                */
/*********************************************************************/
factory()
{
   JUMPBUF;
 
 /* Define local (stack) variables */
 LLI tod, temp_tod;                                 /* for tod values */
 LLI last_restart_ckpt_tod={0,1};    /* last restart checkpoint tod we know */
 LLI checkpoint_delay = {1*60,0};    /* 30 seconds between checkpoints (in big seconds ) */
 struct KernelPage *jp = (struct KernelPage *)0x0200000;
 struct {                                /* used to note IPL occurred */
     uint16  m_class;                                /* message class */
     char    m_data[3];                            /* message data  */
 } message = { 1, "\00\02\00" };
 struct crash_table *ct = (struct crash_table *)0x0300000;
  UINT32 rc;
 
 int cursor = 0;
 
 /* Get keys from parameter node */
 KALL(PARMNODE, Node_Fetch+PARMBWAITKEY)  KEYSTO (BWAITKEY);
 KALL(PARMNODE, Node_Fetch+PARMCKPTKEY)   KEYSTO (CKPTKEY);
 KALL(PARMNODE, Node_Fetch+PARMPRIVKEY)   KEYSTO (CDUMPKEY);
 KALL(PARMNODE, Node_Fetch+PARMSYSTIMER)  KEYSTO (SYSTIMER);
 KALL(PARMNODE, Node_Fetch+PARMCONSOLE)   KEYSTO (CONSOLE);
#ifdef cdump
 KALL(CDUMPKEY, Node_Fetch+3)             KEYSTO (CDUMPKEY);
#endif
 KALL(CONSOLE,0) KEYSTO(,SOK) RCTO(rc);
 
 /* Main Program Loop                                               */
 /* Get current time-of-day.                                        */
 /* Then check for restart by comparing local variable to the       */
 /*   restart checkpoint TOD in the kernal journal page.            */
 /* If there was a restart, store info in crash table and get dump. */
 /* Then see if checkpoint time interval has expired.               */
 /* If so, then take a checkpoint, else wait till time expires.     */
 
 for(;;) {
   KALL(SYSTIMER,7) STRUCTTO(tod);
 
   /* test if there was a restart */
   if (llicmp(&last_restart_ckpt_tod,&jp->KP_RestartCheckPointTOD) != 0) {
 
     /* yes, save the information locally */
     last_restart_ckpt_tod = jp->KP_RestartCheckPointTOD;
     ct[cursor].last_checkpoint = jp->KP_RestartCheckPointTOD;
     temp_tod = tod;
     llisub(&temp_tod, &jp->KP_RestartCheckPointTOD);
     ct[cursor].downtime = temp_tod;
 
     /* increment cursor, "wrap" if necessary */
     if (++cursor >= CRASHTABLESIZE) cursor = 0;
 
     /* get errorlog key and leave message about IPL */
#ifdef cdump
     KALL(PARMNODE, Node_Fetch+PARMPRIVKEY) KEYSTO (K0);
     KALL(K0, Node_Fetch+DERRORLOG) KEYSTO (K0);
     KALL(K0, 0) STRUCTFROM(message);            /* note IPL occurred */
     KALL(CDUMPKEY, 0);                          /* collect any dump */
#endif
   }
   /* Now see if we need to take a checkpoint */
   /* Get most recent value of LASTCKPTTOD or RESTARTTOD */
   temp_tod = jp->KP_LastCheckPointTOD;
   if (llicmp(&jp->KP_RestartTOD, &temp_tod) > 0) {
      temp_tod = jp->KP_RestartTOD;
   }
   /* add the checkpoint delay time and test if "less than now" */
   lliadd(&temp_tod, &checkpoint_delay);
   if (llicmp(&temp_tod, &tod) < 0) {
     KALL(CKPTKEY, TAKECHECKPOINT) STRUCTFROM(tod);
   }
   else {
#ifdef debug
 {char buf[64];sprintf(buf,"TOD %lX %lX, next ckpt %lX %lX\n",
         tod.hi,tod.low,temp_tod.hi,temp_tod.low);
               KC(SOK,0) CHARFROM(buf,strlen(buf)) RCTO(rc);
 }
#endif
     KALL(BWAITKEY, BWAITSET) STRUCTFROM(temp_tod);
     KALL(BWAITKEY, BWAITWAIT);
   }
 } /* end for(;;) loop */
 
}/* end ckptdvr */
