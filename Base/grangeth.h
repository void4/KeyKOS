/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_grangeth
#define _H_grangeth

struct GRTReadInfo {      /* Returned information for reading */
   struct Device *device;
   uint32 offset;
   union {
      RANGELOC potaddress;
      PCFA *pcfa;
   } id;
};
 
union GRTRet {             /* Return value, depends on code below: */
   CTE *cte;
   struct GRTReadInfo readinfo;
   RANGELOC rangeloc;
};
 
struct CodeGRTRet {
   union GRTRet ioret;     /* Returned value, depends on code below: */
   int code;               /* Status code for request as follows: */
#define grt_notmounted     0
#define grt_notreadable    1  /* ioret is rangeloc of the page/pot */
#define grt_potincore      2  /* ioret is pointer to CTE for pot */
#define grt_mustread       3  /* ioret is GRTReadInfo */
#define grt_readallopot    4  /* ioret is rangeloc of allocation pot */
};
 
extern struct CodeGRTRet grthomen(CDA cda);
extern struct CodeGRTRet grthomep(const CDA cda);
extern struct GRTReadInfo grtnext(void); /* Klobbers potaddress */
extern RANGELOC grtcrl(CDA cda);
       /* Returns (-1, USHRT_MAX) if not mounted */
 
extern uint32 grtslsba(RANGELOC swaploc);
 
extern struct CodeGRTRet grtrsldl(RANGELOC rl);
 /* Readable swaploc->devloc, returns grt_notmounted, grt_notreadable */
 /*                           and grt_mustread w/device + offset only */
 
extern struct Migrate_DeviceOffset grtslemi(RANGELOC swaploc);
 
extern struct CodeGRTRet grtsl2dl(RANGELOC rl);
             /* Returns grt_notmounted or grt_mustread */
 
extern struct CodeGRTRet grtchdrl(int id); /* ckpt hdr locations */
 
extern struct CodeGRTRet grthomwl(CTE *cte);
             /* Returns grt_notmounted or grt_mustread*/

#endif

