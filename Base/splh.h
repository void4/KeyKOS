unsigned int splhi(void);  /* forbid interrupts */
/* The above returns the previous interrupt enable state in
   the form of a value that can be passed to splx. */
unsigned int splx(unsigned int); /* set interrupt enable */
void spin(unsigned long); /* Wait n clock cycles */
unsigned long xmem(unsigned long, unsigned long *);
unsigned long xmemb(unsigned long, unsigned char *);

