/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "string.h"
#include "lli.h"
#include "keyh.h"
#include "wsh.h"
#include "splh.h"
#include "cpujumph.h"
#include "devkeyh.h"
#include "consdefs.h"
#include "gateh.h"
#include "primcomh.h"
#include "queuesh.h"
#include "ktqmgrh.h"
#include "sparc_uart.h"
#include "kuart.h"
#include "consmdh.h"
#include "kermap.h"
#include "grestarh.h"
#include "sparc_asm.h"
#include "sparc_cons.h"
#include "memutil.h"


/* Definitions that might someday be moved to another header */


   /* Commands addressed to the cmd port of the uart */

#define UART_WRR0            0    /* Write register zero */
#define UART_WRR1            1    /* Write register one */
#define UART_WRR2            2    /* Write register two */
#define UART_WRR3            3    /* Write register three */
#define UART_WRR4            4    /* Write register four */
#define UART_WRR5            5    /* Write register five */
#define UART_REGCOUNT        6    /* Number of registers */
#define UART_ENDEXTSTATUS 0x10    /* Reset external status interrupt */
#define UART_CHANNELRESET 0x18    /* Channel reset */
#define UART_RESETTXINT   0x28    /* Reset Transmitter ready interrupt */
#define UART_ERRORRESET   0x30    /* Error reset */
#define UART_ENDINTERRUPT 0x38    /* End of interrupt processing */

/* Data for register 1 */
#define UART_TXINTENABLE  0x02    /* Enable transmitter interrupts */
#define UART_RXINTENABLE  0x10    /* Enable receiver interrupts */

/* Data for register 2 */
#define UART_INTMODE      0x14    /* Interrupt parameters */

/* Data for register 3 */
#define UART_RXEANBLE     0x01    /* Enable receiver */
#define UART_BITS8        0xc0    /* Set 8 bits / character */

/* Data for register 4 */
#define UART_STOPBIT1     0x04    /* One async stop bit */
#define UART_B9600        0x40    /* Divide clock by 16 == 9600 bps */
                                  /* 0x80 for 4800, 0xc0 for 2400 */

/* Data for register 5 */
#define UART_TXENABLE     0x08    /* Transmitter enable */
#define UART_RTS          0x02    /* Request to send */

/* Status register 0 information from the uart */
#define UART_RXREADY      0x01    /* Receiver has a character */
#define UART_INTPENDING   0x02    /* An interrupt is pending (ch0) */
#define UART_TXREADY      0x04    /* Transmitter can accept a character */

/* Status register 1 information from the uart */
#define UART_PARITYERROR  0x10    /* Parity error in data */
#define UART_RXOVERRUN    0x20    /* Receiver buffer overrun */
#define UART_FRAMEERROR   0x40    /* Framing error in data */

struct Uart {
   volatile unsigned char cmd;    /* cmd/status register in the UART */
#define status cmd
   volatile unsigned char :8; 	  /* filler */
   volatile unsigned char data;   /* data register in the UART */
   volatile unsigned char :8;     /* reserved locations */
};
typedef struct Uart UART;
void drv_usecwait(int);

/* End definitions that might someday be moved to another header */
 
 
 
#define OBUFSIZE 256
#define IBUFSIZE 4096
 
struct lineblock {       /* The modem block */
 
   struct KernelTask kernel_task; /* The kernel task to wake waiters */
 
   UART *uart;          /* Pointer to the physical port */
#define ZSOFF	4       /* byte offset of line a registers */
 
   char *istart,        /* Start of the input buffer */
        *iend,          /* End of the input buffer */
        *ipp,           /* Producer pointer into the input buffer */
        *icp;           /* Consumer pointer into the input buffer */
 
   char *ostart,        /* Start of the output buffer */
        *oend,          /* End of the output buffer */
        *oed,           /* End of output data in the output buffer */
        *ocp;           /* Consumer pointer into the output buffer */
 
   long limit;          /* The SIK caller's limit */
 
   struct QueueHead readcaller;  /* The queue for the read waiter */
   struct QueueHead writecaller; /* The queue for the write waiter */
 
   char uart_status;    /* The last status byte from the port */
                           /* The relevant bits are RR0_DCD, RR0_CTS, */
                           /*    and RR0_BREAK */
 
