/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* kernel data copy key header */

/**********************************************************************/
/*                                                                    */
/*  KC (Datacopy,0) STRUCTFROM(DatacopyArgs) KEYSFROM(fromseg,toseg)  */
/*            STRUCTO(DatacopyArgs)                                   */
/*                                                                    */
/*     Returned Structure updated for amount moved                    */
/**********************************************************************/

#ifndef _H_datacopy
#define _H_datacopy


#define DatacopyF_AKT 0x164
#define Datacopy_AKT 0x64

#define Datacopy_Failed 1

struct DatacopyArgs {
    unsigned long long fromoffset;
    unsigned long long tooffset;
    unsigned long long length;
};

#endif

