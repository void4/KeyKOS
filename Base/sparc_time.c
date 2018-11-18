/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

   /* Timer routines for SPARC */

#include "sysdefs.h"
#include "kktypes.h"
#include "lli.h"
#include "kermap.h"
#include "splh.h"
#include "timeh.h"
#include "kschedh.h"
#include "ktqmgrh.h"
#include "cpujumph.h"
#include "timemdh.h"
#include "primcomh.h"
#include "kernelpk.h"
// #include "clock.h"
extern caddr_t v_eeprom_addr;
struct mostek48T02 {
	volatile uchar_t	clk_ctrl;	/* ctrl register */
	volatile uchar_t	clk_sec;	/* counter - seconds 0-59 */
	volatile uchar_t	clk_min;	/* counter - minutes 0-59 */
	volatile uchar_t	clk_hour;	/* counter - hours 0-23 */
	volatile uchar_t	clk_weekday;	/* counter - weekday 1-7 */
	volatile uchar_t	clk_day;	/* counter - day 1-31 */
	volatile uchar_t	clk_month;	/* counter - month 1-12 */
	volatile uchar_t	clk_year;	/* counter - year 0-99 */
};
#define CLOCK ((struct mostek48T02 *)v_eeprom_addr+0x1FF8)

#define SCHEDULER 2


#define min(a,b) (a<b?a:b)

extern struct KernelPage *kernelpagept;
static uint64 true_time = 0;
static uint64 sys_timer_offset = 0; /* to ensure system timer is unique */
uint64 system_time; /* the sum of the above two */
static const uint64 time10ms = 40960000;
unsigned long process_timer = 0;
bool process_timer_on = FALSE;

/*
 * Tables to convert a single byte to/from binary-coded decimal (BCD).
 */
u_char byte_to_bcd[256] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
};

u_char bcd_to_byte[256] = {             /* CSTYLED */
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0,
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0,
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0,
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
        60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
        70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89,  0,  0,  0,  0,  0,  0,
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
};

#define BYTE_TO_BCD(x)  byte_to_bcd[((x) & 0xff) % 100]
#define BCD_TO_BYTE(x)  bcd_to_byte[(x) & 0xff]

extern void uart_interrupt(void);

#if SCHEDULER == 2
struct DIB *tickdib = 0;
#endif

void checkptwakeup(void)
/* See if the process timer has gone off. */
/* Must be called with clock interrupts disabled. */
/* Maintains the following assertion, which is true whenever
   clock interrupts are enabled:
      processtimerktactive || (process_timer > 0). */
{
   if (processtimerktactive) return; /* already active */
#if SCHEDULER != 2
   if (process_timer == 0) { /* activate */
      processtimerktactive = TRUE;
      enqueue_kernel_task(&processtimerkt);
   }
#else
   processtimerktactive = TRUE;
   tickdib = cpudibp;
   enqueue_kernel_task(&processtimerkt);
#endif
}

void checkwakeup(void)
/* See if kwakeuptime has been reached. */
/* Must be called with clock interrupts disabled. */
/* Maintains the following assertion, which is true whenever
   clock interrupts are disabled:
      timektactive || (kwakeuptime > read_system_timer()). */
{
   if (timektactive) return; /* already active */
   //if (llicmp(&kwakeuptime, &system_time) <= 0)
   if (kwakeuptime <= system_time) { /* activate */
      timektactive = TRUE;
      enqueue_kernel_task(&timekt);
   }
}

uint64 read_system_timer(void)
/* This routine always returns a different value. */
/* Preserves the interrupt enable level. */
{
   unsigned int level = splhi(); /* disable interrupts so
      true_time and sys_timer_offset won't change while we
      are looking at them. */
   ++sys_timer_offset;
   system_time = true_time + sys_timer_offset;
   /* Since we increased the system timer (by one unit, for uniqueness),
      we need to check if there is anyone to wake up. */
   checkwakeup();

   splx(level);    /* restore interrupt level */
   return system_time;
}

void set_system_timer(uint64 time)
/* Called at startup to restore the system time from checkpoint. */
{
   true_time = time;
   sys_timer_offset = 0;
}

