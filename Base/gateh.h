extern void keepjump(struct Key *keeper_key, int);
                           /* Perform implicit jump to a keeper */
extern int resumeType;  /* what sort of resume key to provide */
extern void keyjump(struct Key *jumpee_key);
                           /* Perform explicit jump to a key */
extern void deliver_message(struct exitblock);  /* send a message */
extern void return_message(void);  /* return a message from a primary key */
extern void midfault(void);        /* fault invoking domain */
extern void abandonj(void);        /* abandon an invocation */
void gate(void);
void set_slot(struct Key *, struct Key *);
