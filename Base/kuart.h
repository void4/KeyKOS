/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_uart
#define _H_uart
/*
  Proprietary Material of Key Logic  COPYRIGHT (c) 1990 Key Logic
*/
/***************************************************************
  Definitions for the UART Device
 
  UART(UART_WakeReadWaiter);
  UART(UART_GetMaximumWriteLength) RCTO (uint32);
  UART(UART_WriteData) CHARFROM (string,len) RCTO(uint32);
  UART(UART_WaitandReadData + length) CHARTO(string,length,n) RCTO(uint32);
                          0 < length <= 4096 
  UART(UART_GetGDBPacket +length) CHARTO(string,length,n);
                          0 < length <= 4096
      GetGDBPacket is a hack which is done disabled to allow
      for use of the UART without interrupt support (polled)
  UART(UART_PutGDBPacket) CHARFROM(string,length) RCTO(uint32)

  UART(UART_PutGetGDBPacket) CHARFROM(string.length) CHARTO(string,length,n) RCTO(uint32)

  UART(UART_PutDataGetResponse) CHARFROM(string,length) CHARTO(string,2) RCTO(uint32)
  UART(UART_SendRdyGetData)  CHARTO(string,length) RCTO(uint32)

  UART(KT) RCTO(0x209);
 
***************************************************************/
 
#define UART_WriteData                 0
#define UART_WakeReadWaiter            1
#define UART_GetMaximumWriteLength     2
#define UART_EnableInput               3
#define UART_DisableInput              4
#define UART_MakeCurrentKey            5
#define UART_PutGDBPacket              6
#define UART_WaitandReadData       0xfff
#define UART_GetGDBPacket         0x2fff
#define UART_PutGetGDBPacket      0x4fff

#define UART_PutDataGetResponse   0x6fff
#define UART_SendRdyGetData       0x8fff

/* For getfile transfer protocol using PutDataGetResponse and SendRdyGetData */
#define GETFILEOK                 0x1001
#define GETFILERDY                0x1002
#define GETFILEEOF                0x1003
#define GETFILEERR                0x1004
#define GETFILERESEND             0x1005

#endif
