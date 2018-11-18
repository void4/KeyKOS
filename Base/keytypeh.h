/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

typedef enum {
 datakey =      0,
     /* Never prepared, has databody(6|11) */
 pagekey =      1,
     /* Databyte has R/O bit, designates page */
 segmentkey =   2,
     /* Databyte (see manual), designates node */
 nodekey =      3,
     /* Databyte same as segment, designates node */
 meterkey =     4,
     /* Databyte zero, designates node */
 fetchkey =     5,
     /* Databyte same as segment, designates node */
 startkey =     6,
     /* Databyte anything, designates node */
 resumekey =    7,
     /* Databyte: 0=restart, 2=return, 4=fault, designates node */
 domainkey =    8,
     /* Databyte zero, designates node */
 hookkey =      9,
     /* Always prepared + involvedr + involvedw */
 misckey =     10,
     /* Never prepared, databody11(:0:) is subtype (see below) */
 nrangekey =   11,
     /* Never prepared, uses rangekeycda and rangekeysize */
 prangekey =   12,
     /* Same as nrangekey */
 chargesetkey =14,
     /* Never prepared, databyte = 0, uninvolved: databody is csid */
     /* involved: subject is offset to csrep, leftchain is a link */
 sensekey =    15,
     /* Databyte same as segment, designates node */
 devicekey =   16,
     /* ... to be defined ... */
 copykey =     17,
     /* Never prepared, databyte = 0, databody11 is zero */
 frontendkey =  18,
     /* like a segment key but discrims as startkey */
 
 
/* misckey subtypes */
 returnermisckey =         0,
 domtoolmisckey =          2,
 keybitsmisckey =          4,
 datamisckey =             6,
 discrimmisckey =          8,
  /* 5 is unused */
 bwaitmisckey =            12,
  /* unused                      14 */
  /* unused                      16 */
 takeckptmisckey =         18,
 resynctoolmisckey =       20,
 errormisckey =            22,
 peekmisckey =             24,
 chargesettoolmisckey =    26,
 journalizekeymisckey =    28,
  /* 15 is unused */
 deviceallocationmisckey = 32,
 geterrorlogmisckey =      34,
 iplmisckey =              36,
 measuremisckey =          38,
 cdapeekmisckey =          40,
 migrate2misckey =         42,
 kiwaitmisckey =           44,
 kdiagmisckey =            46,
 kerrorlogmisckey =        48,
 systimermisckey =         50,
 calclockmisckey =         52,
 dat2instmisckey =         54,
 copymisckey =             56,
 Fpeekmisckey =            58,
 Fpokemisckey =            60,
 lastmisckey =             60,
 } keytype_t;

/* for resume key */
#define  restartresume 0
#define reskck2 1
#define returnresume 2
#define reskck3 3
#define faultresume 4
/* Bits in the databyte of page and segmode keys: */
#define readonly 0x80
#define nocall 0x40

     /* For hook key - Databyte is type as follows: */
#define worry_hook 0
#define process_hook 1
#define pihk (hookkey+prepared+involvedr+involvedw)
