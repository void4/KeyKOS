/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

// This is a bloody stump that replaces a (c) file by Sun.
// It was a very short front end to <scsi/scsi.h> with a non-standard pragma.
// I put declarations here that seem to satisfy references to scsi like things
// occurring in files that included the old scsi.h .
#define SENSE_LENGTH 48
// number above pulled out of hat. Was Sun specific. .....
struct scsi_capacity {int lbasize;}; // Really imaginary
struct scsi_address {unsigned short int a_target;
   int a_hba_tran; int a_lun; int a_sublun;};
typedef struct {int a_target;} adrPak;
#define CDB_GROUP1 42 // out of thin air, Sun specific
#define PKT_CONSISTENT 2 // out of thin air, Sun specific
#define FLAG_NOINTR 2 // out of thin air, Sun specific
#define FLAG_NODISCON 2 // out of thin air, Sun specific
#define FLAG_NOPARITY 2 // out of thin air, Sun specific
#define SCMD_READ_CAPACITY 2 // out of thin air, Sun specific
typedef	void *opaque_t;
struct scsi_pkt {adrPak pkt_address; u_char * pkt_scbp; u_char * pkt_cdbp;
    struct buf * pkt_bufp; opaque_t pkt_private; int pkt_flags;
     void (*pkt_comp)(struct scsi_pkt *);
     int scsi_pkt; int pkt_state; int pkt_reason;
     int pkt_resid;};  // order may be wrong.
int scsi_transport(struct scsi_pkt *);
union scsi_cdb {
  struct {uchar tag;
    union {struct scsi_g0{uchar addr1; uchar addr0; uint32 count0;} g0;
           struct {uchar addr3; uchar addr2; uchar addr1; uchar addr0;
           uint32 count1; uint32 count0;} g1;
    } sg;} cdb_un;
  uchar scc_cmd;};
struct scsi_status {int pkt_scbp; int sts_chk; int sts_busy;};
