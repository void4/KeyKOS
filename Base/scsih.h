/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_scsih
#define _H_scsih
/* scsih.h */

#define HOSTID 7    /* our SCSI id */

struct cmdb0 {   /* group 0 */
   unsigned char command,
      log_unit,
      logblk0,
      logblk1,
      translen,
      ctlbyte;
};
struct cmdb1 {   /* group 1 */
   unsigned char command,
      log_unit,
      logblk0,
      logblk1,
      logblk2,
      logblk3,
      reserved,
      translen0,
      translen1,
      ctlbyte;
};
enum DataMode {
   DataIn,
   DataOut,
   NoData
};
struct scsi_cmd {
   struct scsi_cmd *link;
   int id;     /* target id (bit mask) */
   enum DataMode mode;  /* Determines phase after command */
   unsigned char *buf;
   unsigned int size;  /* size of data in bytes */
   void (*doneproc)(struct scsi_cmd *);
          /* routine to call at interrupt level when done */
   struct cmdb0 cmdb;
};
struct scsi_cmd1 {
   struct scsi_cmd *link;
   int id;     /* target id (bit mask) */
   enum DataMode mode;  /* Determines phase after command */
   unsigned char *buf;
   unsigned int size;  /* size of data in bytes */
   void (*doneproc)(struct scsi_cmd *);
          /* routine to call at interrupt level when done */
   struct cmdb1 cmdb;
};
extern enum scsi_rc_enum {
   SCSI_RC_OK,
   SCSI_RC_TIMEOUT
   } scsi_rc;  /* completion code : */

extern unsigned char scsi_status; /* status returned */
extern unsigned char scsi_msgbyte;  /* message returned */
extern bool got_data, got_status, got_msgin;
void start_scsi_cmd0(struct scsi_cmd *);
void start_scsi_cmd1(struct scsi_cmd *);
void start_scsi_cmd2(struct scsi_cmd *);

/*
 *      For Group0's command definitions.
 */
#define TESTCMD         0x0
#define RZERO           0x1
#define REQSENSE        0x3
#define CMDFMT          0x4
#define CHKFMT          0x5
#define FMTTRK          0x6
#define REASIGN         0x7
#define RDCMD           0x8
#define WDCMD           0xa
#define SEEKCMD         0xb
#define ASIGNALT        0xe
#define INQUIRY         0x12
#define MODE_SEL        0x15
#define ASSIGN          0xc2
/*
 *      Group 1 command definitions.
 */
#define READCAPACITY    0x25

/* status */
#define SCSI_ST_GOOD            0x00    /* good */
#define SCSI_ST_CHECK_CND       0x02    /* check condition */
#define SCSI_ST_MET_GD          0x04    /* condition met/good */
#define SCSI_ST_BUSY            0x08    /* busy */
#define SCSI_ST_INT_GD          0x10    /* intermediate good */
#define SCSI_ST_INT_C_GD        0x14    /* intermediate/condition met/good */
#define SCSI_ST_CONFLICT        0x18    /* reservation conflict */
#endif

