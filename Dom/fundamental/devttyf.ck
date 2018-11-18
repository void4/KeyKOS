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
   KC (caller,EXTEND_OC) KEYSTO(uart,,,caller) RCTO(factoc);

   if(factoc) exit(INVALIDOC_RC);   /* must be zero */

   flags = 0;

   KC (uart,UART_MakeCurrentKey) KEYSTO(uart) RCTO(rc);
   KC (uart,UART_EnableInput) RCTO(rc); 
   
   KC (comp,Node_Fetch+COMPCONSOLE) KEYSTO(SOK);
   KC (SOK,0) KEYSTO(,SOK) RCTO(rc);

/*********************************************************************************************
   KEEPER TO SKIP MEMORY FAULTS (set a flag in a communication page)
*********************************************************************************************/

   KC (sb,SB_CreatePage) KEYSTO(central);
   KC (domkey,Domain_GetMemory) KEYSTO(k0);
   KC (k0,Node_Swap+SHAREDPAGESLOT) KEYSFROM(central);   /* page for clone */
   KC (domkey,Domain_GetKey+domkey) KEYSTO(notify);      /* my domain key for clone */

   if(!fork()) {   /* I need a simple keeper */

       KC (domkey,Domain_GetMemory) KEYSTO(k0);
       KC (k0,Node_Swap+SHAREDPAGESLOT) KEYSFROM(central);   /* shared page */
       KC (domkey,Domain_MakeStart) KEYSTO(k0,,caller);
       KC (notify,Domain_SwapKeeper) KEYSFROM(k0) KEYSTO(notify);  /* notify has old keeper */

       LDEXBL (caller,0);  /* DK(0) */
       for(;;) {  /* keeper loop */
          LDENBL OCTO(oc) STRUCTTO(drac) KEYSTO(,,k0,caller);
          RETJUMP();

          if(oc == 4) {
              exit(0);
          }

          if(*memorywanterror) {
               *memoryerror = 1;  /* flag error */

               drac.Control.PC=drac.Control.NPC;
               drac.Control.NPC=drac.Control.PC+4;

               LDEXBL (k0,Domain_ResetSPARCStuff) KEYSFROM(,,,caller) STRUCTFROM(drac);
               continue;
          }
          else {   /* pass it on */
             LDEXBL (notify,oc) STRUCTFROM(drac) KEYSFROM(,,k0,caller);
             continue;
          }
       }
   }