   char state;          /* The current interface state */
#define CONIDLE 0          /* Not writing */
#define CONWRITE 1         /* A write operation is active */
 
   char keyflags;       /* Flags associated with key state */
#define KEYWRITEDONE 1     /* A write operation has completed */
#define KEYREADWAKE  2     /* Wake read waiter in progress */
#define KEYKERTASK   4     /* A kernel task is queued for the port */
#define INPUTENABLED 8     /* Input has been enabled */

   char flags;          /* Control flags */
#define IOVERRUN     8     /* Input buffer has overflowed */

   char console;	   /* 1 if this line is used as console */

   char regs[UART_REGCOUNT]; /* Current contents of uart registers */
 
   char ibuffer[IBUFSIZE]; /* The input buffer */
   char obuffer[OBUFSIZE]; /* The output buffer */
 
} linea, lineb;
 
typedef struct lineblock LINEBLOCK;
 
struct QueueHead *lineaqueues = &linea.readcaller; /* Pointers for */
struct QueueHead *linebqueues = &lineb.readcaller; /* Pointers for */

extern caddr_t line_addr; 	/* uart mapped register addresses from fillsysinfo.c */
/* ZZZ */
char probe_a, probe_b;
char *input_device, *output_device;
char t_char, r_char, ts_char, rs_char, ra1[15], ra2[15], ra3[15], rb1[15], rb2[15], rb3[15];
int splvalue, read0, read3;
int txcnt1, txcnt2, txcnt3;
int rxcnt1, rxcnt2, rxcnt3;
int uart_debug = 0;
/*
idprom_t idprom;
short cputype;
*/


#define SCC_READA(reg, var) { \
	linea.uart->cmd = reg; \
	var = linea.uart->cmd; \
}
#define SCC_READB(reg, var) { \
	lineb.uart->cmd = reg; \
	var = lineb.uart->cmd; \
}

void read_readrega(char *pt)
{
	SCC_READA(1, pt[1]);
	SCC_READA(2, pt[2]);
	SCC_READA(3, pt[3]);
	SCC_READA(8, pt[8]);
	SCC_READA(10, pt[10]);
	SCC_READA(12, pt[12]);
	SCC_READA(13, pt[13]);
	SCC_READA(15, pt[15]);
}
 
void read_readregb(char *pt)
{
	SCC_READB(1, pt[1]);
	SCC_READB(2, pt[2]);
	SCC_READB(3, pt[3]);
	SCC_READB(8, pt[8]);
	SCC_READB(10, pt[10]);
	SCC_READB(12, pt[12]);
	SCC_READB(13, pt[13]);
	SCC_READB(15, pt[15]);
}
 
static void reset_port(LINEBLOCK *port)
                           /* Clear the input and output buffers */
/* Must be called from level 7 if port could be active */
{
   port->istart = port->ibuffer;
   port->iend = port->ibuffer + sizeof port->ibuffer;
   port->ipp = port->ibuffer;
   port->icp = port->ibuffer;
   port->ostart = port->obuffer;
   port->oend = port->obuffer + sizeof port->obuffer;
   port->oed = port->obuffer;
   port->ocp = port->obuffer;
   port->flags = 0;                  /* Set default flags */
}


static void set_bit(LINEBLOCK *port, int reg, uchar bit)
{
   int oldlevel = spltty();
   uchar byte = port->regs[reg];
   UART *uart = port->uart;
   
   byte |= bit;
   port->regs[reg] = byte;
   uart->cmd = reg;
   uart->cmd = byte;
   uart->cmd = UART_ERRORRESET;
   
   splx(oldlevel);
} /* End set_bit */
 
 
/**********************************************************************
   The kernel task routine
**********************************************************************/
 
static void console_kernel_task(struct KernelTask *kert)
                             /* Serve the queues that must be served */
{
   LINEBLOCK *port = (LINEBLOCK *)kert;
   char keyflags;
   int s;
 
   s = spltty();                  /* Goto tty priority level */

   keyflags = port->keyflags;     /* Get the active flags */
   port->keyflags &= ~KEYKERTASK; /* Reset the kernel task scheduled */
 
   (void) splx(s);                /* Back to old priority */
 
   if (port->icp != port->ipp) {
      enqmvcpu(&port->readcaller);
   }
   if (keyflags & KEYWRITEDONE) {
      enqmvcpu(&port->writecaller);
   }
}
 
 
 
