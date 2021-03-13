/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usb.h                                                      *
* Description: USB data structures used by host controller         *
*              drivers, device drivers and user apps               *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      03-29-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#ifndef _USB_H_
#define _USB_H_

#include <sys/types.h>


/*
NEW: 
#define USB_UHCI   	                                0x01
#define USB_EHCI   	                                0x02
#define USB_OHCI   	                                0x04
#define USB_XHCI   	                                0x08
OLD: 

#define USB_TYPE_UHCI                               1
#define USB_TYPE_OHCI                               2
#define USB_TYPE_EHCI                               3
#define USB_TYPE_XHCI                               0
*/

#define USB_FULLSPEED                               0
#define USB_LOWSPEED                                1
#define USB_HIGHSPEED                               2

#define USB_MAXADDR                                 127


/****************************************************************
 * usb structs and flags
 ****************************************************************/

/* USB mandated timings (in ms)*/
#define USB_TIME_SIGATT                             100
#define USB_TIME_ATTDB                              100
#define USB_TIME_DRST                               10
#define USB_TIME_DRSTR                              50
#define USB_TIME_RSTRCY                             10

#define USB_TIME_SETADDR_RECOVERY 2

#define USB_PID_OUT                                 0xe1
#define USB_PID_IN                                  0x69
#define USB_PID_SETUP                               0x2d

#define USB_DIR_OUT                                 0               /* to device */
#define USB_DIR_IN                                  0x80            /* to host */

#define USB_TYPE_MASK                               (0x03 << 5)
#define USB_TYPE_STANDARD                           (0x00 << 5)
#define USB_TYPE_CLASS                              (0x01 << 5)
#define USB_TYPE_VENDOR                             (0x02 << 5)
#define USB_TYPE_RESERVED                           (0x03 << 5)

#define USB_RECIP_MASK                              0x1f
#define USB_RECIP_DEVICE                            0x00
#define USB_RECIP_INTERFACE                         0x01
#define USB_RECIP_ENDPOINT                          0x02
#define USB_RECIP_OTHER                             0x03

#define USB_REQ_GET_STATUS                          0x00
#define USB_REQ_CLEAR_FEATURE                       0x01
#define USB_REQ_SET_FEATURE                         0x03
#define USB_REQ_SET_ADDRESS                         0x05
#define USB_REQ_GET_DESCRIPTOR                      0x06
#define USB_REQ_SET_DESCRIPTOR                      0x07
#define USB_REQ_GET_CONFIGURATION                   0x08
#define USB_REQ_SET_CONFIGURATION                   0x09
#define USB_REQ_GET_INTERFACE                       0x0A
#define USB_REQ_SET_INTERFACE                       0x0B
#define USB_REQ_SYNCH_FRAME                         0x0C

struct usb_ctrlrequest {
    uint8_t bRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};
typedef struct usb_ctrlrequest                      usb_ctrlrequest_t;


#define USB_DT_DEVICE                               0x01
#define USB_DT_CONFIG                               0x02
#define USB_DT_STRING                               0x03
#define USB_DT_INTERFACE                            0x04
#define USB_DT_ENDPOINT                             0x05
#define USB_DT_DEVICE_QUALIFIER                     0x06
#define USB_DT_OTHER_SPEED_CONFIG                   0x07

struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};
typedef struct usb_device_descriptor                usb_device_descriptor_t;


#define USB_CLASS_PER_INTERFACE                     0       /* for DeviceClass */
#define USB_CLASS_AUDIO                             1
#define USB_CLASS_COMM                              2
#define USB_CLASS_HID                               3
#define USB_CLASS_PHYSICAL                          5
#define USB_CLASS_STILL_IMAGE                       6
#define USB_CLASS_PRINTER                           7
#define USB_CLASS_MASS_STORAGE                      8
#define USB_CLASS_HUB                               9

struct usb_config_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
};
typedef struct usb_config_descriptor                usb_config_descriptor_t;

struct usb_interface_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
};
typedef struct usb_interface_descriptor             usb_interface_descriptor_t;

struct usb_endpoint_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
};
typedef struct usb_endpoint_descriptor              usb_endpoint_descriptor_t;

#define USB_ENDPOINT_NUMBER_MASK                    0x0f    /* in bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK                       0x80

#define USB_ENDPOINT_XFERTYPE_MASK                  0x03    /* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL                   0
#define USB_ENDPOINT_XFER_ISOC                      1
#define USB_ENDPOINT_XFER_BULK                      2
#define USB_ENDPOINT_XFER_INT                       3
#define USB_ENDPOINT_MAX_ADJUSTABLE                 0x80

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#endif 