void timerinterrupt(void)
{
   /* ZZZ poll uart for now */
   uart_interrupt();

   if (lowcoreflags.timeless)
	return;

   // lliadd(&true_time, &time10ms); /* bump the clock */
   true_time += time10ms;
   //sys_timer_offset.low -=  /* reset unique counter */
   //   min(sys_timer_offset.low, time10ms.low);
   sys_timer_offset =  sys_timer_offset < time10ms ? 0 : sys_timer_offset - time10ms;
   /* Get value of system timer and check for wakeup. */
   system_time = true_time;
   // lliadd(&system_time, &sys_timer_offset);
   system_time += sys_timer_offset;
   /* update the time stamp in the shared kernel page */
   /* if the journal page is to be read from a checkpoint, we may not
      have set kernelpagept when the clock interrupt was first started */
   if (kernelpagept)
      kernelpagept->KP_system_time = system_time;
   checkwakeup();
#if SCHEDULER != 2
   if (process_timer_on) {
      if (process_timer > 160000)
         process_timer -= 160000;
      else {
         process_timer = 0;
         checkptwakeup();
      }
   }
#else
   if (process_timer_on) checkptwakeup();
#endif
}

void start_process_timer(void)
{  process_timer_on = TRUE;
}

bool stop_process_timer(void)
/* Returns TRUE iff it was on. */
{  
   bool oldpto = process_timer_on;
   process_timer_on = FALSE;
   return oldpto;
}

void set_process_timer(
   unsigned long v)
{
   int s;

   s = splhi();
   process_timer = v;
#if SCHEDULER != 2
   checkptwakeup();
#endif
   splx(s);
}

unsigned long read_process_timer(void)
{  return process_timer;
}

static unsigned char clockcen = 0x20; /* the century */

struct CalClock read_calendar_clock(void)
{
   	struct CalClock ret;

	/* ZZZ
	srmmu_chgprot(&kas, (caddr_t)((u_int)CLOCK & PAGEMASK), PAGESIZE,
		PROT_READ | PROT_WRITE);

	CLOCK->clk_ctrl |= CLK_CTRL_READ;
	*/
   	ret.value[0] = clockcen;
   	ret.value[1] = BYTE_TO_BCD(BCD_TO_BYTE(CLOCK->clk_year) + 68/*YRBASE*/);
   	ret.value[2] = CLOCK->clk_month & 0x1f;
   	ret.value[3] = CLOCK->clk_day & 0x3f;
   	ret.value[4] = CLOCK->clk_weekday & 0x7;
   	ret.value[5] = CLOCK->clk_hour & 0x3f;
   	ret.value[6] = CLOCK->clk_min & 0x7f;
   	ret.value[7] = CLOCK->clk_sec & 0x7f;

	/* ZZZ
	CLOCK->clk_ctrl &= ~CLK_CTRL_READ;

	srmmu_chgprot(&kas, (caddr_t)((u_int)CLOCK & PAGEMASK), PAGESIZE,
			PROT_READ);
	*/
   	return ret;
}

void jcalclock(void)
/* Handle invocation of calclock key. */
{
	struct CalClock clockval;
	struct CalSysTime	times;
	   
   switch (cpuordercode) {

    case 8: /* read clock */
      clockval = read_calendar_clock();
      cpuargaddr = (char *)&clockval;
      cpuarglength = 8;
      cpuexitblock.argtype = arg_regs;
      cpuordercode = 0;
      jsimple(0);  /* no keys */
      return;

    case 9: /* set clock */
      pad_move_arg(&clockval, 8);
      simplest(KT+2); /* not implemented yet */
      return;

	case 10: /* return calendar time and systime */
		times.CalTime = read_calendar_clock();
 
		times.SysTime = read_system_timer();  /* read the timer */
		cpuexitblock.argtype = arg_regs;
		cpuargaddr = (char *)&times;
		cpuarglength = 16;
		cpuordercode = 0;
		jsimple(0);  /* no keys */
		return;

    case KT:
      simplest(0x609);
      return;
    default:
      simplest(KT+2);
      return;
   } /* end of switch cpuordercode */
}


void clkinit(void)
{  /* nothing to do */
}
