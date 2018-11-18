#ident "@(#)enexmdh.h	1.2 02 Mar 1995 18:05:03 %n%"
#define get_arg_pointer(dib) dib->regs[8+3]	/* Domain's o3 */
#define get_arg_length(dib) dib->regs[8+4]	/* Domain's o4 */
#define get_parm_pointer(dib) dib->regs[8+5]	/* Domain's o5 */
#define get_parm_length(dib) dib->regs[1]	/* Domain's g1 */
#define get_entry_block(dib) *(long *)&cpuentryblock = dib->regs[8+2]
#define put_string_length(l) cpudibp->regs[8+4] = (l)
#define put_ordercode() cpudibp->regs[8+0] = cpuordercode
#define put_data_byte(db,dib)    (dib)->regs[8+3] = (db)
/* The following are used in testing */
#define put_entry_block(dib, eb) (dib)->regs[8+2] = (eb)
#define put_exit_block(dib, xb)  (dib)->regs[8+1] = (xb)
#define get_ordercode()    cpudibp->regs[8+0]
#define put_arg_length(dib, x)   (dib)->regs[8+4] = (x)
#define put_parm_length(dib, x)  (dib)->regs[1] = (x)
#define put_arg_pointer(dib, x)  (dib)->regs[8+3] = (x)
#define put_parm_pointer(dib, x) (dib)->regs[8+5] = (x)
