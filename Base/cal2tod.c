/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*   SUBROUTINE   cal2tod C
     TITLE        CONVERT OMRON CALCLOCK TO EPOCH TOD UNITS
  
     Created                                             9/26/90
  
     This subroutine converts the OMROM calander clock into epoch
        timer units.
  
     uint16   cal2tod (OmronCalClock,EpochTod)
            unsigned char    *OmronCalClock;
            unint64          *EpochTOD;
  
        ====> 0  if OmronCalClock contains a valid Epoch time
        ====> 1  if OmronCalClock < 19000101xx000000
                    EpochTod set to 0x0000000000000000
        ====> 2  if OmronCalClock > 20420917xx235337
                    EpochTod set to 0xFFFFFFFFFFFFFFFF
  
         OmronClock (0xYYYYMMDDWWHHMMSS)
  
              0xYYYY =  Year
              0xMM   =  Month Number
              0xDD   =  Day of Month
              0xWW   =  Day of Week
              0xHH   =  Hour of Day (GMT assumed)
              0xMM   =  Minutes
              0xSS   =  Seconds
  
         EpochTimer (0xEEEEEEEEEEEEEEEE)
  
                 =  Time since 1/1/1900 in units of 1/4096 microsecs
  
  *********************************************************************
     Note: cal2tod assumes that it has been passed a valid date.  It
           does NOT check for month >12, hour > 59, etc.  The only
           validity check performed it to insure that the date is
           within the range covered by the epoch timer.
*/
 
#include "kktypes.h"
#include "cal2tod.h"
 
char title [] = "cal2tod";
 
/* Table giving the number of days preceeding the start of the    */
/*    current month (leap year will be adjusted separately )      */
 
uint16  daysTo [12] = {      0,                31,        28+31,
                           28+2*31,        28+30+2*31,   28+30+3*31,
                           28+2*30+3*31, 28+2*30+4*31, 28+2*30+5*31,
                           28+3*30+5*31, 28+3*30+6*31, 28+4*30+6*31  };

/* Prototypes for internal routines */

uint16 unNibble (unsigned char *bite);
 
uint16   cal2tod (unsigned char *cal, uint64 *tod)
{  uint32  year, day, month, hour, mins, secs ;
   uint32  totalDays;   /* Full days since start of Epoch */
   uint64  sinceMid;    /* Seconds since Midnight         */
 
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
     { *tod=0;
        return 1;
      }
   else                                     /* End of Epoch is       */
   if (    year > 2112                      /*    9/17/2042 23:53:47 */
        || month > 9
        || (month == 9 && day  > 17)
        || (month == 9 && day == 17 && hour == 23 && mins > 53)
        || (month == 9 && day == 17 && hour == 23 && mins == 53
               && secs > 47))
     {  *tod = -1LL;
        return 2;
      }
 
/*  Compute & scale seconds since midnight */
/*     Note: Epoch Units = seconds * 1000000 * 4096
                         = seconds * (15635 * 2**6) * (2**12)
         At this time, the LLI package does not support
             LLI = LLI *Uint32, so seconds are first scaled by 15625
         and then "multipled" by 2**18  with llisl.
*/
 
   sinceMid = secs;
   sinceMid += (mins * 60) + (hour * 3600);
   sinceMid *= 15625;
 
 
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
 
   // llitimes (totalDays, (24 * 60 * 60 * 15625), tod);
   *tod = totalDays * (24 * 60 * 60 * 15625);
                                /* Convert day to seconds and scale   */
   // lliadd (tod, &sinceMid);
   *tod += sinceMid;     /*  Add scaled seconds since midnight */
   // llilsl (tod, 18);
   *tod <<= 18;           /* Complete multiplication */
   return 0;
 }
 
uint16 unNibble (bite)
      unsigned char    *bite;
{  uint32 val;
   val = *bite;
 return ( 10*(val >> 4) + ( 0x0f & val));
 }
