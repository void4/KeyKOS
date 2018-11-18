/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "eval.h"
#include "kcpp.h"
 
extern int noecho;

extern int assembler;
 
/* enbl and exbl hold the entry and exit masks that we compute */
static unsigned long enbl, exbl;
 
extern char *xbtext, *nbtext, *krtext;
 
/* xtype and ntype flag whether we need to generate a runtime mask *
 * or have precalculated a constant mask.                          */
int xtype;     /* description of an exit block */
int ntype;     /* description of an entry block */
int is_kc;     /* is it kc or any other calls */
extern int kjump;
static unsigned long xntype;
/* Blank gives us a pointer to see if an argument was set -- if it *
 *   wasn't set, it points at Blank and we don't call free()       */
/* ik = invoked key,                                               *
 * rc = return code,  oc = order code, db = data byte,             *
 * id = identifier, {rp}{sl} = {returned, passed} {string, length} */
 
char Blank[] = "-1";
char *ik = Blank, *al = Blank;
char *rc = Blank, *oc = Blank, *db = Blank, *id = Blank;
char *ps = Blank, *pl = Blank, *rs = Blank, *rl = Blank;
 
/* NoKey is a variable in gnosis.h (so we can take its address).     *
 * Blankkeys, which is composed of NoKeys, serves the same purpose as*
 *   Blank does for regular text                                     *
 * pk and rk store the pointers to arrays of key text pointers, with *
 * the keys in the arrays being in REVERSE order                     */
 
char NoKey[] = "_0";
char *Blankkeys[] = { NoKey, NoKey, NoKey, NoKey };
char **pk = Blankkeys, **rk = Blankkeys;
 
/* the g*() functions (mapped by the g*() macros) provide an          *
 * interface to generate the masks and macros that can be used no     *
 * matter what sort of syntax is being processed -- only change call.c*/
genik(val, name)
short val;
char *name;
{
    ik = name;
    if(val == VARKEY)  xtype |= VarKey;
    else   exbl |= val << 0x14;
}
 
genk(type,name,val)
int type;
char *name[];
short val[];
{
    int i;
    int  off, *flag;
    unsigned long *block;
 
    if(type == PK){block = &exbl; off = 034; flag = &xtype; pk = name;}
    else          {block = &enbl; off = 024; flag = &ntype; rk = name;}
 
    for(i=0;i<4;i++) {
        if(val[i] == VARKEY) *flag |= VarKey;
        else *block |= (val[i] & 0x0f) << (i << 2);
        if(val[i] != NOKEY)
            *block |= 01 << (off + i);
    }
}
 
/*
   put curls around new text
*/
curls() { print1("{"); }
 
/* the gen*() functions provide a universal keycall back-end for     *
 * kcpp.  As with the g*() functions, only call.c needs be changed   *
 * to support a different syntax.  These generate macros interpreted *
 * by gnosis.h and keep track of internal variables                  */
 
genxb()
{
    int i;
    char xt[4], *ptr;;
    if(xtype & VarKey) exbl &= 0xff000000;
    if(ik != Blank) {
       if ((xtype & VarKey) || kjump) {
            print1("IK("); print(ik,");");
        }
        free(ik); ik = Blank;
    }
    if(oc != Blank) {
	if (*oc == '%')	/* order code is in a register */
		print1("OCR(");
	else
		print1("OC("); 
	print(oc,");");
        free(oc); oc = Blank;
    }
    if(ps != Blank) {
        if (pl != Blank) {
           print1("PS2("); print(ps,","); print(pl,");"); free(pl);
        }
        else { print1("PS1("); print(ps,");"); }
        free(ps);
        ps = pl = Blank;
    }
    if ((xtype & VarKey) || kjump /* || (is_kc == 0) */) {
       print1("KP(");
       for(i=0;i<4;i++) { print(pk[i],(i==3 ? ");" : ",")); }
    }
    print1("XB("); print(hex(exbl),");");
    exbl = 0x00000000;
    pk = Blankkeys;
}
 
