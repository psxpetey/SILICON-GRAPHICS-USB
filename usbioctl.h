/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbioctl.h                                                 *
* Description: Defines and ioctls used by driver and user          *
*              space applications                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-24-2012     -Added new event defines for usbd.c                                      *
*                                                                                                     *
* BSDero      09-18-2012     -Updated hub_index datatype for negative number support in USB_device_t  *
*                            -Added spec_device_instance_id in USB_device_t                           *
*                            -Added USB_DEVICE_STATUS_OK and USB_DEVICE_STATUS_ERROR definitions      *
*                                                                                                     *
* BSDero      08-15-2012     -Trace macros updated for 64 bit support                                 *
*                                                                                                     *
* BSDero      07-16-2012     -General fixes for usbhub support, macros added                          *
*                                                                                                     *
* BSDero      06-26-2012     -General fixes for usbhub support                                        *
*                                                                                                     *
* BSDero      05-13-2012     -Sequence number added to USB_driver_info_t data structure               *
*                            -USB_driver_info_t data type added                                       *
*                                                                                                     *
* BSDero      04-11-2012     -New ioctls based on linux USB implementation                            *
*                            -USB_driver_info_t data type added                                       *
*                                                                                                     *
* BSDero      03-18-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/
#ifndef _USBIOCTL_H_
#define _USBIOCTL_H_

#include <sys/types.h>
#include "usbio.h"



/*
*******************************************************************************************************
* USB IOCTL SPECS                                                                                     *
* -Arguments bigger than 256 bytes should be passed through a pointer                                 *
* -Letter specifications for ioctls:                                                                  *
*    * For USB core                                                                                   *
*      'E' for EHCI passthrough                                                                       *
*      'O' for OHCI passthrough                                                                       *
*      'U' for UHCI passthrough                                                                       *
*      'X' for XHCI passthrough                                                                       *
*      'E' for EHCI passthrough                                                                       *
*      'B' for USB driver                                                                             *
*      'D' for Pseudo device                                                                          *
*                                                                                                     *
*                                                                                                     *
*    * For USB devices                                                                                *
*      'M' for USB Massive Storage                                                                    *
*      'K' for USB Keyboard                                                                           *
*      'P' for USB Mouse or pointing device                                                           *
*      'C' for USB Camera                                                                             *
*      'S' for USB Scanners                                                                           *
*      'N' for USB Network interfaces                                                                 *
*      'R' for USB Printers                                                                           *
*      'G' for USB Game controllers                                                                   *
*      'Z' for USB Custom devices                                                                     *
*                                                                                                     *
*******************************************************************************************************
*/




/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_DRIVER_INFO                                                                   *
*  Returns USB driver information                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_DRIVER_INFO                   _IOCTL_R('D', 1, USB_driver_info_t)

