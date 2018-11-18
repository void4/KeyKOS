/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************       
                                                                        
   Primitive Scheduler Defines            


   PSchedf(PSchedF_CreateSimpleScheduler{0};SB,M,SB,SMNODE,OMNODE => c;PSADMIN,PSNOTIFY,PSCHEDSTATUS
   PSchedAdmin(PSched_CreateMeter => c;METER,PSCHEDCHANGE,PSCHEDMETERID)
   PSchedAdmin(PSched_SetPolicy,(PSched_Policy);PSCHEDMETERID => c)
   PSchedAdmin(PSched_DestroyMeter;PSCHEDMETERID => c)
   PSchedAdmin(PSched_StopMeter;PSCHEDMETERID => c)
   PSchedAdmin(PSched_StartMeter;PSCHEDMETERID => c);
   PSchedNotify(PSched_Wait => c,(PSched_Policy);PSCHEDMETERID)
   PSchedStatus(PSched_GetStatus;PSCHEDMETERID => c,(PSched_MeterStatus))
   PSchedChange(PSched_ChangePolicy,(PSched_Policy) => c)


****************************************************************/
                                                                        
#ifndef _H_psched
#define _H_psched

#define PSchedF_AKT              0x0168
#define PSchedAdmin_AKT          0x0068
#define PSchedNotify_AKT         0x0268
#define PSchedStatus_AKT         0x0368
#define PSchedChange_AKT         0x0468

#define PSchedF_CreateSimpleScheduler 0

#define PSched_CreateMeter  0
#define PSched_SetPolicy    1
#define PSched_DestroyMeter 2
#define PSched_StopMeter    3
#define PSched_StartMeter   4

#define PSched_Wait         10

#define PSched_GetStatus    20

#define PSched_ChangePolicy 30

/* return codes */

#define PSched_TooManyMeters 4
#define PSched_ChangeNotNode 5
#define PSched_ChangeNotData 6
#define PSched_ChangeNotMagic 7
#define PSched_InvalidIDKey  8


/* definitions of simple policies supported */

#define POLICYUNUSED   0
#define POLICYPRIORITY 1
#define POLICYPERCENTAGE 2

struct pformat0 {                 /* Policy description format 0 */
     short policy;                /* one of  supported types     */
     short policyvalue;           /* 0 - 100 for priority and percentage */
};

struct sformat0 {    /* Meter Status format 0 */
     short policy;                /* one of supported types */ 
     short policyvalue;           /* 0-100 */
     short relpercent;            /* percentage used since last period */
     short percent;               /* percent used since creation */
     long long used;              /* microseconds used since creation */
};

struct PSched_Policy {
     short format;          /* format version number */
     
     union {
        struct pformat0 p0;
     } un;
};

struct PSched_MeterStatus {
     short format;

     union {
        struct sformat0 s0;
     } un;
};
                                                                        
#endif