gennb()
{
    int i;
 
    if(ntype & VarKey) enbl &= 0xffff0000;
    if(rc != Blank) {
        if (kjump) print1("RCK(");
        else       print1("RC(");
        print(rc,");");
        free(rc); rc = Blank;
    }
    if(db != Blank) {
        print1("DB("); print(db,");");
        free(db); db = Blank;
    }
    if(rs != Blank)                           {
        if(*al == '\0') al = Blank;
        if (ntype & String1)      {
           print1("RS1("); print(rs,");");
        }
        else if (ntype & Char2)   {
           print1("RS2("); print(rs,","); print(rl,");");
        }
        else if (ntype & Char3)   {
           if (kjump) print1("RSK(");
           else       print1("RS3(");
           print(rs,","); print(rl,","); print(al,");");
        }
        free(rs);
        if(rl != Blank) free(rl);
        if(al != Blank) free(al);
        rs = rl = al = Blank;
    }
    if ((ntype & VarKey) /* || (is_kc == 0) */ || kjump) {
       print(krtext,"(");
       for(i=0;i<4;i++) { print(rk[i],(i==3 ? ");" : ",")); }
    }
    print1("NB("); print(hex(enbl),");");
    enbl = 0x00000000;
    rk = Blankkeys;
}
 
genkent()
{
    if(id != Blank) {
        print("if (0==1) { _gn",id); print1(": ");
        free(id); id = Blank;
    } else {
        print1("if (0==1) { _gn: ");
    }
}
 
genkret()
{
    if(id  != Blank) {
        print("goto _gn",id); print1("; ");
        free(id); id = Blank;
    } else  print1("goto _gn; ");
}
 
genxntyp()
{
    xntype = ((long)xtype <<24);
    xntype |= ((long)ntype <<16);
}
 
genrj()
{
    int i;
    genxntyp();
    if (kjump) {
       print("KRJ(",hex(xntype));
       print(",",xbtext); print1(", _kn);");
    }
    else  {
       print("rj(", hex(xntype));
       print(",&",xbtext); print1(");");
    }
}
 
genfj()
{
    int i;
    genxntyp();
    if (kjump) print1("KFJ(");
    else       print1("fj(");
    print1(hex(xntype));
    print(",&",xbtext); print1(");");
}
 
/* calljump */
gencjmp()
{
    genxntyp();
    if(exbl & 0x10000000) error("Only 3 passed keys allowed in kall");
    if (kjump)   {
       /* key cache */
       print("KCJ(",hex(xntype));
       print(",",xbtext); print1(", _kn);");
    }
    else if ((xtype & VarKey) || (ntype & VarKey))  {
       /* variable key slots */
       print( "cj(",hex(xntype));
       print(",&",xbtext); print1(");");
    }
    else  {
       /* const keys, different cases */
       if ( (!(xtype & String1)) && (!(ntype & String1)) )
          /* const keys, STRINGTO, STRINGFROM not allowed */
          print("cjcc(",hex(xntype));
       else if ( (!(xtype & Char2)) &&
                 (!(ntype & Char2)) && (!(ntype & Char3)) )
          /* const keys, CHARTO, CHARFROM not allowed */
          print("CJSS(",hex(xntype));
       else
          /* const key, mixed form, like CHARFROM, STRINGTO */
          print("CJ0(",hex(xntype));
       print(",&",xbtext); print1(");");
    }
} /* end of gencjmp */
 
 
/* installkey() handles declarations such as:
 *      KEY reskey = 2, domkey = 3;    -- declares two key names
 *      KEY startkey;                  -- declares a key variable
 */
installkey()
{
    char *name, c;
    int  val;
 
    nexttok(NOSPACE);
    c = *toktext();
/* KLUDGE for Prototype definintions */
    if (! isalnum(c) && c != '_') return; /* not an identifier -
             could be a pointer declarator, open paren,
             and/or a parameter type in a function prototype */
    for(;;) {

/*  KLUDGE!!!!! to handle prototype function declarations the next IF skips
    a variable of the name KEY.  Thus  KEY a, KEY b in a function declaration
    will be accepted.  However KEY KEY x will also be accepted anywhere else
    the compiler will catch the later.  !!!!!!  */

        if((toktype() & CLASS) == DECL
           && (toktype() & VAL) == DKEY) nexttok(NOSPACE);
        name=save(toktext());
        if(nexttok(NOSPACE) == EQ) { /* check for initialization */
           val = getval();
           nexttok(NOSPACE);
        }
        else val = VARKEY;
        enter(name, TKEY | val);
        switch(toktype()) {
          case SC: return;        /* end declaration */
          case CM: break;        /* continue list */
          /* this should handle array declarations too */
          case OP:                /* assume function declaration */
            do {
              nexttok(NOSPACE);
            } while (*toktext() != CP);
            return;
          default:
            return;	
 /*          error("Comma or Semicolon expected in KEY declaration"); */
        }
        nexttok(NOSPACE);
    }
}
 
 
/* dclstring() handles declarations such as:
 *      STRING name(4)                 -- actually allocates storage
 *      STRING str;                    -- declares a parameter
 *      STRING f();                    -- extern function declaration
 */
