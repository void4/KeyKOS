/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "kcpp.h"
 
/* external library declarations */
extern char *malloc();
 
int incnum;
#define STRSIZ   4096   /* maximum string size */
#define TOKSIZ   8000   /* token buffer size, comment is limited
                           to 100 lines */
extern int kjump;
extern int noecho;
 
extern int *errline;
char tokbuf[TOKSIZ], *tbp;    /* Token storage space */
int  tokint;
extern char *rbp;
 
#define TRACEON 1
 

/* 6/22/1998 - changed size from 5 to 16 bytes
   32 bit integers need 10 decimal places plus
   a trailing zero and possibly a leading '-'.
   */
static char decimalbuf[16];
 
/* int2decimal(val) converts val to printable decimal characters */
char *int2decimal(int val)
{
    if (sprintf(decimalbuf, "%d", val) < 0)
        fatal("decimal conversion error");
    return(decimalbuf);
}
 
static char hextab[] = "0123456789ABCDEF";
static char hexbuf[11];
 
/* hex(val) converts val to printable hex characters */
char *hex(val)
unsigned long val;
{
    hexbuf[10] = '\0';
    hexbuf[9] = hextab[(val & 0x0000000F)>> 0x00 ];
    hexbuf[8] = hextab[(val & 0x000000F0)>> 0x04 ];
    hexbuf[7] = hextab[(val & 0x00000F00)>> 0x08 ];
    hexbuf[6] = hextab[(val & 0x0000F000)>> 0x0C ];
    hexbuf[5] = hextab[(val & 0x000F0000)>> 0x10 ];
    hexbuf[4] = hextab[(val & 0x00F00000)>> 0x14 ];
    hexbuf[3] = hextab[(val & 0x0F000000)>> 0x18 ];
    hexbuf[2] = hextab[(val & 0xF0000000)>> 0x1C ];
    hexbuf[1] = 'x';
    hexbuf[0] = '0';
    return(hexbuf);
}
 
 
struct stent {           /* Entry structure */
    struct stent *next;  /*   next entry on list */
    char         *text;  /*   text to match      */
    int            val;  /*   value (or pointer) stored */
} SymTab[256];         /* 256 Hash buckets */
 
/* hashval returns a hash value between 0 and 255 */
static int hashval(str)
char *str;
{
    char val;
 
    val = *str;
    while(*str++ != '\0')
        val ^= *str;         /* XOR bytes together */
    return(val);
}
 
/* enter() enters a (name,val) pair into the symbol table */
enter(name,val)
char *name;
int val;
{
    char i;
    struct stent *ent, *tmp;
    int oldval;
 
    i = hashval(name);
    if((ent = (struct stent *)malloc(sizeof(struct stent))) == 0)
       fatal("Out of memory");
 
    /*
       check if any constant KEY was already defined
       under this name
    */
    oldval = lookup(name);
 
    if (oldval != (NOTFOUND | IDENT))    {
       /* name was in use already */
       if ((oldval & ~TKEY) < 0x10)    {
          /* name was used by const KEY previously */
          if ((val & ~TKEY) < 0x10)
             error("Duplicate name for constant KEY");
          else if ((val & ~TKEY) > 0x10)
             error("Constant KEY name can not be reused");
          }
       else                            {
          /* name was used by variable KEY previously */
          if ((val & ~TKEY) < 0x10)
             error("Illegal name for constant KEY");
       }
    }
 
    for(tmp = &SymTab[i]; tmp->next != SymTab; tmp = tmp->next);
    tmp->next = ent;
    ent->next = SymTab;
    ent->text = name;
    ent->val  = val;
}
 
/* lookup() returns the val of a string, if it can find it */
lookup(str)
char *str;
{
    char i;
    struct stent *tmp;
 
    i = hashval(str);
    for(tmp = SymTab[i].next; tmp != SymTab; tmp = tmp->next) {
        if(strcmp(str,tmp->text) == 0) {
            return(tmp->val);
        }
    }
    return(NOTFOUND|IDENT);
}
 