/**********************************************************************************************
   The READER domain
**********************************************************************************************/
   ddb.Databyte = READERDB;
   KC (domkey,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(central);
   if(!fork()) {   /* reader domain */

       KC (domkey,Domain_GetKey+comp) KEYSTO(,,caller);       /* zap caller key */
       len=0;   /* initial string length is zero */
       while(1) {
          KC (central,0) KEYSFROM(k0,domkey) CHARFROM(buf,len) KEYSTO(k0) RCTO(rc); 
                           /* notify domain comes as k0, passed around and comes to notify domain */
          len=0;   /* start counting characters from beginning */
          while(1) {
             KC (uart,UART_WaitandReadData+1) CHARTO(&buf[len],1,actlen) RCTO(rc);
             if(!actlen) break;  /* activate now doing close */
             KC (uart,UART_WriteData) CHARFROM(&buf[len],1) RCTO(rc);
             if(buf[len] == '\r') {
                buf[len]='\n';
                KC (uart,UART_WriteData) CHARFROM(&buf[len],1) RCTO(rc);
                len++;
                break;
             }
             else len++;
          }
       }
       exit(0);
   }

   LDENBL OCTO(oc) KEYSTO(,k0,,reader);
   RETJUMP();
   KC (domkey,Domain_GetMemory) KEYSTO(k1);
   KC (k1,Node_Swap+NODEREADERDOM) KEYSFROM(k0); 
   flags |= HAVEREADER;

/**********************************************************************************************
   The WRITER domain
**********************************************************************************************/
   ddb.Databyte = WRITERDB;
   KC (domkey,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(central);
   if(!fork()) {
      KC (domkey,Domain_GetKey+comp) KEYSTO(,,caller);       /* zap caller key */

      while(1) {
         KC (central,0) KEYSFROM(k0,domkey) CHARTO(buf,255,len) KEYSTO(k0) RCTO(rc);
                   /* notify domain comes as k0, passed around and comes to notify domain */
//sprintf(pbuf,"Writer len=%d\n",len);
//outsok(pbuf);
         KC (uart,UART_WriteData) CHARFROM(buf,len) RCTO(rc);
      }
      exit(0);
   }
   LDENBL OCTO(oc) KEYSTO(,k0,,writer);
   RETJUMP();
   KC (domkey,Domain_GetMemory) KEYSTO(k1);
   KC (k1,Node_Swap+NODEWRITERDOM) KEYSFROM(k0); 
   flags |= HAVEWRITER;

/**********************************************************************************************
   The NOTIFY domain
**********************************************************************************************/
   ddb.Databyte = NOTIFYDB;
   KC (domkey,Domain_MakeStart) STRUCTFROM(ddb) KEYSTO(central);
   if(!fork()) {


      while(1) {
         KC (central,0) KEYSFROM(domkey) STRUCTTO(parm.dior) KEYSTO(k0) RCTO(rc);
         KC (k0,rc) STRUCTFROM(parm.dior) RCTO(rc);   /* notify of completion */
      }
      exit(0);
   }
   LDENBL OCTO(oc) KEYSTO(k0,,,notify);
   RETJUMP();
   KC (domkey,Domain_GetMemory) KEYSTO(k1);
   KC (k1,Node_Swap+NODENOTIFYDOM) KEYSFROM(k0); 
   flags |= HAVENOTIFY;


/**********************************************************************************************
   The CENTRAL domain
**********************************************************************************************/
   KC (sb,SB_CreateNode) KEYSTO(k0);
   KC (domkey,Domain_GetMemory) KEYSTO(k1);
   KC (k0,Node_Swap+0) KEYSFROM(k1);
   ndb.Byte = 7;    /* 256 meg slots */
   KC (k0,Node_MakeNodeKey) STRUCTFROM(ndb) KEYSTO(k0);
   KC (domkey,Domain_SwapMemory) KEYSFROM(k0);

   *memoryerror = 0;

   KC (domkey,Domain_MakeStart) KEYSTO(k0);
   LDEXBL (caller,0) KEYSFROM(k0);

   while(1) {
      LDENBL OCTO(oc) DBTO(db) STRUCTTO(parm,,len) KEYSTO(central,k0,,caller);  
                             /* central has notification keys from reader/writer */
                             /*  k0 is the user memory key when called from the keeper */
      RETJUMP();

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
          KC (domkey,Domain_SwapKey+caller) KEYSTO(reader);   /* save reader key, null caller    */

          if(flags & ISCLOSING) {
              flags &= ~ISCLOSING;
              KC (domkey,Domain_GetMemory) KEYSTO(k0);
              KC (k0,Node_Fetch+NODEUNIXCALLER) KEYSTO(k0);
              LDEXBL (k0,Device_IOComplete);   /* end the close operation */ 
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
                 LDEXBL (notify,0) STRUCTFROM(polldior) KEYSFROM(central);
                 FORKJUMP();
                 flags &= ~HAVENOTIFY;
                 LDEXBL (caller,0);
                 continue;
              }
              KC (domkey,Domain_GetMemory) KEYSTO(k1);
              KC (k1,Node_Swap+NODENOTIFICATION) KEYSFROM(central);
              LDEXBL (caller,0);
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

              KC (domkey,Domain_GetMemory) KEYSTO(k0);
              KC (k0,Node_Fetch+NODEUNIXCALLER) KEYSTO(k0);
              LDEXBL (k0,Device_IOComplete) STRUCTFROM(readdior);
              continue;
          }
          
          /* the ASYNC case */
          if((flags & IOCANCELLED) || !(flags & NOTIFYREAD) || !(flags & HAVENOTIFY)) {  /* the odd cases */
              if(len > 256) len=256;
              memcpy(inputbuf,parm.input,len);
              inputlength=len;
              flags |= HAVEDATA;
              flags &= ~IOCANCELLED;
              
              KC (domkey,Domain_GetMemory) KEYSTO(k1);
              KC (k1,Node_Swap+NODENOTIFICATION) KEYSFROM(central);
              LDEXBL (caller,0);
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
          
          LDEXBL (notify,0) STRUCTFROM(readdior) KEYSFROM(central);
          FORKJUMP();
          flags &= ~HAVENOTIFY;
          
          LDEXBL (caller,0);
          continue;
              
      case WRITERDB:
          /* the deed is done.  We just worry about notification */
          
//outsok("Writer done\n");

          flags |= HAVEWRITER;
          KC (domkey,Domain_SwapKey+caller) KEYSTO(writer);

          if(!(flags & ISOPENED)) {
             LDEXBL (caller,0);
             continue;
          }

          if(flags & POLLWRITE) {
//outsok("POLLWRITE\n");
              polldior.parameter |= POLLOUT;
              if(flags & HAVENOTIFY) {
                 flags &= ~POLLREAD;
                 flags &= ~POLLWRITE;
                 LDEXBL (notify,0) STRUCTFROM(polldior) KEYSFROM(central);
                 FORKJUMP();
                 flags &= ~HAVENOTIFY;
                 LDEXBL (caller,0);
                 continue;
              }
              KC (domkey,Domain_GetMemory) KEYSTO(k1);
              KC (k1,Node_Swap+NODENOTIFICATION) KEYSFROM(central);
              LDEXBL (caller,0);
              continue;
           } 
          
          if(flags & SYNCIO) {
//sprintf(pbuf,"Returning to Unix %d  %X(%d)\n",writedior.fh,writedior.address,writedior.length);
//outsok(pbuf);
              flags &= ~SYNCIO;
              KC (domkey,Domain_GetMemory) KEYSTO(k0);
              KC (k0,Node_Fetch+NODEUNIXCALLER) KEYSTO(k0);
              LDEXBL (k0,Device_IOComplete) STRUCTFROM(writedior);
              continue;
          }
          /* ASYNCH case */
          if((flags & IOCANCELLED) || !(flags & NOTIFYWRITE) || !(flags & HAVENOTIFY)) {  /* the odd cases */
              flags &= ~IOCANCELLED;
              KC (domkey,Domain_GetMemory) KEYSTO(k1);
              KC (k1,Node_Swap+NODENOTIFICATION) KEYSFROM(central);
              LDEXBL (caller,0);
              continue;
          } 
          
          flags &= ~NOTIFYWRITE;
          LDEXBL (notify,0) STRUCTFROM(writedior) KEYSFROM(central);
          FORKJUMP();
          
          flags &= ~HAVENOTIFY;
          LDEXBL (caller,0);
          continue;      
      
      case NOTIFYDB:
      	  flags |= HAVENOTIFY;
          KC (domkey,Domain_SwapKey+caller) KEYSTO(notify);   /* save notify key, null caller    */

          if(flags & (POLLREAD + POLLWRITE)) {
//outsok("NOTIFY OF POLL\n");
             KC (domkey,Domain_GetMemory) KEYSTO(k1);
             KC (k1,Node_Fetch+NODENOTIFICATION) KEYSTO(k0);

             LDEXBL (notify,0) STRUCTFROM(polldior) KEYSFROM(k0);
             FORKJUMP();
             flags &= ~HAVENOTIFY;
             flags &= ~POLLREAD;
             flags &= ~POLLWRITE;

             LDEXBL (caller,0);
             continue;
          }
          
          if((flags & NOTIFYREAD) && (flags & HAVEDATA)) { /* needed to notify before return, do it now */
             flags &= ~NOTIFYREAD;
             flags &= ~HAVEDATA;
             KC (domkey,Domain_GetMemory) KEYSTO(k1);
             KC (k1,Node_Fetch+NODENOTIFICATION) KEYSTO(k0);
             if(inputlength > readdior.length) inputlength=readdior.length;
             *memorywanterror=1;
             memcpy((unsigned char *)(usermem+readdior.address),inputbuf,inputlength);
             if(*memoryerror) {
                 readdior.error=EFAULT;
                 readdior.length=-1;
             }
             else readdior.length=inputlength;
             *memorywanterror=0;

             LDEXBL (notify,0) STRUCTFROM(readdior) KEYSFROM(k0);
             FORKJUMP();
             flags &= ~HAVENOTIFY;
             
             LDEXBL (caller,0);
             continue;
          }
          
          if(flags & NOTIFYWRITE) {
             flags &= ~NOTIFYWRITE;
             KC (domkey,Domain_GetMemory) KEYSTO(k1);
             KC (k1,Node_Fetch+NODENOTIFICATION) KEYSTO(k0);
             LDEXBL (notify,0) STRUCTFROM(writedior) KEYSFROM(k0);
             FORKJUMP();
             flags &= ~NOTIFYWRITE;
             
             LDEXBL (caller,0);
             continue;
          }
          
          LDEXBL (caller,0);
          continue;
          
      default:

         if(oc == KT) {
            LDEXBL (caller,DevTTY_AKT);
            continue;
         }

         if(oc == KT+4) {  /* must zap reader, writer, notify */
            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Fetch+0) KEYSTO(k1);          /* the base memory */
            KC (domkey,Domain_SwapMemory) KEYSFROM(k1);
            KC (sb,SB_DestroyNode) KEYSFROM(k0);
            KC (k1,Node_Fetch+SHAREDPAGESLOT) KEYSTO(k1);
            KC (sb,SB_DestroyPage) KEYSFROM(k1);

            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Fetch+NODEREADERDOM) KEYSTO(k0);
            KC (k0,Domain_MakeBusy) RCTO(rc);
            KC (k0,Domain_GetMemory) KEYSTO(k1);
            KC (k1,Node_Fetch+1) KEYSTO(k2);
            KC (psb,SB_DestroyPage) KEYSFROM(k2);
            KC (psb,SB_DestroyNode) KEYSFROM(k1);
            KC (dc,DC_DestroyDomain) KEYSFROM(k0,psb); 

            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Fetch+NODEWRITERDOM) KEYSTO(k0);
            KC (k0,Domain_MakeBusy) RCTO(rc);
            KC (k0,Domain_GetMemory) KEYSTO(k1);
            KC (k1,Node_Fetch+1) KEYSTO(k2);
            KC (psb,SB_DestroyPage) KEYSFROM(k2);
            KC (psb,SB_DestroyNode) KEYSFROM(k1);
            KC (dc,DC_DestroyDomain) KEYSFROM(k0,psb); 

            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Fetch+NODENOTIFYDOM) KEYSTO(k0);
            KC (k0,Domain_MakeBusy) RCTO(rc);
            KC (k0,Domain_GetMemory) KEYSTO(k1);
            KC (k1,Node_Fetch+1) KEYSTO(k2);
            KC (psb,SB_DestroyPage) KEYSFROM(k2);
            KC (psb,SB_DestroyNode) KEYSFROM(k1);
            KC (dc,DC_DestroyDomain) KEYSFROM(k0,psb); 

            exit(0);
         }

         switch(oc) {
         /* we are not looking at the FH parameter as we only allow a single open */
         case DeviceOpen:
            if(flags & ISOPENED) {
                LDEXBL (caller,Device_MultiOpen);
                continue;
            }

//outsok("Open Device\n");

            flags |= ISOPENED;
            KC (domkey,Domain_GetMemory) KEYSTO(k1);
            KC (k1,Node_Swap+USERSLOT) KEYSFROM(k0);
            opendior=parm.dior;
            LDEXBL (caller,Device_IOComplete);
            continue;
            
         case DeviceClose:
            flags &= ~ISOPENED;
            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Swap+USERSLOT);

            if(!(flags & HAVEREADER)) {  /* probably doing a poll */
               flags |= ISCLOSING;
               KC (uart,UART_WakeReadWaiter) RCTO(rc);  /* kick reader */
               KC (domkey,Domain_GetMemory) KEYSTO(k0);
               KC (domkey,Domain_SwapKey+caller) KEYSTO(k1);
               KC (k0,Node_Swap+NODEUNIXCALLER) KEYSFROM(k1);
               LDEXBL (caller,0);
               continue;
            }
