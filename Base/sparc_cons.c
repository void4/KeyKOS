/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include <string.h>
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
#include "sparc_asm.h"
// #include "promif.h"
#include "kuart.h"
#include "consmdh.h"
#include "kermap.h"
#include "grestarh.h"
#include "scafoldh.h"
#include "sparc_cons.h"
#include "memutil.h"

#define INTR 0x03
#define LF 0x0a
#define CR 0x0d
#define BS 0x08
#define CTLS 0x13
#define CTLQ 0x11

 
 
#define BUFSIZE 4096

extern int prom_mayput(char c);
extern int prom_mayget(void);
#define consmayput prom_mayput
#define consmayget prom_mayget
 
struct conblock {       /* The console block */
 
   struct KernelTask kernel_task; /* The kernel task to wake waiters */
 
   uchar *istart,       /* Start of the input buffer */
        *iend,          /* End of the input buffer */
        *ipp,           /* Producer pointer into the input buffer */
        *iechop,        /* Echo pointer into the input buffer */
                        /* Also last char+1 of data to return to sik */
        *icp;           /* Consumer pointer into the input buffer */
 
   long limit;          /* The SIK caller's limit */
 
   struct QueueHead readcaller;  /* The queue for the read waiter */
   struct QueueHead writecaller; /* The queue for the write waiter */
 
   uchar activationset[32]; /* The activation set */
   uchar echoset[32];  /* The echo set */

   uchar state;          /* The current interface state */
#define CONIDLE 0          /* Neither reading or writing */
#define CONREAD 2          /* CONSIK is active with no echo */
#define CONREADEAI 4       /* CONSIK echo at interrupt active */
#define CONREADEFB 6       /* CONSIK echo from buffer active */
#define CONREADIDLE (~6)   /* To turn read type active bits off */
        /* State modifiers */
#define CONXONXOFF 8       /* XON/XOFF backpressure in effect */
#define CONRTSCTS 16       /* RTS/CTS backpressure in effect */

   uchar keyflags;       /* Flags associated with key state */
#define KEYSOKDONE 1       /* Allow next sok caller to continue */
#define KEYSIKDONE 2       /* Allow next sik caller to receive input */
#define KEYACTIVATENOW 4   /* Next sik caller must activate w/o read */
#define KEYCHANGECIRCUIT 8 /* New circuit has arrived or circuit zap */
#define KEYKERTASK 128     /* Kernel task has been scheduled */

   uchar flags;          /* Control flags */
#define BACKPRESSURED 1    /* Interface has sent XOFF to us */
#define BACKPRESSUREON 2   /* We sent XOFF to the interface */
#define GOBBLING 4         /* We are discarding output */
#define IOVERFLOW 8        /* Input buffer has overflowed */
#define CRasCRLF 16        /* Echo CR as CRLF */
#define LFasLFCR 32        /* Echo LF as LFCR */
#define HALFECHO 64        /* 1st char of a CRLF or LFCR echoed */
#define CONNECTED 128      /* Terminal is connected */
 
   uchar ibuffer[BUFSIZE]; /* The input buffer */

} romconsole;
 
typedef struct conblock CONBLOCK;
 
struct QueueHead *romconsolequeues = &romconsole.readcaller; /* CHECK ptr */
 
static void reset_port(CONBLOCK *port)
                           /* Clear the input and output buffers */
/* Must be called from level 7 if port could be active */
{
   port->istart = port->ibuffer;
   port->iend = port->ibuffer + sizeof port->ibuffer;
   port->ipp = port->ibuffer;
   port->iechop = port->ibuffer;
   port->icp = port->ibuffer;
   port->flags = CRasCRLF;      /* Set default flags */
   memzero(port->activationset, 32); /* Set the activation set */
   Memcpy(port->activationset, "\x08\x04", 2); /* to ^D and CR only */
   Memcpy(port->activationset+16, port->activationset, 16);
   memzero(port->echoset, 32); /* Set the echo set to CR + printable */
   Memcpy(port->echoset,
       "\0\204\0\0\377\377\377\377\377\377\377\377\377\377\377\376", 16);
   Memcpy(port->echoset+16, port->echoset, 16);
}


