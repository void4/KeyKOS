/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*
    Defines the Allocation Pot data and the Allocation Pot itself
*/
typedef struct AlocData ALOCDATA;
 
struct AlocData {
   uchar flags;             /* Flags as follows: */
#define ADATAINTEGRITY  0xe0   /* These bits are used as a counter to */
                               /* check that an allocation pot was */
                               /* correctly written.  The counter in */
                               /* pot's the first flag must match */
                               /* the counter in the last byte of */
                               /* the pot. These bits are zero in the */
                               /* other flag fields in the pot. */
#define ADATAINTEGRITYONE 0x20
#define ADATACHECKREAD   0x04  /* Check page for correct write by */
                               /* ensuring that counter in the first */
                               /* 4 bytes matches the last 4 bytes. */
#define ADATAVIRTUALZERO 0x02  /* Page is all zero */
#define ADATAGRATIS      0x01  /* Page is gratis (not currently used */
   uchar allocationid[4]; /* allocation ID for the page */
};
 
#define APOTDATACOUNT (pagesize/sizeof(struct AlocData))
 
struct AlocPot {
   ALOCDATA entry[APOTDATACOUNT];  /* The allocation data entries */
   uchar checkbyte;        /* Check byte for correct write.  The */
                           /* first 3 bits must match the first 3 */
                           /* bits of the first flag field. The last */
                           /* 5 bits are the inverse of the previous */
                           /* byte to check errors with disks that */
                           /* replicate the last byte received on */
                           /* certain errors. (e.g. overrun on a 370) */
};
