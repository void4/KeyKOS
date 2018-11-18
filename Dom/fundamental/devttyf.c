/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/**************************************************************
  This code supports the Asynchronous Device protocol using
  a Uart port to emulate a Unix TTY device.  Early versions
  do not support any cooking or other advanced features.
  Early versions are used for testing and illustration of
  the protocol.

  There is plenty of room for optimization in this module.
  One obvious thing to do is to avoid using the Reader and Writer
  domains for Synchronous I/O.  Just call the UART key directly.

*************************************************************/

/*************************************************************
  LINKING STYLE: 1 RO Section - NO RW Global storage
*************************************************************/

#include "keykos.h"
#include "kktypes.h"
#include "domain.h"
#include "dc.h"
#include "sb.h"
#include "unixdevice.h"
#include "ocrc.h"
#include "kuart.h"
#include "node.h"
#include <sys/errno.h>
#include <poll.h>

   KEY comp        = 0;
#define COMPCONSOLE 15
   KEY sb          = 1;    /* Space bank parameter */
   KEY caller      = 2;
   KEY domkey      = 3;
#define NODEREADERDOM 4
#define NODEWRITERDOM 5
#define NODENOTIFYDOM 6
#define NODENOTIFICATION 7
#define NODEUNIXCALLER 8
   KEY psb         = 4;
   KEY meter       = 5;
   KEY dc          = 6;

   KEY uart        = 7;
   KEY notify      = 8;
   KEY reader      = 9;
   KEY writer      = 10;

   KEY SOK  = 11;   /* DEBUGGING */
   KEY central     = 12;   /* used by helpers to call central */
                           /* used by the central domain to hold the completion notification key */
   KEY k2          = 13;
   KEY k1          = 14;
   KEY k0          = 15;

    char title[]="DEVTTY   ";

#define READERDB   100
#define WRITERDB   101
#define NOTIFYDB   102

#define SHAREDPAGESLOT 2
   unsigned char *memoryerror =     (unsigned char *)0x00200000;
   unsigned char *memorywanterror = (unsigned char *)0x00200001;

#define USERSLOT 2
   unsigned char *usermem = (unsigned char *)0x20000000;


