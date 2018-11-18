extern void clkinit(void);
void checkptwakeup(void);
void checkwakeup(void);
extern uint64 read_system_timer(void);
void set_system_timer(uint64 time);
extern void start_process_timer(void);
extern bool stop_process_timer(void);
extern void set_process_timer(unsigned long);
extern unsigned long read_process_timer(void);
extern void timerinterrupt(void);
void jcalclock(void);

struct CalClock {
   unsigned char value[8];   /* BCD YYYYMMDDWWHHMMSS */
};

struct CalSysTime {
	struct CalClock CalTime;
	uint64 SysTime;
	};

struct CalClock read_calendar_clock(void);