/*
   If we are still writing we close anyway.  Writer returning will notice being
   closed and not take any action
*/
            LDEXBL (caller,Device_IOComplete);
            continue;
            
         case DeviceRead:
            if(!(flags & ISOPENED)) {
               LDEXBL(caller,Device_Closed);
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

               LDEXBL (caller,Device_IOComplete) STRUCTFROM(parm.dior);
               continue;
            }

            if(flags & HAVEREADER) {  /* send it off */
              /* send the reader off to do the work */
               readdior=parm.dior;
               LDEXBL (reader,0) KEYSFROM(central);
               FORKJUMP();   /* start reader */
               flags &= ~HAVEREADER;
            }
            
            if(readdior.flags & DEVASYNC) {  /* async here */
                flags |= NOTIFYREAD;
                LDEXBL(caller,Device_IOStarted);
                continue;
            }
            /* synchronous, we wait for reader to call back */
          
            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (domkey,Domain_SwapKey+caller) KEYSTO(k1);
            KC (k0,Node_Swap+NODEUNIXCALLER) KEYSFROM(k1);
            flags |= SYNCIO;
            LDEXBL (caller,0);
            continue;
            
         case DeviceWrite:
            if(!(flags & ISOPENED)) {
               LDEXBL(caller,Device_Closed);
               continue;
            }
            flags &= ~IOCANCELLED;
            flags &= ~POLLWRITE;
            flags &= ~POLLREAD;