initsymtab()
{
    int i;
    for(i=0;i<256;i++) {              /* Set up hash buckets */
        SymTab[i].next = SymTab;
        SymTab[i].text = "*SymTab*";
        SymTab[i].val  = NOTFOUND;
    }
    enter("KC",      CALL|CJUMPK);
    enter("KALL",    CALL|CJUMPK);
    enter("CALLJUMP",CALL|CJUMPC);
    enter("LDENBL",  CALL|CLOADN);
    enter("LDEXBL",  CALL|CLOADX);
    enter("RETJUMP", CALL|CJUMPR);
    enter("FORKJUMP",CALL|CJUMPF);
    enter("KENTRY",  CALL|TKENTRY);
    enter("KRETURN", CALL|KRTURN);
    enter("KFORK",   CALL|KFORKC);
    enter("KEYSFROM",PARM|PK);
    enter("KEYSTO",  PARM|RK);
 
    enter("STRINGFROM",SPARM|PSTR);
    enter("CHARFROM",SPARM|PCHR);
    enter("STRUCTFROM",SPARM|PSCT);
    enter("STRINGTO",SPARM|RSTR);
    enter("CHARTO",  SPARM|RCHR);
    enter("STRUCTTO",  SPARM|RSCT);
 
    enter("RCTO",    PARM|RC);
    enter("OCTO",    PARM|RC);
    enter("DBTO",    PARM|DB);
    enter("KENTRYID",PARM|ID);
    enter("KEY",     DECL|DKEY);
    enter("STRING",  DECL|PLISTR);
    enter("#include",DECL|_FILE);
}
 
/* For speed these could be macros */
char *toktext() { return(tokbuf); }
toktype() { return(tokint); }
 
/* GetLogicalChar() returns the next character in the input
   after parsing trigraphs and continuation lines. */
char GetLogicalChar()
{
    char c;
repeat:
    /* Ensure there is a character to read (possibly TEOF) */
    while (*rbp == '\0') newrec();
    switch(c = *tbp++ = *rbp++) {
    case '?': /* look for trigraphs */
       if (*rbp != '?') return(c);
       switch (*++rbp) {
       case '<':  *tbp++ = '?'; *tbp++ = *rbp++; return('{');
       case '>':  *tbp++ = '?'; *tbp++ = *rbp++; return('}');
       case '\'': *tbp++ = '?'; *tbp++ = *rbp++; return('^');
       case '/':  *tbp++ = '?'; *tbp++ = *rbp++; goto backslash;
       default: rbp--; return(c);
       }
    case '\\': backslash: /* look for continuation line */
       if (*rbp != '\n') return('\\');
       *tbp++ = *rbp++;  /* consume the newline */
       goto repeat;
    default: return(c);
    }
}
 
static char logicalChar2 = '\0', /* Peeked-at logical char,
                                    '\0' if none */
            tokbuf2[80],       /* Source text for log. char */
            *tbp2,               /* Ptr to end of the above */
            *tbp1;               /* Ptr to end of consumed source */
/* Backup() backs up over the last logical character read, so it will
   be read again by nexttok. */
void Backup()
{
    /* tokbuf through tbp1 is consumed source.
       tbp1 through tbp is unconsumed source. */
    char *q = tbp1;
    tbp2 = tokbuf2;  /* copy unconsumed source to tokbuf2 */
    while (tbp1 < tbp) *tbp2++ = *tbp1++;
    tbp = q;
}
 
/* nexttok() reads tokens into the buffer and returns the type of the
        first one we find specified in lookfor and provides echo */
