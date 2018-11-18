/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "asm_linkage.h"
/*
The Special Fault Convention:
The code in the file callseg.ck that is obeyed by the domain
keeper provides this service.
The code below relies on this service.
Upon a store fault the address of the failing store command
is compared with field 0 of each tripple of PCs, that is found
in the 0 terminated array of tripples found at "hyperGo".
If a match is found the other two tripple members replace
PC and NPC, the access fault code replaces the contents of %o0,
and the domain resumes execution.

int CopyIfYouCan(void* to, void* from, int cc, void ** failAddr);
This routine attempts to copy cc characters from "from" to "to".
It  does this by character loads and stores that conform to the
"Special Fault Convention".
If one of these should fault, then the faulting data 
address is placed in "failAddr" and the access fault code is returned.
0 is returned if the copy finishes with no faults.
The current application, "callseg", does not require maximal 
performance and moving by character is tolerable.
 */
  ENTRY(CopyIfYouCan)
  add %o0,%o2,%o0
  add %o1,%o2,%o1
  sub %o0,1,%o5
  sub %o1,1,%o1
  subcc %g0,%o2,%o2
L: bz good
  addcc %o2,1,%o2
load: ldub [%o1+%o2], %l0
  b L
store: stb %l0, [%o5+%o2]
good: retl
  or %g0,%g0,%o0
hyperGo: .word load, loadFail, fail
 .word store, storeFail, fail
 .word 0
.globl hyperGo
loadFail: add %o1, %o2, %o1
storeFail: add %o5, %o2, %o1
fail:  st %o1, [%o3]
  st %o0, [%o3] ! record the fault code.
  retl
  or %g0, 1, %o0
  SET_SIZE(CopyIfYouCan)
.globl Panic
Panic: ta 0x7E
