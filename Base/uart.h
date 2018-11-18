/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_uart
#define _H_uart

/***************************************************************
  Definitions for the UART Device
 
  UART(UART_WakeReadWaiter);
  UART(UART_GetMaximumWriteLength) RCTO (uint32);
  UART(UART_WriteData) CHARFROM (string);
  UART(UART_WaitandReadData + length) CHARTO(string,length,n);
                          0 < length <= 4096 
  UART(KT) RCTO(0x209);
 
***************************************************************/
 
#define UART_WriteData                 0
#define UART_WakeReadWaiter            1
#define UART_GetMaximumWriteLength     2
#define UART_WaitandReadData       0xfff
#define UART_GetGDBPacket         0x2fff
#define UART_PutGetGDBPacket      0x4fff
#define UART_PutDataGetResponse   0x6fff
#define UART_SendRdyGetData       0x8fff
#endif