/**********************************************************************
   The kernel task routine
**********************************************************************/
 
static void console_kernel_task(struct KernelTask *kert)
                             /* Serve the queues that must be served */
{
   CONBLOCK *port = (CONBLOCK *)kert;
   uchar keyflags;
   int s;
 
   s = spltty();                        /* Goto tty priority level */

   keyflags = port->keyflags;     /* Get the active flags */
   port->keyflags &= ~(KEYKERTASK|KEYSOKDONE); /* Reset flags */
 
   splx(s);                        /* Back to old priority level */
 
   if (keyflags & KEYSIKDONE) {
      enqmvcpu(&port->readcaller);
   }
   if (keyflags & KEYSOKDONE) {
      enqmvcpu(&port->writecaller);
   }
}
 
 
 
/**********************************************************************
   Begin initialization routines
**********************************************************************/
 
static void init_port(register CONBLOCK *port)  /* Initialize a port */
{
   port->kernel_task.kernel_task_function = console_kernel_task;
   reset_port(port);
   port->readcaller.head = (union Item *)&port->readcaller;
   port->readcaller.tail = (union Item *)&port->readcaller;
   port->writecaller.head = (union Item *)&port->writecaller;
   port->writecaller.tail = (union Item *)&port->writecaller;
   port->state = CONIDLE;
   port->keyflags = 0;
}
 

void jromconinit(void)    /* Initialize the conblock */
{
   init_port(&romconsole);
}

 
/**********************************************************************
   End initialization routines
**********************************************************************/
 
 
 
/**********************************************************************
   Begin routines that run at interrupt level 7
**********************************************************************/
 
 
static void wakeup(register CONBLOCK *port)
                         /* puts port's task on the kernel task queue */
{
   if (!(port->keyflags & KEYKERTASK)) {
      enqueue_kernel_task(&port->kernel_task);
      port->keyflags |= KEYKERTASK;
   }
}
 
 
static void bufic(              /* Buffer an input character */
   register uchar c,                 /* The input character */
   register CONBLOCK *port)          /* The console block to use */
{
   c &= 0x7f;

   if (port->flags & IOVERFLOW) return; /* Overflowed */
   *port->ipp++ = c;              /* Put char into buffer */
   if (port->ipp == port->iend)    /* If cursor at end */
      port->ipp = port->istart;      /* Wrap cursor */
   if (port->ipp == port->icp) {   /* Buffer overflow */
      port->ipp--;                   /* Backup cursor */
      if (port->ipp == port->istart - 1) port->ipp = port->iend - 1;
      port->flags |= IOVERFLOW;       /* Indicate overflow */
   }
}
 

static void checkactivation(c, port) /* Check if SIK should activate */
   register uchar c;                 /* The input character */
   register CONBLOCK *port;          /* The console block to use */
{
   if (port->activationset[c>>3] & 0x80 >> (c & 7)
        || (port->ipp >= port->icp
              ? port->ipp - port->icp
              : port->ipp + BUFSIZE - port->icp)
             >= port->limit
        || (port->flags & IOVERFLOW
            && port->ipp == (port->icp-1 < port->istart
                             ?port->iend-1
                             :port->icp-1))) {
      port->keyflags |= KEYSIKDONE;
      wakeup(port);
      port->state &= CONREADIDLE;
   }
}


