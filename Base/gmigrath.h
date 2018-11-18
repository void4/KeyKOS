extern const int *prioritytable;  /* Current priorities for I/O queueing */
    /* An array with one element for each req->type value. */
    /* Lower numbers represent higher priorities. */
extern uint32 do_migrate0(void);
extern uint32 do_migrate1(void);
extern void gmidmpr(void);
extern void gmiimpr(void);