typedef struct {
    char                     long_name[128];        /* device driver long name                       */
    char                     short_name[16];        /* device driver short name                      */
    char                     version[16];           /* device driver version                         */
    char                     short_version[8];      /* Short version string                          */
    char                     seqn[32];              /* Sequence number                               */
    char                     build_date[16];        /* Build date                                    */
    char                     reserved[40];          /* Reserved                                      */
} USB_driver_info_t;                                /* size: 256 bytes */





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_NUM_MODULES                                                                   *
*  Returns USB attached drivers information                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_NUM_MODULES                   _IOCTL_R('D', 2, int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_NUM_MODULES                                                                   *
*  Returns USB attached drivers information                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_MODULE_INFO                   _IOCTL_WR('D', 3, USB_driver_module_info_t)

typedef struct {
    int                         num_module;         /* Module Index                                  */
    uint32_t                    module_id;
#define USB_MOD_CORE								0x00
#define USB_MOD_EHCI								0x01    
#define USB_MOD_UHCI								0x02    
#define USB_MOD_OHCI								0x03    
#define USB_MOD_XHCI								0x04    
#define USB_MOD_HUB									0x05

    char                        short_description[12];
    char                        long_description[80];
    char                        module_name[32];
    uint32_t                    type;
#define USB_DRIVER_IS_HCD                           0
#define USB_DRIVER_IS_CORE                          1
#define USB_DRIVER_IS_USB_DEVICE                    2

} USB_driver_module_info_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_ROOT_HUB_INFO                                                                 *
*  Returns USB ROOT HUB information                                                                   *
*******************************************************************************************************
*/

#define IOCTL_USB_GET_ROOT_HUB_INFO                 _IOCTL_R('D', 4, USB_root_hub_info_t)  
  
typedef struct{
    int                         hub_type;
#define USB_HUB_PCI                                 0x01
#define USB_HUB_USB                                 0x02
    
    int                         hub_index;          /* index of hub                                  */
	int                         hub_parent;         /* index of hub parent                           */
	uchar_t                     hcd_drivers;        /* HCD supported driver by this HUB              */
#define USB_UHCI   	                                0x01
#define USB_EHCI   	                                0x02
#define USB_OHCI   	                                0x04
#define USB_XHCI   	                                0x08

    uchar_t                     pci_bus;
	uchar_t                     pci_slot;
	int                         pci_function;
    uint16_t                    product_id;
	uint16_t                    vendor_id;
    uchar_t                     hardware_description[80];
    int                         ports_number; 
	uint16_t                    address;
	uchar_t                     fs_device[80];
} USB_root_hub_info_t;





/*
*******************************************************************************************************
*  IOCTLS: IOCTL_USB_SET_DEBUG_LEVEL and IOCTL_USB_GET_DEBUG_LEVEL                                    *
*  Returns and set debug messages trace level and class                                               *
*  Set debug level                                                                                    *
*  0 = no debug messages                                                                              *
*  1 = informational/no recoverable error messages                                                    *
*  2 = all error/warning messages                                                                     *
*  3 = entry/exit of functions and callbacks                                                          *
*  4 = debug, all messages showing                                                                    *
*******************************************************************************************************
*/
/* Trace classes */
#define TRC_INIT                                    0x0000000000000001  /* pfxinit and pfxreg                */
#define TRC_UNLOAD                                  0x0000000000000002  /* pfxunreg and pfxunload            */
#define TRC_ATTACH                                  0x0000000000000004  /* pfxattach                         */
#define TRC_DETACH                                  0x0000000000000008  /* pfxdetach                         */
#define TRC_OPEN                                    0x0000000000000010  /* pfxopen                           */                
#define TRC_CLOSE                                   0x0000000000000020  /* pfxclose                          */
#define TRC_READ                                    0x0000000000000040  /* pfxread                           */
#define TRC_WRITE                                   0x0000000000000080  /* pfxwrite                          */
#define TRC_IOCTL                                   0x0000000000000100  /* pfxioctl                          */
#define TRC_STRATEGY                                0x0000000000000200  /* pfxstrategy                       */
#define TRC_POLL                                    0x0000000000000400  /* pfxpoll                           */
#define TRC_MAP                                     0x0000000000000800  /* pfxmap and pfxunmap               */
#define TRC_INTR                                    0x0000000000001000  /* pfxintr                           */
#define TRC_ERRFUNC                                 0x0000000000002000  /* pfxerr                            */
#define TRC_HALT                                    0x0000000000004000  /* pfxhalt                           */
                
				
/* Help Functionalities */
#define TRC_GC_DMA                                  0x0000000000010000  /* memory lists and dma              */
#define TRC_HELPER                                  0x0000000000020000  /* helper & utility functions        */
#define TRC_MESSAGES                                0x0000000000040000  /* for module function calls         */
#define TRC_EVENTS                                  0x0000000000080000  /* for usb stack core events         */
#define TRC_ERROR                                   0x0000000000100000  /* error flag                        */
#define TRC_WARNING                                 0x0000000000200000  /* warning flag                      */
#define TRC_START_END                               0x0000000000400000  /* start/end in functions            */
/* bits 23-32 reserved */

/* modules remaining bits */
#define TRC_MOD_UHCI   	                            0x0000000100000000
#define TRC_MOD_EHCI   	                            0x0000000200000000
#define TRC_MOD_OHCI   	                            0x0000000400000000
#define TRC_MOD_XHCI   	                            0x0000000800000000
#define TRC_MOD_CORE                                0x0000001000000000  
#define TRC_MOD_HUB                                 0x0000002000000000  



#define TRC_NO_MESSAGES                             0x0000000000000000  /* no trace/debug messages           */
#define TRC_ENTRY_POINTS                            0x000000000000ffff  /* all entry points                  */
#define TRC_ALL_FUNCTIONALITIES                     0x0000000000ff0000

/* HCD drivers for bits 32-35 */
#define TRC_HCD_SET(x)                              (x << 32)
#define TRC_HCD_GET(x)                              ((x >> 32) & 0x0000000f)


#define TRC_ALL_MODULES                             0x000000ff00000000
#define TRC_ALL_HCD_MODULES                         0x0000000f00000000

#define TRC_ALL                                     0xffffffffffffffff  /* debug all                         */


/* ioctl for set and get debug class tracing */
#define IOCTL_USB_SET_DEBUG_LEVEL                   _IOCTL_W('D', 5, USB_trace_class_t)
#define IOCTL_USB_GET_DEBUG_LEVEL                   _IOCTL_R('D', 6, USB_trace_class_t)

typedef struct{
    uchar_t                     level;
    uint64_t                    class; 
}USB_trace_class_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_ROOT_HUB_STATUS                                                               *
*  Returns USB ROOT HUB status                                                                        *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_ROOT_HUB_STATUS               _IOCTL_R('D', 6, USB_root_hub_status_t)

typedef struct{
#define USB_HUB_MAX_PORTS                      16
    uint32_t                    ports_status[USB_HUB_MAX_PORTS];
#define USB_PORT_CONNECTED                          0x00000001
#define USB_PORT_ENABLED                            0x00000002
#define USB_PORT_SUSPENDED                          0x00000004
#define USB_PORT_IN_USE                             0x00000008

#define USB_PORT_FULL_SPEED                         0x00000010
#define USB_PORT_LOW_SPEED                          0x00000020
#define USB_PORT_HIGH_SPEED                         0x00000040
#define USB_PORT_GET_SPEED( x)                      ((x >> 4) & 0x0007)

#define USB_PORT_UHCI   	                        USB_UHCI
#define USB_PORT_EHCI   	                        USB_EHCI
#define USB_PORT_OHCI   	                        USB_OHCI
#define USB_PORT_XHCI   	                        USB_XHCI

#define USB_PORT_GET_HCD_OWNER(x)                   ((x >> 8) & 0x000F)
#define USB_PORT_SET_HCD_OWNER(x)                   (x << 8)


#define USB_PORT_STATUS_OK                          0x00001000
#define USB_PORT_STATUS_ERROR                       0x00002000

#define USB_PORT_POWER                              0x00010000

	uint32_t                    hc_condition; 	    
#define USB_HC_OK                                   0x00
#define USB_HC_ERROR                                0x01
    int                         ports_number;
}USB_root_hub_status_t;                             





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_DEVICE                                                                        *
*  Returns USB DEVICE, passing an ID to the current device                                            *
*  A value of 0 should be passed to get the first device, being the first one the root HUB            *
*  This should be used to create a connected devices map. For USB descriptors of each device,         *
*  IOCTL_USB_GET_DEVICE_DESCRIPTOR ioctl should be used instead                                       *          
*******************************************************************************************************
*/
#define IOCTL_USB_GET_DEVICE                        _IOCTL_WR('U', 1, USB_device_t)
typedef struct{
    uint16_t                    device_index;       /* [IN/OUT] device index                         */
    char                        hardware_description[80];
    char                        fs_device[80];
    uint32_t                    device_id;
    uint32_t                    vendor_id;
    uint32_t                    class_id;
    uint32_t                    interface_id;
    uint32_t                    instance_id;
	int16_t                     hub_index;          /* [OUT] connected to this hub index             */
	uint16_t                    port_index;         /* [OUT] connected to this port                  */
	
#define USB_DEVICE_STATUS_OK                        0
#define USB_DEVICE_STATUS_ERROR                     1
	
	uchar_t                     status;             /* [OUT] device status                           */
	uint16_t                    type;               /* next fields are OUT                           */
	uint16_t                    subtype; 
	uint16_t                    subtype2; 
	uchar_t                     id_string[48];      
	uchar_t                     serial_number;
	uchar_t                     vendor_str[48];
	uchar_t                     product_str[48];
	uchar_t                     module_name[16];
	uint32_t                    module_id;
	uint16_t                    address;
	uint32_t                    spec_device_instance_id;
}USB_device_t; 





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_DEVICES_NUM                                                                   *
*  Returns number of connected USB devices                                                            *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_DEVICES_NUM                   _IOCTL_R('U', 2, int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_CONTROL                                                                           *
*  Perform a USB control transfer                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_CONTROL                           _IOCTL_WR('U', 3, struct usb_ctrltransfer)
struct usb_ctrltransfer {

    uint8_t  bRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;

    uint32_t timeout;	                            /* in milliseconds */
    void *data;                                     /* pointer to data */
};
typedef struct usb_ctrltransfer                     usb_ctrltransfer_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_BULK                                                                              *
*  Perform a USB bulk transfer                                                                        *
*******************************************************************************************************
*/
#define IOCTL_USB_BULK                              _IOCTL_WR('U', 4, struct usb_bulktransfer)
struct usb_bulktransfer {
    
    unsigned int ep;
    unsigned int len;
    unsigned int timeout;                           /* in milliseconds */
    void *data;                                     /* pointer to data */
};
typedef struct usb_bulktransfer                     usb_bulktransfer_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_RESETEP                                                                           *
*  Reset an endpoint                                                                                  *
*******************************************************************************************************
*/
#define IOCTL_USB_RESETEP                           _IOCTL_W('U', 5, unsigned int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_RESETEP                                                                           *
*  Reset an endpoint                                                                                  *
*******************************************************************************************************
*/
#define IOCTL_USB_SETINTF                           _IOCTL_W('U', 6, struct usb_setinterface)
struct usb_setinterface {

    unsigned int interface;
    unsigned int altsetting;
};
typedef struct usb_setinterface                     usb_setinterface_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_SETCONFIG                                                                         *
*  Set a USB configuration                                                                            *
*******************************************************************************************************
*/
#define IOCTL_USB_SETCONFIG	                        _IOCTL_W('U', 7, unsigned int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GETDRIVER                                                                         *
*  Gets driver name                                                                                   *
*******************************************************************************************************
*/
#define IOCTL_USB_GETDRIVER                         _IOCTL_R('U', 8, struct usb_getdriver)

#define USB_MAXDRIVERNAME 255
struct usb_getdriver {
	unsigned int interface;
	char driver[USB_MAXDRIVERNAME + 1];
};
typedef struct usb_getdriver                        usb_getdriver_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_SUBMITURB                                                                         *
*  Submit a USB Request Block                                                                         *
*******************************************************************************************************
*/
#define IOCTL_USB_SUBMITURB	                        _IOCTL_W('U', 9, struct usb_urb)
#define USB_URB_DISABLE_SPD                         1
#define USB_URB_ISO_ASAP                            2
#define USB_URB_QUEUE_BULK                          0x10

#define USB_URB_TYPE_ISO                            0
#define USB_URB_TYPE_INTERRUPT                      1
#define USB_URB_TYPE_CONTROL                        2
#define USB_URB_TYPE_BULK                           3

struct usb_iso_packet_desc {
    unsigned int length;
    unsigned int actual_length;
    unsigned int status;
};

struct usb_urb {
    unsigned char type;
    unsigned char endpoint;
    int status;
    unsigned int flags;
    void *buffer;
    int buffer_length;
    int actual_length;
    int start_frame;
    int number_of_packets;
    int error_count;
    unsigned int signr;                             /* signal to be sent on error, -1 if none should be sent */
    void *usercontext;
    struct usb_iso_packet_desc iso_frame_desc;
};
typedef struct usb_iso_packet_desc                  usb_iso_packet_desc_t;
typedef struct usb_urb                              usb_urb_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_DISCARDURB                                                                        *
*  Discards a USB Request Block                                                                       *
*******************************************************************************************************
*/
#define IOCTL_USB_DISCARDURB                        _IOCTL_('U', 10)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_REAPURB                                                                           *
*  Reap a USB Request Block                                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_REAPURB                           _IOCTL_R('U', 11, void *)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_REAPURBNDELAY                                                                     *
*  Reap a USB Request Block on delay                                                                  *
*******************************************************************************************************
*/
#define IOCTL_USB_REAPURBNDELAY                     _IOCTL_R('U', 12, void *)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_CLAIMINTF                                                                         *
*  Claim a USB interface                                                                              *
*******************************************************************************************************
*/
#define IOCTL_USB_CLAIMINTF	                        _IOCTL_W('U', 13, unsigned int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_RELEASEINTF                                                                       *
*  Release a USB interface                                                                            *
*******************************************************************************************************
*/
#define IOCTL_USB_RELEASEINTF                       _IOCTL_W('U', 14, unsigned int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_CONNECTINFO                                                                       *
*******************************************************************************************************
*/
#define IOCTL_USB_CONNECTINFO                       _IOCTL_W('U', 15, struct usb_connectinfo)
struct usb_connectinfo {
    unsigned int devnum;
    unsigned char slow;
};
typedef struct usb_connectinfo                      usb_connectinfo_t;
/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_IOCTL                                                                             *
*  Passthrough USB commands                                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_IOCTL                             _IOCTL_WR('U', 16, struct usb_ioctl)

struct usb_ioctl {
    int ifno;                                       /* interface 0..N ; negative numbers reserved */
    int ioctl_code;                                 /* MUST encode size + direction of data so the
			                                         * macros in <asm/ioctl.h> give correct values */
    void *data;	                                    /* param buffer (in, or out) */
};
typedef struct usb_ioctl                            usb_ioctl_t;





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_HUB_PORTINFO                                                                      *
*  Returns port unformation                                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_HUB_PORTINFO                      _IOCTL_R('U', 17, struct usb_hub_portinfo)

struct usb_hub_portinfo {
    unsigned char numports;
    unsigned char port[127];	                    /* port to device num mapping */
};





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_RESET                                                                             *
*  Reset USB stack and hc devices                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_RESET                             _IOCTL_('U', 18)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_CLEAR_HALT                                                                        *
*  Reset USB stack and hc devices                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_CLEAR_HALT                        _IOCTL_W('U', 19, unsigned int)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_DISCONNECT                                                                        *
*  Disconnect USB                                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_DISCONNECT                        _IOCTL_('U', 20)





/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_CONNECT                                                                           *
*  Connect USB                                                                                        *
*******************************************************************************************************
*/
#define IOCTL_USB_CONNECT                           _IOCTL_('U', 21)


/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_POLL_START                                                                        *
*  Connect USB                                                                                        *
*******************************************************************************************************
*/
#define IOCTL_USB_POLL_START                        _IOCTL_('U', 22)
#define IOCTL_USB_POLL_END                          _IOCTL_('U', 23)
#define IOCTL_USB_POLL_GET_NEXT_EVENT               _IOCTL_R('U', 24, usb_event_t)
#define IOCTL_USB_SUSCRIBE_EVENTS                   _IOCTL_W('U', 25, uint32_t)
#define IOCTL_USB_POLL_GET_EVENTS_NUM               _IOCTL_R('U', 26, int)

#define USB_EVENT_PORT_CONNECT                      0x00000001
#define USB_EVENT_PORT_DISCONNECT                   0x00000002
#define USB_EVENT_HUB_ERROR                         0x00000004
#define USB_EVENT_DEVICE_DETECTED                   0x00000008


typedef struct{
	uint32_t event_id;
	uint32_t param;
}usb_event_t;


/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_SET_DEBUG_VALUES                                                                  *
*  Connect USB                                                                                        *
*******************************************************************************************************
*/
typedef struct{
    uint32_t id;
    uint32_t values[8];
}usb_debug_values_t;

#define IOCTL_USB_SET_DEBUG_VALUES                 _IOCTL_W('U', 90, usb_debug_values_t)


#define IOCTL_USB_RUN_DEBUG_OP                     _IOCTL_W('U', 91, uint32_t)


/*
*******************************************************************************************************
*  Next macros are for linux usbfs compatibility                                                      *
*******************************************************************************************************
*/
#define usbfs_ctrltransfer                          usb_ctrltransfer 
#define usbfs_bulktransfer                          usb_bulktransfer
#define usbfs_setinterface                          usb_setinterface
#define usbfs_getdriver                             usb_getdriver
#define usbfs_urb                                   usb_urb
#define usbfs_connectinfo                           usb_connectinfo
#define usbfs_ioctl                                 usb_ioctl
#define usbfs_hub_portinfo                          usb_hub_portinfo

#define IOCTL_USBFS_CONTROL                         IOCTL_USB_CONTROL
#define IOCTL_USBFS_BULK                            IOCTL_USB_BULK
#define IOCTL_USBFS_RESETEP                         IOCTL_USB_RESETEP
#define IOCTL_USBFS_SETINTF                         IOCTL_USB_SETINTF
#define IOCTL_USBFS_SETCONFIG                       IOCTL_USB_SETCONFIG
#define IOCTL_USBFS_GETDRIVER                       IOCTL_USB_GETDRIVER
#define IOCTL_USBFS_SUBMITURB                       IOCTL_USB_SUBMITURB   
#define IOCTL_USBFS_DISCARDURB                      IOCTL_USB_DISCARDURB
#define IOCTL_USBFS_REAPURB                         IOCTL_USB_REAPURB   
#define IOCTL_USBFS_REAPURBNDELAY                   IOCTL_USB_REAPURBNDELAY           
#define IOCTL_USBFS_CLAIMINTF                       IOCTL_USB_CLAIMINTF   
#define IOCTL_USBFS_RELEASEINTF                     IOCTL_USB_RELEASEINTF   
#define IOCTL_USBFS_CONNECTINFO                     IOCTL_USB_CONNECTINFO   
#define IOCTL_USBFS_IOCTL                           IOCTL_USB_IOCTL     
#define IOCTL_USBFS_HUB_PORTINFO                    IOCTL_USB_HUB_PORTINFO           
#define IOCTL_USBFS_RESET                           IOCTL_USB_RESET           
#define IOCTL_USBFS_CLEAR_HALT                      IOCTL_USB_CLEAR_HALT   
#define IOCTL_USBFS_DISCONNECT                      IOCTL_USB_DISCONNECT   
#define IOCTL_USBFS_CONNECT                         IOCTL_USB_CONNECT   

#endif

