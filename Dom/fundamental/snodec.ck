/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*******************************************************************
  SNODE - Super Node
 
********************************************************************/
#include "kktypes.h"
#include "node.h"
#include "sb.h"
#include "domain.h"
#include "snode.h"
 
#include "keykos.h"
 
     KEY comp   = 0;
     KEY sb2    = 1;
     KEY caller = 2;
     KEY dom    = 3;
     KEY sb     = 4;
     KEY meter  = 5;
     KEY domcre = 6;
 
     KEY ukey   = 7;
     KEY l1     = 8;  /* working slots as walk tree */
     KEY l2     = 9;
     KEY l3     = 10;
     KEY l4     = 11;
     KEY l5     = 12;
     KEY l6     = 13;
     KEY l7     = 14;
     KEY l8     = 15;

#define MEMNODEDOMCRE 2
 
 
   struct working {
     UINT32 rc;
     UINT32 oc;
     SINT16 databyte;
     SINT16 depth;
     SINT16 addeddepth;
     SINT16 selldepth;
     UINT32 slot;
   };
 
SINT32 qdepth(),adddepth(),moredepth(),lessdepth();
void deldepth(),sellnodes(),dofetch(),doempty(),backout();
UINT32 dostore();
 
   char title[]="SNODEC  ";
 
   char b1 = 1;

SINT32 factory(ordercode,ordinal)
    UINT32 ordercode,ordinal;
{
   JUMPBUF;
 
   struct working w;
 
   KC (sb2,SB_CreateNode) KEYSTO(l8) RCTO(w.rc);   /* initial root */
   if(w.rc) {
     LDEXBL (caller,2);
     FORKJUMP();
     return;
   }
   w.depth=1;
   KC (dom,Domain_GetMemory) KEYSTO(ukey);
   KC (ukey,Node_Swap+MEMNODEDOMCRE) KEYSFROM(domcre);
//   KC (dom,Domain_Swap+11) KEYSFROM(domcre);

   KC (dom,Domain_MakeStart) KEYSTO(ukey);
 
   LDEXBL (caller,0) KEYSFROM(ukey,dom);
   for(;;) {
      LDENBL OCTO(w.oc) KEYSTO(ukey,comp,domcre,caller) CHARTO(&w.slot,4)
           DBTO(w.databyte);
      RETJUMP();
 
      w.addeddepth=0;
      w.selldepth=0;
 
      if(w.oc == KT) {
        LDEXBL (caller,0x30D);
        continue;
      }
      if(w.oc == KT+4) {
        if(w.databyte == 1) {
           LDEXBL (caller,KT+2);
        }
        else {
           doempty(&w,w.depth);
           KC (sb2,SB_DestroyNode) KEYSFROM(l1) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l2) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l3) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l4) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l5) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l6) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l7) RCTO(w.rc);
           KC (sb2,SB_DestroyNode) KEYSFROM(l8) RCTO(w.rc);
           KC (dom,Domain_GetMemory) KEYSTO(domcre);
           KC (domcre,Node_Fetch+MEMNODEDOMCRE) KEYSTO(domcre);
//           KC (dom,Domain_Get+11) KEYSTO(domcre);
           return 0;
        }
        continue;
      }
      if(w.oc >= 0 && w.oc <= 15) {
         w.slot=w.oc;
         dofetch(&w);
         LDEXBL (caller,0) KEYSFROM(ukey);
         continue;
      }
      if(w.oc >= 16 && w.oc <= 31) {
         w.slot=w.oc-16;
         w.oc=dostore(&w);
         LDEXBL (caller,w.oc) KEYSFROM(ukey);
         continue;
      }
      switch(w.oc) {
         case SNode_Weaken:
           KC(dom,Domain_MakeStart) CHARFROM(&b1,1) KEYSTO(ukey);
           LDEXBL (caller,0) KEYSFROM(ukey);
           break;
         case SNode_Clear:
           if(w.databyte == 1) {
              LDEXBL (caller,KT+2);
           }
           else {
              doempty(&w,w.depth);
              w.depth=1;
              LDEXBL (caller,0);
           }
           break;
         case SNode_Fetch:
           dofetch(&w);
           LDEXBL (caller,0) KEYSFROM(ukey);
           break;
         case SNode_Swap:
           w.oc=dostore(&w);
           LDEXBL (caller,w.oc) KEYSFROM(ukey);
           break;
         case SNode_MoveRegs:
           LDEXBL (caller,0) KEYSFROM(ukey,comp,domcre);
           break;
         default:
           LDEXBL (caller,KT+2);
      }
   }
}
/******************************************************************
    DOFETCH(w)  fetch the key.  If the tree is not deep enough
                simply return a Zero Data key rather than expand
                the tree.
*******************************************************************/
void dofetch(w)
    struct working *w;
{
    UINT32 rc;
    JUMPBUF;
 
