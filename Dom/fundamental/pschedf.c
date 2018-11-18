/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************************

  Primitive scheduler

  Because the schedule must account for down time when calculating the
  amount of CPU available, it seems best to reset the scheduler every
  restart.   This means having the SYSTIMER key and the journal page
  so that restarts can be detected and SYSTIMER can get the restarted
  system time.

  Each meter fault presents the current SYSTIMER value which is used
  during operation to determine the intervals.

  If all the meters are allocated a short amount of CPU time there is no
  need to set a timer in the scheduler.  If no CPU time is consumed, there
  is no need to schedule.  As soon as 1 meter traps, a time interval is
  established and new allocations can be made based on the use of CPU.

  Any meter that doesn't trap hasn't consumed the allocated resources and
  is be definition UNDER used.  The primeter can be read to determine what
  the total CPU used during the interval is.  Alas reading the primemeter is
  the most expensive operation on meters as it causes all meters to be
  scavanged and refilled.

  This uses the simplest memory model

  There is 1 page allocated to hold meter information.  This will support
  100 scheduled meters


*****************************************************************************/

#include "kktypes.h"
#include "kernelpk.h"
#include "keykos.h"
#include "psched.h"
#include "domain.h"
#include "sb.h"
#include "node.h"
#include "ocrc.h"
#include "primekeys.h"
#include "snode.h"
#include "dc.h"

     char title[] = "PSCHEDF";
     int  stacksiz = 4096;

   KEY comp            = 0;  /* components */

#define COMPBWAIT       0
#define COMPSYSTIME     1
#define COMPKERNELPAGE  2
#define COMPSNODEF      3
#define COMPCONSOLE    15

   KEY sb              = 1;
   KEY caller          = 2;
   KEY domkey          = 3;
   KEY psb             = 4;
   KEY meter           = 5;
   KEY domcre          = 6;
   KEY usermeternode   = 7;    /* scheduled meter */
   KEY systemmeternode = 8;    /* overhead meter */

   KEY meternode       = 9;    /* the node of the faulted meter */
                               /* the user is returned Node_MakeMeter of this */
   KEY snode           = 10;   /* all keys stashed here */

   KEY meterid         = 11;
   KEY meterchange     = 12;   /* these are really temps */


   KEY k2         = 13;
   KEY k1         = 14;
   KEY k0         = 15;

#define MAGIC  29873634

   struct KernelPage *KP = (struct KernelPage *)0x00200000;

   struct meter {
       unsigned char state;          /* state of meter */
#define METERRUNNING       0x01
#define METEREXHAUSTED     0x02
#define METERSTOPPED       0x04
       unsigned char policy;         /* priority or percentage */
       short policyvalue;            /* priority (1-100) or percentage (1-100) */
       short requestpolicy;          /* if non-zero request for change outstanding */
       short requestvalue;           /* if non-zero request for change outstanding */
       unsigned long long stopped;   /* value to restore when starting meter */
       unsigned long long totused;   /* total time used */
       unsigned long long lastused;  /* time in most recent period */
       unsigned long long allocated; /* quantum allocated for period */
       unsigned long long timestarted; /* Time stamp when meter is given allocation */
   };

/*
    Keys are saved in the supernode for each meter

    A busy domain is substituted for the keeper in any meter that
    is stopped or exhausted.  This will catch any domains as stallees
    on the busy domain.  When a meter is started or replenished the
    real keeper key is restored and MakeAvailable is called on the busy
    domain.  This restarts all the stalled domains.

    With lots of meters this is probably cheaper (especially with 1 node
    domains) than holding thousands of resume keys in a supernode
*/

#define METERNODE       0
#define METERCHANGENODE 1
#define BUSYDOM         2
#define FIRSTRESUME     3
/* if slots are added, change the following */
#define MAXSLOTS        4

#define maxmeters ((4096-2*sizeof(long long))/sizeof(struct meter))

   struct meterpage {
       unsigned long long restarttime;  /* system time when system was restarted */
       unsigned long long lasttime;     /* time scheduler was last active */
       unsigned long long lastidle;     /* idletime value when schedule was last active */
       struct meter meters[maxmeters];  /* top level meters */
   };

   struct meterpage *MP = (struct meterpage *)0x00300000;

