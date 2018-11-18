/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************
  Defines for Fresh Segment
 
  KC (FSF,FSF_Create) KEYSFROM(sb,m,sb) KEYSTO(FS)
 
  KC (FS,FS_ReturnLength)         STRUCTTO(FS_SegmentLength)
  KC (FS,FS_TruncateSegment)      STRUCTFROM(FS_SegmentLength)
  KC (FS,FS_CreateSibling)        KEYSFROM(sb) KEYSTO(FS)
  KC (FS,FS_CreateROSegmentKey)   KEYSTO(FSR)
  KC (FS,FS_CreateNCSegmentKey)   KEYSTO(FSR)
  KC (FS,FS_CreateRONCSegmentKey) KEYSTO(FSR)
  KC (FS,FS_SetLimit)             STRUCTFROM(FS_SegmentLength)
  KC (FS,FS_GetLimit)             STRUCTTO(FS_SegmentLength)
  KC (FS,FS_CopyMe)               STRCUTFROM(FS_CopyArgs) KEYSFROM(sb)
                                       KEYSTO(NewFS)
  KC (FS,FS_Freeze)               KEYSTO(FSR)
 
***************************************************************/
#ifndef _H_fs
#define _H_fs

#include <lli.h>
 
#define FSF_AKT                0x40D
#define FS_AKT                 0x50D
 
#define FSF_Create                 0

#define FS_InternalError           3
#define FS_CopyError               4
 
#define FS_ReturnLength            1
#define FS_TruncateSegment         5
#define FS_CreateSibling          18
#define FS_CreateROSegmentKey      0
#define FS_CreateNCSegmentKey      2
#define FS_CreateRONCSegmentKey    3
#define FS_SetLimit                6
#define FS_CopyMe                  7
#define FS_GetLimit                8
#define FS_Freeze                  9
#define FS_SetMetaData            10
#define FS_GetMetaData            11
 
 struct FS_SegmentLength {
   LLI Length;
 };

 struct FS_CopyArgs {
   unsigned long offset;
   unsigned long length;
 };

 struct FS_UnixMeta {
   unsigned char vacant;    /* zero because of 11 byte data limit on data keys */
   unsigned char groupid;   /* 1 byte because of 11 byte data limit */
   unsigned short userid;
   unsigned short mode;
   unsigned short inode;    /* Device number is contextually determined by ukeeper */
   unsigned long length;    /* 4 gig files for this emulation */
 };
#endif