/**********************************************************************
   Begin initialization routines
**********************************************************************/
 
static void init_port(register LINEBLOCK *port)  /* Initialize a port */
{

   port->kernel_task.kernel_task_function = console_kernel_task;
   reset_port(port);
   port->readcaller.head = (union Item *)&port->readcaller;
   port->readcaller.tail = (union Item *)&port->readcaller;
   port->writecaller.head = (union Item *)&port->writecaller;
   port->writecaller.tail = (union Item *)&port->writecaller;
   port->state = CONIDLE;
   port->keyflags = 0;
   port->uart_status = 0;
   memzero(port->regs, sizeof port->regs);
 
      /* Set up the uart modes */
   
   set_bit(port, UART_WRR4, UART_STOPBIT1 | UART_B9600);
   set_bit(port, UART_WRR3, UART_BITS8 | UART_RXEANBLE);
   set_bit(port, UART_WRR5, UART_BITS8>>1 | UART_TXENABLE | UART_RTS);
   
   set_bit(port, UART_WRR1, UART_RXINTENABLE | UART_TXINTENABLE);
}
 
void jconinit(void)    /* Initialize the uart */
{
   volatile int i;     /* For pausing for the uart */
	int s;

   jromconinit();      /* Initialize the ROM console support */

/* find out if any of the uart is been used as console

   output_device = prom_stdoutpath(cputype);
   input_device = prom_stdinpath(cputype);

   if (Strcmp(output_device, "ttya") || Strcmp(input_device, "ttya"))
	linea.console = 1;
   else linea.console = 0;

   if (Strcmp(output_device, "ttyb") || Strcmp(input_device, "ttyb"))
	lineb.console = 1;
   else lineb.console = 0;
*/

   linea.console = 1;
   lineb.console = 0;
   read0 = 1;
   read3 = 1;

/*   
 * Do a probe by writing octal 017 to its control register address,
 * then read it back.  A Z8530 does not use the D0 & D2 bits of register
 * 15, so they should be zero.
 */

   linea.uart = (UART *)((int)(line_addr)|ZSOFF);
   lineb.uart = (UART *)(line_addr);
//   stop_mon_clock();
   s=spltty();
   if (read0) {
	read_readrega(ra1);
	read_readregb(rb1);
   }

   linea.uart->cmd = '\017';
   drv_usecwait(2);
   probe_a = linea.uart->cmd;
   drv_usecwait(2);
   if (probe_a &5)
	linea.console++;

   lineb.uart->cmd = '\017';
   drv_usecwait(2);
   probe_b = lineb.uart->cmd;
   drv_usecwait(2);
   if (probe_b &5)
	lineb.console++;


//   start_mon_clock();
   splx(s);

   if ((!linea.console) || (!lineb.console)){
//      stop_mon_clock();
      s=spltty();
      linea.uart->cmd = UART_CHANNELRESET;    /* Reset the uart */
      for (i=0; i<1000; i++) ;               /* Pause for 4 uart cycles */
      drv_usecwait(2);
      set_bit(&linea, UART_WRR2, UART_INTMODE);

      if (!linea.console){
      	init_port(&linea);
	read_readrega(ra2);
      }

      if (!lineb.console){
      	init_port(&lineb);
	read_readregb(rb2);
      }

      while (linea.uart->status & UART_INTPENDING)
         linea.uart->cmd = UART_ENDINTERRUPT;

      if (!linea.console)
         linea.uart->cmd = UART_RESETTXINT;

      if (!lineb.console)
         lineb.uart->cmd = UART_RESETTXINT;

//      start_mon_clock();
      splx(s);
   }
   
//   stop_mon_clock();

   s=spltty();

   if (read3) {
	read_readrega(ra3);
	read_readregb(rb3);
   }
//   start_mon_clock();
   splx(s);
}
 
 
/**********************************************************************
   End initialization routines
**********************************************************************/
 
 
 