static void interruptecho(       /* Echo a character when received */
   register uchar c,                 /* The input character */
   register CONBLOCK *port)          /* The console block to use */
{
   if (port->echoset[c>>3] & 0x80 >> (c & 7)) { /* Char in echo set */
      consmayput(c);    /* Echo our character */
      if ((c == CR || c == CR+0x80) && port->flags & CRasCRLF) {
         consmayput(LF);
      } /* End carriage return and crlf mode */
      else if ((c == LF || c == LF+0x80) && port->flags & LFasLFCR) {
         consmayput(CR);
      } /* End linefeed and lfcr mode */
      else if (c == BS || c == BS+0x80) {  /* echo backspace */
         consmayput(' ');
         consmayput(BS);
         if (port->ipp != port->icp   /* Remove backspace */
            && --(port->ipp) < port->istart) port->ipp = port->iend-1;
         if (port->ipp != port->icp   /* Remove char backspaced over */
            && --(port->ipp) < port->istart) port->ipp = port->iend-1;
      }
      port->iechop = port->ipp;
      checkactivation(c,port);
   } /* End char is in echo set */
   else {             /* Char is not in the echo set */
      port->iechop = port->ipp;   /* Just skip the character */
      checkactivation(c,port);
   } /* End char is not in the echo set */
}

 
void romconsoleinterrupt(void)    /* Receive character ready */
{
   for (;;) {                        /* For all pending i/p chars */
      register int c = consmayget();
 
      /* If we encounter L1-A, enter debugger */
      if ((c & 0x7f) == 0x0) {
	Panic(); // Once was omak_default_breakpt();
//        sparc_gdb_enter();
	return;
      }
      if (-1 == c)
   break;
      if (CTLS == (c&0x7f) && !(romconsole.flags&BACKPRESSURED)) {
         romconsole.flags |= BACKPRESSURED;
   continue;
      }
      if (CTLQ == (c&0x7f) && (romconsole.flags&BACKPRESSURED)) {
         romconsole.flags &= ~BACKPRESSURED;
         romconsole.keyflags |= KEYSOKDONE;
         wakeup(&romconsole);
   continue;
      }
      switch (romconsole.state) {
       case CONIDLE:           /* Neither reading or writing */
       case CONREADEFB:        /* CONSIK echo from buffer active */
         bufic(c,&romconsole);    /* Buffer the input character */
         break;
       case CONREAD:           /* CONSIK is active with no echo */
         bufic(c,&romconsole);    /* Buffer the input character */
         romconsole.iechop = romconsole.ipp;/* Set end of data to return */
         checkactivation(c,&romconsole); /* Check for activation */
         break;
       case CONREADEAI:        /* CONSIK echo at interrupt active */
         bufic(c,&romconsole);    /* Buffer the input character */
         if (!(romconsole.flags & IOVERFLOW))  /* If it went into buffer */
            interruptecho(c,&romconsole); /* Echo and activation test */
         break;
       default: crash("CONSOLE001 Invalid port state");
      } /* End switch on port state */
   } /* End for all pending i/p chars */
}
 

static void echofrombuffer(      /* Echo a character from the ibuffer */
   register CONBLOCK *port)          /* The console block to use */
{
   register uchar *ep;

   for (ep=port->iechop; ep != port->ipp;) { /* Step through echos */
      const uchar c = *ep & 0x7f;

      if (port->echoset[c>>3] & 0x80 >> (c & 7)){ /* In echo set */
         consmayput(c);    /* Echo our character */
         port->iechop++;   /* And step echo pointer */
         if ((c == CR) && port->flags & CRasCRLF) {
            consmayput(LF);
         } /* End carriage return and crlf mode */
         else if ((c == LF) && port->flags & LFasLFCR) {
            consmayput(CR);
         } /* End linefeed and lfcr mode */
         else if (c == BS) {  /* echo backspace */
            consmayput(' ');
            consmayput(BS);
            if (port->iechop != port->icp   /* Remove backspace */
               && --(port->iechop) < port->istart) port->iechop = port->iend-1;
            if (port->iechop != port->icp   /* Remove char backspaced over */
               && --(port->iechop) < port->istart) port->iechop = port->iend-1;
         }
      }
      if (++ep == port->iend) ep = port->istart;
      checkactivation(c,port);
      if (port->keyflags & KEYSIKDONE) { /* Input finished */
         uchar *top;

         /* Move typed ahead data down to cover backspaced data */
         for (top=port->iechop; ep != port->ipp;) {
            *top = *ep;
            if (++ep == port->iend) ep = port->istart;
            if (++top == port->iend) top = port->istart;
         }
         port->ipp = top;

   return;
      }
   }  /* End step through echos */

   port->state = CONREADEAI;                /* Echo at int */
   port->ipp = port->iechop;   /* Start reading after last echoed */
} /* End echofrombuffer */
 
