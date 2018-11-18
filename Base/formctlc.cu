/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "kktypes.h"
#include "cvt.h"
#include "scsih.h"
#include "scd.h"
#include "formsubh.h"
#include "formsu2h.h"
#include "memomdh.h" /* for first_physdev_win */
#include "sysgenh.h"
#include "dskiomdh.h"
#include "consmdh.h"
#include <stdio.h>

static char getecho(void)
{
   extern char consgetchar(void);
   extern void consmayput(char);
   char c = consgetchar();
   consmayput(c);
   consprint("\n");
   return c;
}

bool doingdiskformat(void)
{  return TRUE;}

static volatile bool req_done;
   /* volatile because shared with interrupt level */
static void req_doneproc(
   struct scsi_cmd *scp)
{
   req_done = TRUE;
}

/* Variables shared by formatting routines. */
/* Could be static. */
int f_targetid; /* in binary */
unsigned long f_blkoffset;
          /* absolute block number of first block in partition */
unsigned long f_nblk; /* number of blocks in partition */
unsigned long f_currentpage;

void formmsg(const char *m)
{  consprint(m);}

static const unsigned char zeropage[4096] = {0};
static unsigned int progresscounter = 0;
int formwrt(
   enum WriteType code,
   const void *buf)
{
   unsigned long dskaddr;
   struct scsi_cmd sc = {NULL, 00, DataOut,
         NULL, 4096, req_doneproc,
         {WDCMD, 0,0,00, 8, 0}
         };

   if (f_currentpage*8+8 > f_nblk)
      crash("formatting past end of partition");
   switch (code) {
    case NODATA:
      break; /* no write, just bump currentpage. */
    case ZERO:
      buf = zeropage; /* and fall into */
    case WRDATA:
      /* Write the page. */
      if (++progresscounter >= 20) { /* every 20th write */
         progresscounter = 0;
         consprint("."); /* Progress indicator */
      }
      req_done = FALSE;
      sc.id = 1<<f_targetid;
      sc.buf = (unsigned char *)buf;
      dskaddr = f_currentpage*8 + f_blkoffset; /* in blocks */
      sc.cmdb.logblk1 = dskaddr & 0xff;
      sc.cmdb.logblk0 = (dskaddr>>8) & 0xff;
      sc.cmdb.log_unit = (dskaddr>>16) & 0xff;
      start_scsi_cmd1(&sc);
      while (!req_done) ; /* spin waiting for completion */
      if (scsi_rc != SCSI_RC_OK
          || !got_status
          || scsi_status != SCSI_ST_GOOD ) {
         /* some problem */
         consprint("DSKS004 Error writing format\n");
         return 1;
      }
   }
   f_currentpage++;
   return 0;
}

void formclosedev(void) {}

   /* This format is fixed for a disk of 20,000 pages. */

static struct packstring packstr1 =
         {"PACK001","KEYKOSX", CKPTHDR1};
static struct packstring packstr2 =
         {"PACK002","KEYKOSX", CKPTHDR2};
static struct rangeinfo rng1 = {PDRDNORMAL, {0x80,0,0,0,0,1}, 570, 2};
              /* Primordial nodes */
static struct rangeinfo rng2 = {PDRDNORMAL, {   0,0,0,0,0,1}, 2000, 2};
              /* Primordial pages */
static struct rangeinfo rng3 = {PDRDNORMAL, {0x80,0,0,0,0x10,00}, 38000, 2};
              /* Normal nodes */
static struct rangeinfo rng4a =    /* Swap range */
         {PDRDSWAPAREA1, {0,0,0,0,0,1}, 8702, 1};
static struct rangeinfo rng4b =    /* Swap range */
         {PDRDSWAPAREA1, {0,0,0,0,0x87,0x34}, 8702, 1};
static struct rangeinfo rng5a =    /* Swap range */
         {PDRDSWAPAREA2, {0,0,0,0,0x43,0x9a}, 8702, 1};
static struct rangeinfo rng5b =    /* Swap range */
         {PDRDSWAPAREA2, {0,0,0,0,0xca,0xcd}, 8702, 1};
static struct rangeinfo rng6 = {PDRDNORMAL, {   0,0,0,0,0x10,00}, 4095, 2};
              /* Normal pages */