/**********************************************************************
   Begin routines that run at interrupt level 12
**********************************************************************/
 
 
#define txready(uart) (uart->status & UART_TXREADY)
#define rxready(uart) (uart->status & UART_RXREADY)
#define sendchar(c,uart) (uart->data = c)
#define receivechar(uart) (uart->data)
#define reset_error(uart) (uart->cmd = UART_ERRORRESET)

int sparc_lineb_putchar(char c)
{
      while(!txready(lineb.uart));
      sendchar(c,lineb.uart);

      return 0;
}

char sparc_lineb_getchar()
{
      while(!rxready(lineb.uart));
      return (receivechar(lineb.uart));
}
 
int timed_sparc_lineb_getchar(count)
      int count;
{
      while(count) {
        if(rxready(lineb.uart)) break;
        count--;
      }
      if(!count) return -1;
      return (receivechar(lineb.uart));
}
 
static void wakeup(register LINEBLOCK *port)
                         /* puts port's task on the kernel task queue */
{
   if (!(port->keyflags & KEYKERTASK)) {
      enqueue_kernel_task(&port->kernel_task);
      port->keyflags |= KEYKERTASK;
   }
}
 
 
static void bufic(              /* Buffer an input character */
   register char c,                  /* The input character */
   register LINEBLOCK *port)          /* The console block to use */
{
   if (port->flags & IOVERRUN) return; /* Overflowed */
   *port->ipp++ = c;              /* Put char into buffer */
   if (port->ipp == port->iend)    /* If cursor at end */
      port->ipp = port->istart;      /* Wrap cursor */
   if (port->ipp == port->icp) {   /* Buffer overflow */
      port->ipp--;                   /* Backup cursor */
      if (port->ipp == port->istart - 1) port->ipp = port->iend - 1;
      port->flags |= IOVERRUN;       /* Indicate overflow */
   }
}
 
static void checkactivation(      /* Check if SIK should activate */
   register LINEBLOCK *port)          /* The console block to use */
{
   if (port->icp != port->ipp
       && port->readcaller.head != (union Item *)&port->readcaller) {
      wakeup(port);
   }
}
 
 
static void sendoutput(register LINEBLOCK *port)
                          /* Process output chars from obuffer */
                          /* The transmitter must be ready */
{
   register UART *uart = port->uart;
 
txcnt1++;
   if (port->ocp != port->oed) {      /* Send a character */
      register uchar c = *(port->ocp++); /* The character to send */
 
      sendchar(c,uart);                  /* Send it */
txcnt2++;
      while ((port->ocp != port->oed) && txready(uart)){
                txcnt3++;
                c = *(port->ocp++);
                sendchar(c,uart);
      }; 
   } /* End send a character */

   if (port->ocp == port->oed) {                        /* No more to send */
      uart->cmd = UART_RESETTXINT;       /* Reset interrupt */
      if (port->state == CONWRITE) {
         port->keyflags |= KEYWRITEDONE;
         port->state = CONIDLE;  /* Output done */
         wakeup(port);
      }
   }
}
 
static void rxreadyinterrupt(      /* Receive character ready */
   register LINEBLOCK *port)          /* The console block to use */
{
   UART *uart = port->uart;
 
/* ZZZ when are we going to pass this interrupt to OBP? */
/*   if (&mousekbd == port && !(port->keyflags & INPUTENABLED)) {
      romconsoleinterrupt();
      return;
   }
*/
rxcnt1++;
   for (;;) {                        /* For all pending i/p chars */
      register uchar c = receivechar(uart);  /* The input character */
      register uchar status;
 
      if (uart_debug){
	 if txready(uart)
		sendchar(c,uart);
      } else
        bufic(c,port);           /* Buffer the input character */
      checkactivation(port);   /* Check for wakeup */
      if (!rxready(uart))
   break;
rxcnt2++;
 
      /* Must check error register before getting the next character */
 
      uart->cmd = UART_WRR1;     /* Read status register one */
      status = uart->status;
      if (status & (  UART_PARITYERROR /* Test for errors */
                    | UART_RXOVERRUN      
                    | UART_FRAMEERROR)) {
         reset_error(uart);
         if (status & UART_RXOVERRUN) {
            port->flags |= IOVERRUN;
         }
      } /* End error on character */
   } /* End for all pending i/p chars */
}
 
