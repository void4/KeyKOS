/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/****************************************************************

  A generic interface specification for all Unix devices including
  sockets.  This interface supports an asynchronous protocol

  UnixDevice(DeviceOC,(iorequest);NotificationKey,MemoryKey -> c,(iorequest))

****************************************************************/
#ifndef _H_Unixdevice
#define _H_Unixdevice

#define DevTTYF_AKT  0x269
#define DevTTY_AKT  0x369
#define DevCONSF_AKT 0x469
#define DevCONS_AKT 0x569

#define UnixDeviceF_Create  0

#define DeviceOpen 0
#define DeviceClose 1
#define DeviceIOCTL 2
#define DeviceRead 3
#define DeviceWrite 4
#define DevicePoll 5
#define DeviceCancel 6

struct DeviceIORequest {
    long fh;            /* fh of file, used by keeper on notification */
                        /* may include some KEEPER ID in upper bits */
    unsigned short flags;
#define DEVASYNC 0x8000
#define DEVPOLL 0x4000
    unsigned short sequence;  /* to match notifications */
    unsigned long parameter;  /* second parameter of IOCTL */
    unsigned long address;    /* user buffer address */
    long length;     /* request size OR amoutn transferred */
    long error;      /* if error code */
};

#define Device_IOComplete 0
#define Device_MultiOpen 1
#define Device_IOStarted 2 
#define Device_MultiIO   3
#define Device_Closed    4

#define NOTIFYCOMPLETE 0
    
#endif
