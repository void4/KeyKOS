#define BLOCKSPERPAGE (pagesize/512)

enum ccwsret {
   CCWSBUILT=0,
   NOPAGES,     /* Reads only */
   NOSWAPSPACE  /* REQDIRECTORYWRITE only.
                   Devreq->status set to DEVREQNODEVICE */
};
enum ccwsret build_ccws(DEVREQ *);
   /* Build CCWs for a DEVREQ for initial device start. */
