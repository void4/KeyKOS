 
/* Prototypes for key functionality routines called only from gate */
 
/* These routines process individual key types. They use cpudibp */
/* and jumperparm */
 
extern void jdata      (struct Key *key);
extern void jpage      (struct Key *key);
extern void jsegment   (struct Key *key);
extern void jnode      (struct Key *key);
extern void jmeter     (struct Key *key);
extern void jfetch     (struct Key *key);
extern void jstart     (struct Key *key);
extern void jresume    (struct Key *key);
extern void jdomain    (struct Key *key);
extern void jhook      (struct Key *key);
extern void jmisc      (struct Key *key);
extern void jcopy      (struct Key *key);
extern void jnrange    (struct Key *key);
extern void jprange    (struct Key *key);
extern void jchargeset (struct Key *key);
extern void jsense     (struct Key *key);
extern void jdevice    (struct Key *key);
extern void jconsole   (struct Key *key);
extern void jckfckpt   (void);
extern void jmigrate   (void);

extern void jnode1(NODE *node);/* In jnode to do key fetches and stores */
