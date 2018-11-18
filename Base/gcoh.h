#include "ioreqsh.h"
#include "devmdh.h"
void enqueue_devreq(DEVREQ *drq);
void enqueue_devreqs(REQUEST *req);
extern REQUEST *enqrequestworkqueue;
void gcoreenq(void);
void check_request_done(REQUEST *req);
void return_completed_requests(void);
void dequeue_pending_devreqs(REQUEST *req);
void select_devreq(DEVREQ *drq); /* was GCOSELDQ */
void gcodidnt(DEVREQ *drq);
void gcodismtphys(PHYSDEV *);
void unlinkdevreq(DEVREQ *);
extern REQUEST *nopagesrequestqueue;
void serve_nopagesrequestqueue(void);

