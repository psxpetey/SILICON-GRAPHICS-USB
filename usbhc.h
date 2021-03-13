/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbhc.h                                                    *
* Description: USB data structures used by host controller         *
*              drivers, device drivers and user apps               *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      03-07-2013     -Added macro to convert big endian to little endian 16 bit integers      *
*                            -Added definitions for SetAddress and GetDeviceInfo8 commands            * 
*                                                                                                     *
* BSDero      01-25-2013     -Updated usb_pipe_t data type                                            *
*                            -Added macros for alloc and destroy pipes                                * 
*                                                                                                     *
* BSDero      10-02-2012     -Added maxaddr and type to usbhub_instance data type                     *
*                                                                                                     *
* BSDero      09-24-2012     -Fix to avoid the connect/disconnect event messages to send twice        *
*                                                                                                     *
* BSDero      07-31-2012     -Added ports_hcds field in usbhub_instance_t data type                   *
*                            -Redefine event id macros                                                * 
*                                                                                                     *
* BSDero      07-26-2012     -Added two extra arguments to USBcore_event_func_t function pointer data *
*                                type                                                                 *
*                                                                                                     *
* BSDero      07-23-2012     -Added pipe data structure definition                                    *
*                                                                                                     *
* BSDero      07-20-2012     -New function pointers in hcd_methods_t                                  *
*                            -Added macros for USBcore/usbhub events                                  *
*                            -Added macros for handle multiple instances of hcds to usb_hub_instances *
*                                                                                                     *
* BSDero      07-10-2012     -Created usb_generic_methods_t data type                                 *
*                            -Updates to usbhub_instance_t                                            *
*                                                                                                     *
* BSDero      07-05-2012     -Updated extra fields in hub_instance_t for filesystem dev support       *
*                                                                                                     *
* BSDero      06-21-2012     -Updated UHCI macros for PCI processing                                  *
*                            -Created usbhub_instance_t data type                                     *
*                            -Updated data fields in module_header_t datatype                         *
*                            -Added mutex for HCD locks                                               *
*                                                                                                     *
* BSDero      05-23-2012     -Updated UREAD1 and UREAD2 macros, fix un usbuhci fixed                  *
*                            -Added extra fields to module_header_t data type                         *
*                                                                                                     *
* BSDero      05-13-2012     -Added USBCORE_event_func_t function pointer data type                   *
*                                                                                                     *
* BSDero      05-10-2012     -Added module_header_t data type                                         *
*                            -Added methods in hcd_methods                                            *
*                                                                                                     *
* BSDero      05-10-2012     -Added module_header_t data type                                         *
*                            -Added methods in hcd_methods                                            *
*                                                                                                     *
* BSDero      05-04-2012     -Added macros for PCI registers read, methods in hcd_methods             *
*                            -Added vertex types and function pointer types                           *
*                                                                                                     *
* BSDero      04-18-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#ifndef _USBHC_H
#define _USBHC_H_

#include <sys/sema.h>
#include "usb.h"
#include "usbioctl.h"


#define USB_FSPATH                                  "/hw/usb/"


/* globals for all modules */
#define USECDELAY(ms)                               delay(drv_usectohz(ms))

/* Transfer descriptors number and addresses */
#define TD_ADDR_SIZE_VEC                            16
typedef struct{
    char        *kaddr;
    uint32_t     daddr;
    char        *data_kaddr;
    uint32_t     data_daddr;
    int          data_size;
}td_addr_t;



/* Information on a USB end point. */
struct usb_pipe {
	void        *hub;
	int         port_num;
    uint8_t     type;
#define USB_PIPE_CONTROL                            1    
    uint8_t     ep;
    uint8_t     devaddr;
    uint8_t     speed;
    uint8_t     transfer_type;
    uint16_t    maxpacket;
    int         single_dma_segment; 
    void        *hcd;
    void        *dma_node;
    void        *qh;
    td_addr_t   tds[TD_ADDR_SIZE_VEC];
    int         num_tds;
};
typedef struct usb_pipe                             usb_pipe_t;

