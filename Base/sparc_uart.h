void jconinit(void);              /* Initialize the uart */
void jconsole(struct Key *key);   /* uart key service */
void uart_interrupt(void);        /* Handle interrupt from the uart */
