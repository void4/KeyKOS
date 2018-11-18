/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* counterh.h - list of performance statistics counters */
 
/* defctr(x) defines a  counter. */
/* defctra(x,n) defines an array of n  counters. */
/* deftmr(t) defines a deftmr(timer. */ 
/* lastcounter() mark the end of counters and the beginning of timers. */
/* Define the above functions before including this header. */
 
defctr(outstandingioclash) /* Number of times outstandingio found 1 */
defctr(requestsout)                 /* REQOUT in assembler */
defctr(devreqsout)                  /* DEVRQOUT in assembler */
defctr(nocleanlistentries)          /* CLEANOUT in assembler */
defctr(checkpointheaderreread)      /* #CKRERD in assembler */
defctr(recleans)                    /* #RECLEAN in assembler */
defctr(readerrors)                  /* #CKRERD in assembler */
defctr(level2ints)      /* Number of level 2 interrupts */
defctr(numbersegtables)
defctr(numberpagetables)
defctr(numberallocationpotsincore)  /* #APOTS in assembler */
defctra(numbernodepotsincore,5)     /* #HNPOTS, #HNPOTSM,
                      #SNPOTS, #SNPOTSM, and #DNPOTS in assembler */
defctr(nodepotfetches)              /* #NPFETCH in assembler */
defctra(nodeinpot,5)                /* #NINPOT in assembler */
defctr(rangelistinuse)              /* rangelistcount */
defctr(rangelistmaxused)            /* #DEVRANG in assembler */
defctr(maxuserranges)               /* #USERANG in assembler */
defctr(maxswapranges)               /* #SWPRANG in assembler */
defctr(missinginstances)            /* #PARTMNT in assembler */
defctr(obsoleteranges)              /* #OBSRANG in assembler */
defctr(resyncscomplete)             /* #RESYNC in assembler */
defctr(checkpointfordirentries)     /* #CKPDIR in assembler */
defctr(checkpointforswapspace)      /* #CKPSWAP in assembler */
defctr(checkpointkeycall)           /* #CKPKEY in assembler */
defctr(checkpointduringmigration)   /* #CKPMIGR in assembler */
defctr(numberofcheckpointsstarted)  /* #CKPTS in assembler */
defctr(numberofcheckpointsdone)     /* NCKPT in assembler */
defctr(completedmigrations)         /* NMIGR in assembler */
defctr(virtualzeropages)            /* #VZPAGES in assembler */
defctr(nonzerobytesscanned)         /* #VZBYTES in assembler */
defctr(nonzeropages)                /* #NZPAGES in assembler */
defctr(logicalpageio)               /* #LPAGEIO in assembler */
defctr(migrateio)                   /* #MIGRIO in assembler */
defctra(requestscompletedbytype,8)  /* #IOLOGIC in assembler */
defctr(pageframesallocated)         /* PFRMALOC in assembler */
defctr(nodeframesallocated)         /* NFRMALOC in assembler */
defctr(pagewraps)                   /* PAGEWRAP in assembler */
defctr(nodewraps)                   /* NODEWRAP in assembler */
defctr(cleansckpt2)                 /* #IOCEND2 in assembler */
defctr(cleansckptx)                 /* #IOCENDC in assembler */
defctr(cleansnockpt)                /* #IOCENDN in assembler */
defctra(numberkeyinvstarted,18)     /* NKJS in assembler */
lastcounter()
deftmr(maxckptp2time)             /* CKPP1HI in assembler */
deftmr(currentckptp2time)         /* CKPP1CUR in assembler */
deftmr(highmigrationprioritytime) /* MIGRHPTM in assembler */
deftmr(maxmigrationoutage)         /* RMIGRHI in assembler */
deftmr(cumulativemigrationoutage)