//outsok("Device Write\n");
            if(!(flags & HAVEWRITER)) {
               LDEXBL (caller,Device_MultiIO);
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

            LDEXBL (writer,0) CHARFROM(buf,writedior.length) KEYSFROM(central);
            FORKJUMP();
            flags &= ~HAVEWRITER;
            
            if(writedior.flags & DEVASYNC) {  /* async here */
                flags |= NOTIFYWRITE;
                LDEXBL(caller,Device_IOStarted);
                continue;
            }
            
            /* synchronous, we wait for writer to call back */
            
//outsok("Waiting for Writer\n");
            KC (domkey,Domain_SwapKey+caller) KEYSTO(k1);
            KC (domkey,Domain_GetMemory) KEYSTO(k0);
            KC (k0,Node_Swap+NODEUNIXCALLER) KEYSFROM(k1);
            flags |= SYNCIO;
            LDEXBL (caller,0);
            continue;
         
         case DeviceCancel:
            if(!(flags & ISOPENED)) {
               LDEXBL(caller,Device_Closed);
               continue;
            }
            /* be sure to clear NOTIFY flags */
            
            flags |= IOCANCELLED;
            flags &= ~NOTIFYREAD;
            flags &= ~NOTIFYWRITE;
            
            LDEXBL (caller,0);
            continue;
            
         case DeviceIOCTL:
            if(!(flags & ISOPENED)) {
               LDEXBL(caller,Device_Closed);
               continue;
            }
            LDEXBL (caller,Device_IOComplete);
            continue;
              
         case DevicePoll:

            if(!(flags & ISOPENED)) {
               LDEXBL(caller,Device_Closed);
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
                LDEXBL (caller,Device_IOComplete) STRUCTFROM(parm.dior);
                continue;
            }

            if(parm.dior.parameter & POLLOUT)             flags |= POLLWRITE;
            if(parm.dior.parameter & (POLLIN + POLLNORM)) flags |= POLLREAD;

            if((flags & HAVEREADER)) {
                LDEXBL (reader,0) KEYSFROM(central);
                FORKJUMP();   /* start reader */
                flags &= ~HAVEREADER;
            }

//outsok("Poll started \n");
 
            polldior = parm.dior;
            polldior.parameter = 0;

            LDEXBL (caller,Device_IOStarted) STRUCTFROM(parm.dior);
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

     KC (SOK,0) CHARFROM(str,strlen(str)) KEYSTO(,,,SOK) RCTO(rc); 
}
