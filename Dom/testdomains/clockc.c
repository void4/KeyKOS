/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*   MODULE     CLOCK C                                                *
/*   TITLE      CLOCK - TIME OF DAY ROUTINES                           *
/*   Converted from CLOCKF ASSEMBLE      5/17/90                       *
/***********************************************************************
/***********************************************************************
/***********************************************************************
/*                                                                     *
/*        This routine reads the Time of Day clock and returns the     *
/*              time and data in some usable format.                   *
/*                                                                     *
/*        CLOCK(OC,(8,CLOCK)==>C,(VALUE STRING))                       *
/*              THE FOLLOWING ORDER CODES ARE RECOGNIZED:              *
/*        0     (4,TOD IN UNITS OF 1/38400 SECOND),(4,DATE)            *
/*        1     (4,TOD IN UNITS OF 1/100 SECOND),4(DATE)               *
/*        2     (4,TOD IN FORM PACKED DECIMAL HH MM SS TH),(4,DATE)    *
/*        3     (8,BINARY VALUE OF TIME OF DAY CLOCK SINCE MIDNIGHT)   *
/*        4     (8,BINARY VALUE OF TIME OF DAY CLOCK (EPOCH 1900 ETC)  *
/*        5     (31,TIME, DATE, AND DAY OF WEEK IN EBCDIC AS FOLLOWS:  *
/*               'MM:DD:YYHH:MM:SS.HHZZZWEEKDAY' (WEEKDAY IS 9 CHARS)  *
/*          MONTH,DATE,YEAR,HOURS,MINUTES,SECONDS,HUNDREDTHS,TIMEZONE  *
/*        6     (4,TOD IN FORM PACKED DECIMAL HH MM SS TF),(4,DATE)    *
/*                                                                     *
/*        9     (8,systime-caltimeoffset)                              *
/*                                                                     *
/*        0-6 Return time and data information in the current time zone*
/*        100-106 ARE IDENTICAL TO 0-6, EXCEPT TIME ZONE IS UT         *
/*                                                                     *
/*        FOR CODES 0,1,2 DATE IS IN PACKED DECIMAL IN THE FORM        *
/*         '00YYJJJF' WHERE JJJ IS THE JULIAN DATE AND F IS X'0F'      *
/*                                                                     *
/*        The high two bits of the data byte of the start key          *
/*        are used to restrict destroy and change timezone rights      *
/*        as follows:                                                  *
/*           No_Destroy    = x'80'  destroy rights are restricted      *
/*           No-ZoneChange = x'40'  Change timezone rights restricted  *
/*                                                                     *
/*        Note: Zone 0 is UT, add 1 for each successive hour west.     *
/*        subtract 1 for each successive hour east.                    *
/*        Hour 5 is EST, 6 is CST, 7 is MST, and 8 is PST.             *
/*                                                                     *
/*        IF AN 8 BYTE TIME OF DAY CLOCK VALUE IS PASSED               *
/*        IN THE PARAMETER STRING, IT IS USED AS THE CLOCK VALUE       *
/*        FOR ORDER CODES 0-106, OTHERWISE, THE SYSTEM CLOCK IS READ   *
/*                                                                     *
/***********************************************************************
/*                                                                     *
/*        CLOCK(1001==>C;CLOCK1)                                       *
/*              Produces a new key CLOCK1 with destroy rights          *
/*              restricted                                             *
/*                                                                     *
/*        CLOCK(1002==>C;CLOCK1)                                       *
/*              Produces a new key CLOCK1 with change timezone rights  *
/*              restricted.                                            *
/*                                                                     *
/*        CLOCK(1003==>C;CLOCK1)                                       *
/*              Produces a new key CLOCK1 with both destroy and change *
/*              timezone rights restricted.                            *
/*                                                                     *
/***********************************************************************
/*                                                                     *
/*        CLOCK(1004,((2,houroffset),(2,minuteoffset),(3,code))==>c)   *
/*              Changes the object timezone to the signed offset       *
/*              from UT.  Order code 5 will print <code> in the zone   *
/*              identification field ZZZ (in EBCDIC).                  *
/*              Requires change timezone rights.                       *
/*                                                                     *
/*              e.g. If the desired timezone is 5 hours and            *
/*              30 minutes east of greenwich then use:                 *
/*                CLOCK(1004,X'FFFBFFE2'==>) {...(2,-5),(2,-30)...}    *
/*                                                                     *
/***********************************************************************
/*                                                                     *
/*        CLOCK(kt+4==>) Destroys the clock object.  Requires          *
/*              destroy rights.                                        *
/*                                                                     *
/***********************************************************************
*/