/* Common information for usb controllers. */
struct usb_s {
    struct usb_pipe *defaultpipe;
    mutex_t resetlock;
    int busid;
    uint16_t bdf;
    uint8_t type;
    uint8_t maxaddr;
};
typedef struct usb_s                                usb_s_t;

/* Information for enumerating USB hubs */
struct usbhub_s {
    struct usbhub_op_s *op;
    struct usb_pipe *pipe;
    struct usb_s *cntl;
    mutex_t lock;
    uint32_t powerwait;
    uint32_t port;
    uint32_t threads;
    uint32_t portcount;
    uint32_t devcount;
};
typedef struct usbhub_s                             usbhub_t;




/* Hub callback (32bit) info*/
struct usbhub_op_s {
    int (*detect)(struct usbhub_s *hub, uint32_t port);
    int (*reset)(struct usbhub_s *hub, uint32_t port);
    void (*disconnect)(struct usbhub_s *hub, uint32_t port);
};


#ifdef _SGIO2_
 #define EREAD1(sc, a)                               pciio_pio_read8(  (uint8_t *)  ((sc)->pci_io_caps) + a )
 #define EREAD2(sc, a)                               pciio_pio_read16( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) )
 #define EREAD4(sc, a)                               pciio_pio_read32( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) )
 #define EWRITE1(sc, a, x)                           pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a  )
 #define EWRITE2(sc, a, x)                           pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + (a>>1)  )
 #define EWRITE4(sc, a, x)                           pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + (a>>2)  )
 #define EOREAD1(sc, a)                              pciio_pio_read8(   (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOREAD2(sc, a)                              pciio_pio_read16( (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 1) )
 #define EOREAD4(sc, a)                              pciio_pio_read32( (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 2) )
 #define EOWRITE1(sc, a, x)                          pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOWRITE2(sc, a, x)                          pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) )
 #define EOWRITE4(sc, a, x)                          pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) )
#else
 #define EREAD1(sc, a)                               ( (uint8_t )   *(  ((sc)->pci_io_caps) + a ))
 #define EREAD2(sc, a)                               ( (uint16_t)   *( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) ))
 #define EREAD4(sc, a)                               ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) ))
 #define EWRITE1(sc, a, x)                           ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a       ) = x)
 #define EWRITE2(sc, a, x)                           ( *(    (uint16_t *)  ((sc)->pci_io_caps) + (a>>1)  ) = x)
 #define EWRITE4(sc, a, x)                           ( *(    (uint32_t *)  ((sc)->pci_io_caps) + (a>>2)  ) = x)
 #define EOREAD1(sc, a)                              ( (uint8_t )   *(  ((sc)->pci_io_caps) + a + (sc)->sc_offs ))
 #define EOREAD2(sc, a)                              ( (uint16_t )  *( (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ))   
 #define EOREAD4(sc, a)                              ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ))   
 #define EOWRITE1(sc, a, x)                          ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs ) = x) 
 #define EOWRITE2(sc, a, x)                          ( *(    (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ) = x)
 #define EOWRITE4(sc, a, x)                          ( *(    (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ) = x)
#endif

#define UREAD1(sc, a)                               EOREAD1(sc, a)
#define UREAD2(sc, a)                               EOREAD2(sc, a)
#define UREAD4(sc, a)                               EOREAD4(sc, a)
#define UWRITE1(sc, a, x)                           EOWRITE1( sc, a, x)
#define UWRITE2(sc, a, x)                           EOWRITE2( sc, a, x)
#define UWRITE4(sc, a, x)                           EOWRITE4( sc, a, x)




#define PCI_CFG_GET(c,b,o,t)                        pciio_config_get(c,o,sizeof(t))
#define PCI_CFG_SET(c,b,o,t,v)                      pciio_config_set(c,o,sizeof(t),v)


