/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GMIGRATEC - Migrate swap area to home - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "kktypes.h"
#include "sysdefs.h"
#include "migrate.h"
#include "keyh.h"
#include "cpujumph.h"
#include "wsh.h"
#include "primcomh.h"
#include "gateh.h"
#include "gdi2jmih.h"
#include "gmigrath.h"
#include "kernkeyh.h"
#include "getih.h" 
#include "memutil.h" 
 
 
/*********************************************************************
setupfordatastring - Sets up the returnee for the data string
 
  Input -
 
  Output - See below:
 
  Notes:  When jump is not blocked, the caller must set up:
          cpuordercode - Set to return code
          cpuarglength - Set to length of returned string
          cpuargaddr   - Set to address of data
     and then:
          return_message();
          return;
     Otherwise just return
*********************************************************************/
#define sufds_blocked 0
#define sufds_returndata 1
 
static int setupfordatastring(void)
{
   cpuexitblock.argtype = arg_regs;
   cpuexitblock.keymask = 0;
   if (cpuexitblock.jumptype == jump_call) {   /* Use fast method */
      cpuinvokeddatabyte = returnresume;
      cpuinvokedkeytype = resumekey+prepared;
      if (setupdestpageforcall()) {
         abandonj();
         return sufds_blocked;
      }
   /* End dry run */
      return sufds_returndata;
   }
   else {                                      /* Use the slow way */
      switch (ensurereturnee(pagesize)) {      /* Will return string */
       case ensurereturnee_wait:  {
         abandonj();
         return sufds_blocked;
       }
       case ensurereturnee_overlap: {
         midfault();
         return sufds_blocked;
       }
       case ensurereturnee_setup: handlejumper();
      }
   /* End dry run */
      if (! getreturnee()) return sufds_returndata;
      return sufds_blocked;
   }
} /* End setupfordatastring */
 
 
/*********************************************************************
jmigrate - The migrate key
 
  Input - None
 
  Output - None
*********************************************************************/
void jmigrate(void)
{
   uint32 retcode;
 
   if (cpuordercode > Migrate_ScanDirectory) {
      simplest(KT+2);      /* kt == KT+2 */
      return;
   }

   if (migrationpriority)   /* Boost caller's priority */
      memzero (cpuactor->domprio.nontypedata.dk11.databody11,
               sizeof cpuactor->domprio.nontypedata.dk11.databody11);
   switch (cpuordercode) {
    case Migrate_Wait:
      retcode = do_migrate0();
      if (KT == retcode) {
         checkforcleanstart();
         abandonj();
      }
      else simplest(retcode);
      return;
 
    case Migrate_MigrateThese:
      retcode = do_migrate1();
      if (KT == retcode) {
         checkforcleanstart();
         abandonj();
      }
      else simplest(retcode);
      return;
 
    case Migrate_ReadDirectory:
      if (setupfordatastring() == sufds_returndata) {
         cpuargaddr = cpuargpage;
         cpuarglength = gdimigr2((uchar *)cpuargaddr);
         if (cpuarglength) cpuordercode = 0;
         else cpuordercode = 1;
         return_message();
      }
      return;
 
    case Migrate_StartScan:
      gdimigr3();
      simplest(0);
      return;
 
    case Migrate_ScanDirectory:
      if (setupfordatastring() == sufds_returndata) {
         cpuargaddr = cpuargpage;
         cpuarglength = gdimigr4((uchar *)cpuargaddr);
         if (cpuarglength < 0) {
            cpuarglength = 0;
            cpuordercode = 2;
         }
         else if (cpuarglength) cpuordercode = 0;
         else cpuordercode = 1;
         return_message();
      }
      return;
 
    default:
      simplest(KT+2);      /* kt == KT+2 */
      return;
   }
} /* End jmigrate */