#include "keykos.h"
#include "lli.h"
#include <string.h>
#include <ctype.h>
#include "domain.h"
#include "node.h"
#include "dc.h"
#include "sb.h"
#include <stdio.h>
#include "kernelp.h"

 KEY  COMPONENTS = 0;     /* Factory components node                 */
 KEY  CODE       = 0;     /* New code segment to be installed        */
 KEY  SYMS       = 1;     /* New symbol segment to be installed      */
 KEY  SB         = 1;     /* Non-prompt space bank from the caller   */
 KEY  CALLER     = 2;     /* Resume key to the caller                */
 KEY  DOMKEY     = 3;     /* Domain key to this domain               */
 KEY  PSB        = 4;     /* Prompt space bank passed by the caller  */
 KEY  METER      = 5;     /* The meter passed by the caller          */
 KEY  DC         = 6;     /* The domain creator for this domain      */
 KEY  SK         = 8;     /* Slot for start keys to this domain      */
 KEY  ROPAGE     = 9;     /* The read only page we were created with */
 KEY  RWPAGE     =10;     /* A R/W copy of the read only page        */
 KEY  MEMNODE    =11;     /* The memory node                         */
 KEY  SYSTIME    =12;	
 KEY  CONSOLE    =14;
 KEY  K0         =15;

#define COMPSYSTIME 0
#define COMPCALCLOCK 1
#define COMPKERNELP  2
#define COMPCONSOLE 15

#define No_Destroy         0x80
#define No_ZoneChange      0x40
#define microsPerHour  3600000000U
#define microsPerMin     60000000U

        LLI *ctod1();
extern  abs ();

         char title [] = "CLOCK";

/***********************  Retained Clock Values *********************/

         LLI     timezone_offset;    /* Clock's offset in microsecs */

         char    timezone_code  [4];

/************** Parameters passed to/from the Clock  ****************/

         uint32  returncode, ordercode;

         uint32  paramlen;        /* Actual parameter length recieved */
                                  /*  or length to be returned        */
         sint16  databyte;        /* Start Key data byte              */
         uchar   new_databyte;    /* Data byte for new start key      */

                        /* Multiple views of KeyKos string paramter   */
union    pstr {  char          String[32];   /* character string    */
                struct {  sint16   H_offset;
                          sint16   M_offset;
                          char     Code [3];
                        }      Zone;          /* Time Zone data       */

                long           User_Epoch;    /* Epoch time           */

                struct {  uint32  timeI4;     /* Time as 4 byte int   */
                          long    dateP4;     /*  with Packed Date    */
                         }     L8;            /* 8 byte param string  */

                struct {  LLI      timeI8;    /* Time as 8 byte int   */
                          long     dateP4;    /*  with Packed Date    */
                         }     L12;           /* 12 byte param string */
              } param;