int nexttok(lookfor)
int lookfor;
{
    int len, c, linenum;
 
    for(;;) {  /* loop until we find the right type or EOF */
        if(!noecho) print1nl(tokbuf);
        tbp = tokbuf;
        if (logicalChar2) { /* get peeked-at character */
           char *p = tokbuf2;
           while (p < tbp2) *tbp++ = *p++; /* copy tokbuf2 to tokbuf */
           c = logicalChar2;
           logicalChar2 = '\0';  /* consume it */
        }
        else c = GetLogicalChar();
        if(isspace(c)) {
            tokint = SPACE;
            /* Must keep newline separate for preprocessor. */
            if (c != '\n') {
               while(isspace(*rbp) && (*rbp) != TEOF
                     && (*rbp) != '\0' && (*rbp) != '\n')
                   /* No need to call GetLogicalChar here.
                      The worst that can happen is we will fail to
                      concatenate whitespace. */
                   *tbp++ = *rbp++; /* accumulate whitespace */
            }
        }
        else if(isalnum(c) || c == '_' || c == '#' || c == '*' ) {
            tokint = IDENT;
            for (;;) {
               tbp1 = tbp;  /* save possible end of identifier */
               c = GetLogicalChar();
               if (isalnum(c) || c == '_') continue;
               break;
            }
            Backup();
            logicalChar2 = c;
        }
        else switch(c) {
        case '/':    /* possible start of comment */
            tbp1 = tbp;  /* save possible end of slash */
            c = GetLogicalChar();
            if (c != '*') {  /* not a comment */
                Backup();
                logicalChar2 = c;
                tokint = JUNK;
            }
            else {          /* a comment */
#ifdef V1 /* version allows infinite size of the comment */
                linenum = *errline;
                for (c = GetLogicalChar(); c != TEOF; ) {
                    if (c == '*') {  /* possible end of comment */
                        c = GetLogicalChar();
                        if (c == '/') break; /* end of comment */
                    }
                    else c = GetLogicalChar();
                    if (*errline > linenum)  {
                       if (!noecho)  {
                          *tbp = '\0';
                          print1nl(tokbuf);
                       }
                       tbp = tokbuf;            /* reset buffer */
                       linenum = *errline;      /* incr line number */
                    }
                }
                *tbp = '\0'; /* end token (in case error) */
#else /* the size of the comment is limited to 8000 bytes */
                len = 0;
                c = GetLogicalChar();
                while ((c != TEOF) && len < TOKSIZ) {
                    if (c == '*') {  /* possible end of comment */
                        c = GetLogicalChar();
                        if (c == '/') break; /* end of comment */
                    }
                    else c = GetLogicalChar();
                    len++;
                }
                *tbp = '\0'; /* end token (in case error) */
                if (len == TOKSIZ) error("Comment too long");
#endif
                if(c == TEOF){ error("EOF in comment");return(TEOF);}
                tokint = SPACE;
            }
            break;
        case '"': /* " */ /* begin a string */
#ifdef V1   /* allow infinite size of the string */
            linenum = *errline;
            for (; (c = GetLogicalChar()) != TEOF; ) {
               if (c == '"') break;
               if (c == '\\') GetLogicalChar();
                  /* numeric escapes pose no problem here */
               if (*errline > linenum)  {
                  if (!noecho)  {
                     *tbp = '\0';
                     print1nl(tokbuf);
                  }
                  tbp = tokbuf;         /* reset buffer */
                  linenum = *errline;   /* advance line number */
               }
            }
            *tbp = '\0'; /* end token (in case error) */
#else       /* size of the string is limited to 4096 bytes */
            len = 0;
            for (; (c = GetLogicalChar()) != TEOF && len < STRSIZ; ) {
               if (c == '"') break;
               if (c == '\\') GetLogicalChar();
                  /* numeric escapes pose no problem here */
               len++;
            }
            *tbp = '\0'; /* end token (in case error) */
            if (len == STRSIZ) error("String too long");
#endif
            if(c == TEOF){ error("EOF in String");return(TEOF);}
            tokint = TSTRING;
            break;
        case '\'':   /* a character constant */
            for (; (c = GetLogicalChar()) != TEOF; ) {
               if (c == '\'') break;
               if (c == '\\') GetLogicalChar();
                  /* numeric escapes pose no problem here */
            }
            *tbp = '\0'; /* end token (in case error) */
            if(c == TEOF){ error("EOF in String");return(TEOF);}
            tokint = CHARCONST;
            break;
        case ';': tokint = SC; break;
        case '(': tokint = OP; break;
        case ')': tokint = CP; break;
        case ',': tokint = CM; break;
        case '=': tokint = EQ; break;
        case '{': tokint = OB; break;
        case '}': tokint = CB; break;
        case TEOF: return(TEOF);
        default:  tokint = JUNK; break;
        }
        *tbp = '\0'; /* end token */
        if(tokint == IDENT) tokint = lookup(tokbuf);
        switch(lookfor) {
        case ALL:                         return(tokint);
        case NOSPACE: if(tokint != SPACE) return(tokint); break;
        case KEYWORD: if(tokint & CLASS)  return(tokint); break;
        case RESET:   if(tokint == SC)    return(tokint); break;
        }
    }
}
 