static struct rangeinfo rng7 = {PDRDNORMAL, {0x7f,0xff,0,0,0,0}, 64, 2};
              /* pixel buffer pages */
static struct rangeinfo rng8a =    /* Swap range */
         {PDRDSWAPAREA1, {0,0,0,0,0,1}, 17305, 1};
static struct rangeinfo rng8b =    /* Swap range */
         {PDRDSWAPAREA1, {0,0,0,0,0x87,0x34}, 17305, 1};
static struct rangeinfo rng9a =    /* Swap range */
         {PDRDSWAPAREA2, {0,0,0,0,0x43,0x9a}, 17306, 1};
static struct rangeinfo rng9b =    /* Swap range */
         {PDRDSWAPAREA2, {0,0,0,0,0xca,0xcd}, 17306, 1};
static struct rangeinfo rng10 = {PDRDNORMAL, {   0,0,0,0,0x1f,0xff}, 4095, 2};
              /* Normal pages */
static struct rangeinfo rng11 = {PDRDNORMAL, {   0,0,0,0,0x2f,0xfe}, 4095, 2};
              /* Normal pages */

static unsigned char workarea[16000];
void format_partition(
   int xtargetid, /* in binary */
   int formattype,  /* 1, 2, 3, or 4 */
   unsigned long xblkoffset,
          /* absolute block number of first block in partition */
   unsigned long xnblk) /* number of blocks in partition */
{
   unsigned long worksize = forminit();

   if (worksize > sizeof(workarea)) crash("Format workarea too small");
   f_targetid = xtargetid; /* Communicate variables to other routines */
   f_blkoffset = xblkoffset;
   f_nblk = xnblk;
   f_currentpage = 0; /* Start at the beginning */

   switch (formattype) {
    case 1:
      formdev(workarea, &packstr1);
      if (formrng(workarea, &rng1)) crash("rng1");
      if (formrng(workarea, &rng2)) crash("rng2");
      if (formrng(workarea, &rng3)) crash("rng3");
      if (formrng(workarea, &rng4a)) crash("rng4");
      if (formrng(workarea, &rng5a)) crash("rng5");
      if (formrng(workarea, &rng6)) crash("rng6");
      if (formrng(workarea, &rng7)) crash("rng7");
      break;
    case 2:
      formdev(workarea, &packstr2);
      if (formrng(workarea, &rng1)) crash("rng1");
      if (formrng(workarea, &rng2)) crash("rng2");
      if (formrng(workarea, &rng3)) crash("rng3");
      if (formrng(workarea, &rng4b)) crash("rng4");
      if (formrng(workarea, &rng5b)) crash("rng5");
      if (formrng(workarea, &rng6)) crash("rng6");
      if (formrng(workarea, &rng7)) crash("rng7");
      break;
    case 3:
      formdev(workarea, &packstr1);
      if (formrng(workarea, &rng1)) crash("rng1");
      if (formrng(workarea, &rng2)) crash("rng2");
      if (formrng(workarea, &rng3)) crash("rng3");
      if (formrng(workarea, &rng8a)) crash("rng8");
      if (formrng(workarea, &rng9a)) crash("rng9");
      if (formrng(workarea, &rng6)) crash("rng6");
      if (formrng(workarea, &rng10)) crash("rng10");
      if (formrng(workarea, &rng11)) crash("rng11");
      if (formrng(workarea, &rng7)) crash("rng7");
      break;
    case 4:
      formdev(workarea, &packstr2);
      if (formrng(workarea, &rng1)) crash("rng1");
      if (formrng(workarea, &rng2)) crash("rng2");
      if (formrng(workarea, &rng3)) crash("rng3");
      if (formrng(workarea, &rng8b)) crash("rng8");
      if (formrng(workarea, &rng9b)) crash("rng9");
      if (formrng(workarea, &rng6)) crash("rng6");
      if (formrng(workarea, &rng10)) crash("rng10");
      if (formrng(workarea, &rng11)) crash("rng11");
      if (formrng(workarea, &rng7)) crash("rng7");
      break;
   }
   if (formfmt(workarea)) crash("formfmt");
}

