#include "kktypes.h"
extern uint16 cal2tod(uchar *omroncalclock, uint64 *epochtod);
 
/* Returns: 0  if OmronCalClock contains a valid Epoch time   */
/*          1  if OmronCalClock < 19000101xx000000            */
/*                EpochTod set to 0x0000000000000000          */
/*          2  if OmronCalClock > 20420917xx235337            */
/*                EpochTod set to 0xFFFFFFFFFFFFFFFF          */
