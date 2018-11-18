/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*********************************************************************
  clock.h
**********************************************************************

*                                                                     *
*        This routine reads the Time of Day clock and returns the     *
*              time and data in some usable format.                   *
*                                                                     *
*        CLOCK(OC,(8,CLOCK)==>C,(VALUE STRING))                       *
*              THE FOLLOWING ORDER CODES ARE RECOGNIZED:              *
*        0     (4,TOD IN UNITS OF 1/38400 SECOND),(4,DATE)            *
*        1     (4,TOD IN UNITS OF 1/100 SECOND),4(DATE)               *
*        2     (4,TOD IN FORM PACKED DECIMAL HH MM SS TH),(4,DATE)    *
*        3     (8,BINARY VALUE OF TIME OF DAY CLOCK SINCE MIDNIGHT)   *
*        4     (8,BINARY VALUE OF TIME OF DAY CLOCK (EPOCH 1900 ETC)  *
*        5     (31,TIME, DATE, AND DAY OF WEEK IN EBCDIC AS FOLLOWS:  *
*               'MM:DD:YYHH:MM:SS.HHZZZWEEKDAY' (WEEKDAY IS 9 CHARS)  *
*          MONTH,DATE,YEAR,HOURS,MINUTES,SECONDS,HUNDREDTHS,TIMEZONE  *
*        6     (4,TOD IN FORM PACKED DECIMAL HH MM SS TF),(4,DATE)    *
*                                                                     *
*        9     (8,systime-caltimeoffset)                              *
*                                                                     *
*        0-6 Return time and data information in the current time zone*
*        100-106 ARE IDENTICAL TO 0-6, EXCEPT TIME ZONE IS UT         *
*                                                                     *
*        FOR CODES 0,1,2 DATE IS IN PACKED DECIMAL IN THE FORM        *
*         '00YYJJJF' WHERE JJJ IS THE JULIAN DATE AND F IS X'0F'      *
*                                                                     *
*        The high two bits of the data byte of the start key          *
*        are used to restrict destroy and change timezone rights      *
*        as follows:                                                  *
*           No_Destroy    = x'80'  destroy rights are restricted      *
*           No-ZoneChange = x'40'  Change timezone rights restricted  *
*                                                                     *
*        Note: Zone 0 is UT, add 1 for each successive hour west.     *
*        subtract 1 for each successive hour east.                    *
*        Hour 5 is EST, 6 is CST, 7 is MST, and 8 is PST.             *
*                                                                     *
*        IF AN 8 BYTE TIME OF DAY CLOCK VALUE IS PASSED               *
*        IN THE PARAMETER STRING, IT IS USED AS THE CLOCK VALUE       *
*        FOR ORDER CODES 0-106, OTHERWISE, THE SYSTEM CLOCK IS READ   *
*                                                                     *
***********************************************************************

***********************************************************************
*                                                                     *
*        CLOCK(1001==>C;CLOCK1)                                       *
*              Produces a new key CLOCK1 with destroy rights          *
*              restricted                                             *
*                                                                     *
*        CLOCK(1002==>C;CLOCK1)                                       *
*              Produces a new key CLOCK1 with change timezone rights  *
*              restricted.                                            *
*                                                                     *
*        CLOCK(1003==>C;CLOCK1)                                       *
*              Produces a new key CLOCK1 with both destroy and change *
*              timezone rights restricted.                            *
*                                                                     *
***********************************************************************
*                                                                     *
*        CLOCK(1004,((2,houroffset),(2,minuteoffset),(3,code))==>c)   *
*              Changes the object timezone to the signed offset       *
*              from UT.  Order code 5 will print <code> in the zone   *
*              identification field ZZZ (in EBCDIC).                  *
*              Requires change timezone rights.                       *
*                                                                     *
*              e.g. If the desired timezone is 5 hours and            *
*              30 minutes east of greenwich then use:                 *
*                CLOCK(1004,X'FFFBFFE2'==>) {...(2,-5),(2,-30)...}    *
*                                                                     *
***********************************************************************
*                                                                     *
*        CLOCK(kt+4==>) Destroys the clock object.  Requires          *
*              destroy rights.                                        *
*                                                                     *
***********************************************************************/

 

#ifndef _H_clock
#define _H_clock

#define Clock_AKT               0x0012
#define ClockF_AKT              0x0112


#define Clock_TOD_38000         0
#define Clock_TOD_100           1
#define Clock_TOD_PDHMSTH       2
#define Clock_TOD_BINMIDNITE    3
#define Clock_TOD_BINEPOC       4
#define Clock_TOD_ASCII         5
#define Clock_TOD_PDHMSTF       6
#define Clock_Get_TOD_Offset    9

#define Clock_TOD_UT          100

#define Clock_NODESTROY      1001
#define Clock_NOTIMEZONE     1002
#define Clock_NORIGHTS       1003
#define Clock_SetTimeZone    1004

#endif
