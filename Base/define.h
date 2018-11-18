/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include <string.h>
typedef struct{char * which; unsigned long where;} rec;
rec site_list[] = {
#include "sites.h"
};
#define dcl(n, t) define(#n, #t)
#define include(h) enclude(#h)
#define h_s 200
#define var_lim 1000
static int ndx=0, hndx = 0;
char * who, * nm;
typedef struct{char * name; char * type;} record;
record tab[var_lim];
char * h_tab[h_s];
void define(char * n, char * t)
{if(ndx==var_lim) printf("Too many symbols");
 tab[ndx].name = n; tab[ndx].type = t; ++ndx;}
void enclude (char * h)
{if(hndx==h_s) printf("Too many header files!");
 h_tab[hndx] = h; ++hndx;}

void expound()
{FILE * p2 = fopen(nm, "w");
 int ln = Strlen(who);
 fprintf(p2, "#include <stdio.h>\n");
 {int x; for(x=0; x<hndx; x++)
  fprintf(p2, "#include \"%s\"\n", h_tab[x]);}
 fprintf(p2, "typedef struct{\n");
 {int j; for(j=0; j<ndx; j++)
  fprintf(p2, "  %s %s;\n", tab[j].type, tab[j].name);}
 fprintf(p2, "} z;\n");
 {int j; for(j=0; * site_list[j].which; j++)
   if(!Strcmp(who, site_list[j].which)) goto got;
   printf("Can\'t find %s in site data base.\n", who);
   return;
got: fprintf(p2, "#define zz (*(z *)%#x)\n", site_list[j].where);
   fprintf(p2, "\nvoid main()\n{if(sizeof(z)+ %#x > %#x)\n", 
         site_list[j].where, site_list[j+1].where);}
 fprintf(p2, " printf(\"Variables exceed site allocation.\\n\");\n");
 {char dot_h[40]; Strncpy(dot_h, who, ln-2); /*.....*/
  fprintf(p2, " {FILE * p3 = fopen(\"%s.h\", \"w\");\n", who);
  fprintf(p2, "  FILE * p4 = fopen(\"%s.s\", \"w\");\n", who);}
 {int j; for(j=0; j<ndx; j++) 
   {fprintf(p2, "  fprintf(p3, \"#define %s (*(%s *)%%#x)"
     " /*%%u*/\\n\", &zz.%s, &zz.%s);\n", 
        tab[j].name, tab[j].type, tab[j].name, tab[j].name);
    fprintf(p2, "  fprintf(p4, \" global _%s\\n def _%s,%%#x\\n\""
       ", &zz.%s);\n",
      tab[j].name, tab[j].name, tab[j].name);}
 fprintf(p2, "  fclose(p3); fclose(p4);}}\n");}
}
int main(int argc, char * argv[], char * * env)
{who = argv[1]; nm = argv[2];
