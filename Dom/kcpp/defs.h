/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
#define TRUE   1
#define FALSE  0
 
/* Tokens are an int with a type or value in the low byte and a   *
 * class as the next higher byte (class 0 being nothing special)  */
 
#define SPACE  ' '
#define IDENT  'I'
#define TSTRING '"'
#define CHARCONST '\''
#define OP     '('
#define CP     ')'
#define CM     ','
#define SC     ';'
#define EQ     '='
#define OB     '{'
#define CB     '}'
#define TEOF    0x3
#define JUNK   'J'
 
#define CLASS    0xff00 /* mask to determine token class */
#define VAL      0x00ff /* mask to determine token value */
 
/* Return types for nexttok() */
#define ALL     00
#define NOSPACE 01
#define KEYWORD 02
#define RESET   03
 
/* Class definitions */
#define NOTFOUND 0x0000     /* must be zero */
#define CALL     0x0100
#define DECL     0x0200
#define PARM     0x0300
#define TKEY     0x0400
#define SPARM    0x0500
 
/* CALLs */
#define CJUMPK  0x0000  /* KC */
#define CLOADX  0x0001  /* LDEXBL */
#define CLOADN  0x0002  /* LDENBL */
#define CJUMPR  0x0003  /* RETJMP */
#define CJUMPF  0x0004  /* FORKJMP */
#define TKENTRY 0x0005  /* KENTRY */
#define KRTURN  0x0006  /* KRTURN */
#define KFORKC  0x0007  /* KFORK */
#define CJUMPC  0x0008  /* CALLJUMP */
 
/* PARMs */
#define NONE  00
#define PK  0x01 /* KEYSFROM */
#define RK  0x02 /* KEYSTO   */
#define RC  0x10 /* RCTO, OCTO */
#define DB  0x40 /* DBTO     */
#define ID  0x80 /* KENTRYID */
 
/* SPARMs */
#define PSTR  0x01 /* STRINGFROM */
#define RSTR  0x02 /* STRINGTO   */
#define PCHR  0x04 /* CHARFROM */
#define RCHR  0x08 /* CHARTO   */
#define PSCT  0x10 /* STRUCTFROM */
#define RSCT  0x20 /* STRUCTTO */
 
/* DECLs */
#define DKEY   00 /* KEY */
#define _FILE  01 /* #include */
#define PLISTR 02 /* STRING */
 
/* Key Stuff */
#define NOKEY  0xf0
#define VARKEY 0xff
 
#define VarKey   0x02
#define Char2    0x08
#define Char3    0x10
#define String1  0x20

