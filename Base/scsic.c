/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "devmdh.h"
#include "stddef.h"
#include "sysdefs.h"
#include "memutil.h"

extern struct scsi_pkt * esp_scsi_init_pkt();

void makecmd_g0(struct scsi_pkt *pktp, u_char cmd, unsigned long addr, unsigned long cnt){

    ((union scsi_cdb *)(pktp)->pkt_cdbp)->scc_cmd = (cmd);
//  FORMG0ADDR(((union scsi_cdb *)(pktp)->pkt_cdbp), (addr));
    {union scsi_cdb * cdb = (union scsi_cdb *)pktp->pkt_cdbp;
       cdb->cdb_un.tag = addr >> 16;
       cdb->cdb_un.sg.g0.addr1 = (addr >> 8) & 255;
       cdb->cdb_un.sg.g0.addr0 = addr & 255;}
//  FORMG0COUNT(((union scsi_cdb *)(pktp)->pkt_cdbp), (cnt));
    ((union scsi_cdb *)(pktp)->pkt_cdbp)->cdb_un.sg.g0.count0  = cnt;
}

void makecmd_g1(struct scsi_pkt *pktp, u_char cmd, unsigned long addr, unsigned long cnt){

    ((union scsi_cdb *)(pktp)->pkt_cdbp)->scc_cmd = (cmd);
//  FORMG1ADDR(((union scsi_cdb *)(pktp)->pkt_cdbp), (addr));
    {union scsi_cdb * cdb = (union scsi_cdb *)pktp->pkt_cdbp;
                cdb->cdb_un.sg.g1.addr3  = addr >> 24;
				cdb->cdb_un.sg.g1.addr2  = (addr >> 16) & 0xFF;
				cdb->cdb_un.sg.g1.addr1  = (addr >> 8) & 0xFF;
				cdb->cdb_un.sg.g1.addr0  = addr & 0xFF;}
//  FORMG1COUNT(((union scsi_cdb *)(pktp)->pkt_cdbp), (cnt));
     {union scsi_cdb * cdb = (union scsi_cdb *)pktp->pkt_cdbp;
        cdb->cdb_un.sg.g1.count1 = cnt >> 8; 
        cdb->cdb_un.sg.g1.count0 = cnt & 0xFF;}
}

/*
 * Completion function called when a auto sense request is finished.
 */
void reqsense_completion_proc(struct scsi_pkt *sense_sp){

   PHYSDEV *pdev;
   struct scsi_pkt *sp;

   /* Calculate the corresponding PHYSDEV */
   pdev = (PHYSDEV *)(sense_sp->pkt_private);

   sp = &pdev->pd_saved_scsi_pkt;

   /* Is it ok now? */
   if (sense_sp->pkt_reason == 0/*CMD_CMPLT*/
       && (sense_sp->pkt_state & 16/*STATE_GOT_STATUS*/)
       && !(((struct scsi_status *)sense_sp->pkt_scbp)->sts_chk)
       && !(((struct scsi_status *)sense_sp->pkt_scbp)->sts_busy)){
          ((struct scsi_status *)sp->pkt_scbp)->sts_chk = 0;
          ((struct scsi_status *)sp->pkt_scbp)->sts_busy = 0;
   }

   /* turn off the sense bit */
   ((struct scsi_status *)sp->pkt_scbp)->sts_chk = 0;
   /* Call the user specified completion function */
   sp->pkt_comp = pdev->pd_saved_completion_proc;
   (*sp->pkt_comp)(sp);
}

/*
 * Completion function called when is the 1st scsi command returns. 
 */
void step_completion_proc(struct scsi_pkt *sp){

   PHYSDEV *pdev;
   struct scsi_pkt *ssp;

   /* Calculate the corresponding PHYSDEV */
   pdev = (PHYSDEV *)sp->pkt_private;

   if ((sp->pkt_reason == 0/*CMD_CMPLT*/)
       && (sp->pkt_state & 16/*STATE_GOT_STATUS*/)
       && (((struct scsi_status *)sp->pkt_scbp)->sts_chk)){

       /* save the old scsi_pkt */
       ssp = &pdev->pd_saved_scsi_pkt;
       ssp->pkt_reason = sp->pkt_reason;
       ssp->pkt_state = sp->pkt_reason;
       Memcpy(ssp->pkt_scbp, sp->pkt_scbp, 4);
       Memcpy(ssp->pkt_cdbp, sp->pkt_cdbp, sizeof(union scsi_cdb));

       /* Request sense command */
       sp = esp_scsi_init_pkt(&pdev->pd_scsi_addr, (struct scsi_pkt *)NULL,
		&pdev->pd_reqsense_buf, 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
       sp->pkt_flags = FLAG_NODISCON;
       sp->pkt_comp = reqsense_completion_proc;
       sp->pkt_private = (opaque_t)pdev;
       makecmd_g0(sp, 3/*SCMD_REQUEST_SENSE*/, 0, SENSE_LENGTH);
       if (scsi_transport(sp) != 1/*TRAN_ACCEPT*/){
         crash("Error on transport request to read capacity");
       }

   } else {
        /* No sense request needed, just call user specified
           completion function. */
	sp->pkt_comp = pdev->pd_saved_completion_proc;
	(*sp->pkt_comp)(sp);
   }
}



/* 
 * Start a scsi command specified by sp.
 * Should a check condition is needed,
 * do a sense request in the completion function.
 */
void start_scsi_cmd_sense(struct scsi_pkt *sp){

   PHYSDEV *pdev;

   /* Calculate the corresponding PHYSDEV */
   pdev = (PHYSDEV *)sp->pkt_private;
   /* Save the original completion function */
   pdev->pd_saved_completion_proc = sp->pkt_comp;
   sp->pkt_comp = step_completion_proc;

   /* start the prepared scsi command */
   if (scsi_transport(sp) != 1/*TRAN_ACCEPT*/){
      crash("Error on scsi transport request (with sense check)");
   }  
}

int esp_start(adrPak *, struct scsi_pkt *); // temporary prototype
int scsi_transport(struct scsi_pkt *sp){

        sp->pkt_flags |= FLAG_NODISCON;
	return ((int) esp_start(&(sp->pkt_address), sp));
}
