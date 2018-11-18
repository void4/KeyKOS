/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "item.h"
// #include "itemh.h"
#include "sysdefs.h"
#include <stdlib.h>

static void nkey( // Snitched from itemh.h
  short type,  /* the key type */
  short db,    /* the data byte */
  struct symr *np)  /* the node referenced */
{
   union DiskKey *p = &(nodes[curr_node_cda].keys[curr_key]);
   nkeynosym(type, db,
             np->isdef ? np->v.value : (long)np->v.place);
   if (!np->isdef) {np->v.place=p->ik.cda;}
   check_visited(np, 0);
}

prim_info_t *
def_initial_items()
{ 
  prim_info_t *prim_infop;

#ifndef restart_kernel

/* A restart kom (v.s. big bang kom) does not need to define the following
   domains.  It will get everything from a checkpoint.  For more detail
   information, check the comments in kernel/common/grestart.c */

/* primary node structure */

  dclsym(rootnode);
  dclsym(earlynode);
  dclsym(firstobj);
  dclsym(firstobjb);
  dclsym(earlydnode);
  dclsym(kernelkeys);
  dclsym(kernelmkeys);
  dclsym(kerneldnode);
  dclsym(bwaitnode);
  dclsym(tarsegnode);
  dclsym(kernelnode);
  dclsym(journalpage);
  dclsym(journalnode);
  dclsym(uartnode);
  dclsym(scsinode);
  dclsym(atanode);
  dclsym(ethernode);
  dclsym(earlybanks);
  dclsym(earlymeters);

  dclsym(wombfacil);
  dclsym(resfacil);
  dclsym(fundfacil);
  dclsym(suppfacil);   /* needed by DCCC, KIDC, and FCC  ... sigh */
  dclsym(privnode);

  dclsym(primemeter);
  dclsym(systemmeter);
  dclsym(usermeter);
  dclsym(basicmeter);

  dclsym(sbmeter);

  dclsym(sbcode);   /* SB and SBT */
  dclsym(sbseg);
  dclsym(sbstack);
  dclsym(sbnodemap);
  dclsym(sbpagemap);
  dclsym(sbsbdata);
  dclsym(sbstoragenode);

  dclsym(psbguard);
  dclsym(psbnode);
 
  dclsym(sbmrt);
  dclsym(sbdom);
  dclstatesym(sbgrn);
  dclsym(sbgkn);

  dclsym(sbshivacode);  /* SBSHIVA a helper */
  dclsym(sbshivaseg);
  dclsym(sbshivastack);

  dclsym(sbshivamrt);
  dclsym(sbshivadom);
  dclstatesym(sbshivagrn);
  dclsym(sbshivagkn);

  dclsym(ckptmeter);
  dclsym(ckptparm);  /* parameter node */
  dclsym(ckptlog);   /* log page */
  dclsym(ckptstack); /* stack page */
  dclsym(ckptcode);  /* code pages */
  dclsym(ckptcseg);  /* code segment */
  dclsym(ckptmrt);   /* memory root node */
  dclsym(ckptdom);   /* domain root */
  dclstatesym(ckptgrn);   /* registers */
  dclsym(ckptgkn);   /* keys */

  dclsym(emigmeter);
  dclsym(emigcode);
  dclsym(emigseg);
  dclsym(emigstack);
  dclsym(emigmrt);
  dclsym(emigdom);
  dclstatesym(emiggrn);
  dclsym(emiggkn);

   dclsym(eresynccmeter);
   dclsym(eresyncccode);
   dclsym(eresynccseg);
   dclsym(eresynccstack);
   dclsym(eresynccmrt);
   dclsym(eresynccdom);
   dclstatesym(eresynccgrn);
   dclsym(eresynccgkn);
 
  dclsym(dccseg);
  dclsym(dcccode);
  dclsym(dcccompnode);

  dclsym(sbtdcstack);  /* SBT DC */
  dclsym(sbtdcmrt);
  dclsym(sbtdcdom);
  dclstatesym(sbtdcgrn);
  dclsym(sbtdcgkn);

  dclsym(dccdcstack);  /* DCC DC */
  dclsym(dccdcmrt);
  dclsym(dccdcdom);
  dclstatesym(dccdcgrn);
  dclsym(dccdcgkn);

  dclsym(dccstack);
  dclsym(dccmrt);
  dclsym(dccdom);
  dclstatesym(dccgrn);
  dclsym(dccgkn);

  dclsym(wombkeepdom);
  dclsym(wombkeepcseg);
  dclsym(wombkeepcode);
  dclsym(wombkeepmrt);
  dclsym(wombkeepstack);
  dclstatesym(wombkeepgrn);
  dclsym(wombkeepgkn);
//  dclsym(womblist);

  dclsym(lsfsimseg);
  dclsym(lsfsimcode);

  dclsym(fcc);     
  dclsym(fcccseg);
  dclsym(fcccode);
  dclsym(kidc);   
  dclsym(kidccseg);
  dclsym(kidccode);
  dclsym(fsc);
  dclsym(fsccseg);
  dclsym(fsccode);
  dclsym(tdo);
  dclsym(tdocseg);
  dclsym(tdocode);
  dclsym(pcs);
  dclsym(pcscseg);
  dclsym(pcscode);
  dclsym(snode2);
  dclsym(snode2cseg);
  dclsym(snode2code);

  dclsym(tarrootcseg);   /* tar segment for testing */
  dclsym(tarrootcode);
  dclsym(tarinitcseg);
  dclsym(tarinitcode);   /* tar segment for init */

  def_node(&kernelnode,0);    /* KERNELNODE must be first node CDA=1 */
    msckey(errormisckey,0);
    dk0s(15);

  pages(&journalpage,1,"");   /* JOURNALPAGE must be first page CDA=1 */
 
  def_node(&primemeter,0);    /* Prime Meter must be CDA = 2 */
    dk(0);
    nkey(meterkey,0,&supermeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

  def_node(&systemmeter,0);
    dk(0);
    nkey(meterkey,0,&primemeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

  def_node(&usermeter,0);
    dk(0);
    nkey(meterkey,0,&primemeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

  def_node(&basicmeter,0);
    dk(0);
    nkey(meterkey,0,&systemmeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);


  def_node(&privnode,0);
    dk(0);
    dk(0);
    dk(0);
    dk(0);                                /* CDUMP for ckptdvr */
    dk0s(12);                             /* slot 6 gets KIDC for use by FCC build */

  def_node(&wombfacil,0);
    nkey(nodekey,0,&fundfacil); 
    nkey(nodekey,0,&suppfacil);
    nkey(nodekey,0,&resfacil);
    dk0s(13);

  def_node(&fundfacil,0);
    msckey(discrimmisckey,0);
    msckey(datamisckey,0);
    msckey(returnermisckey,0);
    dk(0);
    nkey(startkey,1,&sbdom);
    dk0s(11);
  
  def_node(&suppfacil,0);
    nkey(startkey,0,&dccdom);
    dk0s(15);

  def_node(&resfacil,0);
    nkey(meterkey,0,&basicmeter);
    nkey(segmentkey,144,&psbnode);
    msckey(errormisckey,0);
    dk0s(13);

  def_node(&rootnode,0);       /* ROOTNODE */
    dk(0);  /* place for sys. */
    dk(0);  /* place for builder. */
    dk(0);  /* place for admin.*/
    nkey(nodekey,0,&earlynode);
    nkey(nodekey,0,&earlydnode);
    nkey(nodekey,0,&firstobj);
    nkey(nodekey,0,&firstobjb);
    nkey(nodekey,0,&kernelkeys);
    nkey(nodekey,0,&kernelmkeys);
    nkey(nodekey,0,&kerneldnode);
    nkey(nodekey,0,&bwaitnode);
    nkey(nodekey,0,&tarsegnode);
    dk(0);
    dk(0);
    dk(0);
    dk(0);

  def_node(&earlynode,0);                     /* EARLYNODE */
    nkey(startkey,1,&sbdom);                  /* SBT */ 
    nkey(segmentkey,144,&psbnode);            /* PRIMESB */
    nkey(meterkey,0,&primemeter);             /* PRIMEMETER */
    nkey(startkey,0,&dccdom);                 /* DCC */
    devkey(255,0,0,0);                        /* CONSOLE KEY */
    nkey(nodekey,0,&primemeter);              /* PRIMEMETER NODE */
    nkey(segmentkey,128+64+3,&lsfsimseg);     /* LSFSIM Segmeent */
    nkey(nodekey,0,&kernelnode);
    nkey(nodekey,0,&journalnode);
    nkey(nodekey,0,&earlybanks);
    nkey(nodekey,0,&earlymeters);
#ifdef diskless_kernel
    irangekey(prangekey,0,0x1000);
    dk0s(2);
#else
    dk0s(3);
#endif
/* temp */
    nkey(nodekey,0,&wombfacil);
    nkey(nodekey,0,&privnode);
/* end temp */

  def_node(&earlybanks,0);
    dk0s(16);
  def_node(&earlymeters,0);
  // The 3 nkey calls below had an extra 0 arg at the end. I don't know why.
    nkey(nodekey,0,&systemmeter);
    nkey(nodekey,0,&usermeter);
    nkey(nodekey,0,&basicmeter);
    dk0s(13);

  def_node(&earlydnode,0);
    nkey(domainkey,0,&dccdcdom);
    nkey(domainkey,0,&dccdom);
    nkey(domainkey,0,&sbtdcdom);
    nkey(domainkey,0,&sbdom);
    nkey(domainkey,0,&sbshivadom);
    nkey(domainkey,0,&emigdom);
    nkey(domainkey,0,&eresynccdom);
    nkey(domainkey,0,&ckptdom);
    nkey(domainkey,0,&wombkeepdom);
    dk0s(7);

  def_node(&firstobj,0);
    dk0s(16);
  def_node(&firstobjb,0);
    dk0s(16);

  def_node(&kernelkeys,0);
    msckey(errormisckey,0);
    msckey(kiwaitmisckey,0);
    msckey(discrimmisckey,0);
    msckey(returnermisckey,0);
    msckey(datamisckey,0);
    msckey(systimermisckey,0);
    msckey(calclockmisckey,0);
    msckey(cdapeekmisckey,0);
    msckey(chargesettoolmisckey,0);
    msckey(deviceallocationmisckey,0);  
    msckey(domtoolmisckey,0);
    msckey(peekmisckey,0);
    msckey(Fpeekmisckey,0);
    msckey(Fpokemisckey,0);
    msckey(keybitsmisckey,0);             
    dk0s(1);

  def_node(&kernelmkeys,0);
    msckey(kerrorlogmisckey,0);
    msckey(geterrorlogmisckey,0);
    dk(0);
    dk(0);
    dk(0);
    dk0s(11);

  def_node(&journalnode,0);   /* JOURNAL NODE */
    pkey(&journalpage,0,0);
    msckey(takeckptmisckey,0);
    msckey(journalizekeymisckey,0);
    dk0s(13);

  def_node(&tarsegnode,0);
    nkey(nodekey,128+5,&tarinitcseg);      /* All early domains */
    nkey(nodekey,128+5,&tarrootcseg);      /* test domains      */
    dk0s(14);

  def_node(&kerneldnode,0);
    nkey(nodekey,0,&uartnode);
    nkey(nodekey,0,&ethernode);
    nkey(nodekey,0,&scsinode);
    nkey(nodekey,0,&atanode);
    dk0s(12);

  def_node(&uartnode,0);
    devkey(255,1,0,0);
    devkey(255,2,0,0);   /* uart b */ 
    dk0s(14);

  def_node(&scsinode,0);
    dk0s(16);

  def_node(&atanode,0);
    dk0s(16);

  def_node(&ethernode,0);
    dk0s(16);

  def_node(&bwaitnode,0);    /* nbwaitkeys in timeh.h is set to 4 */
    dk(0);                   /* 0 is used by chkpt driver */
    msckey(bwaitmisckey,1);  /* 1 is used by the bwait multiplexor */
    msckey(bwaitmisckey,2);  /* 2 is used by the scheduler  */
    msckey(bwaitmisckey,3);  /* 3 is used by the TCP/IP stack */
    dk0s(12); 
 
  cmsfile(&tarrootcode,768,"RootDir.tarseg",1);
  genseg4096(&tarrootcseg,768,&tarrootcode);
  cmsfile(&tarinitcode,2048,"Init.tarseg",1);
  genseg4096(&tarinitcseg,2048,&tarinitcode);

  cmsfile(&lsfsimcode,5,"lsfsim",1);    /* CODE for LSFSIM, the early loader */
  genseg(&lsfsimseg,5,&lsfsimcode);    /* define segment */
  
/*****************************************************************************

  Here starts the hard coded part of the WOMB including
       EMIG   - not in diskless version
       CKPTDVR- not in diskless version
       SB and SBT 
       DCC and some DC's

  All these objects are built using the DWL shell script and each object
  MUST define the variable "bootwomb=1" so that CFSTART will NOT do anything
  with the memory tree which is described completely in this section.  The
  presence of the bootwomb=1 variable is what distinguishes these objects from
  the later objects that use CFSTART (type 407 objects) or LSFSIM (type 413 objects)
  to build their memory trees (after a spacebank becomes available).

*****************************************************************************/

/* SBMETER a meter for spacebank and other early stuff stuff */

  def_node(&sbmeter,0); 
    dk(0);
    nkey(meterkey,0,&basicmeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

   cmsfile(&dcccode,6,"dccc", 1);   /* DCCC code */
   genseg(&dccseg,6,&dcccode);
   pages(&sbtdcstack,1,"");
   pages(&dccdcstack,1,"");
   pages(&dccstack,1,"");
   
   def_node(&dcccompnode,0);     /* "Components node" for DCC */
     msckey(returnermisckey,0);
     msckey(domtoolmisckey,0);
     nkey(domainkey,0,&sbtdcdom);
     nkey(startkey,1,&sbdom);
     nkey(fetchkey,0,&wombfacil);
     dk0s(11);
   
/* SB and SBT , also SHIVA */

   cmsfile(&sbcode,30,"sbc",1);   /* SBC code */
   genseg256(&sbseg,30,&sbcode);
   pages(&sbstack,1,"");
   pages(&sbnodemap,1,"");
   pages(&sbpagemap,1,"");
   pages(&sbsbdata,1,"");

   def_node(&sbstoragenode,0);     /* SBC storage node */
     dk(0);
    devkey(255,0,0,0);
     dk0s(14);
	
   cmsfile(&sbshivacode,6,"sbshivac",1);   /* SBSHIVA code */
   genseg(&sbshivaseg,6,&sbshivacode);
   pages(&sbshivastack,1,"");

/* define SB code */

   def_node(&sbmrt,0);
     nkey(nodekey,4,&sbseg);
     pkey(&sbnodemap,0,0);
     dk(0);
     pkey(&sbpagemap,0,0);
     dk0s(3);
     pkey(&sbsbdata,0,0);
     dk0s(7);
     pkey(&sbstack,0,0);

   def_node(&sbdom,1);
     nkey(domainkey,0,&sbtdcdom); /* brand with domcre */
     nkey(meterkey,0,&sbmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&sbmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&sbgkn);
     nkey(nodekey,0,&sbgrn);
   def_node(&sbgkn,0);
     dk0s(3);
     nkey(domainkey,0,&sbdom);
     msckey(returnermisckey,0);
     dk(0);
     nkey(startkey,0,&sbtdcdom);
     nkey(nodekey,0,&sbstoragenode);
     dk0s(3);
     nkey(nodekey,5,&sbmrt);
     dk0s(2);
     irangekey(prangekey,0x1000,0x600000-0x1000);
     irangekey(nrangekey,0x1000,0x600000-0x1000);

   def_statenode(sbgrn,0,0x00F00F80);

/* The Prime Spacebank red segment node , Segment keys with db=144 to this are Primebank */

   pages(&psbguard,1,"");
   def_node(&psbnode,0);     /* segment keys to this are the PSB db=144 */
    pkey(&psbguard,0,0);     /* startkeys db=1 to SBDOM are the SBT */
    dk0s(9);
    nkey(nodekey,0,&psbnode);
    nkey(nodekey,0,&psbnode);
    dk0s(2);
    nkey(startkey,0,&sbdom);
    dk(0x0F9FEF03);

/*  SHIVA domain for SB (helper) */

   def_node(&sbshivamrt,0);
     nkey(nodekey,3,&sbshivaseg);
     dk0s(14);
     pkey(&sbshivastack,0,0);

   def_node(&sbshivadom,1);
     nkey(nodekey,0,&sbshivadom); /* inaccessible brand */
     nkey(meterkey,0,&sbmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&sbshivamrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&sbshivagkn);
     nkey(nodekey,0,&sbshivagrn);
   def_node(&sbshivagkn,0);
     dk0s(3);
     nkey(domainkey,0,&sbshivadom);
     msckey(returnermisckey,0);
     dk0s(6);
     nkey(nodekey,5,&sbshivamrt);
     dk(0);
     nkey(startkey,2,&sbdom);
     irangekey(prangekey,0x1000,0x600000-0x1000);
     irangekey(nrangekey,0x1000,0x600000-0x1000);

   def_statenode(sbshivagrn,0,0x00F00F80);

 /* EMIG - external migrator */

  def_node(&emigmeter,0);
    dk(0);
    nkey(meterkey,0,&supermeter);   /* must run when prime meter is stopped */
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

   cmsfile(&emigcode,35,"emigc",1);
   genseg256(&emigseg,35,&emigcode);
   pages(&emigstack,1,"");

   def_node(&emigmrt,0);
     nkey(nodekey,4,&emigseg);
     dk0s(14);
     pkey(&emigstack,0,0);

#if diskless_kernel
   def_node(&emigdom,0);
#else
   def_node(&emigdom,1);  
#endif
     nkey(nodekey,0,&emigdom); /* inaccessible brand */
     nkey(meterkey,0,&emigmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&emigmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&emiggkn);
     nkey(nodekey,0,&emiggrn);
   def_node(&emiggkn,0);
     dk0s(3);
     msckey(migrate2misckey,0);
     dk0s(12);
   def_statenode(emiggrn,0,0x00F00F80);

  /* eresyncc - external resync'er */
 
   def_node(&eresynccmeter,0);
     dk(0);
     nkey(meterkey,0,&basicmeter);
     dk(0);
     dk2(0xffffff,0xffffffff);
     dk2(0xffffff,0xffffffff);
     dk2(0xffffff,0xffffffff);
     dk0s(10);
 
    cmsfile(&eresyncccode,4,"eresyncc",1);
    genseg(&eresynccseg,4,&eresyncccode);
    pages(&eresynccstack,1,"");
 
    def_node(&eresynccmrt,0);
      nkey(nodekey,3,&eresynccseg);
      dk0s(14);
      pkey(&eresynccstack,0,0);
 
#if diskless_kernel
    def_node(&eresynccdom,0);
#else
    def_node(&eresynccdom,1);  
#endif
      nkey(nodekey,0,&eresynccdom); /* inaccessible brand */
      nkey(meterkey,0,&eresynccmeter);
      msckey(errormisckey,0);
      nkey(nodekey,5,&eresynccmrt);
      dk0s(9);
      dk(1); /*busy*/
      nkey(nodekey,0,&eresynccgkn);
      nkey(nodekey,0,&eresynccgrn);
    def_node(&eresynccgkn,0);
      msckey(resynctoolmisckey,0);
      dk0s(15);
    def_statenode(eresynccgrn,0,0x00F00F80);
 
/* CKPTDVR */

  def_node(&ckptmeter,0);
    dk(0);
    nkey(meterkey,0,&basicmeter);
    dk(0);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk2(0xffffff,0xffffffff);
    dk0s(10);

    cmsfile(&ckptcode,5,"ckptdvrc",1);
    genseg(&ckptcseg,5,&ckptcode);
    pages(&ckptstack,1,"");
    pages(&ckptlog,1,"");

    def_node(&ckptparm,0);
      msckey(bwaitmisckey,0);     /* 0 */
      msckey(takeckptmisckey,0);  /* 1 */
      nkey(nodekey,0,&privnode);  /* 2 */
      msckey(systimermisckey,0);  /* 3 */
      devkey(255,0,0,0);
      dk0s(11);
    def_node(&ckptmrt,0);
      nkey(nodekey,3,&ckptcseg);
      pkey(&ckptstack,0,0);
      pkey(&journalpage,0,128);
      pkey(&ckptlog,0,0);
      dk0s(12);
#if diskless_kernel
    def_node(&ckptdom,0);
#else /* if NOT diskless kernel */
    def_node(&ckptdom,1);  
#endif
      nkey(domainkey,0,&ckptdom);
      nkey(meterkey,0,&ckptmeter);
      msckey(errormisckey,0);
      nkey(nodekey,5,&ckptmrt);
      dk0s(9);
      dk(1); /* busy */
      nkey(nodekey,0,&ckptgkn);
      nkey(nodekey,0,&ckptgrn);
    def_node(&ckptgkn,0);
      dk0s(7); 
      nkey(nodekey,0,&ckptparm);
      dk0s(8);
   def_statenode(ckptgrn,0,0x00100F80);

/* need SBTDOMCR, DCCDOMCR, DCC */

   def_node(&sbtdcmrt,0);
     nkey(nodekey,128+3,&dccseg);
     pkey(&sbtdcstack,0,0);
     dk0s(14);
   def_node(&sbtdcdom,1);
     nkey(nodekey,0,&sbtdcdom); /* inaccessable */
     nkey(meterkey,0,&sbmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&sbtdcmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&sbtdcgkn);
     nkey(nodekey,0,&sbtdcgrn);
   def_node(&sbtdcgkn,0);
     nkey(fetchkey,0,&dcccompnode);
     dk0s(2);
     nkey(domainkey,0,&sbtdcdom);
     dk0s(12);
   def_statenode(sbtdcgrn,1,0x00100F80);

   def_node(&dccdcmrt,0);
     nkey(nodekey,128+3,&dccseg);
     pkey(&dccdcstack,0,0);	 
     dk0s(14);

   def_node(&dccdcdom,1);
     nkey(nodekey,0,&dccdcdom); /* inaccessable brand */
     nkey(meterkey,0,&basicmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&dccdcmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&dccdcgkn);
     nkey(nodekey,0,&dccdcgrn);
   def_node(&dccdcgkn,0);
     nkey(fetchkey,0,&dcccompnode);
     dk0s(2);
     nkey(domainkey,0,&dccdcdom);
     dk0s(12);
   def_statenode(dccdcgrn,1,0x00100F80);

   def_node(&dccmrt,0);
     nkey(nodekey,128+3,&dccseg);
     pkey(&dccstack,0,0);
     dk0s(14);

   def_node(&dccdom,1);
     nkey(nodekey,0,&dccdom); /* inaccessable brand */
     nkey(meterkey,0,&basicmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&dccmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&dccgkn);
     nkey(nodekey,0,&dccgrn);
   def_node(&dccgkn,0);
     nkey(fetchkey,0,&dcccompnode);
     dk0s(2);
     nkey(domainkey,0,&dccdom);
     dk0s(2);
     nkey(startkey,0,&dccdcdom);
     dk0s(9);
   def_statenode(dccgrn,0,0x00100F80);

  /* the WOMBKEEPER !! */

   cmsfile(&wombkeepcode,15,"wkc",1);   /* WOMBKEEP code */
   genseg(&wombkeepcseg,15,&wombkeepcode);
   pages(&wombkeepstack,1,"");

   def_node(&wombkeepmrt,0);
     nkey(nodekey,3,&wombkeepcseg);  /* writes on its memory */
     pkey(&wombkeepstack,0,0);
//     pkey(&directorypage,0,0);
     dk(0);
     dk0s(13);

   def_node(&wombkeepdom,1);
     nkey(nodekey,0,&wombkeepdom); /* inaccessable brand */
     nkey(meterkey,0,&basicmeter);
     msckey(errormisckey,0);
     nkey(nodekey,5,&wombkeepmrt);
     dk0s(9);
     dk(1); /*busy*/
     nkey(nodekey,0,&wombkeepgkn);
     nkey(nodekey,0,&wombkeepgrn);
   def_node(&wombkeepgkn,0);
     nkey(nodekey,0,&rootnode);
     nkey(segmentkey,144,&psbnode);
     nkey(meterkey,0,&basicmeter);
     nkey(nodekey,0,&kidc);  /* first in AUXLIST */
     dk0s(8);
     nkey(nodekey,5,&wombkeepmrt);
     nkey(nodekey,0,&earlynode);
     nkey(nodekey,0,&firstobj);
     nkey(nodekey,0,&firstobjb);

   def_statenode(wombkeepgrn,0,0x00100F80);

/************************************************************************************

   Here follows the AUXLIST which is a series of nodes that describe the rest of the
   WOMB.  Additions to this list require changes to WKC.C to build the object.
   There are two kinds of objects described in this list.  AUXSUBR objects which
   are not factories (like KIDC), and MAKEFACT objects.   The AUXLIST description
   is the same for both objects.  NOTE that the chaining of AUXLIST nodes is
   explicit and must be maintained.
 
   The environment for factories and auxsubr objects has been unified.  The initial
   slots of AUXSUBR objects is the same as for factories.  This is the reason that
   the old KEY 4,5 has been changed to 8,9 so that slots 4,5 can contain a spacebank
   and meter.  

   The elf file can be either a single loadable segment (text and data) or
   two loadable segments (text and data).  When only one loadable segment
   is specified CFSTART is responsible for building the memory tree while
   the dual segment types use lsfsim to build the memory tree according to
   SPARC ABI standards (almost, the text starts at zero).  These objects
   are linked with CSTART.

   Some single segment objects have the symbol bootwomb defined in which case
   CFSTART does nothing to the memory tree.  These objects usually map the
   single segment as RW containing both text and data.

   CFSTART supports FORK() and CSTART support SBRK().

   stacksize is used by CFSTART of LSFSIM to determine the stack size which
   does not grow.

***********************************************************************************/

  cmsfile(&kidccode,8,"kidc",1);            /* read in a.out file            */
  genseg(&kidccseg,8,&kidccode);            /* define segment node           */
  def_node(&kidc,0);                        /* AUXNODE                       */
    nkey(segmentkey,192+3,&kidccseg);       /* read only code segment (a.out)*/
    dk(0);                                  /* symbol segment                */
    dk(0xAC<<16);                           /* start address (of LSFSIM)     */
    msckey(keybitsmisckey,0);               /* KEY 8 - KEYBITS               */
    dk(0);                                  /* key 9                         */
    dk(4096);                               /* stack size                    */
    dkstr("kidc");                          /* file name for checking in WKC */
    dk(0);                                  /* unused!!                      */
    dk0s(7);                                /* used by WKC                   */
    nkey(nodekey,0,&fcc);                   /* should be nkey(&nextnode)     */

  cmsfile(&fcccode,10,"fcc",1);             /* read in a.out file            */
  genseg(&fcccseg,10,&fcccode);             /* define segment node           */
  def_node(&fcc,0);                         /* AUXNODE                       */
    nkey(segmentkey,192+3,&fcccseg);        /* read only code segment (a.out)*/
    dk(0);                                  /* symbol segment                */
    dk(0xAC<<16);                           /* start address (of LSFSIM)     */
    dk(0);                                  /* key 8                         */
    nkey(fetchkey,0,&privnode);             /* KEY 9 - PRIVNODE for KIDC     */
    dk(4096);                               /* stack size                    */
    dkstr("fcc");                           /* file name for checking in WKC */
    dk(0);                                  /* unused!!                      */
    dk0s(7);                                /* used by WKC                   */
    nkey(nodekey,0,&fsc);                   /* should be nkey(&nextnode)     */

  cmsfile(&fsccode,9,"fsc",1);              /* read in a.out file */
  genseg(&fsccseg,9,&fsccode);              /* define segment node           */
  def_node(&fsc,0);                         /* AUXNODE                       */
    nkey(segmentkey,192+3,&fsccseg);        /* read only code segment (a.out)*/
    dk(0);                                  /* symbol segment                */
    dk(0xAC<<16);                           /* start address (of LSFSIM)     */
    msckey(keybitsmisckey,0);               /* KEY 8 - KEYBITS               */
    dk(0);                                  /* key 9                         */
    dk(4096);                               /* stack size                    */
    dkstr("fsc");                           /* file name for checking in WKC */
    dk(0);                                  /* unused!!                      */
    dk0s(7);                                /* used by WKC                   */
    nkey(nodekey,0,&snode2);                /* should be nkey(&nextnode)     */

  cmsfile(&snode2code,8,"snode2",1);       /* read in a.out file */
  genseg(&snode2cseg,8,&snode2code);       /* define segment node           */
  def_node(&snode2,0);                     /* AUXNODE                       */
    nkey(segmentkey,192+3,&snode2cseg);    /* read only code segment (a.out)*/
    dk(0);                                 /* symbol segment                */
    dk(0xAC<<16);                          /* start address (of LSFSIM)     */
    dk(0);                                 /* key 8                         */
    dk(0);                                 /* key 9                         */
    dk(4096);                              /* stack size                    */
    dkstr("snode2");                       /* file name for checking in WKC */
    dk(0);                                 /* unused!!                      */
    dk0s(7);                               /* used by WKC                   */
    nkey(nodekey,0,&tdo);                  /* should be nkey(&nextnode)     */

  cmsfile(&tdocode,9,"tdo",1);             /* read in a.out file */
  genseg(&tdocseg,9,&tdocode);             /* define segment node           */
  def_node(&tdo,0);                        /* AUXNODE                       */
    nkey(segmentkey,192+3,&tdocseg);       /* read only code segment (a.out)*/
    dk(0);                                 /* symbol segment                */
    dk(0xAC<<16);                          /* start address (of LSFSIM)     */
    dk(0);                                 /* key 8                         */
    dk(0);                                 /* key 9                         */
    dk(4096);                              /* stack size                    */
    dkstr("tdo");                          /* file name for checking in WKC */
    dk(0);                                 /* unused!!                      */
    dk0s(7);                               /* used by WKC                   */
    nkey(nodekey,0,&pcs);                  /* should be nkey(&nextnode)     */

  cmsfile(&pcscode,40,"pcs",1); /* read in a.out file */
  genseg256(&pcscseg,40,&pcscode);            /* define segment node           */
  def_node(&pcs,0);                       /* AUXNODE                       */
    nkey(segmentkey,192+4,&pcscseg);      /* read only code segment (a.out)*/
    dk(0);                                /* symbol segment                */
    dk(0xAC<<16);                         /* start address (of LSFSIM)     */
    dk(0);                                /* key 8                         */
    dk(0);                                /* key 9                         */
    dk(4096);                             /* stack size                    */
    dkstr("pcs");                         /* file name for checking in WKC */
    dk(0);                                /* unused!!                      */
    dk0s(7);                              /* used by WKC                   */
    dk(0);                                /* should be nkey(&nextnode)     */


/*****************************************************************************

  End of the initial item space definitions.  REVIEW() checks to see that 
  all of the declared symbols have been defined.

*****************************************************************************/
    review();

#endif /* ifndef restart_kernel */
  prim_infop = (prim_info_t *)malloc(sizeof(prim_info_t));

  prim_infop->nodecnt = curr_node_cda;
  prim_infop->plistcnt = curr_plist;
  return prim_infop;
}