/********************  Values for current request *********************/

       LLI      working_offset,     /* Offset for current request     */
                tempLLI;            /* Work area for LLI calculations */
       sint32   net_offset;         /* Net ofset in minutes           */
       uchar    leapyear;           /* 1 if leapyear, else 0          */

                      /* Epoch Time - since 00:00:00 Jan/1/1900       */
       LLI      epochTOD,           /*   in 1/4096 microseconds       */
                epochMicros;        /*   in microsecs                 */
       uint32   epochDays;          /*   in days                      */

                      /* Intermediate values in time calculations     */
                      /*     Saved to avoid repeat calculations       */
       uint32   modEpoch_days,     /*  days since 1901                */
                quads,             /*  quadrennial blocks since 1901  */
                qDays,             /*  days since leap year           */
                qYears;            /*  years since leap year          */

       uint32   julYY,             /*  Calendar year as integer       */
                julDDD;            /*  Day of year as integer         */

       uint32  JulianDate;         /* Work area for contruction of    */
                                   /*  packed Julian Date 00YYJJJF    */

       uint32  gregMM,             /*  Gregorian Month                */
               gregDD,             /*  Gregorian Day                  */
               gregYY;             /*  Greg Year = (julYY mod 100)    */

       LLI     dayMicros;          /*  Microseconds since midnight    */
       double  dayMicrD;           /*  dayMicros in floating point    */
       uint32  dayCents;           /*  1/100 seconds since midnight   */

       uint16  HH,                 /* Hours                           */
               MM,                 /* Minutes                         */
               CC;                 /* 1/100 Seconds                   */

       uint32  temp32,            /* Unsigned workareas              */
               t2mp32;

       char    timeStr [9];      /* HHMMSSTH Time String            */

       char    JulianStr[7];     /*  Character representation of    */
                                   /*    Julian Date                  */
       char    GregStr[9];       /*    Gregorian Date               */

       char    zoneStr[4];       /*  Time zone string to return     */


       char    WeekDay [7] [10] =
                {   "MONDAY   ", "TUESDAY  ", "WEDNESDAY",
                    "THURSDAY ", "FRIDAY   ", "SATURDAY ", "SUNDAY   "
                 };
       LLI     calsysoffset;
       LLI     restarttod={-1,-1};

       struct Node_DataByteValue ndb7={7};
       struct  KernelPage *KP=(struct KernelPage *)0x80000000;   /* at 8 meg */

       JUMPBUF;

