/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "scd.h"
#include "pdrh.h"
#include "memomdh.h" /* for first_physdev_win */
#include "sysgenh.h"
#include "dskiomdh.h"
#include "gintdskh.h"
#include "sparc_mem.h"
#include "misc.h"
#include "buf.h"
#include "param.h"
#include "devmdh.h"
// #include <string.h>
#include <stdio.h>
#include "consmdh.h"
#include "memutil.h"

extern struct scsi_pkt * esp_scsi_init_pkt();

static volatile bool req_done;
   /* volatile because shared with interrupt level */
static void req_doneproc(
   struct scsi_pkt *scp)
{
   req_done = TRUE;
}

struct scd_dk_label disk_label;
unsigned char superblock[8192]; /* and, hopefully, PDR */


void dskinit(int targetid)
/* Check out a disk. */
{
   struct scsi_pkt *sp;
   PHYSDEV *pdev;
   caddr_t buffer;
   unsigned long i;
   struct buf *bp;
   struct scsi_address scsi_addr;
   char buf[80];

   /* Try to allocate a PHYSDEV to this disk. */
   if (physdevlast >= physdevs + MAXPHYSDEVS){
      consprint("Not enough physdevs");
      return;
   }

   /* Initialize the PHYSDEV. */
   pdev = physdevlast++;
   pdev->ioqlast = pdev->ioqfirst = (DEVREQ *)pdev;
   pdev->lastaddress = 0;
   pdev->enqstate = DEVSTART;
   pdev->windownum = first_physdev_win + (pdev-physdevs);
   pdev->pd_reqsense_windownum = first_reqsense_win + (pdev-physdevs);
   pdev->pd_reqsense_page = first_reqsense_page + (pdev-physdevs);
   pdev->kertask.kernel_task_function = disk_kertask_function;

   /* Initialize scsi_pkt */
   sp = (struct scsi_pkt *)&pdev->pd_saved_scsi_pkt;
   sp->pkt_address.a_target = targetid;
   sp->pkt_scbp = (u_char *)&pdev->pd_scsi_scb[0];
   sp->pkt_cdbp = (u_char *)&pdev->pd_scsi_cdb;
   sp->pkt_bufp = &pdev->pd_scsi_buf;
   sp->pkt_private = (opaque_t)pdev;

   pdev->pd_scsi_addr.a_target = (u_short) targetid;
   pdev->pd_scsi_addr.a_hba_tran = 0;
   pdev->pd_scsi_addr.a_lun = 0;
   pdev->pd_scsi_addr.a_sublun = 0;

#if LATER
   /* Initialize request sense scsi_pkt */
   reqsp = (struct scsi_pkt *)&pdev->pd_reqsense_pkt;
   reqsp->pkt_address.a_target = targetid;
   reqsp->pkt_scbp = (u_char *)&pdev->pd_reqsense_scb[0];
   reqsp->pkt_cdbp = (u_char *)&pdev->pd_reqsense_cdb;
   reqsp->pkt_bufp = &pdev->pd_reqsense_buf;
   makecmd_g0(reqsp, SCMD_REQUEST_SENSE, 0, SENSE_LENGTH);
#endif
   pdev->pd_reqsense_buf.b_un.b_addr = (caddr_t)map_uncached_window(
	pdev->pd_reqsense_windownum, (pdev->pd_reqsense_page)>>4, MAP_WINDOW_RW);
   pdev->pd_reqsense_buf.b_bcount = SENSE_LENGTH;
   pdev->pd_reqsense_buf.b_flags = B_READ;
 
   /***************************************************************************
    *							    		      *
    * Do a Read Capacity - so we can make sure the disk block size is 512.    *
    *									      *
    ***************************************************************************/
        scsi_addr.a_target = (u_short) targetid;
        scsi_addr.a_hba_tran = 0;
        scsi_addr.a_lun = 0;
        scsi_addr.a_sublun = 0;
   req_done = FALSE;
   pdev->pd_scsi_buf.b_un.b_addr = (caddr_t)map_uncached_window(pdev->windownum, 
	disk_init_page>>4, MAP_WINDOW_RW);
   
   pdev->pd_scsi_buf.b_bcount = sizeof (struct scsi_capacity);
   pdev->pd_scsi_buf.b_flags = B_READ;
   sp = esp_scsi_init_pkt (&scsi_addr, (struct scsi_pkt *) NULL, 
	&pdev->pd_scsi_buf, CDB_GROUP1, 1, 8, PKT_CONSISTENT);
   *((caddr_t *)&buffer) = sp->pkt_bufp->b_un.b_addr;
   sp->pkt_comp = req_doneproc;
   sp->pkt_flags = FLAG_NOINTR | FLAG_NODISCON | FLAG_NOPARITY;

   sp->pkt_private = (opaque_t)pdev;
   makecmd_g1(sp, SCMD_READ_CAPACITY, 0, 0);
   if (scsi_transport(sp) != 1/*TRAN_ACCEPT*/){
      crash("Error on transport request to read capacity");
   }
   for (i =0; i< 1000; i++) {
	if (req_done)
		break;
   }
   if ((i>=1000) && (!req_done))
	Panic (); // no interrupt from SCSI

   while (!req_done) ;		/* spin waiting for completion */
   if (sp->pkt_reason != 0/*CMD_CMPLT*/
       || !(sp->pkt_state & 16/*STATE_GOT_STATUS*/)
       || ((struct scsi_status *)sp->pkt_scbp)->sts_busy
       || ((struct scsi_status *)sp->pkt_scbp)->sts_chk
       || sp->pkt_resid != 0 /* some residual count */ ) {
      /* some problem */
      crash("DSKS001 Error on read capacity");
          /* Crash so I can diagnose the problem! */
      return;
   }
   i = ((struct scsi_capacity *)buffer)->lbasize;
   if (i != 512){
      physdevlast--;
      consprint("DSKS003 Block length not 512");
      return;
   }
	
   /***************************************************************************
    *							    		      *
    * Read the disk label - It's the first 512 byte of the disk.              *
    *									      *
    ***************************************************************************/
   /* Read the disk label. */
   req_done = FALSE;
   pdev->pd_scsi_buf.b_bcount = sizeof (struct scd_dk_label);
   pdev->pd_scsi_buf.b_flags = B_READ;
   sp = esp_scsi_init_pkt (&scsi_addr, (struct scsi_pkt *) NULL, 
	&pdev->pd_scsi_buf, 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
   sp->pkt_comp = req_doneproc;
   sp->pkt_flags = FLAG_NODISCON | FLAG_NOINTR;
   sp->pkt_private = (opaque_t)pdev;
   makecmd_g1(sp, 40/*SCMD_READ_G1*/, 0, (sizeof(struct scd_dk_label))/512);
   start_scsi_cmd_sense(sp);
   while (!req_done) ; /* spin waiting for completion */
   if (sp->pkt_reason != 0/*CMD_CMPLT*/
       || !(sp->pkt_state & 16/*STATE_GOT_STATUS*/)
       || ((struct scsi_status *)sp->pkt_scbp)->sts_busy
       || ((struct scsi_status *)sp->pkt_scbp)->sts_chk
       || sp->pkt_resid != 0 /* some residual count */ ) {
      /* some problem */
      physdevlast--;
      consprint("DSKS001 Error reading block 0");
      return;
   }
   bp = (struct buf *)sp->pkt_bufp; 
   Memcpy((char *)&disk_label, bp->b_un.b_addr,
		 sizeof(struct scd_dk_label)); 
   if (disk_label.dkl_magic != DKL_MAGIC ) {
      physdevlast--;
      consprint("DSKS001 Error magic of block 0");
      return;
   }

   sprintf(buf, "Scan the partitions of disk target_id = %d:\n", targetid);
   consprint(buf);
   /* ... init pdev->type and packid */
   /* Scan the logical partitions. */
   for (i=0; i<NLPART; i++) {
      /******************************************
       *                                        *
       * For each disk partition:		*
       *					*
       ******************************************/
      if (disk_label.dkl_map[i].dkl_nblk > 32) {
         /* It is big enough to be of interest. */
         unsigned long dskaddr, sec_idx;

	 /***************************************
	  *					*
          * Read the 1st page of superblock -   *
	  * 	page 3 of this partition.	*
	  *					*
	  ***************************************/
         req_done = FALSE;
   	 pdev->pd_scsi_buf.b_bcount = PAGESIZE;
   	 pdev->pd_scsi_buf.b_flags = B_READ;
   	 sp = esp_scsi_init_pkt (&scsi_addr, (struct scsi_pkt *) NULL, 
		&pdev->pd_scsi_buf , 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
   	 sp->pkt_comp = req_doneproc;
   	 sp->pkt_flags = FLAG_NODISCON | FLAG_NOINTR;
   	 sp->pkt_private = (opaque_t)pdev;

	 /* The disk_label stores "cylinder" number in dkl_blkno,
	    to get the real block number, one has to multiply 
	    tracks/cylinder and sectors/track to the cylinder number,
	    that is, dkl_nhead*dkl_nsect*dkl_blkno. */

   	 sec_idx = disk_label.dkl_map[i].dkl_blkno*
		disk_label.dkl_nhead*disk_label.dkl_nsect;
   	 makecmd_g1(sp, 40/*SCMD_READ_G1*/, 16+sec_idx, PAGESIZE/512);
         start_scsi_cmd_sense(sp);
   	 while (!req_done) ; /* spin waiting for completion */
   	 if (sp->pkt_reason != 0/*CMD_CMPLT*/
       	    || !(sp->pkt_state & 16/*STATE_GOT_STATUS*/)
            || ((struct scsi_status *)sp->pkt_scbp)->sts_busy
            || ((struct scsi_status *)sp->pkt_scbp)->sts_chk
            || sp->pkt_resid != 0 /* some residual count */ ) {
            /* some problem */
            consprint("DSKS002 Error reading superblock1");
            return;
         }
         /* Check the magic number. */
         dskaddr = *(long *)(pdev->pd_scsi_buf.b_un.b_addr+PDR_MAGIC_OFFSET);
         if (dskaddr == 0x11954){ /* Unix file system */
            sprintf(buf, "partition %d contains Unix file system\n", (u_int)i);
            consprint(buf);
         } else if (dskaddr == PDR_MAGIC) {
	    /************************************
	     *					*
             * Read the 2nd page of superblock -*
	     * 	page 4 of this partition.	*
	     *					*
	     ************************************/
            req_done = FALSE;
   	    pdev->pd_scsi_buf.b_bcount = PAGESIZE;
   	    pdev->pd_scsi_buf.b_flags = B_READ;
            sp = esp_scsi_init_pkt (&scsi_addr, (struct scsi_pkt *) NULL, 
	     		&pdev->pd_scsi_buf , 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
   	    sp->pkt_comp = req_doneproc;
   	    sp->pkt_flags = FLAG_NODISCON | FLAG_NOINTR;
   	    sp->pkt_private = (opaque_t)pdev;
   	    makecmd_g1(sp, 40/*SCMD_READ_G1*/, 24+sec_idx, PAGESIZE/512);
    	    start_scsi_cmd_sense(sp);
   	    while (!req_done) ; /* spin waiting for completion */
   	    if (sp->pkt_reason != 0/*CMD_CMPLT*/
       	       || !(sp->pkt_state & 16/*STATE_GOT_STATUS*/)
               || ((struct scsi_status *)sp->pkt_scbp)->sts_busy
               || ((struct scsi_status *)sp->pkt_scbp)->sts_chk
               || sp->pkt_resid != 0 /* some residual count */ ) {
               /* some problem */
               consprint("DSKS002 Error reading superblock3");
               return;
            }
            pdev->lastaddress = 24+sec_idx;
            /* Try to mount as a KeyKOS disk. */
            sprintf(buf, "partition %d mounted as keykos disk\n", (u_int)i);
            consprint(buf);
            if (!gdddovv(pdev,
                    sec_idx,
                    disk_label.dkl_map[i].dkl_nblk,
                    (PDR *)(pdev->pd_scsi_buf.b_un.b_addr))
               ) crash("pack rejected\n");
         } else {
            sprintf(buf, "partition %d unknown disk type\n", (u_int)i);
            consprint(buf);
	 }
      }
   }
   
} /* end of dskinit */
