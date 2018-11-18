/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "unprndh.h"
#include "locksh.h"
#include "prepkeyh.h"
#include "domainh.h"
#include "dependh.h"
#include "meterh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "domamdh.h"
 
 
unprnode_ret unprnode(        /* Unprepare a prepared node */
   NODE *node)
/* Output - */
/* unprnode_cant - Can't unprepare node because of preplocked node */
/* unprnode_unprepared - Node has been unprepared */
{
   if(0) { // When enabled, this code detects bugs
      // that are exploitable by holders of the domain tool
      // and that are also fatal in MP configurations.
      // The good news is that the diagnoses of the problem
      // is clear and immediate.
      // The bad news is that it is not always clear what to do about it.
      // There are probably several other places in the kernel
      // where requiring that an operand be locked would find
      // similar problems.
      if(!(node -> preplock & 0x80)) Panic();}
   switch (node->prepcode) {
 
    case prepasdomain: /* Prepared as a domain root */
      unprdr(node);    /* Have unprdr do the work */
      break; /* all ok */

    case unpreparednode:
      break; /* nothing to do */
 
    case prepasmeter:
      unprmet(node);   /* Have unprmet do the work */
      break; /* all ok */
 
/*
      Node is prepared as a General Registers node or as a Keys node
 
      Check the chain for the involved key to this node, skipping HOOKS
      Since this is a Domain Annex, the involved key to this node
         should be in a Domain Root; if not, CRASH
      If that Domain Root Node is PREPLOCKED, then RETURN "can't"
      Else call UNPRDR to unprepare the Domain Root which will
      unprepare the annexes as well.
*/
    case prepasstate:
    case prepasgenkeys:
      {
         NODE *rn = node->pf.dib->rootnode;
 
         if (preplock_node(rn,lockedby_unprnode))
            return unprnode_cant;
         unprdr(rn);
         unpreplock_node(rn);
         return unprnode_unprepared;
      }
 
    case prepassegment:
      unprseg(node);
      break;
 
    default:
      crash("UNPRND005 Undefined value of node->prepcode");
   }
   return unprnode_unprepared;
} /* End unprnode */
 
 
unprnode_ret superzap(key)           /* Uninvolve a slot */
/* Input - */
struct Key *key;            /* The key (and slot) to uninvolve */
/*
   Output -
      unprnode_cant - Can't unprepare node because of preplocked node
      unprnode_unprepared - Key in slot has been uninvolved
 
   There is a peculiar difference in UNPRND's effects on
   its operand's stallee's hook,
   contrasted with the presumably desired effects of
   SUPERZAP on a slot holding a hook.
*/
{
   /* Get the node containing this key (slot) */
   NODE *hn = keytonode(key);
 
   /* If the key is a HOOK, then crash */
 
   if (key->type == pihk) crash("UNPRND006 Superzap called for hook");
 
   /* See how the node holding this key is prepared */
 
   switch (hn->prepcode) {
 
    case prepasdomain:
      return superzap_dom_md(hn, key - hn->keys);
 
    case unpreparednode:
    case prepasmeter:
    case prepasstate:
    case prepasgenkeys:
      if (unprnode(hn) == unprnode_cant) return unprnode_cant;
      uninvolve(key);
      break;
 
    case prepassegment:
/*
      The slot is in a node prepared as a segment.
      Clear all table entries depending upon this slot
*/
      slotzap(key);
      uninvolve(key);
      break;
 
    default:
      crash("UNPRND008 Undefined value of node->prepcode");
   }
 
   return unprnode_unprepared;
} /* End superzap */
