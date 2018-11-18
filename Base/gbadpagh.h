/* Prototypes for routines in GBADPAGC */
 
extern int gbadread(struct Device *dev, uint32 offset);
                /* Returns 0==page ok, !0==page bad */
 
extern void gbadlog(struct Device *dev, uint32 offset);
 
extern void gbaddmnt(struct Device *dev);
 
extern void gbadrewt(struct Device *dev, uint32 offset);
