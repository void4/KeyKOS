/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

.global superfetch
.type superfetch, #function

superfetch:     /* superfetch(snodekey,slot,tokey) */
     mov %o1,%o5

     set 0x0D000000,%o1
     sll %o0,28,%o0
     srl %o0,8,%o0
     or  %o1,%o0,%o1

     set 41,%o0

     set 0x00800000,%o3
     sll %o2,28,%o2
     srl %o2,16,%o2
     or  %o3,%o2,%o2

     set 13*4,%o3
     set 4,%o4
     ta  0x16
     retl
     nop