UINT32 factory(factoc,factord)
   UINT32 factoc,factord;
{
   JUMPBUF;
   UINT32 oc,rc;
   union {
      struct DeviceIORequest dior;
      char input[256];
   } parm;
   struct Domain_DataByte ddb;
   struct Node_DataByteValue ndb;
   struct Domain_SPARCRegistersAndControl drac;

   char pbuf[256];

   char buf[256];       /* used in all domains for parameters and data */
   int  len,actlen;
   char inputbuf[256];  /* used to hold input data in central domain */
   int  inputlength;
   unsigned long  flags;
#define HAVEREADER    0x80000000
#define HAVEWRITER    0x40000000
#define HAVENOTIFY    0x20000000
#define HAVEDATA      0x10000000
#define NOTIFYREAD    0x08000000
#define NOTIFYWRITE   0x04000000
#define POLLREAD      0x02000000
#define POLLWRITE     0x01000000
#define ISOPENED      0x00800000
#define SYNCIO        0x00400000
#define IOCANCELLED   0x00200000
#define ISCLOSING     0x00100000

   unsigned short db;
   struct DeviceIORequest readdior,writedior,opendior,polldior;  /* when outstanding */
   unsigned short events;

   if(factoc != EXTEND_OC) exit(INVALIDOC_RC);
   {OC(EXTEND_OC);XB(0x00200000);RC(factoc);NB(0x08907002);cjcc(0x00000000,&_jumpbuf); }

   if(factoc) exit(INVALIDOC_RC);   /* must be zero */

   flags = 0;

   {OC(UART_MakeCurrentKey);XB(0x00700000);RC(rc);NB(0x08807000);cjcc(0x00000000,&_jumpbuf); }
   {OC(UART_EnableInput);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }

   {OC(Node_Fetch+COMPCONSOLE);XB(0x00000000);NB(0x0080B000);cjcc(0x00000000,&_jumpbuf); }
   {OC(0);XB(0x00B00000);RC(rc);NB(0x08400B00);cjcc(0x00000000,&_jumpbuf); }

/*********************************************************************************************
   KEEPER TO SKIP MEMORY FAULTS (set a flag in a communication page)
*********************************************************************************************/

   {OC(SB_CreatePage);XB(0x00100000);NB(0x0080C000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+SHAREDPAGESLOT);XB(0x80F0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   /* page for clone */
   {OC(Domain_GetKey+domkey);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }      /* my domain key for clone */

   if(!fork()) {   /* I need a simple keeper */

       {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
       {OC(Node_Swap+SHAREDPAGESLOT);XB(0x80F0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }   /* shared page */
       {OC(Domain_MakeStart);XB(0x00300000);NB(0x00A0F020);cjcc(0x00000000,&_jumpbuf); }
       {OC(Domain_SwapKeeper);XB(0x8080F000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }  /* notify has old keeper */

       {OC(0);XB(0x00200000); }  /* DK(0) */
       for(;;) {  /* keeper loop */
          {RC(oc);RS2(&(drac),sizeof(drac));NB(0x0B3000F2); }
          {rj(0x00080000,&_jumpbuf); }

          if(oc == 4) {
              exit(0);
          }

          if(*memorywanterror) {
               *memoryerror = 1;  /* flag error */

               drac.Control.PC=drac.Control.NPC;
               drac.Control.NPC=drac.Control.PC+4;

               {OC(Domain_ResetSPARCStuff);PS2(&(drac),sizeof(drac));XB(0x14F00002); }
               continue;
          }
          else {   /* pass it on */
             {OC(oc);PS2(&(drac),sizeof(drac));XB(0x348000F2); }
             continue;
          }
       }
   }

/**********************************************************************************************
   The READER domain
**********************************************************************************************/
   ddb.Databyte = READERDB;
   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
   if(!fork()) {   /* reader domain */

       {OC(Domain_GetKey+comp);XB(0x00300000);NB(0x00200020);cjcc(0x00000000,&_jumpbuf); }       /* zap caller key */
       len=0;   /* initial string length is zero */
       while(1) {
          {OC(0);PS2(buf,len);XB(0xC4C0F300);RC(rc);NB(0x0880F000);cjcc(0x08000000,&_jumpbuf); }
                           /* notify domain comes as k0, passed around and comes to notify domain */
          len=0;   /* start counting characters from beginning */
          while(1) {
             {OC(UART_WaitandReadData+1);XB(0x00700000);RC(rc);RS3(&buf[len],1,actlen);NB(0x0B000000);cjcc(0x00100000,&_jumpbuf); }
             if(!actlen) break;  /* activate now doing close */
             {OC(UART_WriteData);PS2(&buf[len],1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
             if(buf[len] == '\r') {
                buf[len]='\n';
                {OC(UART_WriteData);PS2(&buf[len],1);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
                len++;
                break;
             }
             else len++;
          }
       }
       exit(0);
   }

   {RC(oc);NB(0x08500F09); }
   {rj(0x08000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+NODEREADERDOM);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   flags |= HAVEREADER;

/**********************************************************************************************
   The WRITER domain
**********************************************************************************************/
   ddb.Databyte = WRITERDB;
   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
   if(!fork()) {
      {OC(Domain_GetKey+comp);XB(0x00300000);NB(0x00200020);cjcc(0x00000000,&_jumpbuf); }       /* zap caller key */

      while(1) {
         {OC(0);XB(0xC0C0F300);RC(rc);RS3(buf,255,len);NB(0x0B80F000);cjcc(0x00100000,&_jumpbuf); }
                   /* notify domain comes as k0, passed around and comes to notify domain */
//sprintf(pbuf,"Writer len=%d\n",len);
//outsok(pbuf);
         {OC(UART_WriteData);PS2(buf,len);XB(0x04700000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }
      }
      exit(0);
   }
   {RC(oc);NB(0x08500F0A); }
   {rj(0x08000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+NODEWRITERDOM);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   flags |= HAVEWRITER;

/**********************************************************************************************
   The NOTIFY domain
**********************************************************************************************/
   ddb.Databyte = NOTIFYDB;
   {OC(Domain_MakeStart);PS2(&(ddb),sizeof(ddb));XB(0x04300000);NB(0x0080C000);cjcc(0x08000000,&_jumpbuf); }
   if(!fork()) {


      while(1) {
         {OC(0);XB(0x80C03000);RC(rc);RS2(&(parm.dior),sizeof(parm.dior));NB(0x0B80F000);cjcc(0x00080000,&_jumpbuf); }
         {OC(rc);PS2(&(parm.dior),sizeof(parm.dior));XB(0x04F00000);RC(rc);NB(0x08000000);cjcc(0x08000000,&_jumpbuf); }   /* notify of completion */
      }
      exit(0);
   }
   {RC(oc);NB(0x0890F008); }
   {rj(0x08000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+NODENOTIFYDOM);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   flags |= HAVENOTIFY;


/**********************************************************************************************
   The CENTRAL domain
**********************************************************************************************/
   {OC(SB_CreateNode);XB(0x00100000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
   {OC(Node_Swap+0);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
   ndb.Byte = 7;    /* 256 meg slots */
   {OC(Node_MakeNodeKey);PS2(&(ndb),sizeof(ndb));XB(0x04F00000);NB(0x0080F000);cjcc(0x08000000,&_jumpbuf); }
   {OC(Domain_SwapMemory);XB(0x8030F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

   *memoryerror = 0;

   {OC(Domain_MakeStart);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
   {OC(0);XB(0x8020F000); }

   while(1) {
      {RC(oc);DB(db);RS3(&(parm),sizeof(parm),len);NB(0x0FD0CF02); }
                             /* central has notification keys from reader/writer */
                             /*  k0 is the user memory key when called from the keeper */
      {rj(0x00100000,&_jumpbuf); }

//sprintf(pbuf,"DEVTTY DB(%d) OC(%X) PARM.DIOR %X %X(%d) len=%d\n",db,oc,parm.dior.fh,
//         parm.dior.address,parm.dior.length,len);
//outsok(pbuf);

      switch(db) {
      case READERDB:
          /* reader called back.   If the IO is Asyncronous then central is the notification key */
          /* that was given to the reader to hold.  We have to send the notifier off to do the   */
          /* notification.   If the IO is synchronous then unixcaller is returned to with the    */
          /* results, In either case the read data is in parm.input,len                            */

          flags |= HAVEREADER;
          {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00809000);cjcc(0x00000000,&_jumpbuf); }   /* save reader key, null caller    */

          if(flags & ISCLOSING) {
              flags &= ~ISCLOSING;
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+NODEUNIXCALLER);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Device_IOComplete);XB(0x00F00000); }   /* end the close operation */
              continue;
          }

          if(flags & POLLREAD) {
              polldior.parameter |= (POLLIN + POLLNORM);

              if(len > 256) len=256;
              memcpy(inputbuf,parm.input,len);
              inputlength=len;
              inputbuf[len]=0;
//sprintf(pbuf,"READPOLL - '%s' %d\n",inputbuf,inputlength);
//outsok(pbuf);
              flags |= HAVEDATA;

              if(flags & HAVENOTIFY) {
                 flags &= ~POLLREAD;
                 flags &= ~POLLWRITE;
                 {OC(0);PS2(&(polldior),sizeof(polldior));XB(0x8480C000); }
                 {fj(0x08000000,&_jumpbuf); }
                 flags &= ~HAVENOTIFY;
                 {OC(0);XB(0x00200000); }
                 continue;
              }
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+NODENOTIFICATION);XB(0x80E0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0x00200000); }
              continue;
           }

          if(flags & SYNCIO) {  /* the "easy" case */
              flags &= ~SYNCIO;
              if(len > readdior.length) len=readdior.length;
              *memorywanterror=1;
              memcpy((unsigned char *)(usermem+readdior.address),parm.input,len);
              if(*memoryerror) {
                  readdior.error=EFAULT;
                  readdior.length=-1;
              }
              else readdior.length=len;
              *memorywanterror=0;

              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+NODEUNIXCALLER);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Device_IOComplete);PS2(&(readdior),sizeof(readdior));XB(0x04F00000); }
              continue;
          }

          /* the ASYNC case */
          if((flags & IOCANCELLED) || !(flags & NOTIFYREAD) || !(flags & HAVENOTIFY)) {  /* the odd cases */
              if(len > 256) len=256;
              memcpy(inputbuf,parm.input,len);
              inputlength=len;
              flags |= HAVEDATA;
              flags &= ~IOCANCELLED;

              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+NODENOTIFICATION);XB(0x80E0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0x00200000); }
              continue;
          }
          flags &= ~NOTIFYREAD;
          if(len > readdior.length) len=readdior.length;
          *memorywanterror=1;
          memcpy((unsigned char *)(usermem+readdior.address),parm.input,len);
          if(*memoryerror) {
              readdior.error=EFAULT;
              readdior.length=-1;
          }
          else readdior.length=len;
          *memorywanterror=0;

          /* now send this packet to the notify domain */

          {OC(0);PS2(&(readdior),sizeof(readdior));XB(0x8480C000); }
          {fj(0x08000000,&_jumpbuf); }
          flags &= ~HAVENOTIFY;

          {OC(0);XB(0x00200000); }
          continue;

      case WRITERDB:
          /* the deed is done.  We just worry about notification */

//outsok("Writer done\n");

          flags |= HAVEWRITER;
          {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080A000);cjcc(0x00000000,&_jumpbuf); }

          if(!(flags & ISOPENED)) {
             {OC(0);XB(0x00200000); }
             continue;
          }

          if(flags & POLLWRITE) {
//outsok("POLLWRITE\n");
              polldior.parameter |= POLLOUT;
              if(flags & HAVENOTIFY) {
                 flags &= ~POLLREAD;
                 flags &= ~POLLWRITE;
                 {OC(0);PS2(&(polldior),sizeof(polldior));XB(0x8480C000); }
                 {fj(0x08000000,&_jumpbuf); }
                 flags &= ~HAVENOTIFY;
                 {OC(0);XB(0x00200000); }
                 continue;
              }
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+NODENOTIFICATION);XB(0x80E0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0x00200000); }
              continue;
           }

          if(flags & SYNCIO) {
//sprintf(pbuf,"Returning to Unix %d  %X(%d)\n",writedior.fh,writedior.address,writedior.length);
//outsok(pbuf);
              flags &= ~SYNCIO;
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Fetch+NODEUNIXCALLER);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Device_IOComplete);PS2(&(writedior),sizeof(writedior));XB(0x04F00000); }
              continue;
          }
          /* ASYNCH case */
          if((flags & IOCANCELLED) || !(flags & NOTIFYWRITE) || !(flags & HAVENOTIFY)) {  /* the odd cases */
              flags &= ~IOCANCELLED;
              {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
              {OC(Node_Swap+NODENOTIFICATION);XB(0x80E0C000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
              {OC(0);XB(0x00200000); }
              continue;
          }

          flags &= ~NOTIFYWRITE;
          {OC(0);PS2(&(writedior),sizeof(writedior));XB(0x8480C000); }
          {fj(0x08000000,&_jumpbuf); }

          flags &= ~HAVENOTIFY;
          {OC(0);XB(0x00200000); }
          continue;

      case NOTIFYDB:
      	  flags |= HAVENOTIFY;
          {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x00808000);cjcc(0x00000000,&_jumpbuf); }   /* save notify key, null caller    */

          if(flags & (POLLREAD + POLLWRITE)) {
//outsok("NOTIFY OF POLL\n");
             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Fetch+NODENOTIFICATION);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }

             {OC(0);PS2(&(polldior),sizeof(polldior));XB(0x8480F000); }
             {fj(0x08000000,&_jumpbuf); }
             flags &= ~HAVENOTIFY;
             flags &= ~POLLREAD;
             flags &= ~POLLWRITE;

             {OC(0);XB(0x00200000); }
             continue;
          }

          if((flags & NOTIFYREAD) && (flags & HAVEDATA)) { /* needed to notify before return, do it now */
             flags &= ~NOTIFYREAD;
             flags &= ~HAVEDATA;
             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Fetch+NODENOTIFICATION);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
             if(inputlength > readdior.length) inputlength=readdior.length;
             *memorywanterror=1;
             memcpy((unsigned char *)(usermem+readdior.address),inputbuf,inputlength);
             if(*memoryerror) {
                 readdior.error=EFAULT;
                 readdior.length=-1;
             }
             else readdior.length=inputlength;
             *memorywanterror=0;

             {OC(0);PS2(&(readdior),sizeof(readdior));XB(0x8480F000); }
             {fj(0x08000000,&_jumpbuf); }
             flags &= ~HAVENOTIFY;

             {OC(0);XB(0x00200000); }
             continue;
          }

          if(flags & NOTIFYWRITE) {
             flags &= ~NOTIFYWRITE;
             {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
             {OC(Node_Fetch+NODENOTIFICATION);XB(0x00E00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
             {OC(0);PS2(&(writedior),sizeof(writedior));XB(0x8480F000); }
             {fj(0x08000000,&_jumpbuf); }
             flags &= ~NOTIFYWRITE;

             {OC(0);XB(0x00200000); }
             continue;
          }

          {OC(0);XB(0x00200000); }
          continue;

      default:

         if(oc == KT) {
            {OC(DevTTY_AKT);XB(0x00200000); }
            continue;
         }

         if(oc == KT+4) {  /* must zap reader, writer, notify */
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+0);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }          /* the base memory */
            {OC(Domain_SwapMemory);XB(0x8030E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyNode);XB(0x8010F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+SHAREDPAGESLOT);XB(0x00E00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyPage);XB(0x8010E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+NODEREADERDOM);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_MakeBusy);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyPage);XB(0x8040D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(DC_DestroyDomain);XB(0xC060F400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+NODEWRITERDOM);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_MakeBusy);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyPage);XB(0x8040D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(DC_DestroyDomain);XB(0xC060F400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+NODENOTIFYDOM);XB(0x00F00000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_MakeBusy);XB(0x00F00000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00F00000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Fetch+1);XB(0x00E00000);NB(0x0080D000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyPage);XB(0x8040D000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(SB_DestroyNode);XB(0x8040E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            {OC(DC_DestroyDomain);XB(0xC060F400);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            exit(0);
         }

         switch(oc) {
         /* we are not looking at the FH parameter as we only allow a single open */
         case DeviceOpen:
            if(flags & ISOPENED) {
                {OC(Device_MultiOpen);XB(0x00200000); }
                continue;
            }

//outsok("Open Device\n");

            flags |= ISOPENED;
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+USERSLOT);XB(0x80E0F000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            opendior=parm.dior;
            {OC(Device_IOComplete);XB(0x00200000); }
            continue;

         case DeviceClose:
            flags &= ~ISOPENED;
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+USERSLOT);XB(0x00F00000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }

            if(!(flags & HAVEREADER)) {  /* probably doing a poll */
               flags |= ISCLOSING;
               {OC(UART_WakeReadWaiter);XB(0x00700000);RC(rc);NB(0x08000000);cjcc(0x00000000,&_jumpbuf); }  /* kick reader */
               {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
               {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
               {OC(Node_Swap+NODEUNIXCALLER);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
               {OC(0);XB(0x00200000); }
               continue;
            }
/*
   If we are still writing we close anyway.  Writer returning will notice being
   closed and not take any action
*/
            {OC(Device_IOComplete);XB(0x00200000); }
            continue;

         case DeviceRead:
            if(!(flags & ISOPENED)) {
               {OC(Device_Closed);XB(0x00200000); }
               continue;
            }
            flags &= ~IOCANCELLED;
            flags &= ~POLLREAD;
            flags &= ~POLLWRITE;

            if (flags & HAVEDATA) {  /* this happens when a read is cancelled */
//sprintf(pbuf,"READ from buf '%s'(%d)\n",inputbuf,inputlength);
//outsok(pbuf);
               *memorywanterror=1;
               memcpy((unsigned char *)(usermem+parm.dior.address),inputbuf,inputlength);
               if(*memoryerror) {
                   parm.dior.error=EFAULT;
                   parm.dior.length=-1;
               }
               else parm.dior.length=inputlength;
               *memorywanterror=0;

               flags &= ~HAVEDATA;

               {OC(Device_IOComplete);PS2(&(parm.dior),sizeof(parm.dior));XB(0x04200000); }
               continue;
            }

            if(flags & HAVEREADER) {  /* send it off */
              /* send the reader off to do the work */
               readdior=parm.dior;
               {OC(0);XB(0x8090C000); }
               {fj(0x00000000,&_jumpbuf); }   /* start reader */
               flags &= ~HAVEREADER;
            }

            if(readdior.flags & DEVASYNC) {  /* async here */
                flags |= NOTIFYREAD;
                {OC(Device_IOStarted);XB(0x00200000); }
                continue;
            }
            /* synchronous, we wait for reader to call back */

            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+NODEUNIXCALLER);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            flags |= SYNCIO;
            {OC(0);XB(0x00200000); }
            continue;

         case DeviceWrite:
            if(!(flags & ISOPENED)) {
               {OC(Device_Closed);XB(0x00200000); }
               continue;
            }
            flags &= ~IOCANCELLED;
            flags &= ~POLLWRITE;
            flags &= ~POLLREAD;

//outsok("Device Write\n");
            if(!(flags & HAVEWRITER)) {
               {OC(Device_MultiIO);XB(0x00200000); }
               continue;
            }
            writedior = parm.dior;
//sprintf(pbuf,"Started writer %d - %X(%d)\n",writedior.fh,writedior.address,writedior.length);
//outsok(pbuf);
            if (writedior.length > 256) writedior.length=256;

            *memorywanterror=1;
            memcpy(buf,(unsigned char *)(usermem+writedior.address),writedior.length);
            if(*memoryerror) {
                writedior.error=EFAULT;
                writedior.length=-1;
            }
            *memorywanterror=0;

            {OC(0);PS2(buf,writedior.length);XB(0x84A0C000); }
            {fj(0x08000000,&_jumpbuf); }
            flags &= ~HAVEWRITER;

            if(writedior.flags & DEVASYNC) {  /* async here */
                flags |= NOTIFYWRITE;
                {OC(Device_IOStarted);XB(0x00200000); }
                continue;
            }

            /* synchronous, we wait for writer to call back */

//outsok("Waiting for Writer\n");
            {OC(Domain_SwapKey+caller);XB(0x00300000);NB(0x0080E000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Domain_GetMemory);XB(0x00300000);NB(0x0080F000);cjcc(0x00000000,&_jumpbuf); }
            {OC(Node_Swap+NODEUNIXCALLER);XB(0x80F0E000);NB(0x00000000);cjcc(0x00000000,&_jumpbuf); }
            flags |= SYNCIO;
            {OC(0);XB(0x00200000); }
            continue;

         case DeviceCancel:
            if(!(flags & ISOPENED)) {
               {OC(Device_Closed);XB(0x00200000); }
               continue;
            }
            /* be sure to clear NOTIFY flags */

            flags |= IOCANCELLED;
            flags &= ~NOTIFYREAD;
            flags &= ~NOTIFYWRITE;

            {OC(0);XB(0x00200000); }
            continue;

         case DeviceIOCTL:
            if(!(flags & ISOPENED)) {
               {OC(Device_Closed);XB(0x00200000); }
               continue;
            }
            {OC(Device_IOComplete);XB(0x00200000); }
            continue;

         case DevicePoll:

            if(!(flags & ISOPENED)) {
               {OC(Device_Closed);XB(0x00200000); }
               continue;
            }

            events = 0;

            if((flags & HAVEWRITER) && (parm.dior.parameter & POLLOUT)) {
                events |= POLLOUT;
            }
            if((flags & HAVEDATA) && (parm.dior.parameter & (POLLIN + POLLNORM))) {
                events |= (POLLIN + POLLNORM);
            }
            if(events) {  /* we respond immediately */
                parm.dior.parameter = events;
                {OC(Device_IOComplete);PS2(&(parm.dior),sizeof(parm.dior));XB(0x04200000); }
                continue;
            }

            if(parm.dior.parameter & POLLOUT)             flags |= POLLWRITE;
            if(parm.dior.parameter & (POLLIN + POLLNORM)) flags |= POLLREAD;

            if((flags & HAVEREADER)) {
                {OC(0);XB(0x8090C000); }
                {fj(0x00000000,&_jumpbuf); }   /* start reader */
                flags &= ~HAVEREADER;
            }

//outsok("Poll started \n");

            polldior = parm.dior;
            polldior.parameter = 0;

            {OC(Device_IOStarted);PS2(&(parm.dior),sizeof(parm.dior));XB(0x04200000); }
            continue;
         }
      }
   }

}

outsok(str)
     char *str;
{
     JUMPBUF;
     UINT32 rc;

     {OC(0);PS2(str,strlen(str));XB(0x04B00000);RC(rc);NB(0x0810000B);cjcc(0x08000000,&_jumpbuf); }
}