struct scd_dk_label disk_label;
bool read_disk_label(int targetid)
/* Read the label of a disk. */
{
   struct scsi_cmd readsc = {NULL, 00, DataIn,
         (unsigned char *)&disk_label, sizeof(disk_label), req_doneproc,
         {RDCMD, 0,0,0, 1, 0}
         };

   /* Read the disk label. */
   req_done = FALSE;
   readsc.id = 1<<targetid;
   start_scsi_cmd1(&readsc);
   while (!req_done) ; /* spin waiting for completion */
   if (scsi_rc != SCSI_RC_OK
       || !got_status
       || scsi_status != SCSI_ST_GOOD
       || disk_label.dkl_magic != DKL_MAGIC ) {
      /* some problem */
      consprint("FMTCTL001 Error reading block 0\n");
      return FALSE;
   }
   return TRUE;
}

unsigned char superblock[8192];
bool read_superblock(
   int targetid,
   int partno)
{
   unsigned long dskaddr;
   struct scsi_cmd readsc = {NULL, 00, DataIn,
         superblock, sizeof(superblock), req_doneproc,
         {RDCMD, 0,0,00, sizeof(superblock)/512, 0}
         };

   if (disk_label.dkl_map[partno].dkl_nblk < 204856) {
      consprint("FMTCTL002 That partition is too small.\n\
The partition must have at least 204856 blocks.\n");
      return FALSE;
   }
   /* Read the superblock. */
   req_done = FALSE;
   readsc.id = 1<<targetid;
   dskaddr = 16 + disk_label.dkl_map[partno].dkl_blkno;
   readsc.cmdb.logblk1 = dskaddr & 0xff;
   readsc.cmdb.logblk0 = (dskaddr>>8) & 0xff;
   readsc.cmdb.log_unit = (dskaddr>>16) & 0xff;
   start_scsi_cmd1(&readsc);
   while (!req_done) ; /* spin waiting for completion */
   if (scsi_rc != SCSI_RC_OK
       || !got_status
       || scsi_status != SCSI_ST_GOOD ) {
      /* some problem */
      consprint("DSKS002 Error reading superblock\n");
      return FALSE;
   }
   return TRUE;
}

bool confirm_unix(void)
{
   char c;
   consprint("Are you SURE you want to erase that Unix file system? ");
   c = getecho();
   switch (c) {
    case 'y':
    case 'Y':
      return TRUE;
    default:
      return FALSE;
   }
}

void tryformatting(void)
{
   int targetid, partno, packid;
   char c;

   for (;;) {   /* Loop formatting disks */
      consprint("Type scsi id of disk to format, or N to exit: ");
      c = getecho();
      switch (c) {
       default: consprint("Invalid input.\n");
         break;
       case 'n':
       case 'N':
         consprint("You should now do a big bang.\n");
         return;
       case '0':
       case '1':
       case '2':
       case '3':
       case '4':
       case '5':
       case '6':
         targetid = c-'0';
         if (!read_disk_label(targetid)) continue;
         consprint("Type partition number to format: ");
         c = getecho();
         switch (c) {
          default: consprint("Partition must be from 0 to 6.\n");
            break;
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
            partno = c-'0';
            if (read_superblock(targetid, partno)) {
               /* Check the magic number. */
               long magic = *(long *)(superblock+PDR_MAGIC_OFFSET);
               if (magic != 0x11954 /* Unix file system */
                   || confirm_unix()) {
                  consprint("Type pack id (1 and 2 are a small pair,\n"
                     "3 and 4 are a big pair (minimum 400000 blocks)): ");
                  c = getecho();
                  switch (c) {
                   default: consprint("Invalid input.\n");
                     break;
                   case '1':
                   case '2':
                   case '3':
                   case '4':
                     packid = c-'0';
                     {char str[80];
 sprintf(str,"Format scsi id %d, partition %d to be packid %d? Confirm Y/N: ",
                              targetid, partno, packid);
                      consprint(str);}
                     c = getecho();
                     switch (c) {
                      default: break;
                      case 'y':
                      case 'Y':
                        consprint("Formatting ...");
                        format_partition(targetid,
                                   packid,
                                   disk_label.dkl_map[partno].dkl_blkno,
                                   disk_label.dkl_map[partno].dkl_nblk );
                        consprint("\n");
                     } /* end switch on confirmation */
                  } /* end switch on packid */
               }
            }
         } /* end of switch on partition number */
      }  /* end of switch on scsi id */
   } /* end of loop formatting disks */
}