#define DB_INTERNAL 1
#define DB_ADMIN    2
#define DB_NOTIFY   3
#define DB_STATUS   4
#define DB_CHANGE   5
#define DB_KEEPER   6

#define TICKSECONDS  10


factory(foc,ford)
    UINT32 foc,ford;
{
    UINT32 oc,rc;
    JUMPBUF;
    struct Domain_DataByte ddb;
    struct PSched_Policy psp;
    struct PSched_MeterStatus pss;
    union {
        struct PSched_Policy psp;
        unsigned long long system_time;
    } un;
    int actlen;
    UINT16 db;

    if(foc != EXTEND_OC) {
        exit(INVALIDOC_RC);
    }

    {OC(EXTEND_OC);XB(0x00200000);RC(oc);NB(0x08D07802);cjcc(0x00000000,&_jumpbuf); }

    if(oc != PSchedF_CreateSimpleScheduler) {
        exit(INVALIDOC_RC);
    }

    {OC(SB_CreatePage);XB(0x00100000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }
    if(rc) {
        exit(NOSPACE_RC);
    }

    {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+3);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }                        /* meterpage */
    {OC(Node_Fetch+COMPKERNELPAGE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(Node_Swap+2);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }                        /* kernel page */
    MP->restarttime = KP->KP_RestartTOD;

/****************************************************************************
    Need a ticktock domain to tickle psched every N seconds
****************************************************************************/
    ddb.Databyte = DB_INTERNAL;
    {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
    if(!fork()) {  /* the ticker */

        uint64 bwaittime;
        uint64 bwaitseconds;

        bwaitseconds  = TICKSECONDS *4096;
        bwaitseconds  = bwaitseconds *1000000;

        {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Fetch+COMPKERNELPAGE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_Swap+2);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }                    /* kernel page */

        {OC(Node_Fetch+COMPBWAIT);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        while(1) {
            bwaittime = KP->KP_system_time;                  /* current time */
            bwaittime = bwaittime + bwaitseconds;            /* plus seconds */

            {OC(1);PS2(&(bwaittime),sizeof(bwaittime));XB(0x04F00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }                 /* set time */
            {OC(0);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                              /* wait */

            {OC(0);XB(0x00900000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }                       /* inform psched */
            if(rc) {
                break;                                       /* die when psched goes */
            }
        }
        exit(0);
    }
/*************************************************************************/

    {OC(COMPSNODEF);XB(0x00000000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }
    {OC(SNodeF_Create);XB(0xE0A01510);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }

    {OC(COMPSYSTIME);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(7);XB(0x00F00000);RS2(&(MP->lasttime),sizeof(MP->lasttime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }

    ddb.Databyte = DB_ADMIN;
    {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
    ddb.Databyte = DB_NOTIFY;
    {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
    ddb.Databyte = DB_STATUS;
    {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080D000);cjcc(0x08000000,&_jumpbuf); }

    {OC(OK_RC);XB(0xE020FED0); }
    while(1) {
        {RC(oc);DB(db);RS3(&(un),sizeof(un),actlen);NB(0x0FB0F092); }
        {rj(0x00100000,&_jumpbuf); }

        switch(db) {
        case DB_INTERNAL:
             rc=doticktock(oc);
             {OC(rc);XB(0x00200000); }
             continue;

        case DB_ADMIN:
             rc=doadmin(oc,&(un.psp),actlen);     /* sets KEY k0, meterchange, meterid */
             {OC(rc);XB(0xE020FCB0); }
             continue;

        case DB_NOTIFY:
             rc=donotify(oc,&(un.psp));    /* waits and sets KEY meterid */
             {OC(rc);PS2(&(un.psp),sizeof(un.psp));XB(0x8420B000); }
             continue;

        case DB_STATUS:
             rc=dostatus(oc,&pss);    /* uses KEY meterid */
             {OC(rc);PS2(&(pss),sizeof(pss));XB(0x04200000); }
             continue;

        case DB_CHANGE:
             rc=dochange(oc,&(un.psp),actlen);    /* uses KEY meternode to get databyte */
             {OC(rc);XB(0x00200000); }
             continue;

        case DB_KEEPER:
             rc=dokeeper(oc,un.system_time,actlen);
             {OC(0);XB(0x00200000); }
             continue;
        }
    }
}
/**********************************************************************************
   DOTICKTOCK

   Input:  oc
   Output: none

   Side effects:  Meters that are behind schedule will get some CPU time
**********************************************************************************/

int doticktock(oc)
    UINT32 oc;
{
    JUMPBUF;
    UINT32 rc;
    unsigned long long interval,thistime,idletime;

#ifdef xx
    {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);XB(0x00F00000);RC(rc);NB(0x08400F00);cjcc(0x00000000,&_jumpbuf); }
    {OC(0);PS2(".",1);XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
#endif

    if(KP->KP_RestartTOD != MP->restarttime) { /* system restarted */
        MP->restarttime=KP->KP_RestartTOD;
        MP->lasttime = MP->restarttime;    /* advance clock */
        return 0;
    }

    {OC(COMPSYSTIME);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(7);XB(0x00F00000);RS2(&(thistime),sizeof(thistime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
    interval = thistime - MP->lasttime;
    MP->lasttime=thistime;
    {OC(8);XB(0x00F00000);RS2(&(thistime),sizeof(thistime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
    thistime = thistime * 256;    /* in tod units */
    idletime = thistime - MP->lastidle;
    MP->lastidle = thistime;

    updateidlemeters(interval,idletime);

    return 0;
}

/**********************************************************************************
   DOADMIN

   Input:  oc  or  oc,policy,length of parameter
   Output: new meter or policy update, new change key

   Side effects:  Policy update may change meter time allocations (probably wait
                  till next fault or ticktock)
**********************************************************************************/

int doadmin(oc, psp, actlen)
    UINT32 oc;
    struct PSched_Policy *psp;
    int actlen;
{
    struct Node_KeyValues meternkv = {3,5,
     {{0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
      {0,0,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff}}
     };
     static struct Node_KeyValues EXformat= {15,15,
         {Format1K(0,13,15,14,0,0)}
     };
     static int magic = MAGIC;
     struct Node_KeyValues EXdatabyte = {0,0,{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
     struct Node_KeyValues MeterID = {13,13,{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
     struct Domain_DataByte ddb;
     unsigned long long quantum;
     JUMPBUF;
     UINT32 rc;
     unsigned long index,slot,tmagic;
     char pbuf[256];
     unsigned char *ptr;

    if(oc == DESTROY_OC) {
        {OC(DESTROY_OC);XB(0x00A00000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        exit(0);            /* ticktock will die on own */
    }
    if(oc == KT) {
        return PSchedAdmin_AKT;
    }

    if(oc == PSched_CreateMeter) {

        for (index=0;index<maxmeters;index++) {
            if(MP->meters[index].policy == POLICYUNUSED) {
               break;
            }
        }
        if(index == maxmeters) {
            return PSched_TooManyMeters;
        }

        MP->meters[index].state = METERRUNNING;
        MP->meters[index].policy = POLICYPRIORITY;
        MP->meters[index].policyvalue = 50;
        MP->meters[index].totused = 0;
        MP->meters[index].lastused = 0;
        MP->meters[index].allocated = 0;

/* FOR NOW we give meter all it needs */
        quantum = 0xffffffffffffff00ull;  /* max in tod units */

/****************************************************************************
   The Following is for testing         BUG BUG
****************************************************************************/
        if(0) {
           quantum = (TICKSECONDS*1000000)/10;  /* 1 second */
           quantum = quantum * 4096;
        }

        MP->meters[index].allocated = quantum;  /* in TOD values */

        {OC(SB_CreateTwoNodes);XB(0x00100000);RC(rc);NB(0x08E09CB0);cjcc(0x00000000,&_jumpbuf); }
                                       /* meterid will be meterchange (ronocall) */
        if(rc) {
            return NOSPACE_RC;
        }

/* make a meter here in meternode using "usermeter" as the superior meter"  */
/* the meter ID is the index of the meter structure in the meterpage */

/* get meter data key value from .allocated set above */

        {OC(COMPSYSTIME);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(7);XB(0x00F00000);RS2(&(MP->meters[index].timestarted),sizeof(MP->meters[index].timestarted));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
        memcpy(&(meternkv.Slots[0].Byte[9]),&(MP->meters[index].allocated),7);  /* divides allocated by 256 */

  if(0) {
  ptr=&(meternkv.Slots[0].Byte[0]);
  sprintf(pbuf,"1 %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
         ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
         ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);

  {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
  {OC(0);XB(0x00F00000);RC(rc);NB(0x08400F00);cjcc(0x00000000,&_jumpbuf); }
  {OC(0);PS2(pbuf,strlen(pbuf));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
  }

        {OC(Node_WriteData);PS2(&(meternkv),sizeof(meternkv));XB(0x04900000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Node_MakeMeterKey);XB(0x00700000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }       /* must make meter from node */
        {OC(Node_Swap+1);XB(0x8090F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        ddb.Databyte = DB_KEEPER;
        {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Node_Swap+2);XB(0x8090F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

        slot = index*MAXSLOTS + METERNODE;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84A09000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

/* make a front end key here for the change key */

        {OC(Node_WriteData);PS2(&(EXformat),sizeof(EXformat));XB(0x04C00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
        ddb.Databyte = DB_CHANGE;
        {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080E000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Node_Swap+14);XB(0x80C0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

        memcpy(&(EXdatabyte.Slots[0].Byte[12]),&index,4);
        memcpy(&(EXdatabyte.Slots[0].Byte[8]),&magic,4);
        {OC(Node_WriteData);PS2(&(EXdatabyte),sizeof(EXdatabyte));XB(0x04C00000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

        memcpy(&(MeterID.Slots[0].Byte[12]),&index,4);
        memcpy(&(MeterID.Slots[0].Byte[8]),&magic,4);
        {OC(Node_WriteData);PS2(&(MeterID),sizeof(MeterID));XB(0x04900000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

        slot = index*MAXSLOTS + METERCHANGENODE;
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84A0C000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

        {OC(DC_CreateDomain);XB(0x80601000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        slot = index*MAXSLOTS + BUSYDOM;
        {OC(Domain_MakeBusy);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
        {OC(SNode_Swap);PS2(&(slot),sizeof(slot));XB(0x84A0F000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

/* make return keys */

        {OC(Node_MakeFetchKey);XB(0x00C00000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_MakeFrontendKey);XB(0x00C00000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
        {OC(Node_MakeMeterKey);XB(0x00900000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }   /* now have a meter */

        return 0;
    }

    if(oc == PSched_DestroyMeter) {   /* k0 is meterid fetch key to FE key */
        char keybuf[16];

        {OC(Node_Fetch+0);XB(0x00F00000);RC(rc);NB(0x08E0FCB0);cjcc(0x00000000,&_jumpbuf); }
        if(rc) {
           return PSched_InvalidIDKey;
        }
        {OC(1);XB(0x00F00000);RC(rc);RS2(keybuf,16);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
        if(rc != KT+1) {
           return PSched_InvalidIDKey;
        }
        memcpy(&tmagic,&keybuf[8],4);
        memcpy(&index,&keybuf[12],4);
        if(tmagic != MAGIC) {
           return PSched_InvalidIDKey;
        }
/* now destroy meter with index index */

        slot=index*MAXSLOTS + METERNODE;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04A00000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
        slot=index*MAXSLOTS + METERCHANGENODE;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04A00000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
        slot=index*MAXSLOTS + BUSYDOM;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04A00000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
        {OC(DC_DestroyDomain);XB(0xC060F100);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

        MP->meters[index].policy = POLICYUNUSED;
        MP->meters[index].policyvalue = 0;
        MP->meters[index].totused = 0;
        MP->meters[index].lastused = 0;
        MP->meters[index].allocated = 0;

        {OC(SB_DestroyNode);XB(0x80109000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
        {OC(SB_DestroyNode);XB(0x8010C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

        return 0;
    }


    return 0;
}

/**********************************************************************************
   DONOTIFY

   Input:  oc
   Output: Policy change request, meterid KEY

   Side effects: none
**********************************************************************************/

int donotify(oc, psp)
    UINT32 oc;
    struct PSched_Policy *psp;
{
    if(oc == KT) {
        return PSchedNotify_AKT;
    }

    return 0;
}

/**********************************************************************************
   DOSTATUS

   Input:  oc, meterid KEY (in meternode)
   Output: status

   Side effects: none
**********************************************************************************/

int dostatus(oc,pss)
    UINT32 oc;
    struct PSched_MeterStatus *pss;
{
    JUMPBUF;
    UINT32 rc;
    char keybuf[16];
    unsigned long magic,index,slot;
    uint64 starting = 0x00FFFFFFFFFFFFFF;
    uint64 cpu,dif;

    if(oc == KT) {
        return PSchedStatus_AKT;
    }
/***************************************************************************
  HACK:  Temporary readmeter since inception - no keeper activity
***************************************************************************/

    if(oc == PSched_GetStatus) {
        {OC(Node_Fetch+0);XB(0x00F00000);RC(rc);NB(0x08E0FCB0);cjcc(0x00000000,&_jumpbuf); }
        if(rc) {
           return PSched_InvalidIDKey;
        }

        {OC(1);XB(0x00F00000);RC(rc);RS2(keybuf,16);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
        if(rc != KT+1) {
           return PSched_InvalidIDKey;
        }
        memcpy(&magic,&keybuf[8],4);
        memcpy(&index,&keybuf[12],4);
        if(magic != MAGIC) {
           return PSched_InvalidIDKey;
        }
        slot=index*MAXSLOTS + METERNODE;
        {OC(SNode_Fetch);PS2(&(slot),sizeof(slot));XB(0x04A00000);NB(0x00809000);cjcc(0x08000000,&_jumpbuf); }
        {OC(Node_Fetch+3);XB(0x00900000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
        {OC(1);XB(0x00F00000);RC(rc);RS2(keybuf,16);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
        memcpy(&cpu,&keybuf[8],8);
        dif = starting-cpu;
        dif = dif/16;                     /* microseconds since creation */

        pss->un.s0.used = dif;
        return 0;
    }

    return INVALIDOC_RC;
}

/**********************************************************************************
   DOCHANGE

   Input:  oc, policy, length of parameter, change KEY node
   Output: none

   Side effects:  Notify is awakened with the request
**********************************************************************************/

int dochange(oc,psp,actlen)
    UINT32 oc;
    struct PSched_Policy *psp;
    int actlen;
{
    JUMPBUF;
    UINT32 rc;
    unsigned char keybuf[16];
    int index,magic;

    if(oc == KT) {
         return PSchedChange_AKT;
    }

    {OC(Node_Fetch+0);XB(0x00900000);RC(rc);NB(0x0880F000);cjcc(0x00000000,&_jumpbuf); }
    if (rc) {
        return PSched_ChangeNotNode;
    }
    {OC(1);XB(0x00F00000);RC(rc);RS2(keybuf,16);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
    if(rc != KT+1) {
        return PSched_ChangeNotData;
    }
    memcpy(&magic,&keybuf[8],4);
    memcpy(&index,&keybuf[12],4);

    if(magic != MAGIC) {
        return PSched_ChangeNotMagic;
    }

    if(0) {
       char buf[256];
       {OC(COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);XB(0x00F00000);RC(rc);NB(0x08400F00);cjcc(0x00000000,&_jumpbuf); }
       sprintf(buf,"Change for ID %d\n",index);
       {OC(0);PS2(buf,strlen(buf));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
    }

    return 0;
}

/**********************************************************************************
   DOKEEPER

   Input:  oc, system_time, length of parameter, meternode KEY
   Output: none

   Side effects: meter may be given CPU time.  Other meters may be adjusted
**********************************************************************************/

int dokeeper(oc,system_time,actlen)
    UINT32 oc;
    uint64 system_time;
    int actlen;
{
/*********************************************************************************
    Meter has exhausted its quantum.   There is time left in the sample period.

    Calculate the total quantum to be divvied up here as what is left of this
    period plus the full next sample interval.

    See if ahead or behind of schedule

    If ahead of schedule and there is idle time, give it some more time
        how much??

    If ahead of schedule and there is no idle time see if any other meters
    are behind schedule.  If not give more time.  If yes, don't refill
        how much??

    If behind schedule then give it more time.
         The quantum to give is the amount behind schedule capped by the size
         of the nominal adjustment interval.

         Adjust the quantum of other meters.
               If there is no idle time, stop meters that are ahead of schedule.

               If there is no idle time and there are no meters ahead of schedule
               to stop stop some lower priority meter.  Pick the meter closest to
               on schedule or ahead of schedule.  Stop the lowest Priority meter

               If there is idle time then subtract from each meter that is ahead
               of schedule the correct percentage of the time given to the behind
               schedule meter

    For percentage meters the schedule is easy to calculate

    For priority meters the ahead/behind is harder to pick.  One scheme is

       calculate the total quantum allowed to priority meter by subtracting
       the guaranteed percentage from 100 * the total quantum (time interval)

       calculate the weighted quantum based on priority.
           sum the priority values of all priority domains
           divide the total priority quantum by the sum
           give each meter priority * dividend

       compare the amount used with the allotment calculated above.

       If behind then give allotment

       If ahead give allotment if there is idle time else don't refill

    How does one detect idle time?  Well one way is to read the idle
    time from the kernel (running the idle dib) but if a low priority
    meter consumes its quantum before the end of the sample period
    this suggests that higher priority meters are not consuming all
    of their time.  Hence, if this meter weren't running there would
    be idle time.

    Hence one must compare the elapsed time with the quantum time.  If
    a meter consumes its quantum near the end of the sample, it should
    not be refilled.  If it is near the time expected if it is the only
    meter consuming, then the decision whether to refill it is based on
    whether or not the scheduler should overbook or reserve CPU in case
    a high priority meter wakes up.

**********************************************************************************/
    char pbuf[256];  /* place to format some diagnostic info */
    char keybuf[16];
    unsigned long magic,index;
    unsigned long long thistime,interval,sampleinterval;
    unsigned long long intervalmicros,samplemicros,quantum;
    int i,j,k,l;
    struct Node_KeyValues meterCPU = {3,3, {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}} };
    JUMPBUF;
    UINT32 rc;
    unsigned char *ptr;

    {OC(Node_Fetch+13);XB(0x00900000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(1);XB(0x00F00000);RC(rc);RS2(keybuf,16);NB(0x0B000000);cjcc(0x00080000,&_jumpbuf); }
    memcpy(&magic,&keybuf[8],4);
    memcpy(&index,&keybuf[12],4);

    {OC(COMPSYSTIME);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
    {OC(7);XB(0x00F00000);RS2(&(thistime),sizeof(thistime));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
    interval = thistime - MP->meters[index].timestarted;


/* NOPE must time stamp the time the quantum was added to the meter */

    sampleinterval = TICKSECONDS * 1000000;
    sampleinterval = sampleinterval * 4096;

    intervalmicros = interval/4096;
    samplemicros = sampleinterval/4096;

    quantum = MP->meters[index].allocated;
    quantum = quantum/4096;

    i=samplemicros;
    j=quantum;
    k=(samplemicros-intervalmicros);
    l=intervalmicros;

 if(0) {
       sprintf(pbuf,"Meter[%d] S %d, I %x(%d), Q %d, R %d\n",index,i,l,l,j,k);
       {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);XB(0x00F00000);RC(rc);NB(0x08400F00);cjcc(0x00000000,&_jumpbuf); }
       {OC(0);PS2(pbuf,strlen(pbuf));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
 }

    /* for now we refresh with the same amount. just testing mechanism */

    MP->meters[index].timestarted=thistime;
    memcpy(&(meterCPU.Slots[0].Byte[9]),&(MP->meters[index].allocated),7);   /* divides allocated by 256 */
    {OC(Node_WriteData);PS2(&(meterCPU),sizeof(meterCPU));XB(0x04900000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }

  if(0) {
  ptr=&(meterCPU.Slots[0].Byte[0]);
  sprintf(pbuf,"2 %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
         ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
         ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);

  {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
  {OC(0);XB(0x00F00000);RC(rc);NB(0x08400F00);cjcc(0x00000000,&_jumpbuf); }
  {OC(0);PS2(pbuf,strlen(pbuf));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
  }

    return 0;
}

/**********************************************************************************
   UPDATEIDLEMETERS

   Input:  interval (microseconds/4096) - since last ticktock
           idle (microseconds/4096) - amount of idle time in last interval
   Output: none

   Side effects: Meters may have their quantum updated

   Not all meters have quantum's assigned because they may have been
   at or over their scheduled consumption.  These meters may be given
   some time by this call.  Meters that haven't consumed their time
   are not affected.
**********************************************************************************/

updateidlemeters(interval,idle)
   unsigned long long interval;
   unsigned long long idle;
{
   int index;

   for (index=0;index<maxmeters;index++) {
       if(MP->meters[index].state == METEREXHAUSTED) {
       }
   }
}