/**********************************************************************
   End routines that run at interrupt level 7
**********************************************************************/
 
 
 
/**********************************************************************
   Begin routines that run at interrupt level 0
     N.B. These routines may goto interrupt level 7 for serialization.
**********************************************************************/


static void cosimple(     /* Simple return from sik or sok key */
   long rc,                  /* The return code to pass */
   struct Key *key)          /* The key being invoked */
/*
   Output - following set up:
     cpudibp - pointer to the dib to run
     cpuentrytblock - Describes parameters for the jumpee
*/
{
   cpuordercode = rc;               /* Set return code */
   cpuarglength = 0;                /* No string */
   cpup4key = *key;
   jsimple(1);  /* co routine */
}


int getconschar(void)
{
   int c;
   int s;

   s = spltty();                    /* Goto tty priority level */

   if (romconsole.icp != romconsole.ipp) {        /* There is input */
      c = *romconsole.icp & 0x7f;
      if (romconsole.iechop == romconsole.icp) {
         if (++romconsole.iechop == romconsole.iend)
             romconsole.iechop = romconsole.istart;
      }
      if (++romconsole.icp == romconsole.iend)
          romconsole.icp = romconsole.istart;
   } else {                             /* No input */
      c = -1;
   }

   splx(s);

   return c;
}

void consprint(const char *p)  /* Kernel debugging output */
{
   int s;

   /* ZZZ need to turn off clock interrupt,
      the same was done to prom_printf(). */
     
   s = splclock();
   while (*p) {
      consmayput(*p);
      if (*p == '\n') consmayput('\015');
      p++;
   }
   (void) splx(s);
}


char consgetchar(void)
{
   for (;;) {
      register char c = consmayget();
      if (-1 != c) return c;
   }
}


void jromconsole(    /* Handle device key calls for romconsole device */
   struct Key *key)     /* The key being invoked */