dclstring()
{
    char *name;
    char *len;
    char *init;
    int   chrptr, i, cnt;
    chrptr = 0;
 
    for(;;) {
       noecho++;             /* disable echo */
       name = getid();
       nexttok(NOSPACE);
 
       if (toktype() == EQ) {
          if (name[0] == '*')
             error("Illegal initialization for STRING pointer");
          else error("Size is expected in STRING declaration");
       }
       else if (toktype() == OP)  {
          len = getxpr();
          if(toktype() != CP) {    /* check syntax */
              if(nexttok(NOSPACE) != CP) error("Missing ')'");
          } else{nexttok(NOSPACE);}
          if (*len == '\0') { /* no length specified */
             if (chrptr == 0) { print("char *",name); chrptr = 1; }
             else print("*",name);
             print1("()");
          }
          else {
             if ((name[0] == '*') && (toktype() == EQ))
                error("Illegal initialization for STRING pointer");
             if (chrptr == 1) error("Error in STRING declaration");
             if (chrptr != 2) print1("static");
             print(" struct { int m,a; char s[",len);
             print("]; } _",name);
             print("= {",len);
             if(toktype() == EQ) { /* check for initialization */
                nexttok(NOSPACE); init = toktext();
                for (cnt = 0, i = 0; i < (int)strlen(init); i++)
                   if (init[i] != '\\') cnt++;
                print(",",int2decimal(cnt-2));
                print(",",init);
                nexttok(NOSPACE);
             } else print1(",0");
             print1("}; char *");
             print(name," = (char *)&_");
             print1(name);
             chrptr = 2;
          }
          free(len);
       }
       else  {
          if (chrptr == 0) {  print("char *",name); chrptr = 1; }
          else if (chrptr == 1) print("*",name);
          else error("Error in STRING declaration");
       }
       free(name);
       noecho--;             /* possibly restore echo */
       switch(toktype()) {
       case SC: return;                      /* end declaration */
       case CM: print1(",");break;          /* continue list */
       default:
           error("Comma or Semicolon expected in STRING declaration");
       }
    }
}
 
static void doko()
{
    short t;
    char *tt;

    xtype = 0;
    op();
    t=getkey();
    tt=save(toktext());
 /*   genik(getkey(),save(toktext()));  */
    genik(t,tt);
    cm(); oc = getxpr();
    cp();
}
 
static void doid()
{ op(); id = getxpr(); cp(); }
 
char *pname[4];
char *rname[4];
short val[4];
/* parameters() is called by docall() and parses a list of parameters
 *              to a keycall, disallowing illegal and multiply used
 *              parameters.
 */
