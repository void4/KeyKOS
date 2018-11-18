/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "types.h"
#include "keyh.h"
#include "percpu.h"
#include "memomdh.h"
#include "locksh.h"
#include "primcomh.h"
#include "domamdh.h"
#include "misc.h"
// #include "pte.h"
#include "memoryh.h"
#include "gateh.h"
#include "memutil.h"

#define P 4096
#define copySource COPYWINDOW
#define copySink COPYWINDOW+1
/* Global variables below initialized only because it 
 appears that the code runs faster that way. */
uchar * sink=0, * source=0;
CTE * ptl[2]={0,0};
struct Key *fromkey=0, *tokey=0;
struct {uint64 fromoffset, tooffset, length; uint32 error;} q
 = {0,0,0,0};

static int copy(int much){
int e = much >= q.length, m = e?q.length: much;
Memcpy(sink, source, m);
q.fromoffset += m; q.tooffset += m; q.length -= m;
return e;}

static void report(int rep){ /* return 24 byte string to domain. */
handlejumper();
{struct exitblock eb = {0,arg_memory,0,0,0,0,0,0,0};
  cpuexitblock = eb;}
cpuordercode = rep;
q.error = error_code;
cpuargaddr = (char *)&q; cpuarglength = 28;
if(!getreturnee())
  return_message();
if(segkeeperslot) call_seg_keep(rep==2?reskck2:reskck3);
if (ptl[1]) {coreunlock_page(7, ptl[1]); ptl[1]=0;}
if (ptl[0]) {coreunlock_page(8, ptl[0]); ptl[0]=0;}}

void jcopy(struct Key *key){
    if(cpuordercode){cpuarglength=0;
      cpuordercode = (cpuordercode == KT) ? 0x64:KT+2;
      jsimple(0); return;}
    /* get input arguments */
    pad_move_arg((char *)&q, 28);
    fromkey = ld1(); tokey = ld2();
    switch(ensurereturnee(1)){
       case ensurereturnee_overlap: midfault(); return;
       case ensurereturnee_wait: goto bail;
       case ensurereturnee_setup: break;}
    if(!q.length) {report(0); return;}
    source = accessSeg(fromkey, 0, q.fromoffset, 0, copySource);
    if(!source) {if(error_code) {report(2); return;} else goto bail;}
    sink =   accessSeg(  tokey, 1,   q.tooffset, q.length<P, copySink  );
    if(!sink) {if(error_code) {report(3); return;} else goto xbail;}
    if(((int)source&~-P) < ((int)sink&~-P)) goto movetosi;
movetoso: {int ca = P - ((int)source&~-P);
    if(copy(ca)) {report(0); return;}
    coreunlock_page(8, ptl[0]); sink += ca;
    source = accessSeg(fromkey, 0, q.fromoffset, 0, copySource);}
    if(!source) {report(error_code ? 2 : 1); return;}
movetosi: {int ca = -(int)sink & ~-P;
    if(ca && copy(ca)) {report(0); return;}
    coreunlock_page(10, ptl[1]); source += ca;
     sink =   accessSeg(  tokey, 1,   q.tooffset, q.length<P, copySink  );
    if(!sink) {report(error_code ? 3 : 1); return;}
    goto movetoso;}
xbail: coreunlock_page(12, ptl[0]);
bail: abandonj(); /* Try later after I/O. */
}

void init_copywin(){};
/* ZAP copykey. ... */