#define PCI_CFG_GET8(c,o)                           PCI_CFG_GET(c,0, o, uchar_t )
#define PCI_CFG_GET16(c,o)                          PCI_CFG_GET(c,0, o, uint16_t)
#define PCI_CFG_GET32(c,o)                          PCI_CFG_GET(c,0, o, uint32_t)
#define PCI_CFG_SET8(c,o,v)                         PCI_CFG_SET(c,0, o, uchar_t, v)
#define PCI_CFG_SET16(c,o,v)                        PCI_CFG_SET(c,0, o, uint16_t, v)
#define PCI_CFG_SET32(c,o,v)                        PCI_CFG_SET(c,0, o, uint32_t, v)

/*
*******************************************************************************************************
* USB module header type                                                                              *
*******************************************************************************************************
*/
typedef struct{
    int (*set_trace_level)                            (void *, void *);	
    int (*process_event)                              (void *, void *, int, void *);
    int (*process_event_from_usbcore)                 (void *, int, void *, void *, void *);
    uint64_t										  flags;
}usb_generic_methods_t;

typedef struct{
    int (*hcd_reset)                                  (void *);
    int (*hcd_start)                                  (void *);    
    int (*hcd_init)                                   (void *);    
    int (*hcd_stop)                                   (void *);    
    int (*hcd_shutdown)                               (void *);    
    int (*hcd_suspend)                                (void *);    
    int (*hcd_resume)                                 (void *);
    int (*hcd_status)                                 (void *); 
	void (*hcd_free_pipe)                             (void *, struct usb_pipe *);
    usb_pipe_t *(*hcd_alloc_control_pipe)             (void *, int, td_addr_t *);
    usb_pipe_t *(*hcd_alloc_bulk_pipe)                (void *, struct usb_pipe *, struct usb_endpoint_descriptor *);
    int (*hcd_send_control)                           (void *, struct usb_pipe *, int, void *, int, void *, int);
    int (*hcd_set_address)                            (void *, struct usb_pipe *, int, void *, int, void *, int);
    int (*hcd_usb_send_bulk)                          (void *, struct usb_pipe *, int, void *, int);
    int (*hcd_alloc_intr_pipe)                        (void *, struct usb_pipe *, struct usb_endpoint_descriptor *);
    int (*hcd_usb_poll_intr)                          (void *, struct usb_pipe *, void *);
    int (*hcd_ioctl)                                  (void *, int, void *);
    int (*hcd_set_trace_level)                        (void *, void *);
    int (*hcd_hub_info)                               (void *, void *); 
    int (*hcd_port_action)                            (void *, int, int);
    uint32_t (*hcd_get_port)                          (void *, int); 
    int (*hcd_set_roothub)                            (void *, void *);
    int (*hcd_set_debug_values)                       (void *, void *);
    uint32_t                                          flags; 
}hcd_methods_t;

typedef struct{
	int (*update_hub_info)                            (void *, void *);
	int (*process_event_from_hcd)                     (void *, void *, int, void *, void *);
}hub_methods_t;


typedef struct{
    uint32_t                    module_id;
    char                        short_description[12];
    char                        long_description[80];
    char                        module_name[32];
    uint32_t                    type;
    usb_generic_methods_t       methods; 
}module_header_t;

typedef struct{
	module_header_t             *module_header;
    char                        hardware_description[80];
    char                        fs_device[80];
    uint32_t                    device_id;
    uint32_t                    vendor_id;
    uint32_t                    class_id;
    uint32_t                    interface_id;
    uint32_t                    info_size;
    uint32_t                    instance_id;
    uint32_t                    spec_device_instance_id;
    int                         hub_index;
    int                         port_index;
/*    hcd_methods_t               *methods; */
    void                        *methods;
    void                        *soft;              /* reserved                                     */
}device_header_t;


/*
*******************************************************************************************************
* Host Controller ioctls                                                                              *
*******************************************************************************************************
*/