    if(qdepth(w)) {               /* not deep enough */
        KC (l8,Node_Fetch+0) KEYSTO(,,ukey); /* return DK0 */
        return;
    }
/*
    At this point there may be DK(0) in the path.  Since this is a
    fetch and DK(0) is the correct response, we just continue on
    ignoring the return codes.
*/
    switch(w->depth) {
      case 8:
         KC (l1,Node_Fetch+((w->slot & 0xF0000000) >>28))  KEYSTO(l2)
            RCTO(rc);
      case 7:
         KC (l2,Node_Fetch+((w->slot & 0x0F000000) >>24))  KEYSTO(l3)
            RCTO(rc);
      case 6:
         KC (l3,Node_Fetch+((w->slot & 0x00F00000) >>20))  KEYSTO(l4)
            RCTO(rc);
      case 5:
         KC (l4,Node_Fetch+((w->slot & 0x000F0000) >>16))  KEYSTO(l5)
            RCTO(rc);
      case 4:
         KC (l5,Node_Fetch+((w->slot & 0x0000F000) >>12))  KEYSTO(l6)
            RCTO(rc);
      case 3:
         KC (l6,Node_Fetch+((w->slot & 0x00000F00) >> 8))  KEYSTO(l7)
            RCTO(rc);
      case 2:
         KC (l7,Node_Fetch+((w->slot & 0x000000F0) >> 4))  KEYSTO(l8)
            RCTO(rc);
      case 1:
         KC (l8,Node_Fetch+(w->slot & 0x0000000F))     KEYSTO (ukey)
            RCTO(rc);
    }
    return;
}
/******************************************************************
    DOSTORE(w)  store the key.  If the tree is not deep enough
                expand the depth.  Also build any missing sections
                of the tree
*******************************************************************/
UINT32 dostore(w)
    struct working *w;
{
    SINT32 i,j;
    UINT32 rc;
    UINT32 slot;
    JUMPBUF;
 
    slot=w->slot;
    if(w->databyte == 1) {            /* check for write protected */
       KC (l8,Node_Fetch+0) KEYSTO(,,ukey);
       return KT+2;
    }
    i=qdepth(w);
    if(i) {                        /* must add depth at the top */
       if(adddepth(w,i)) {         /* it must have failed */
          deldepth(w);
          KC (l8,Node_Fetch+0) KEYSTO(,,ukey);
          return 2;
       }
    }                                /* tree is deep enough */
 