/* save(text) returns a pointer to a copy of the text
 *    in a malloc()ed area
 */
char *save(text)
char *text;
{
    char *safeplace;
 
    safeplace = malloc(strlen(text) + 1);
    strcpy(safeplace,text);
    return(safeplace);
}
 
/* getkey() returns the value of a KEY */
int getkey()
{
    nexttok(NOSPACE);
    if((toktype() & CLASS) == TKEY) return(toktype() & VAL);
    error("Undefined Key ");
}
 
/* getxpr() accumulates text inside of a list and returns
 *     a safe copy
 */
char *getxpr()
{
    int level = 0; /* count parens */
    char xprbuf[512], *xp = xprbuf;
    char *txt;
 
    nexttok(NOSPACE);
    for(;;) {
        txt = toktext();
        if(toktype() == OP) level++;
        if(toktype() == CP && !level--) break;
        if(toktype() == CM && !level)   break;
        if(toktype() == CB) error("Curly bracket inside expression");
        if(toktype() == OB) error("Curly bracket inside expression");
        if(toktype() == SC) error("Semicolon inside expression");
        if(toktype() == TEOF) fatal("EOF occured inside expression");
 /*     if((toktype() & CLASS) != NOTFOUND)
            warn("Key or reserved word inside expression");
            break;
 */
        while((*xp++ = *txt++) != '\0'); /* append token to xprbuf */
        xp--;
        nexttok(ALL);
    }
    *xp = '\0';
    return(save(xprbuf));
}
 
/* dokeys() fills arrays with the names and values in a key list */
dokeys(name,val)
char **name;
short *val;
{
    char count;
 
    for(count=0;count<4;count++) {
         name[count] = save("_0");
         val[count] = NOKEY;
    }
    count = 3;
    while(nexttok(NOSPACE) != CP) {
        switch(toktype()) {
        case CM:
            if(!count--) error("Too many keys specified");
            break;
        default:
            if((toktype() & CLASS) == TKEY) {
                val[count] = toktype() & VAL;
                name[count] = save(toktext());
                break;
            }
            error("Bad syntax or undefined key");
        }
    }
}
 
/* getid() returns a safe copy of an IDENT  */
char *getid()
{
    if(nexttok(NOSPACE) != IDENT)
       error("Identifier expected");
    return(save(toktext()));
}
 
none() { op(); cp(); }
 
/* These check for correct punctuation */
op()
{
    if(toktype() == OP) return;
    if(nexttok(NOSPACE) != OP) error("Missing '('");
}
cm()
{
    if(toktype() == CM) return;
    if(nexttok(NOSPACE) != CM) error("Missing ','");
}
cp()
{
    if(toktype() == CP) return;
    if(nexttok(NOSPACE) != CP) error("Missing ')'");
}
 
/* getval() gets a number specifying a Key ID */
int getval()
{
    int base, num;
    char *txt;
 
    if (nexttok(NOSPACE) != IDENT)  {
       if (*toktext() == '-')   {
          /* negative values are allowed only with KJUMP option */
          if (kjump == FALSE) error("Bad Key value");
          nexttok(NOSPACE);
          if (*toktext() == '1') return(VARKEY);
          else error("Bad Key value");
       }
       error("Key ID expected");
    }
 
    txt = toktext();
    num = 0;
    if(*txt == '0') {
        if(*++txt == 'x')  { base = 0x10; txt++; }
        else               { base =  010;        }
    } else                 { base =   10;        }
    while(*txt != '\0') {
        num *= base;
        if('0' <= *txt && *txt <= '9') num +=  *txt - '0';
        if('A' <= *txt && *txt <= 'F') num += (*txt - 'A') + 0xa;
        if('a' <= *txt && *txt <= 'f') num += (*txt - 'a') + 0xa;
        txt++;
    }
    if (num > 0x0F) error("Bad Key value");
    return(num & 0xf);
}