static void txreadyinterrupt(      /* Transmitter ready for a char */
   register LINEBLOCK *port)          /* The console block to use */
{
   sendoutput(port);        /* Send output characters */
}
 
void uart_interrupt(void)     /* Handle interrupt from the uart */
{
   register UART *uartaddr;

   romconsoleinterrupt();

   /* ttya will only be used as console, not passed as tty key to domain
   uartaddr = linea.uart;
   if (rxready(uartaddr)) rxreadyinterrupt(&linea);
   if (txready(uartaddr)) txreadyinterrupt(&linea);
   uartaddr->cmd = UART_ENDEXTSTATUS;
   */
 
   uartaddr = lineb.uart;
   
   if (rxready(uartaddr)) rxreadyinterrupt(&lineb);
   if (txready(uartaddr)) txreadyinterrupt(&lineb);
   uartaddr->cmd = UART_ENDEXTSTATUS;

}
 
 
/**********************************************************************
   End routines that run at interrupt level 12
**********************************************************************/
 
 
 
/**********************************************************************
   Begin routines that run at interrupt level 0
     N.B. These routines may goto interrupt level 12 for serialization.
**********************************************************************/
 
 
void jconsole(struct Key *key)
                        /* Handle device key calls for a uart device */
/* Input -
   cpudibp - has the jumper's DIB
   cpuordercode - has order code.
   The invoked key type is device and subtype is DEVKMASTERCPU
*/
{
   register LINEBLOCK *port;        /* The console block to use */
   register int s;
 
   switch (key->nontypedata.devk.device) { /* Get selected device */
    case DEVKEYCONSOLE:    jromconsole(key);
                           return;
    case DEVKEYMODEM_A:    if (linea.console) {
				simplest(KT+3);
				return;
			   }
			   port = &linea;
                           break;
    case DEVKEYMODEM_B:    if (lineb.console) {
				simplest(KT+3);
				return;
			   }
    			   port = &lineb;
                           break;
    default:               simplest(KT+3);
                           return;
   } /* End get selected device */
 
   if (UART_MakeCurrentKey == cpuordercode) {  /* Make me a uart key */
      /* Make a current uart key for the caller */
      cpup1key = *key;                    /* Set up UART key */
      Memcpy(cpup1key.nontypedata.devk.serial, &grestarttod,
                sizeof key->nontypedata.devk.serial);
      cpuordercode = 0;                   /* Return code */
      cpuarglength = 0;                   /* No returned string */
      jsimple(8);                          /* Finish jump, First key */
      return;                             /* And return */
   }

   if (Memcmp(key->nontypedata.devk.serial, &grestarttod, 
                sizeof key->nontypedata.devk.serial)) {
      if (cpuordercode == KT) simplest(0x209);
      else {
         simplest(KT+3);
      }
      return;
   }

   switch (cpuordercode) {
    case UART_WakeReadWaiter: {
      switch (ensurereturnee(0)) {
       case ensurereturnee_wait:  abandonj();
          return;
       case ensurereturnee_overlap: midfault();
          return;
       case ensurereturnee_setup: handlejumper();
      }
      /* End dry run */

      s = spltty();                /* Goto tty priority level */

      port->keyflags |= KEYREADWAKE;  /* Mark wake reader */

      splx(s);                /* Back to old level */

      enqmvcpu(&port->readcaller);    /* Run any waiter */

      cpuordercode = 0;        /* No overflow */

      if (getreturnee()) return;
      cpuexitblock.argtype = arg_none;
      cpuexitblock.keymask = 0;
      return_message();
      return;
    }  /* End UART_WakeReadWaiter */
   
/* ZZZ What does this flag means now? */
    case UART_EnableInput: {
      port->keyflags |= INPUTENABLED;
      simplest(0);
      return;
    }  /* End UART_GetMaximumWriteLength */

    case UART_DisableInput: {
      port->keyflags &= ~INPUTENABLED;
      simplest(0);
      return;
    }  /* End UART_GetMaximumWriteLength */

    case UART_GetMaximumWriteLength: {
      simplest(sizeof port->obuffer);
      return;
    }  /* End UART_GetMaximumWriteLength */
   
    case UART_WriteData: {
      /* We can check for queued write caller at level 0 since */
      /* write callers are only queued from level 0 */

      if (sizeof port->obuffer < cpuarglength) {
         simplest(101);       /* Return string too long */
         return;
      }

      if (port->writecaller.head != (union Item *)&port->writecaller) {
         simplest(100);       /* Return already queued */
         return;
      }
 
      s = spltty();                    /* Goto tty priority level */
 
      if (port->keyflags & KEYWRITEDONE) { /* Output finished */
         port->keyflags &= ~KEYWRITEDONE; /* Turn off flag */

         (void) splx(s);                    /* Return to old priority level */
 
         simplest(0);            /* Return */
         return;
      } /* End output finished */
 
      if (!(port->state & CONWRITE)) {  /* Write not in progress */
         long len = cpuarglength;
  
         if (len > sizeof port->obuffer) len = sizeof port->obuffer;
         pad_move_arg(port->ostart, len);       /* Copy data */
         port->ocp = port->ostart;              /* Set pointers */
         port->oed = port->ostart + len;
         port->state |= CONWRITE;          /* Write in progress */
         if (txready(port->uart)) sendoutput(port); /* Start write */
      } /* End write not in progress */
 
      splx(s);                    /* Return to old priority level */
/*
      If the output finishes between now and the call to enqueuedom
      the domain will still be properly dequeued since the kernel
      task stacked by the level 7 code to dequeue it has not yet
      executed.
*/
      enqueuedom(cpudibp->rootnode, &port->writecaller);
      abandonj();
      return;
    }  /* End WriteData */
    default: ;             /* All others - fall thru to read test */
   }  /* end of switch statement */

   if    (UART_WaitandReadData < cpuordercode
         && UART_WaitandReadData+4096 >= cpuordercode) {
      register long limit = cpuordercode-UART_WaitandReadData;
 
      if (limit > sizeof port->ibuffer-1) limit = sizeof port->ibuffer-1;
         
      /* We can check for queued read caller at level 0 since */
      /* read callers are only queued from level 0 */
 
      if (port->readcaller.head != (union Item *)&port->readcaller) {
         simplest(100);      /* Return already queued */
         return;
      }
 
      s = spltty();                    /* Goto tty priority level */
 
      if   (port->icp != port->ipp
            || port->keyflags & KEYREADWAKE) { /* Input finished */
         int len;                         /* length of data */
 
         (void) splx(s);                    /* Return to old priority level */
 
         switch (ensurereturnee(1)) {
          case ensurereturnee_wait:  abandonj();
             return;
          case ensurereturnee_overlap: midfault();
             return;
          case ensurereturnee_setup: handlejumper();
         } /* End switch ensurereturnee */
         /* End dry run */
 
         s = spltty();                    /* Goto tty priority level */
 
         if (port->icp <= port->ipp) { /* Input doesn't wrap */
            len = port->ipp - port->icp; /* Length of data */
            Memcpy(cpuargpage,port->icp, len); /* Copy data */
            port->icp += len;
         } /* End input doesn't wrap */
         else {                           /* Input wraps */
            int len1 = port->iend - port->icp; /* Len of 1st part */
 
            Memcpy(cpuargpage,port->icp, len1);   /* Copy */
            len = port->ipp - port->istart; /* Len of rest */
            Memcpy(cpuargpage+len1, port->istart, len);   /* Copy */
            port->icp = port->istart + len;
            len += len1;
         } /* End input wraps */
 
         if (port->flags & IOVERRUN && port->icp == port->ipp) {
            port->flags &= ~(IOVERRUN|KEYREADWAKE); /* Turn off overflow */
            cpuordercode = 2;          /* Set return code */
         }
         else if (port->keyflags & KEYREADWAKE) {
            port->keyflags &= ~KEYREADWAKE;  /* Turn off flag */
            cpuordercode = 1;          /* Set return code */
         }
         else cpuordercode = 0;        /* No overflow */
 
         splx(s);                    /* Return to old priority level */
         
     /*  cpuordercode = ... 0=ok, 1=wakereadwaiter, 2=dataloss   */
 
         if (getreturnee()) return;
         cpuexitblock.argtype = arg_regs;
         cpuarglength = len;
         cpuargaddr = cpuargpage;
         cpuexitblock.keymask = 0;
         return_message();
         return;
      } /* End input finished */
  
      splx(s);                    /* Return to old priority level */
/*
      If the input finishes between now and the call to enqueuedom
      the domain will still be properly dequeued since the kernel
      task stacked by the Level 7 code to dequeue it has not yet
      executed.
*/
      enqueuedom(cpudibp->rootnode, &port->readcaller);
      abandonj();
      return;
   } /* End UART_WaitandReadData */

   else if (UART_GetGDBPacket < cpuordercode
         && UART_GetGDBPacket+4096 >= cpuordercode) {

      register long limit = cpuordercode-UART_GetGDBPacket;
      int len=0;
      
      switch (ensurereturnee(1)) {
        case ensurereturnee_wait:  abandonj();
           return;
        case ensurereturnee_overlap: midfault();
           return;
        case ensurereturnee_setup: handlejumper();
      } /* End switch ensurereturnee */
     /* End dry run */

     s=spltty();
      
     while(sparc_lineb_getchar() != '$');  /* wait for begin */

     while(len < limit) {
       cpuargpage[len]=sparc_lineb_getchar(); 
       if(cpuargpage[len] == '#') {  // get 2 more for checksum
          len++;
          cpuargpage[len]=sparc_lineb_getchar();
          len++;
          cpuargpage[len]=sparc_lineb_getchar();
          len++;
          break;
       }
       if(cpuargpage[len] == '$') len=0;
       else len++;
     }
     splx(s);

     cpuordercode=0;
     if(getreturnee()) return;
     cpuexitblock.argtype = arg_regs;
     cpuarglength = len;
     cpuargaddr = cpuargpage;
     cpuexitblock.keymask = 0;
     return_message();
     return; 
 
   } /* End of GetGDBPacket HACK!!  */

   else if (UART_PutGDBPacket == cpuordercode) {
     unsigned long len = cpuarglength;
     char *ptr;
     
     if(len > 2048) {
         simplest(KT+3);
         return;
     }
     if(cpuexitblock.argtype != arg_memory) {
         simplest(KT+3);
         return;
     }

     ptr=cpuargaddr; 
     s=spltty();
     while(len) {
         sparc_lineb_putchar(*ptr);
         ptr++;
         len--;
     }
     splx(s);

     simplest(0);
     return;
   }
   else if (UART_PutGetGDBPacket < cpuordercode
         && UART_PutGetGDBPacket+4096 >= cpuordercode) {

      register long limit = cpuordercode-UART_PutGetGDBPacket;
      unsigned long len = cpuarglength;
      char *ptr;

     if(len > 2048) {
         simplest(KT+3);
         return;
     }
     if(cpuexitblock.argtype != arg_memory) {
         simplest(KT+3);
         return;
     }
      switch (ensurereturnee(1)) {
        case ensurereturnee_wait:  abandonj();
           return;
        case ensurereturnee_overlap: midfault();
           return;
        case ensurereturnee_setup: handlejumper();
      } /* End switch ensurereturnee */
     /* End dry run */

     s=spltty();
resend:
     ptr=cpuargaddr; 
     while(len) {
         sparc_lineb_putchar(*ptr);
         ptr++;
         len--;
     }
     if(sparc_lineb_getchar() != '+') goto resend;

     while(sparc_lineb_getchar() != '$');  /* wait for begin */

     len=0;
     while(len < limit) {
       cpuargpage[len]=sparc_lineb_getchar(); 
       if(cpuargpage[len] == '#') {  // get 2 more for checksum
          len++;
          cpuargpage[len]=sparc_lineb_getchar();
          len++;
          cpuargpage[len]=sparc_lineb_getchar();
          len++;
          break;
       }
       if(cpuargpage[len] == '$') len=0;
       else len++;
     }
     splx(s);

     cpuordercode=0;
     if(getreturnee()) return;
     cpuexitblock.argtype = arg_regs;
     cpuarglength = len;
     cpuargaddr = cpuargpage;
     cpuexitblock.keymask = 0;
     return_message();
     return; 
 
   } /* End of PutGetGDBPacket HACK */ 

   else if (UART_PutDataGetResponse < cpuordercode
         && UART_PutDataGetResponse+4096 >= cpuordercode) {

      long len = cpuarglength;
      char *ptr;

      if(len > 4096) {
         simplest(KT+3);
         return;
      }
      if(cpuexitblock.argtype != arg_memory) {
         simplest(KT+3);
         return;
      }

      switch (ensurereturnee(1)) {
        case ensurereturnee_wait:  abandonj();
           return;
        case ensurereturnee_overlap: midfault();
           return;
        case ensurereturnee_setup: handlejumper();
      } /* End switch ensurereturnee */

      s=spltty();

      sparc_lineb_putchar( *(((char *)&len)+2) );
      sparc_lineb_putchar( *(((char *)&len)+3) );
      ptr=cpuargaddr; 
      while(len) {
         sparc_lineb_putchar(*ptr);
         ptr++;
         len--;
      }
      len=timed_sparc_lineb_getchar(10000000);  /* initial 10 seconds */
      if(len == -1) {
          cpuordercode = -1;
          len=0;
          goto leaveputdata;
      }
      cpuargpage[0]=len; 
      len=timed_sparc_lineb_getchar(1000000);
      if(len == -1) {
          cpuordercode = -1;
          len=0;
          goto leaveputdata; 
      }
      cpuargpage[1]=len; 
      splx(s);
      len = 2;

      cpuordercode=0;
leaveputdata:;
      if(getreturnee()) return;
      cpuexitblock.argtype = arg_regs;
      cpuarglength = len;
      cpuargaddr = cpuargpage;
      cpuexitblock.keymask = 0;

      return_message();

   }
   else if (UART_SendRdyGetData < cpuordercode
         && UART_SendRdyGetData+4096 >= cpuordercode) {

      register long limit = cpuordercode-UART_SendRdyGetData;
      unsigned long len = cpuarglength;
      unsigned long count;
      int i;
      char *ptr;

      if(len != 2) {
         simplest(KT+3);
         return;
      }
      if(cpuexitblock.argtype != arg_memory) {
         simplest(KT+3);
         return;
      }

      switch (ensurereturnee(1)) {
        case ensurereturnee_wait:  abandonj();
           return;
        case ensurereturnee_overlap: midfault();
           return;
        case ensurereturnee_setup: handlejumper();
      } /* End switch ensurereturnee */

      s=spltty();
      ptr=cpuargaddr; 
      while(len) {
         sparc_lineb_putchar(*ptr);
         ptr++;
         len--;
      }

      i=timed_sparc_lineb_getchar(1000000);
      if(i == -1) {
          cpuordercode = -1;
          len=0;
          goto leavesndrdy; 
      }
      cpuargpage[0]=i; 
      i=timed_sparc_lineb_getchar(1000000);
      if(i == -1) {
          cpuordercode = -1;
          len=0;
          goto leavesndrdy; 
      }
      cpuargpage[1]=i; 
      count=( (unsigned char)cpuargpage[0] << 8) | (unsigned char)cpuargpage[1];

      if(count > 4096) {
         cpuordercode=count;
         len=0;
         goto leavesndrdy;
      }

      ptr=cpuargpage; 
      len=0;
      while(count) {
          i=timed_sparc_lineb_getchar(1000000);
          if(i == -1) {
             cpuordercode = -1;
             len=0;
             goto leaveputdata; 
          }
         *ptr=i;
         if(len < limit) {
           ptr++;
           len++;
         }
         count--;;
      }
      splx(s);

      cpuordercode=0;
leavesndrdy:;
      if(getreturnee()) return;
      cpuexitblock.argtype = arg_regs;
      cpuarglength = len;
      cpuargaddr = cpuargpage;
      cpuexitblock.keymask = 0;

      return_message();
   }
 
   else {              /* Ordercode unknown */
      if (cpuordercode == KT) simplest(0x209);
      else simplest(KT+2);
      return;
   } /* End unknown order code */
}
 
 
/**********************************************************************
   End routines that run at interrupt level 0
**********************************************************************/
void uart_outtest(void)
{
/*
         Memcpy(linea.ostart, "OUTPUT OK", 9);
         linea.ocp = linea.ostart;     
         linea.oed = linea.ostart + 9;
         linea.state |= CONWRITE;          
*/
}