static void parameters(legal)
short legal;
{
    int type;
    char blank[1];
    char *p1,*p2,*p3;
    char *mybuf;
 
    blank[0] = '\0';
    while(nexttok(NOSPACE) != SC) {      /* ';' terminates list */
        type = toktype();
        if( type == TEOF )
            error("EOF in the middle of a key invocation");
        if((type & CLASS) == PARM ) {
           type &= VAL;
           legal &= ~type;
           switch(type) {
           case RC:
              op(); rc = getxpr(); enbl |= 0x08000000;
              cp(); break;
           case DB:
              op(); db = getxpr(); enbl |= 0x04000000;
              cp(); break;
           case ID:
              doid(); break;
           case PK:
              op(); dokeys(pname,val); cp(); genk(PK,pname,val); break;
           case RK:
              op(); dokeys(rname,val); cp(); genk(RK,rname,val); break;
           default:
               error("Illegal keyword or multiple definition");
           }
        }
        else if ((type & CLASS) == SPARM) {
           type &= VAL;
           legal &= ~type;
           switch(type) {
 
           case PSTR:      /* STRINGFROM */
              op();ps=getxpr();
              pl = Blank; exbl |= 0x04000000;
              xtype |= String1;
              cp(); break;
 
           case RSTR:      /* STRINGTO */
              op();rs=getxpr();
              rl = Blank; al = Blank; enbl |= 0x03000000;
              ntype |= String1;
              cp(); break;
 
           case PCHR:      /* CHARFROM */
              op();ps=getxpr();
              if(toktype() == CM) {cm(); pl=getxpr();}
                else pl=blank;
              exbl |= 0x04000000;
              if (*pl == '\0') error("Bad syntax in CHARFROM");
              else xtype |= Char2;
              cp(); break;
 
           case RCHR:       /* CHARTO */
              op();rs=getxpr();
              if(toktype() == CM) {cm(); rl=getxpr();}
                else rl=blank;
              if(toktype() == CM) {cm(); al=getxpr();}
                else al=blank;
              if (*rl == '\0') error("Bad syntax in CHARTO");
              if (*al == '\0') {al = Blank; ntype |= Char2;}
              else ntype |= Char3;
              enbl |= 0x03000000;
              cp(); break;
 
           case PSCT:       /* STRUCTFROM */
              op();ps=getxpr();
              if(toktype() == CM) {cm(); pl=getxpr();}
              else {
                  mybuf=(char *)malloc(strlen(ps)+32);
                  strcpy(mybuf,"sizeof(");
                  strcat(mybuf,ps);
                  strcat(mybuf,")");
                  pl=mybuf;
              }
              mybuf=(char *)malloc(strlen(ps)+32);
              strcpy(mybuf,"&(");
              strcat(mybuf,ps);
              strcat(mybuf,")");
              free(ps);
              ps=mybuf;
              exbl |= 0x04000000;
              xtype |= Char2;
              cp(); break;
 
           case RSCT:       /* STRUCTTO */
              op();rs=getxpr();
              if(toktype() == CM) {cm(); rl=getxpr();}
                else rl=blank;
              if(toktype() == CM) {cm(); al=getxpr();}
                else al=blank;
              if (*rl == 0) {  /* supply length with sizeof */
                  mybuf=(char *)malloc(strlen(rs)+32);
                  strcpy(mybuf,"sizeof(");
                  strcat(mybuf,rs);
                  strcat(mybuf,")");
                  rl=mybuf;
              }
              mybuf=(char *)malloc(strlen(rs)+32);
              strcpy(mybuf,"&(");
              strcat(mybuf,rs);
              strcat(mybuf,")");
              free(rs);
              rs=mybuf;
              if(*al == 0) {al=Blank;ntype |= Char2;}
              else ntype |= Char3;
              enbl |= 0x03000000;
              cp(); break;
 
           default:
               error("Illegal keyword or multiple definition");
           }
        }
        else error("Non-keyword in key invocation");
    }
    nexttok(ALL);         /* gobble up the semicolon */
}
 
docall(type)
int type;
{
    is_kc = 0;
    noecho++;      /* don't output the source */
    switch(type){  /* this parses the call according to type */
    case CJUMPK:
       ntype = 0;
       doko(); parameters(PSTR|PCHR|PK|RSTR|RCHR|RK|RC); break;
    case CLOADN:
       ntype = 0;
       parameters(RC|RSTR|RCHR|RK|DB); break;
    case CLOADX:
       doko(); parameters(PSTR|PCHR|PK); break;
    case CJUMPR:
       none(); parameters(NONE); break;
    case CJUMPF:
       none(); parameters(NONE); break;
    case TKENTRY:
       ntype = 0;
       doid(); parameters(RC|RSTR|RCHR|RK|DB); break;
    case KRTURN:
       doko(); parameters(PSTR|PCHR|PK|ID); break;
    case KFORKC:
       doko(); parameters(PSTR|PCHR|PK); break;
    case CJUMPC:
       none(); parameters(NONE); break;
    }
    noecho--;     /* resume outputting source */
    if(assembler) prcomment();	
    switch(type){ /* generate the appropriate macro calls */
    case CJUMPK:
       curls(); is_kc = 1;
                genxb(); gennb(); gencjmp(); break;
    case CLOADN:
       curls(); gennb(); break;
    case CLOADX:
       curls(); genxb(); break;
    case CJUMPR:
       curls(); genrj(); break;
    case CJUMPF:
       curls(); genfj(); break;
    case TKENTRY:
       genkent(); gennb(); genrj(); break;
    case KRTURN:
       curls(); genxb(); genkret(); break;
    case KFORKC:
       curls(); genxb(); genfj(); break;
    case CJUMPC:
       curls(); gencjmp(); break;
    }
    print1(" }");
}
