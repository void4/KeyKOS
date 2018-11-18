/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  jresyncc.c - Re-introduce obsolete range - KeyTech Disk I/O */
 
#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "primcomh.h"
#include "gateh.h"
#include "queuesh.h"
#include "ioworkh.h"
#include "ioreqsh.h"
#include "prepkeyh.h"
#include "getih.h"
#include "gdirecth.h"
#include "grt2grsh.h"
#include "kermap.h" /* for lowcoreflags */
#include "jresynch.h"
#include "consmdh.h"
#include "memutil.h"


/* Local static variables */
 
static CDA firstcda, lastcda;        /* CDAs of current range */
static int noderange = -1; /* 1 if node range being resynced,
                            0 if page range being resynced,
                            -1 if no range being resynced. */
static CDA thiscda;
 
 
static bool prepresync(int type)
{
   if (iosystemflags & (CHECKPOINTMODE  /* Reasons to delay */
                        | MIGRATIONINPROGRESS)) {
      enqueuedom(cpuactor, &resyncqueue);
      abandonj();
      return TRUE;
   }
   pad_move_arg(thiscda, sizeof(thiscda));
   if (type != noderange
       || cdacmp(thiscda, firstcda) < 0
       || cdacmp(thiscda, lastcda) > 0) {
      simplest(1);
      return TRUE;
   }
   if (lowcoreflags.prtckpt) consprint("."); /* progress indicator */
   switch (ensurereturnee(0)) {
    case ensurereturnee_wait:
      abandonj();
      return TRUE;
    case ensurereturnee_overlap:
      midfault();
      return TRUE;
    case ensurereturnee_setup: break;
   }
   return FALSE;
}
 
void jresync(void)
{
   switch (cpuordercode) {
    case 0:    /* Wait for range to resync */
      if (-1 != noderange) {
         simplest(2);
         return;
      }
      switch (ensurereturnee(1)) {
       case ensurereturnee_wait:
         abandonj();
         return;
       case ensurereturnee_overlap:
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      {
         uchar *cdas = grtmor();
         if (!cdas) {
            /* No range to resync. */
            enqueuedom(cpuactor, &resyncqueue);
            unsetupreturnee();
            abandonj();
            return;
         }
         Memcpy(firstcda, cdas, sizeof(CDA));
         Memcpy(lastcda, cdas+sizeof(CDA), sizeof(CDA));
         if (cdas[0] & 0x80) {
            noderange = 1;   /* A node range */
            firstcda[0] &=~0x80;
            lastcda[0] &=~0x80;
         }
         else noderange = 0;                    /* A page range */
      }
      Memcpy(cpuargpage, firstcda, 6);
      Memcpy(cpuargpage+6, lastcda, 6);
      cpuordercode = noderange;
      cpuarglength = 12;
      cpuargaddr = cpuargpage;
      cpuexitblock.argtype = arg_regs;
      break;

    case 1:    /* Resync a page cda */
      if (prepresync(0)) return;
      {
         CTE *cte;

         cte = srchpage(thiscda);      /* See if page is in a frame */
         if (cte) {
            cte->flags |= ctchanged; /* Mark it changed */
            cpuordercode = 0;
         } else {
            struct CodeIOret ior;
            ior = getreqp(thiscda, REQRESYNCREAD, getended, cpuactor);
            switch (ior.code) {
             case io_notmounted:
             case io_notreadable:
               /* Abort this resync */
               grtrsyab(firstcda);
               noderange = -1;
               cpuordercode = 2;
               break;
             case io_pagezero:
               gdisetvz(ior.ioret.pcfa);
               cpuordercode = 0;
               break;
             case io_started:
               /* fall into case below */
             case io_cdalocked:
             case io_noioreqblocks:
               checkforcleanstart();
               unsetupreturnee();
               abandonj();
               return;
             default:
               crash("JRESYNC004 Invalid return from getreqp");
            } /* End switch on getreqp return code */
            checkforcleanstart();
         }
      }
      cpuarglength = 0;
      break;   /* return */

    case 2:    /* Resync a node cda */
      if (prepresync(1)) return;
      {
         NODE *nf;

         nf = srchnode(thiscda);       /* See if node is in a frame */
         if (nf) {
            nf->flags |= NFDIRTY;  /* Mark it changed */
            cpuordercode = 0;
         } else {
            struct CodeIOret ior;

            thiscda[0] |= 0x80;      /* Add the node bit */
            ior = getreqn(thiscda, REQRESYNCREAD, getended, cpuactor);
            switch (ior.code) {
             case io_notmounted:
             case io_notreadable:
               /* Abort this resync */
               firstcda[0] |= 0x80;  /* Add the node bit */
               grtrsyab(firstcda);
               noderange = -1;
               cpuordercode = 2;
               break;
             case io_potincore:
               nf = movenodetoframe(thiscda, ior.ioret.cte);
               if (!nf) {
                  enqueuedom(cpuactor, &nonodesqueue);
                  checkforcleanstart();
                  unsetupreturnee();
                  abandonj();
                  return;
               }
               nf->flags |= NFDIRTY;
               cpuordercode = 0;
               break;
             case io_started:
               nodepotfetches++;
               /* fall into case below */
             case io_cdalocked:
             case io_noioreqblocks:
               checkforcleanstart();
               unsetupreturnee();
               abandonj();
               return;
             default:
               crash("JRESYNCC03 Invalid return from getreqn");
            } /* End switch on getreqn return code */
            checkforcleanstart();
         }
      }
      cpuarglength = 0;
      break;   /* return */

    case 3:    /* End resync */
      if (-1 == noderange) {
         simplest(1);
         return;
      }
      switch (ensurereturnee(0)) {
       case ensurereturnee_wait:
         abandonj();
         return;
       case ensurereturnee_overlap:
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      if (lowcoreflags.prtckpt) consprint("resync end\n");
      if (noderange) firstcda[0] |= 0x80;
      grtbrd(firstcda);               /* Indicate basic resync done */
      noderange = -1;
      cpuordercode = 0;
      cpuarglength = 0;
      break;
    default:
      simplest(KT+2);
      return;
   }
   /* At this point returnee is set up,
      cpuordercode has return code,
      cpuarglength is set. */
   handlejumper();
   cpuexitblock.keymask = 0;
   if (! getreturnee()) return_message();
   return;
} /* End doresync */
 