factory(oc,ord)
   uint32 oc;
   uint32 ord;
{
   {OC(COMPCONSOLE);XB(0x00000000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(0);XB(0x00E00000);RC(returncode);NB(0x08400E00);cjcc(0x00000000,&_jumpbuf); }

   if(oc == 42) {  /* freeze */
       int *ptr;

       ptr = (int *)1;
       *ptr = 0;
steppoint:
       oc = 0;
//       oc=dofreeze();
   }
   {OC(SB_CreateNode);XB(0x00100000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_MakeNodeKey);PS2(&(ndb7),sizeof(ndb7));XB(0x04B00000);NB(0x0080B000);cjcc(0x08000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+0);XB(0x80B0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Domain_SwapMemory);XB(0x8030B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

   {OC(COMPKERNELP);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+8);XB(0x80B0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   {OC(COMPSYSTIME);XB(0x00000000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Domain_MakeStart);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }
   timezone_offset.hi  = 0;
   timezone_offset.low = 0;
   memcpy (timezone_code, "UT ", 3 );
   returncode = 0;
   {OC(returncode);XB(0x80208000); }

   for (;;)
   { {RC(ordercode);DB(databyte);RS3(param.String,8,paramlen);NB(0x0F100002); }



     {rj(0x00100000,&_jumpbuf); }

        /* install_new_version ();    /* installs new version of code
        /*                                   while running - DUMMY
        */

     if  (ordercode >= 100 & ordercode <= 106)
       {  working_offset.hi   = 0;
          working_offset.low  = 0;
          strncpy (zoneStr, "UT ", 3);
          ordercode -= 100;                      get_the_time();
        }
     else
     if  (ordercode >= 0 & ordercode <= 6)
       {  working_offset = timezone_offset;
          memcpy (zoneStr, timezone_code, 3);    get_the_time ();
        }
     else
     if  (ordercode == KT)                       return_alleged_key();

     else
     if  (ordercode >= 1001 & ordercode <= 1003) {  get_new_key();
                                                    continue;
                                                  }
     else
     if  (ordercode == 1004)                     set_time_zone ();
     else
     if  (ordercode == KT+4)
          {  if (self_destruct()) break;
           }
     else
     if  (ordercode == 9) get_Offset();
     else {  returncode = KT+2;
             paramlen  = 0;
           }

     {OC(returncode);PS2(param.String,paramlen);XB(0x04200000); }
    }     /* end for (;;) */

   {OC(Node_Fetch+0);XB(0x00B00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }       // put memory back
   {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   {OC(SB_DestroyNode);XB(0x8010B000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

   return 0;
  }       /* end of main  */

dofreeze() {
   {OC(0);PS2("Freezing\n",9);XB(0x04E00000);RC(returncode);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
//   freezedry();

   {OC(COMPCONSOLE);XB(0x00000000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(0);XB(0x00E00000);RC(returncode);NB(0x08400E00);cjcc(0x00000000,&_jumpbuf); }

   {OC(0);PS2("Thawing\n",8);XB(0x04E00000);RC(returncode);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
}

/***********************************************************************
/*                                                                     *
/*  KT    return alleged key type                                      *
/*                                                                     *
/***********************************************************************
*/
return_alleged_key ()
{
   char *ptr;
   char buf[156];

   ptr=(char *)malloc(2048);

   sprintf(buf,"Malloc returned %X\n",ptr);
   {OC(0);PS2(buf,strlen(buf));XB(0x04E00000);RC(returncode);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }

   returncode = 0x12;
   paramlen = 0;
 }


/***********************************************************************
/*                                                                     *
/*  KT+4  self_destruct()                                              *
/*            ===> 1 if OK to Self Destruct                            *
/*            ===> 0 if No Destruct Authority                          *
/*                                                                     *
/***********************************************************************
*/
int self_destruct ( )
{  if (databyte & No_Destroy)       /* permission to self destruct? */
     {  returncode = KT+2;           /* No  - Bad Call */
        paramlen = 0;
        return 0;
      }
   else
     {  return 1;
      }
 }        /* end self_destruct   */

/***********************************************************************
/*                                                                     *
/*  1004  set_time_zone                                                *
/*                                                                     *
/***********************************************************************
*/
set_time_zone ()
{  if ((databyte & No_ZoneChange)      /* Permission to change zone? */
        || (! (paramlen == 7))          /* 7 byte parameter string?   */
        || ( abs (param.Zone.H_offset) > 15)      /* Check range on   */
        || ( abs (param.Zone.M_offset) > 59))     /*    offsets       */
     {  returncode = KT+2;                        /* Bad Call         */
        paramlen = 0;
      }
   else
     {  /*  The following code implements the expression
        /*     timezone_offset =  (param.Zone.H_offset * MicrosPerHour)
        /*                      + (param.Zone.M_offset * MicrosPerMin)
        /*   by decomposing
        /*         MicrosPerMinute = 60000000 into 234375 * (2**8)
        /*   to avoid sign problems inherent in LLI
        */

        net_offset = (param.Zone.H_offset * 60) + param.Zone.M_offset;
        llitimes ((uint32)abs(net_offset), 234375u, &tempLLI);
        llilsl (&tempLLI, 8);
        if (param.Zone.H_offset < 0)
           { timezone_offset.hi = 0;
             timezone_offset.low = 0;
             llisub (&timezone_offset, &tempLLI);
            }
        else timezone_offset = tempLLI;

        memcpy (timezone_code, param.Zone.Code, 3);
        paramlen = 0;
        returncode = 0;

      }
 }              /* end set_time_zone */


/***********************************************************************
/*                                                                     *
/*  1001 - 1003  get_new_key                                           *
/*                                                                     *
/***********************************************************************
*/
get_new_key ()
{  new_databyte = databyte;
   if (ordercode == 1001) new_databyte |= No_Destroy;
   else
   if (ordercode == 1002) new_databyte |= No_ZoneChange;
   else                   new_databyte |= (No_Destroy + No_ZoneChange);
   {OC(Domain_MakeStart);PS2(&new_databyte,1);XB(0x04300000);RC(returncode);NB(0x08808000);cjcc(0x08000000,&_jumpbuf); }


   {OC(returncode);XB(0x80208000); }
 }             /* end of get_new_key   */


/***********************************************************************
/*                                                                     *
/*  000 - 006  get_the_time (local)                                    *
/*  100 - 106  get_the_time (universal)                                *
/*                                                                     *
/*  working_offset has been preset to either                           *
/*              timezone_offset (for local) or 0 (for universal )      *
/***********************************************************************
*/

get_the_time ()
{
  /* First get either system user supplied TOD or system TOD      */

   returncode = 0;
   if (paramlen)                     /* Did user supply the time? */
    { if (paramlen > 8)              /*   Yes, check length       */
        {  returncode = KT+3;        /*       Too long!           */
           paramlen = 0;
           return;
         }
      else
        {  epochTOD.hi  = 0;          /* Zero TOD clock  and left */
           epochTOD.low = 0;          /*   justify user's value   */
           memcpy ( (char *) &epochTOD, param.String, paramlen);
         }
     }                            /* end user supplied clock  */
   else                               /*   No, use system TOD clock */
    { epochTOD = *ctod1();
     }

  /* Convert epoch time to microseconds & days, and adjust for      */
  /*   timezone (if requested).                                     */

   epochMicros = epochTOD;
   llilsr (&epochMicros, 12);

   if (llicmp(&epochMicros, &working_offset) < 0 )
    {  tempLLI = working_offset;        /* If timezone adjusts to    */
       llisub (&tempLLI, &epochMicros); /*   before start of epoch,  */
       epochMicros = tempLLI;           /*   and error flag and      */
       returncode = 1;                  /*   reverse sign on time    */
     }
   else
    {  llisub (&epochMicros, &working_offset);
     }

   llidive(&epochMicros, microsPerMin, &epochDays, &temp32);
   epochDays /= (24*60);     /* Days since epoch; 2 step division   */
                             /* required because of lack of support */
                             /* for LLI/LLI and "signed"            */
                             /* implementation of llidiv.           */

  /*  Convert to calandar year and day within year (Julian )        */

   if (epochDays < 365)
    {   julYY = 70;                                    /* Its 1970  */
        julDDD  = epochDays + 1;
      }
   else
     {  modEpoch_days = epochDays - 365;
        quads = modEpoch_days / ( 4*365 +1 );
        qDays = modEpoch_days - (quads * (4*365 + 1));
        qYears = qDays /365;
        if  (qYears == 4) qYears = 3;   /* special case leap year   */
        julYY  = 1 + (4*quads) + qYears + 70;
        julDDD = qDays - (qYears*365) + 1;
      }

  /* Reformat date into Julian  */

   sprintf (JulianStr,     "%03lu", julYY);
   sprintf (JulianStr+3,   "%03lu", julDDD);

   JulianDate = char2packed (&JulianStr,6);   /* Pack Julian date   */
   JulianDate <<= 4;                          /* Make room for sign */
   JulianDate = JulianDate | 0xF;             /*   insert sign      */

  /*  Calculate Time of Day   */

             /*   The following 3 lines implement the equation
             /*      tempLLI = epochDays * 24 * microsPerHour
             /*   The arithmetic has be decomposed to compensate for
             /*    limitations of the LLI package.  There is a low
             /*    order, one digit, loss of precision.
             */

   temp32 = (epochDays*675);
   llitimes (temp32, 1000000, &tempLLI);
   llilsl (&tempLLI, 7);

   dayMicros = epochMicros;
   llisub (&dayMicros, &tempLLI);
   llidive(&dayMicros, 10000, &dayCents, &temp32);
   CC =  dayCents % 6000;
   HH =  dayCents / 360000;
   MM = (dayCents / 6000) - (HH * 60);
   sprintf (timeStr, "%02u%02u%04u", HH, MM, CC);

   dayMicrD = dayMicros.hi*4294967296. + dayMicros.low;
                                          /* 4294967296 = 0x100000000 */

   switch (ordercode)
        { case 0: {get_Timer_Units ();   break;}
          case 1: {get_Hundredths ();    break;}
          case 2: {get_UDecimal ();      break;}
          case 3: {get_TODTime ();       break;}
          case 4: {get_StdTime ();       break;}
          case 5: {get_Printable ();     break;}
          case 6: {get_PDecimal ();      break;}
         }
 }                        /* end get_the_time  */

get_Offset()
{
   ctod1();
   param.L12.timeI8=calsysoffset;
   paramlen=8;

   returncode=0;
}
/**********************************************************************
/*
/* Get the date and time in the requested format.
/*
/***********************************************************************
/*
/*  order_code = 0 or 100;  Return Formated Time in Timer Units
*/

get_Timer_Units ()
  {  param.L8.timeI4 = dayMicrD / 26.04166;
     param.L8.dateP4 = JulianDate;
     paramlen = 8;
   }

/*  order_code = 1 or 101;  Return Formated Time in Hundredths of sec */

get_Hundredths ()
  {  param.L8.timeI4 = dayCents;
     param.L8.dateP4 = JulianDate;
     paramlen = 8;
   }

/*  order_code = 2 or 102;  Return Formated Time in unsign packed dec */

get_UDecimal ()
  {  param.L8.timeI4 =  char2packed (timeStr, 8);
     param.L8.dateP4 = JulianDate;
     paramlen = 8;
   }

/*  order_code = 3 or 103;  Return Formated Time in TOD Clock Units   */

get_TODTime ()
  {  param.L12.timeI8 = dayMicros;
     llilsl(&param.L12.timeI8, 12);              /* multiple by 4096 */
     param.L12.dateP4 = JulianDate;
     paramlen = 12;
   }

/*  order_code = 4 or 104;  Return Formated Time in Std Epoch Units   */


get_StdTime ()
  {  param.L12.timeI8 = epochTOD;
     param.L12.dateP4 = JulianDate;
     paramlen = 12;
   }

/*  order_code = 5 or 105;  Return Formated Time in Printable Form    */

get_Printable ()
  {  cvtGreg ();
     paramlen = 31;
   }

/*  order_code = 6 or 106;  Return Formated Time signed packed decimal*/

get_PDecimal ()
  {  param.L8.timeI4 = (char2packed (timeStr, 8)) | 0x0F;
     param.L8.dateP4 = JulianDate;
     paramlen = 8;
   }

/**********************************************************************
/*
/* Convert up to 8 characters of a string to a 4 byte packed unsigned
/*   decimal. The low order nibble of each source byte is packed
/*   right to left in successive nibbles of the result.
/***********************************************************************
*/

int  char2packed (source, len)
      char *source;
      int   len;

{  int             packed, count;

   union  {  int   I;
             char  C[4];
           }               work;

  {    packed = 0;
       for (count = 0; count < len;  count++)
         {  work.I = 0;
            work.C [3] = *(source+count);
            work.I &= 0x0F;
            packed = packed << 4;
            packed = packed | work.I;
          }

       return packed;
    }
 }                    /* end char2packed  */

/***********************************************************************
/*
/*        CONVERT JULIAN DATE TO GREGORIAN
/*
/* The following algorithm to convert a Julian Date (YYDDD) to a
/* Gregorian Date (MMDDYY) was adopted from an algorithm entitled
/* tableless date coversion appearing in "COMMUNICATIONS OF THE ACM".
/* VOLUME 13, NUMBER 10,  OCTOBER 1970, P. 621, BY RICHARD A STONE,
/* WESTERN ELECTRIC COMPANY, P.O. BOX 900, PRINCETON, NJ 08540
/*
/* This "C" version is adapted from an IBM 370 Assembler Version
/* originally implemented as part of the CLOCK.
/***********************************************************************
*/
cvtGreg ()                                    /* Is it leap year ?  */
  {  if ((!(julYY % 4)) && (julYY))       /*   1900 was not     */
          leapyear = 1;                       /* Yes, leapyear      */
     else leapyear = 0;                       /* No, not leapyear   */

     if (julDDD > (59 + leapyear))            /* Is it Mar1 or later? */
       gregDD =  julDDD + 2 - leapyear;
     else
       gregDD =  julDDD;

     gregDD += 91;                            /* Intermediate value */

     gregMM =  (gregDD * 100)/ 3055;
     gregDD -= (gregMM * 3055)/100;

     gregMM -= 2;

     gregYY = (julYY%100);

     sprintf (param.String, "%02d/%02d/%02d%02d:%02d:%02d:%02d",
                      gregMM, gregDD, gregYY,
                      HH, MM, (CC/100), (CC%100));
     memcpy (param.String + 19, zoneStr, 3);
     memcpy (param.String + 22, WeekDay [((epochDays+3) % 7)], 9);

   }       /*  end cvtGreg  */
LLI *ctod1()  /* returns offset TOD (S370 time) */
{
   static LLI tod;
   char caltime[18];
   int rc;

   if(llicmp(&restarttod,&(KP->KP_RestartTOD))) {
      {OC(COMPCALCLOCK);XB(0x00000000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
      {OC(8);XB(0x00F00000);RS2(caltime,8);NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
      rc=cal2tod(caltime,&calsysoffset);
//      if(rc) {OC(1024+rc);PS2(caltime,8);XB(0x04000000);NB(0x00000000);cjcc(0x08000000,&_jumpbuf); }
      {OC(7);XB(0x00C00000);RS2(&(tod),sizeof(tod));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
      llisub(&calsysoffset,&tod);
      restarttod=KP->KP_RestartTOD;
   }
   {OC(7);XB(0x00C00000);RS2(&(tod),sizeof(tod));NB(0x03000000);cjcc(0x00080000,&_jumpbuf); }
   lliadd(&tod,&calsysoffset);
   return &tod;
}
/*   SUBROUTINE   cal2tod C
/*   TITLE        CONVERT OMRON CALCLOCK TO EPOCH TOD UNITS
/*
/*   Created                                             9/26/90
/*
/*
/*   This subroutine converts the OMROM calander clock into epoch
/*      timer units.
/*
/*   uint16   cal2tod (OmronCalClock,EpochTod)
/*          unsigned char    *OmronCalClock;
/*          LLI              *EpochTOD;
/*
/*      ====> 0  if OmronCalClock contains a valid Epoch time
/*      ====> 1  if OmronCalClock < 19000101xx000000
/*                  EpochTod set to 0x0000000000000000
/*      ====> 2  if OmronCalClock > 20420917xx235337
/*                  EpochTod set to 0xFFFFFFFFFFFFFFFF
/*
/*       OmronClock (0xYYYYMMDDWWHHMMSS)
/*
/*            0xYYYY =  Year
/*            0xMM   =  Month Number
/*            0xDD   =  Day of Month
/*            0xWW   =  Day of Week
/*            0xHH   =  Hour of Day (GMT assumed)
/*            0xMM   =  Minutes
/*            0xSS   =  Seconds
/*
/*       EpochTimer (0xEEEEEEEEEEEEEEEE)
/*
/*               =  Time since 1/1/1900 in units of 1/4096 microsecs
/*
/**********************************************************************
/*   Note: cal2tod assumes that it has been passed a valid date.  It
/*         does NOT check for month >12, hour > 59, etc.  The only
/*         validity check performed it to insure that the date is
/*         within the range covered by the epoch timer.
*/

/* Table giving the number of days preceeding the start of the    */
/*    current month (leap year will be adjusted separately )      */

uint16  daysTo [12] = {      0,                31,        28+31,
                           28+2*31,        28+30+2*31,   28+30+3*31,
                           28+2*30+3*31, 28+2*30+4*31, 28+2*30+5*31,
                           28+3*30+5*31, 28+3*30+6*31, 28+4*30+6*31  };

/* Prototypes for internal routines */

unsigned int unNibble (unsigned char *bite);

    cal2tod (cal, tod)
    unsigned char    *cal;
    LLI              *tod;

{  uint32  year, day, month, hour, mins, secs ;
   uint32  totalDays;   /* Full days since start of Epoch */
   LLI     sinceMid;    /* Seconds since Midnight         */

/* Convert nibblized date & time to integers    */

   year  = 100 * unNibble (cal) + unNibble (cal+1);
   month = unNibble (cal+2);
   day   = unNibble (cal+3);
   hour  = unNibble (cal+5);
   mins  = unNibble (cal+6);
   secs  = unNibble (cal+7);

/* Make sure calendar clock value is within range of Epoch Timer    */

   if (year >= 1970 && year < 2112)       /* Its definitely in epoch */
     {   }
   else
   if ( year < 1970 )                        /* Before start of epoch */
     {  tod->hi  = 0;
        tod->low = 0;
        return 1;
      }
   else                                     /* End of Epoch is       */
   if (    year > 2112                      /*    9/17/2042 23:53:47 */
        || month > 9
        || (month == 9 && day  > 17)
        || (month == 9 && day == 17 && hour == 23 && mins > 53)
        || (month == 9 && day == 17 && hour == 23 && mins == 53
               && secs > 47))
     {  tod->hi  = 0xffffffff;
        tod->low = 0xffffffff;
        return 2;
      }

/*  Compute & scale seconds since midnight */
/*     Note: Epoch Units = seconds * 1000000 * 4096
/*                       = seconds * (15635 * 2**6) * (2**12)
/*       At this time, the LLI package does not support
/*           LLI = LLI *Uint32, so seconds are first scaled by 15625
/*       and then "multipled" by 2**18  with llisl.
*/

   sinceMid.hi = 0;
   sinceMid.low = secs;
   sinceMid.low += (mins * 60);
   sinceMid.low += (hour * 3600);
   sinceMid.low *= 15625;


/* Calculate number of days to start of year, convert to seconds     */

   year -= 1970;

   totalDays = year*365;
   if (year)
    { totalDays +=  (year - 1)/4;      /* adjust for past leap years */
     }

   totalDays += ( day - 1 + daysTo[month-1] );
              /* Add full days this month and days to start of month */

   if ( (year && !(year % 4 ))           /* If its leap year       */
        && month > 2  )                  /*  and after February    */
    {  totalDays += 1;                   /*    add yet another day */
     }

   llitimes (totalDays, (24 * 60 * 60 * 15625), tod);
                                /* Convert day to seconds and scale   */
   lliadd (tod, &sinceMid);    /*  Add scaled seconds since midnight */

   llilsl (tod, 18);              /* Complete multiplication */
   return 0;
 }

unsigned int unNibble (bite)
      unsigned char    *bite;
{  uint32 val;
   val = *bite;
 return ( 10*(val >> 4) + ( 0x0f & val));
 }

/* quo (which seems to stand for quotient) is really the dividend */
/* dive (which seems to stand for dividend) is really the quotient */
/* div seems to be the divisor */
/* rem seems to be the remainder */
llidive(quo,div,dive,rem)
   LLI *quo;
   unsigned long div,*dive,*rem;
{
   union {
       double xy;
       LLI    lxy;
   } num;
   int exp;
   double divisor;
   double dividend;

   num.lxy=*quo;
   exp=1023+52;

   if(!num.lxy.hi && !num.lxy.low) { /* dividing into 0 */
      *dive=0;
      *rem=0;
      return;
   }
   while(!(num.lxy.hi & 0x00100000)) {
       llilsl(&num.lxy,1);
       exp--;
   }
   num.lxy.hi=(num.lxy.hi & 0x000FFFFF) | exp << 20;

   divisor=div;
   dividend=num.xy / divisor;

   num.xy=dividend;

   exp=num.lxy.hi >> 20;
   num.lxy.hi=(num.lxy.hi & 0xFFFFF)|  0x00100000;
   while(exp != 1023+52) {
      llilsr(&num.lxy,1);
      exp++;
   }
   *dive=num.lxy.low;
}
