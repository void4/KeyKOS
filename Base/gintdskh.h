#include "ktqmgrh.h"
#include "devmdh.h"
void ginioida(PHYSDEV *pdev);
void ginioicp(PHYSDEV *pdev);
void disk_kertask_function(
   struct KernelTask *ktp);
void diskdone_intproc(
   struct scsi_pkt *scp);

