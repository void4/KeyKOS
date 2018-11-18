#ifndef _H_migrateh
#define _H_migrateh
/*
  Proprietary Material of Key Logic  COPYRIGHT (c) 1990 Key Logic
*/
/***************************************************************
  Definitions for the External Migrator Tool
 
  MIGRATE(Migrate_Wait);
  MIGRATE(Migrate_MigrateThese) CHARFROM (Migrate_CDAList);
  MIGRATE(Migrate_ReadDirectory) CHARTO (Migrate_Directory,4096,n);
  MIGRATE(Migrate_StartScan);
  MIGRATE(Migrate_ScanDirectory) CHARTO (Migrate_CDALlist,4096,n);
 
***************************************************************/
#include "kktypes.h"
 
#define Migrate_Wait                  0
#define Migrate_MigrateThese          1
#define Migrate_ReadDirectory         2
#define Migrate_StartScan             3
#define Migrate_ScanDirectory         4
 
#define MIGRATE_CDAsPerPage (4096/6)
 
typedef char Migrate_CDAList[MIGRATE_CDAsPerPage][6];
 
  struct Migrate_DeviceOffset {
     uint16 device;
     uint16 offset;
  };
 
  struct Migrate_DirectoryEntry {
    char cda[6];
    struct Migrate_DeviceOffset first, second;
  };
 
#define MIGRATE_DirectoryEntriesPerPage (4096/                        \
                  sizeof(struct Migrate_DirectoryEntry))
 
typedef struct Migrate_DirectoryEntry
                  Migrate_Directory[MIGRATE_DirectoryEntriesPerPage];
#endif