/* Other input -
   cpudibp - has the jumper's DIB
   cpuordercode - has order code.
   The invoked key type is device, the subtype is DEVKMASTERCPU,
                                   and the device is DEVKEYCONSOLE
*/
{
   register CONBLOCK *port;        /* The console block to use */
   register int cc;
   register int s;

   switch (key->nontypedata.devk.device) { /* Get selected device */
    case DEVKEYCONSOLE: port = &romconsole;
                        break;
    default: simplest(KT+3);
             return;
   } /* End get selected device */

   switch (key->nontypedata.devk.type) { /* Select key subtype */
    case 0:                /* Key is a CCK key */
      switch (cpuordercode) {  /* Switch on order code */
       case CONCCK__WAIT_FOR_CONNECT: {
         /* Terminal is connected */
         cpup1key.type = devicekey;      /* Set up SIK key */
         cpup1key.nontypedata.devk.slot = key->nontypedata.devk.slot;
         cpup1key.nontypedata.devk.device =
                        key->nontypedata.devk.device;
         cpup1key.nontypedata.devk.type = 1;  /* SIK key */
   /*    cpup1key.nontypedata.devk.serial = ...  */

         cpup2key.type = devicekey;      /* Set up SOK key */
         cpup2key.nontypedata.devk.slot = key->nontypedata.devk.slot;
         cpup2key.nontypedata.devk.device =
                        key->nontypedata.devk.device;
         cpup2key.nontypedata.devk.type = 2;  /* SOK key */
   /*    cpup2key.nontypedata.devk.serial = ...  */

         cpup3key.type = devicekey;      /* Set up CCK key */
         cpup3key.nontypedata.devk.slot = key->nontypedata.devk.slot;
         cpup3key.nontypedata.devk.device =
                        key->nontypedata.devk.device;
         cpup3key.nontypedata.devk.type = 0;  /* CCK key */
   /*    cpup3key.nontypedata.devk.serial = ...  */

         cpuordercode = 0;                   /* Return code */
         cpuarglength = 0;                   /* No returned string */
         jsimple(8+4+2);                          /* Finish jump, three keys */
         return;                             /* And return */
       }
       case CONCCK__SET_BACKPRESSURE:
/*       ...
       case CONCCK__SEND_ZAP:
         ...
       case CONCCK__START_GOBBLING:
         ...
       case CONCCK__WAIT_FOR_ZAP:
         ...                            */
       case CONCCK__ACTIVATE_NOW:
         switch (ensurereturnee(0)) {
          case ensurereturnee_wait:  abandonj();
             return;
          case ensurereturnee_overlap: midfault();
             return;
          case ensurereturnee_setup: handlejumper();
         }
         /* End dry run */

         s = spltty();                /* Got tty priority level */

         port->keyflags |= KEYSIKDONE;   /* Next caller will run */
         port->state &= CONREADIDLE;

         splx(s);                /* Back to old level */

         enqmvcpu(&port->readcaller);    /* Run any waiter */

         if (getreturnee()) return;
         cpuexitblock.keymask = 0;
         cpuarglength = 0;
         cpuordercode = 0;
         return_message();
         return;

       case CONCCK__STOP_GOBBLING:
/*       ...
       case CONCCK__CR_AS_CRLF:
         ...
       case CONCCK__CR_AS_CR:
         ...
       case CONCCK__LF_AS_LFCR:
         ...
       case CONCCK__LF_AS_LF:
         ...
       case CONCCK__SET_ACTIVATION_SET:
         ...
       case CONCCK__SET_ECHO_SET:
         ...  */

/* console jump logging support */

       case CONCCK__START_LOG:
           lowcoreflags.gatelogenable=1;
           lowcoreflags.logbuffered=0;
           simplest(0);
           return;
       case CONCCK__START_BUFFERED_LOG:
           lowcoreflags.gatelogenable=1;
           lowcoreflags.logbuffered=1;
           simplest(0);
           return;
       case CONCCK__PRINT_LOG:
           printlog();
           simplest(0);
           return;
       case CONCCK__STOP_LOG:
           lowcoreflags.gatelogenable=0;
           lowcoreflags.logbuffered=0;
           simplest(0);
           return;
/* end console jump logging support */

       default:
         if (cpuordercode == KT) simplest(0x0C09);
         else simplest(KT+2);
         return;
      } /* End switch on CCK order code */

    case 1:                /* Key is a SIK key */
      {
         register long limit = cpuordercode;  /* user's limit */
         register uchar echo;  /* Set to CONREAD or CONREADEFB */

 /*  KLUDGE for polling keyboard during writes   ....  */
         if (limit == -1)   { /* poll request */
            cc=getconschar();
            cosimple(cc,key);
            return;
         }
 /* end KLUDGE  */

         if (limit <= 0) {
            cosimple(KT+2,key);
            return;
         }
         if (limit <= 4096) {      /* Read without echo */
            echo = CONREAD;
         }
         else {                    /* Read with echo */
            limit -= 0x2000;          /* Turn off echo bit */
            if (limit <= 0 || limit > 4096) {
               cosimple(KT+2,key);
               return;
            }
            echo = CONREADEFB;
         }
         if (limit > BUFSIZE-1) limit = BUFSIZE-1;

         if (port->keyflags & KEYSIKDONE) { /* Input finished */
            int len;

            port->keyflags &= ~KEYSIKDONE; /* Turn off flag */
            port->state &= CONREADIDLE;

            switch (ensurereturnee(1)) {
             case ensurereturnee_wait:  abandonj();
                return;
             case ensurereturnee_overlap: midfault();
                return;
             case ensurereturnee_setup: handlejumper();
            } /* End switch ensurereturnee */
            /* End dry run */

            s = spltty();                    /* Goto tty priority level*/

            if (port->icp <= port->iechop) { /* Input doesn't wrap */
               len = port->iechop - port->icp; /* Length of data */
               Memcpy(cpuargpage,port->icp, len); /* Copy data */
               port->icp += len;
            } /* End input doesn't wrap */
            else {                           /* Input wraps */
               int len1 = port->iend - port->icp; /* Len of 1st part */

               Memcpy(cpuargpage,port->icp, len1);   /* Copy */
               len = port->iechop - port->istart; /* Len of rest */
               Memcpy(cpuargpage+len1, port->istart, len);   /* Copy */
               port->icp = port->istart + len;
               len += len1;
            } /* End input wraps */

            if (port->flags & IOVERFLOW && port->icp == port->ipp) {
               port->flags &= ~IOVERFLOW; /* Turn off overflow */
               cpuordercode = 1;          /* Set return code */
            }
            else cpuordercode = 0;        /* No overflow */
     /*     cpuordercode = ... 0=ok, 1=dataloss, 2=linedrop   */

            splx(s);                    /* Goto old priority level */

            if (getreturnee()) return;
            cpuexitblock.argtype = arg_regs;
            cpuarglength = len;
            cpuargaddr = cpuargpage;
            cpuexitblock.keymask = 1;        /* Pass "coroutine" key */
            cpup4key = *key;
            return_message();
            return;
         } /* End input finished */

         if (!(port->state & ~CONREADIDLE)) {/* Read not in progress */
 
            port->limit = limit;                 /* Set sik limit */
            port->state |= echo;                 /* Start read */

            s = spltty();
            if (echo == CONREADEFB) {
               echofrombuffer(port);
            } else {
               for (; !(port->keyflags & KEYSIKDONE)
                       && port->iechop != port->ipp;
                    port->iechop++)
               checkactivation(*port->iechop,port);
            }
            splx(s);                    /* Return to old priority level */
         } /* End read not in progress */

/*
         If the input finishes between now and the call to enqueuedom
         the domain will still be properly dequeued since the kernel
         task stacked by the level 7 code to dequeue it has not yet
         executed.
*/

         enqueuedom(cpudibp->rootnode, &port->readcaller);
         abandonj();
         return;
      } /* End key is a SIK key */

    case 2:                /* Key is a SOK key */
      if (port->flags & BACKPRESSURED) {  /* Backpressure applied */
         enqueuedom(cpudibp->rootnode, &port->writecaller);
         abandonj();
         return;
      }
      switch (cpuordercode) {  /* Switch on order code */
       case 0: {                  /* Normal write */
         long len = cpuarglength;

         if (len > BUFSIZE) len = BUFSIZE;  /* Set length */

         for (; len; len--) {
            uchar c;

            pad_move_arg(&c, 1);
/*  KLUDGE ...*/
   	    /* ZZZ need to turn off clock interrupt,
      		the same was done to prom_printf(). */
    	    s = splclock();
            if(c == '\n') consmayput('\r');
            consmayput(c);
	    splx(s);
/*  KLUDGE ...*/
            if (port->icp != port->ipp) {
               uchar *cp;

               cp = port->ipp-1;
               if (cp<port->istart) cp = port->iend-1;
               cc = *cp & 0x7f;
               if(cc == INTR) {  /* indicate interrupt with 0 limit */
                  if (port->iechop == port->ipp) port->iechop = cp;
                  port->ipp = cp;
                  cosimple(0,key);
                  return;
               }
            }
/* end KLUDGE */
         }

         cosimple(BUFSIZE,key); /* Return new limit */
         return;
       }
       case 1:                    /* Write followed by break */
    /*   ...   */

       default:
         cosimple(KT+2,key);
         return;
      } /* End switch on SOK order code */
    default: crash("CONSOLE003 Invalid subtype in CPU board key");
   }
}


/**********************************************************************
   End routines that run at interrupt level 0
**********************************************************************/
