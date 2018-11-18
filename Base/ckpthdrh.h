/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*
    Defines checkpoint header for 6 byte CDA kernel
*/
 
#define CKPTHEADERNUMDD ((pagesize - sizeof(LLI)                    \
                                   - sizeof(struct CalClock)        \
                                   - 3*sizeof(uint16)               \
                                   - 2*sizeof(uint32)               \
                                   - 2*sizeof(char))                \
                          / (2*sizeof(uint32)))
struct CkPtHeader {
   uint64 tod;                /* Time of checkpoint */
   struct CalClock calclock;  /* Calender clock at checkpoint */
   uint16 number;             /* Number of entries */
   uint16 extension;          /* 1 if there is a extension else 0 */
   uint32 extensionlocs[2]; /* Swap Block Addresses of extension */
   uint32 ddlocs[2*CKPTHEADERNUMDD];
                              /* Disk directory Swap Block Addresses */
   uint16 writecheck;         /* tod.hi & 0xffff to check that the */
                              /* full header was written to disk */
   uchar version;             /* Version number of header == 1 */
   uchar integritybyte;       /* Set to 0xf7 to catch certain errors */
};