    switch(w->depth) {
      case 8:
         KC (l1,Node_Fetch+((w->slot & 0xF0000000) >>28))  KEYSTO(l2)
            RCTO(rc); /* l1 is always a NODE for depth 8 */
      case 7:
         KC (l2,Node_Fetch+((w->slot & 0x0F000000) >>24))  KEYSTO(l3)
            RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l2)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l1,Node_Swap+((w->slot & 0xF0000000) >> 28))
                KEYSFROM(l2) RCTO(rc);
            w->selldepth=7;
         }
      case 6:
         KC (l3,Node_Fetch+((w->slot & 0x00F00000) >>20))  KEYSTO(l4)
              RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l3)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l2,Node_Swap+((w->slot & 0x0F000000) >> 24))
               KEYSFROM(l3) RCTO(rc);
            if(!w->selldepth) w->selldepth=6;
         }
      case 5:
         KC (l4,Node_Fetch+((w->slot & 0x000F0000) >>16))  KEYSTO(l5)
                 RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l4)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l3,Node_Swap+((w->slot & 0x00F00000) >> 20))
               KEYSFROM(l4) RCTO(rc);
            if(!w->selldepth) w->selldepth=5;
         }
      case 4:
         KC (l5,Node_Fetch+((w->slot & 0x0000F000) >>12))  KEYSTO(l6)
                RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l5)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l4,Node_Swap+((w->slot & 0x000F0000) >> 16))
                KEYSFROM(l5) RCTO(rc);
            if(!w->selldepth) w->selldepth=4;
         }
      case 3:
         KC (l6,Node_Fetch+((w->slot & 0x00000F00) >> 8))  KEYSTO(l7)
               RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l6)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l5,Node_Swap+((w->slot & 0x0000F000) >> 12))
                KEYSFROM(l6) RCTO(rc);
            if(!w->selldepth) w->selldepth=3;
         }
      case 2:
         KC (l7,Node_Fetch+((w->slot & 0x000000F0) >> 4))  KEYSTO(l8)
               RCTO(rc);
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l7)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l6,Node_Swap+((w->slot & 0x00000F00) >>  8))
                 KEYSFROM(l7) RCTO(rc);
            if(!w->selldepth) w->selldepth=2;
         }
      case 1:
         KC (l8, Node_Fetch+(w->slot & 0x0000000F)) RCTO(rc); /* test */
         if (rc) {                            /* opps missing node */
            KC (sb2,SB_CreateNode) KEYSTO(l8)  RCTO(rc);
            if (rc) { backout(w);return 3;}
            KC (l7,Node_Swap+((w->slot & 0x000000F0) >>  4))
                KEYSFROM(l8) RCTO(rc);
            if(!w->selldepth) w->selldepth=1;
         }
         KC (l8, Node_Swap+(w->slot & 0x0000000F))
              KEYSFROM(ukey) KEYSTO(ukey) RCTO(rc);
    }
    return 0;
}
/******************************************************************
    BACKOUT(w)   Sell all nodes purchased to extend the tree
                 and all nodes purchesed to increased the depth
*******************************************************************/
void backout(w)
    struct working *w;
{
    JUMPBUF;
    sellnodes(w);
    deldepth(w);
    KC (l8,Node_Fetch+0) KEYSTO(,,ukey);
    return;
}
/*****************************************************************
    DOEMPTY(w,level)   Sell all nodes but the root node
                 This recursive routine walks the entire node
                 tree selling nodes from the leaves up.
******************************************************************/
void doempty(w,level)
    struct working *w;
    SINT16 level;
{
    JUMPBUF;
    UINT16 i;
    UINT32 rc;
 
    switch (level) {
      case 8:
         for(i=0;i<16;i++) {
            KC(l1,Node_Fetch+i) KEYSTO(l2) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
                KEYSFROM(l1) RCTO(rc);
         return;
      case 7:
         for(i=0;i<16;i++) {
            KC(l2,Node_Fetch+i) KEYSTO(l3) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
               KEYSFROM(l2) RCTO(rc);
         return;
      case 6:
         for(i=0;i<16;i++) {
            KC(l3,Node_Fetch+i) KEYSTO(l4) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
               KEYSFROM(l3) RCTO(rc);
         return;
      case 5:
         for(i=0;i<16;i++) {
            KC(l4,Node_Fetch+i) KEYSTO(l5) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
               KEYSFROM(l4) RCTO(rc);
         return;
      case 4:
         for(i=0;i<16;i++) {
            KC(l5,Node_Fetch+i) KEYSTO(l6) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
              KEYSFROM(l5) RCTO(rc);
         return;
      case 3:
         for(i=0;i<16;i++) {
            KC(l6,Node_Fetch+i) KEYSTO(l7) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
              KEYSFROM(l6) RCTO(rc);
         return;
      case 2:
         for(i=0;i<16;i++) {
            KC(l7,Node_Fetch+i) KEYSTO(l8) RCTO(rc);
            if(rc) return;
            doempty(w,level-1);
         }
         if(w->depth != level) KC (sb2,SB_DestroyNode)
             KEYSFROM(l7) RCTO(rc);
         return;
      case 1:
         if(w->depth != level) KC (sb2,SB_DestroyNode)
             KEYSFROM(l8) RCTO(rc);
         return;
    }
}
/*********************************************************************
    QDEPTH(w)     Returns the additional depth required
                  for this slot number.
**********************************************************************/
SINT32 qdepth(w)
    struct working *w;
{
    SINT32 i;
    UINT32 slot;
    JUMPBUF;
 
    if(w->slot == 0) return 0;         /* special case */
    slot=w->slot;
 
    for(i=0;i<(8- w->depth);i++) {  /* count the number of shifts */
      if (slot & 0xF0000000) {      /* until it is normalized */
        i=8-i;                       /* the required depth */
        if(i - w->depth <= 0) return 0;
        else return i- w->depth;
      }
      slot=slot << 4;
    }
    return 0;                         /* at maximum depth */
}
/*******************************************************************
    ADDDEPTH(w,add)    Add nodes at top to increase depth by
                       ADD amount.  Record the number added
                       in W.ADDEDDEPTH.
********************************************************************/
SINT32 adddepth(w,add)
    struct working *w;
    SINT32 add;
{
    SINT32 i;
    UINT32 rc;
    JUMPBUF;
 
      switch(w->depth) {    /* based on current depth */
         case 1:
            KC (sb2,SB_CreateNode) KEYSTO(l7) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l7,Node_Swap+0) KEYSFROM(l8);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 2:
            KC (sb2,SB_CreateNode) KEYSTO(l6) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l6,Node_Swap+0) KEYSFROM(l7);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 3:
            KC (sb2,SB_CreateNode) KEYSTO(l5) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l5,Node_Swap+0) KEYSFROM(l6);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 4:
            KC (sb2,SB_CreateNode) KEYSTO(l4) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l4,Node_Swap+0) KEYSFROM(l5);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 5:
            KC (sb2,SB_CreateNode) KEYSTO(l3) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l3,Node_Swap+0) KEYSFROM(l4);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 6:
            KC (sb2,SB_CreateNode) KEYSTO(l2) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l2,Node_Swap+0) KEYSFROM(l3);            /* chain */
            if(!(add=moredepth(w,add))) break;
         case 7:
            KC (sb2,SB_CreateNode) KEYSTO(l1) RCTO(rc); /* new root */
            if(rc) return 1;
            KC (l1,Node_Swap+0) KEYSFROM(l2);            /* chain */
            if(!(add=moredepth(w,add))) break;
       }
    return 0;
}
/*********************************************************************
    MOREDEPTH(w,add)  bump w->adddepth, w->depth and decrement
                      add.  return the decrmented value.
**********************************************************************/
SINT32 moredepth(w,add)
    struct working *w;
    SINT32 add;
{
    w->addeddepth++;
    w->depth++;
    return add-1;
}
/*********************************************************************
    DELDEPTH(w)    Sell ADDEDDEPTH nodes off the top to reduce
                   the depth (based on ADDDEPTH failure)
**********************************************************************/
void deldepth(w)
    struct working *w;
{
    unsigned long rc;
    JUMPBUF;
 
