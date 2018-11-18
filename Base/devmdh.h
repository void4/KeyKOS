#ifndef _H_devmdh
#define _H_devmdh
/* Machine dependent and machine independent device block */
#include "kktypes.h"
#include "kertaskh.h"
#include "keyh.h" /* for RANGELOC */
#include "buf.h"
#include "types.h"
// #include "dditypes.h" 
// #include "dmaga.h"
// #include "pte.h"
// #include "mmu.h"
// #include "iommu.h"
// #include "ddidmareq.h" 
// #include "ddi_impldefs.h" 
// #include "cmn_err.h" 
#include "vtrace.h"
#include "scsi.h"

typedef unsigned short int u_short;

/* A Device represents a Logical Disk (e.g. a partition of a disk). */
struct Device {
   struct PhysDevice *physdev;  /* The Physical Device that holds this
                                   logical disk. */
   unsigned long physoffset;    /* Page 0 of the logical disk is at this
                                   block address on the physdev */
   unsigned long extent;        /* 1+the highest valid logical page offset */
   uint32 pdraddress;       /* Page offset on logical disk to the Pack
                               Descriptor Record */
   char packid[8];        /* Identifier of the mounted pack */
   char flags;
#define DEVCURRENTSWAPAREAHERE 1  /* There is a current swap area */
                                  /* on this device */
#define DEVMOUNTED             2  /* A pack is mounted on this */
                                  /* device for kernel use */
};

enum dsestatus { /* Completion status for a disk I/O operation */
   NOERROR,
   PERMERROR, /* Permanent problem with the disk location */
   PACKDISMOUNTED  /* Permanent problem with the whole pack */
};

/* A PhysDevice represents a Physical Device. */
struct PhysDevice {
/* Common segment for all devices (used by DEVICEIO) */
 
/* Machine independent section for kernel disks only */

   struct DevReq *ioqlast, *ioqfirst; /* Queue of devreqs */
   uint32 lastaddress;      /* Last/current page offset accessed */
 
   char enqstate;           /* Ordered state of device as follows: */
#define DEVSTART          0     /* Device available, no REQUESTs */
                                /* queued */
#define DEVRUNNINGADD     1     /* Device running, add REQUEST to */
                                /* CCWBLOK chain. NOT USED. */
#define DEVRUNNINGQUEUE   2     /* Device running - queue REQUEST */
#define DEVERRORRECOVERY  4     /* Error recovery, Device may be going */
                                /* off line of being dismounted */
                                /* Not used. */
#define DEVNOTREADY       5     /* Device not ready - reject REQUEST */
 
   void *workunit;        /* While DEVRUNNING, has the DEVREQ * being
                processed, or the CTE * being cleaned. */
   int windownum;         /* Number of the window reserved for this device */
   void (*doneproc)(struct PhysDevice *);
   enum dsestatus donestatus; /* Result of I/O operation */
   RANGELOC ccwswaploc;   /* if workunit is a CTE *, this has
                     the swaploc being cleaned onto. */
   unsigned int diskerrorretrycounter;
   struct KernelTask kertask;
      /* kertask.kernel_task_function is disk_kertask_function. */
   char type[7];          /* Device type or all zeroes */
   char packid[128];        /* Identifier of the mounted pack */

   struct scsi_pkt *pd_dynamic_sp;     /* scsi_pkt allocated by esp */
   struct scsi_address pd_scsi_addr;      /* corresponding scsi device address */
   struct scsi_pkt pd_saved_scsi_pkt;  /* saved SCSA scsi pkt when sense cmd is needed */ 
   struct buf pd_scsi_buf;	/* buffer header associated with this PD */
   char pd_scsi_scb[32]; /* 32 bytes space for pkt_scbp */
   union scsi_cdb pd_scsi_cdb;  /* SCSI command */

   struct buf pd_reqsense_buf;	/* buffer header associated with this PD */
#if LATER
   struct scsi_pkt pd_reqsense_pkt; /* SCSA scsi pkt */ 
   char pd_reqsense_scb[32]; /* 32 bytes space for pkt_scbp */
   union scsi_cdb pd_reqsense_cdb;  /* SCSI command */
#endif
   ulong_t pd_reqsense_page; /* reserved physical page */
   int pd_reqsense_windownum;/* mapping window # reserved for this device */
   void (*pd_saved_completion_proc)(struct scsi_pkt *);
};
 
typedef struct Device DEVICE;
#define LOGDISK DEVICE /* Logical disk */
typedef struct PhysDevice PHYSDEV;

extern void makecmd_g0(struct scsi_pkt *pktp, u_char cmd, unsigned long addr, unsigned long cnt);
extern void makecmd_g1(struct scsi_pkt *pktp, u_char cmd, unsigned long addr, unsigned long cnt);
extern void start_scsi_cmd_sense(struct scsi_pkt *sp);
#endif