/*
*******************************************************************************************************
* USBcore Instance Type                                                                               *
*******************************************************************************************************
*/
typedef int( *USB_func_t)( void *);
typedef int( *USB_ioctl_func_t)(void *, void*);
typedef int( *USBCORE_event_func_t)(void *, void *, int, void *, void *, void *);
typedef struct{
	device_header_t         device_header;
    vertex_hdl_t            conn;
    vertex_hdl_t            masterv;
    vertex_hdl_t            usbdaemon;
    vertex_hdl_t            usbcore;
    int                     mode;
    USB_func_t              register_module; 
    USB_func_t              unregister_module;
    USB_func_t              register_device; 
    USB_func_t              unregister_device;

    USBCORE_event_func_t    process_event_from_hcd;
    USBCORE_event_func_t    process_event_from_device;  
      
    mutex_t                 mutex;   
    /* this mutexes will be used during HCD attach only */  
	/* critical for correct UHCI attach */
	mutex_t                 uhci_mutex; 
	mutex_t                 ehci_mutex;
	uint32_t                debug_port;
}usbcore_instance_t;

	
#define USB_HUB_MAX_PORTS                           16
typedef struct{ 
    device_header_t             device_header;
    int                         hub_type;
    int                         hub_index;          /* index of hub                                  */
	int                         hub_parent;         /* index of hub parent                           */
	uchar_t                     hcd_drivers;        /* HCD supported driver by this HUB              */
	uchar_t                     pci_bus;
	uchar_t                     pci_slot;
	uchar_t                     pci_function;
	uchar_t                     hcd_owner;
	uint16_t                    address;
    int                         ports_number;
    uint32_t                    ports_status[USB_HUB_MAX_PORTS];
    device_header_t             *ports_hcds[USB_HUB_MAX_PORTS]; /* ports handled by hcd */
    usbcore_instance_t          *usbcore;
    vertex_hdl_t                ps_conn;            /* connection for pci services  */
    vertex_hdl_t                ps_vhdl;            /* backpointer to device vertex */
    vertex_hdl_t                ps_charv;           /* backpointer to device vertex */
#define USB_HUB_MAX_HCDS                            4
    device_header_t             *hcds[USB_HUB_MAX_HCDS];
    int                         hcd_instances_num;
	mutex_t                     mutex;              /* sync HCD messages            */
	mutex_t                     uhci_mutex;         /* UHCI need this for correct 
	                                                   sync between instances       */
	                                                
	                                                
	/* we need this 'cause a port connect event in UHCI controller triggers a 
	 * disconnect event in EHCI controller and vice versa, so we have a single event 
	 * creating two event messages in the same port generating a connect and disconnect in 
	 * EHCI/UHCI controllers. So we'll use this to send just one event message to USBcore. */
#define USB_CD_MAX_EVENTS                          8	 
	uint32_t                    cd_port_event[USB_CD_MAX_EVENTS];
	uint32_t                    cd_time_event[USB_CD_MAX_EVENTS];
	int                         cd_event_id[USB_CD_MAX_EVENTS];
	int                         cd_events_num;
    toid_t                      cd_itimeout;
    uint8_t                     maxaddr;
}usbhub_instance_t;



/*
*******************************************************************************************************
* EVENTS FROM HCD                                                                                     *
*******************************************************************************************************
*/
#define USB_HCD_GET_INSTANCE_ID                     1001
#define USB_HCD_ATTACHED_EVENT                      1002
#define USB_HUB_DEVICE_GET_STATUS                   1003
#define USB_HUB_PORT_RESET							1004
#define USB_HCD_SET_OWNER                           1005
#define USB_HUB_SET_ADDRESS                         1006
#define USB_HUB_ALLOC_PIPE                          1007
#define USB_HUB_DESTROY_PIPE                        1008
#define USB_GET_DEVICE_INFO8                        1009
#define USB_SET_DEBUG_VALUES                        1010
#define USB_RUN_DEBUG_OP                            1011
#define USB_GET_DESCRIPTOR                          1012
/*
*******************************************************************************************************
* Events for USBcore in device events (USB Stack only)                                                *
*******************************************************************************************************
*/

#define USB_EVENT_EXIT_MAIN_THREAD                  2001
#define B2LENDIAN( x)                              (uint16_t)(((x & 0x00ff) << 8) | \
                                                   ((x & 0xff00) >> 8))
#endif