    if(!w->addeddepth) return;
    switch (w->depth) {
      case 8:
        KC (sb,SB_DestroyNode) KEYSFROM(l1) RCTO(rc);
        if(!lessdepth(w)) break;
      case 7:
        KC (sb,SB_DestroyNode) KEYSFROM(l2) RCTO(rc);
        if(!lessdepth(w)) break;
      case 6:
        KC (sb,SB_DestroyNode) KEYSFROM(l3) RCTO(rc);
        if(!lessdepth(w)) break;
      case 5:
        KC (sb,SB_DestroyNode) KEYSFROM(l4) RCTO(rc);
        if(!lessdepth(w)) break;
      case 4:
        KC (sb,SB_DestroyNode) KEYSFROM(l5) RCTO(rc);
        if(!lessdepth(w)) break;
      case 3:
        KC (sb,SB_DestroyNode) KEYSFROM(l6) RCTO(rc);
        if(!lessdepth(w)) break;
      case 2:
        KC (sb,SB_DestroyNode) KEYSFROM(l7) RCTO(rc);
        if(!lessdepth(w)) break;
      case 1:;
    }
}
/********************************************************************
    LESSDEPTH(w)      Decrease the depth because of a space failure
                      during expansion.  ADDEDDEPTH has the number
                      of added nodes during the attempted expansion
*********************************************************************/
SINT32 lessdepth(w)
    struct working *w;
{
    w->depth--;
    w->addeddepth--;
    return w->addeddepth;
}
/*******************************************************************
    SELLNODES(w)      Sell back the nodes purchased to increase
                      the depth of the tree
********************************************************************/
void sellnodes(w)
    struct working *w;
{
    UINT32 rc;
 
    JUMPBUF;
 
    switch (w->selldepth) {
      case 7:
        KC (sb2,SB_DestroyNode) KEYSFROM(l2) RCTO(rc);
      case 6:
        KC (sb2,SB_DestroyNode) KEYSFROM(l3) RCTO(rc);
      case 5:
        KC (sb2,SB_DestroyNode) KEYSFROM(l4) RCTO(rc);
      case 4:
        KC (sb2,SB_DestroyNode) KEYSFROM(l5) RCTO(rc);
      case 3:
        KC (sb2,SB_DestroyNode) KEYSFROM(l6) RCTO(rc);
      case 2:
        KC (sb2,SB_DestroyNode) KEYSFROM(l7) RCTO(rc);
      case 1:
        KC (sb2,SB_DestroyNode) KEYSFROM(l8) RCTO(rc);
      case 0:;
    }
    w->selldepth=0;
}
