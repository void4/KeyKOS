/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*   MODULE     EMIGC C                                                *
/*   TITLE      EXTERNAL MIGRATOR - OPTIMIZE MIGRATION                 *
/*   DEVELOPED from EMIG2 ASSEMBLE      5/17/90                       *
/***********************************************************************
/*                                                                     *
/*   This routine obtains the CDAs of pages & nodepots waiting
/*     migration from the External Migrator Tool, sorts them according
/*     to their location in the backup swap area and then instructs
/*     the External Migrator Tool to migrate them.
/*
/*   This version, unlike its predecessor EMIG2, does NOT support
/*     tape checkpoint.
/*                                                                     *
/***********************************************************************
/*                                                                     *
*/
 
#include <keykos.h>
#include <migrate.h>
#include <lli.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

  JUMPBUF;

#define min(x,y) ((x)<(y)?(x):(y))
 
KEY MIGRATE   = 3;                       /*External Migration Tool  */
 
#define buff_len         0x20000L
               /* Size of buffer used to hold directory entries */
 
#define page_adjust       50
               /* pages to hold out of migration for other stuff */
 
struct cda { char  cda1 [6]; };

 int bootwomb=1;
 int stacksiz=4096;
 
char title [] = "EMIGC";
 
/********************************************************************/
 
char   Directory_Buffer [buff_len];   /* Buffer to hold Directory */
                                        /*  segments returned by    */
                                        /*  MIGRATE                 */
 
char    *buff_ptr;                   /* Pointer to Directory Buffer */
char    *buff_end;                   /* Pointer to end of Dir Buff  */
 
struct Migrate_DirectoryEntry   *CDAs_2BMigrated;
                                 /* Pointer to Directory Entries    */
                                 /*  stored in Directory Buffer     */
 
struct cda     Migrate_These_CDAs [MIGRATE_CDAsPerPage];
                                 /* Parameter area containing list  */
                                 /*  of CDAs to be sorted           */
 
uint32  max_pages;               /* Maximum # of pages which can be */
                                 /*   migrated in one pass          */
 
/********************************************************************/
void dup_seek(int l){char *j, *k; if(1)return;
for(j=(char *)Directory_Buffer; j<buff_ptr; j+=14)
for(k=j+14; k<buff_ptr; k+=14)
if(!memcmp(j, k, 6)) /*crash*/ *(long *)1=0;}
 
factory ()
{
  for (;;)
   {  KC (MIGRATE, Migrate_Wait)  RCTO (max_pages);
 
      max_pages -= min(page_adjust, max_pages-7);
             /* leave at least 7 pages for us */
      get_CDAs2Migrate ();
dup_seek(1);
      sort14((unsigned char *)Directory_Buffer, 
        buff_ptr - Directory_Buffer, 6, 4);
dup_seek(2);
      migrate_CDAs ();
 
    }                       /* end forever loop */
 }
 
 
/*********************************************************************
/*  get CDAs2Migrate ( )
/*    Get Directory Segments from MIGRATE and place them in adjacent
/*    locations in memory.  Stop if there are no more segments, or if
/*    the Directory_Buffer becomes full.  Unread segments will be
/*    picked up following the next Wait for Migration Needed.
/*
*/
 
 get_CDAs2Migrate ()
 {
    uint32  return_code;        /* RC for Key Call                 */
    uint32  max_length;         /* Maximum length of Directory     */
                                /*  segment Migrate can return     */
    uint32  segment_length;     /* Actual length of Directory      */
 
 
    buff_ptr = Directory_Buffer;
    buff_end = buff_ptr + buff_len;
    return_code = 0;
 
    while (buff_ptr <
                 (buff_end - sizeof(struct Migrate_DirectoryEntry)) )
      {  if ( (max_length = (buff_end - buff_ptr)) > 4096 )
           max_length = 4096;
 
         KC (MIGRATE, Migrate_ReadDirectory)
                          RCTO ( return_code)
                          CHARTO (buff_ptr, max_length, segment_length);
 
         if (return_code) break;
         buff_ptr += segment_length;
       }              /* End: while (buff_ptr < buff_end) */
  }
 
 
/********************************************************************/
/*  sort_CDAs ( )
/*    Sort the Directory Entries in the Directory Buffer on the first
/*      first backup swap area location (first Migrate_DeviceOffset).
/*    Use the Radix Exchange Method discussed in Knuth, Volume 3,
/*      section 5.2.2.  Note that the field CDAs_2BMigrated->second
/*      contains unneeded data, and hence is not swapped during
/*      the sort.
*/
 
 sort_CDAs ()
 {
   uint32  theStack [32];     /* One entry per bit in the sort key */
   uint32  zeroMask, onesMask;  /* Masks indication which bits are   */
                                /*   zero or one in all keys to be   */
                                /*   sorted.                         */
   uint32 left, right, i,j;
 
   union  {  uint32  DevAddr;
             struct  Migrate_DeviceOffset DevOff;
            }   loc;
 
/* There is no need to sort on a given bit if it has the same value
/*   in all keys.  The following code builds two masks to be used
/*   in eliminating unnecessary passes
*/
   zeroMask = 0;
   onesMask = 0xFFFFFFFF;
 
   return 0;       /* skip the sort for now - it hasn't been written */
#ifdef sort 
   for ( CDAs_2BMigrated =
             (struct Migrate_DirectoryEntry *) Directory_Buffer;
         CDAs_2BMigrated <
             (struct Migrate_DirectoryEntry *) buff_ptr;
         CDAs_2BMigrated ++ )
     {  loc.DevOff.device = CDAs_2BMigrated->first.device;
        loc.DevOff.offset = CDAs_2BMigrated->first.offset;
        zeroMask |= loc.DevAddr;
        onesMask &= loc.DevAddr;
      }                 /* end for (CDAs ...)  */
#endif
  }
 
/********************************************************************/
/*  migrate_CDAs ( )
/*    Move the CDAs from the Directory Buffer to the Parameter area
/*    until the parameter area is full or the count of pages needed
/*    for the migration exceeds the page count returned on Wait For
/*    Migration Needed.  Repeat until the entire buffer has been
/*    migrated
*/
 
migrate_CDAs ()
{
   CDAs_2BMigrated =
                  (struct Migrate_DirectoryEntry *) Directory_Buffer;
   while (CDAs_2BMigrated < (struct Migrate_DirectoryEntry *) buff_ptr)
    {
      unsigned int page_count = 0;
               /* Number of pages required for CDAs requested */
      unsigned int i = 0;  /* Number of CDAs requested */
      uint32   return_code;       /* RC for KEY CAll */

      /* Build a request to migrate a bunch of CDAs. */
      for (;
           page_count < max_pages   /* Don't overtax real memory */
             && i < (4096/6)        /* so request string fits in a page */
             && (CDAs_2BMigrated < (struct Migrate_DirectoryEntry *) buff_ptr);
                                    /* Do as many as we have */
           i++, CDAs_2BMigrated++ ) {
         if (CDAs_2BMigrated->cda[0] & 0x80)
               page_count += 2;      /* Need 2 pages for a node  */
         else  page_count += 1;      /* Need 1 page  for a page  */

         Migrate_These_CDAs [i] =
                      *(struct cda *) CDAs_2BMigrated->cda;
      }
      /* Now migrate the bunch. */
      KC (MIGRATE, Migrate_MigrateThese)
                             STRUCTFROM ( Migrate_These_CDAs, i*6)
                             RCTO (return_code);

   }             /* end while CDAs2BMigrated < buff_ptr */
}
