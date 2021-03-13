/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbcore.c                                                  *
* Description: USB core kernel driver                              *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      14-02-2013     -Added SET_ADDRESS USB command                                           *
*                                                                                                     *
* BSDero      08-15-2012     -TRACE messages with wrong class fixed                                   *
*                            -Issues in 64 bit trace macros solved                                    *
*                                                                                                     *
* BSDero      08-03-2012     -krand() function moved into kutils.c                                    *
*                            -Added _diaginfo_ string in startup trace messages                       * 
*                                                                                                     *
* BSDero      07-30-2012     -Added message queue and event processing function for thread            *
*                                                                                                     *
* BSDero      07-26-2012     -Added usb_core_main_thread() thread and messages                        *
*                                                                                                     *
* BSDero      07-23-2012     -Added process_event_from_device() function                              *
*                                                                                                     *
* BSDero      07-19-2012     -IOCTL_USB_GET_ROOT_HUB_STATUS, IOCTL_USB_GET_DEVICES_NUM and            *
*                                 IOCTL_USB_GET_DEVICE ioctls updated                                 *
*                            -Added kmaddr.c module for get usbcore module base address               *
*                                                                                                     *
* BSDero      07-12-2012     -Added devices register and unregister                                   *
* BSDero      07-12-2012     -Added devices list                                                      *
*                                                                                                     *
* BSDero      06-26-2012     -Added events function handler usbcore_hcd_event() from HCD drivers      *
*                                                                                                     *
* BSDero      06-21-2012     -Added IOCTL_USB_GET_ROOT_HUB_STATUS ioctl                               *
*                                                                                                     *
* BSDero      06-15-2012     -Added IOCTL_USB_GET_ROOT_HUB_INFO ioctl                                 *
*                                                                                                     *
* BSDero      06-14-2012     -Added mutex initialization and destroy                                  *
*                                                                                                     *
* BSDero      06-12-2012     -Added krand function                                                    *
*                            -Added modules register and unregister functions                         *
*                            -Added modules list                                                      *
*                            -Deleted HCD modules references, now hub handles HCD requests            *
*                                                                                                     *
* BSDero      05-18-2012     -Added HCD modules register and unregister functions                     *
*                                                                                                     *
* BSDero      05-14-2012     -Added module header for usbcore                                         *
*                            -Added hcd_modules and modules lists                                     *
*                            -Changed printfs for TRACE calls                                         *
*                                                                                                     *
* BSDero      05-03-2012     -Removed usbcore_instance_t data type. It's on usbhc.h now               *
*                            -Added removal of char devices in filesystem                             *
*                                                                                                     *
* BSDero      04-27-2012     -Added basic ioctl skeleton functions                                    *
*                            -Added ioctl function pointers to ioctl table                            *
*                            -HCD registering capability added                                        *
*                            -get_ioctl_str() function removed, instead get_ioctl_item was added      *
*                                                                                                     *
* BSDero      04-18-2012     -Specific usb stack code basic functionality added                       *
*                            -Support for usbcore and usbdaemon devices in filesystem                 *
*                            -ioctl support added and trace                                           *
*                            -ioctl table added                                                       *
*                                                                                                     *
* BSDero      03-29-2012     -Initial version, created from an existing generic kernel module         *
*                                                                                                     *
*******************************************************************************************************
*/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/poll.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/ksynch.h>
#include <sys/atomic_ops.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/cmn_err.h>
#include <sys/mload.h>
#include <string.h>
#include <ctype.h>




/*
*******************************************************************************************************
* Headers                                                                                             *
*******************************************************************************************************
*/
#include "config.h"
#include "usbioctl.h"
#include "usb.h"
#include "usbhc.h"                                  /* Host controller defines */



/*
*******************************************************************************************************
* Global for all included sources                                                                     *
*******************************************************************************************************
*/
USB_trace_class_t                                   global_trace_class = { 12, TRC_ALL};


/*
*******************************************************************************************************
* Included sources                                                                                    *
*******************************************************************************************************
*/
#include "kmaddr.c"                                 /* module initial address */
#include "trace.c"                                  /* trace macros           */
#include "kutils.c"                                 /* kernel util functions  */
#include "dumphex.c"                                /* dump utility           */
#include "list.c"                                   /* list utility           */
#include "gc.c"                                     /* memory lists utility   */
#include "dma.c"                                    /* dma lists utility      */
#include "queue.c"                                  /* circular queue utility */

/*
*******************************************************************************************************
* IOCTL Table                                                                                         *
*******************************************************************************************************
*/
typedef struct{
    int                  cmd;
	char                *str;
	USB_ioctl_func_t    func;
} ioctl_item_t;


/*
*******************************************************************************************************
* Utility function declaration and source code                                                        *
*******************************************************************************************************
*/
void usbcore_main_thread( void *arg0, void *arg1, void *arg2, void *arg3);

int ioctl_no_func( void *hcd, void *arg);
int usbcore_register_module( void *arg);
int usbcore_unregister_module( void *arg);
int usbcore_register_device( void *arg);
int usbcore_unregister_device( void *arg);


int dump_ioctl( int n);
int run_event( usbcore_instance_t *usbcore, int event, device_header_t *device, void *arg0, void *arg1, void *arg2);
int process_event_from_hcd( void *p_usbcore, void *p_hcd, int event, void *arg0, void *arg1, void *arg2);
int process_event_from_device( void *p_usbcore, void *p_device, int event, void *arg0, void *arg1, void *arg2);
int usb_enumerate( device_header_t *device, uint32_t port_num);

int ioctl_usb_get_driver_info( void *hcd, void *arg);
int ioctl_usb_get_num_modules( void *hcd, void *arg);
int ioctl_usb_not_implemented( void *hcd, void *arg);
int ioctl_usb_get_root_hub_info( void *hcd, void *arg);
int ioctl_usb_set_debug_level( void *hcd, void *arg);
int ioctl_usb_get_debug_level( void *hcd, void *arg);
int ioctl_usb_get_module_info( void *hcd, void *arg);
int ioctl_usb_get_root_hub_status( void *hcd, void *arg);
int ioctl_usb_get_devices_num( void *hcd, void *arg);
int ioctl_usb_get_device( void *hcd, void *arg);
int ioctl_usb_set_debug_values( void *hcd, void *arg);
int ioctl_usb_run_debug_op( void *hcd, void *arg);

ioctl_item_t usbcore_ioctls[]={
    IOCTL_USB_GET_DRIVER_INFO,      "IOCTL_USB_GET_DRIVER_INFO",     ioctl_usb_get_driver_info, /* 0*/
    IOCTL_USB_GET_NUM_MODULES,      "IOCTL_USB_GET_NUM_MODULES",     ioctl_usb_get_num_modules,
    IOCTL_USB_GET_MODULE_INFO,      "IOCTL_USB_GET_MODULE_INFO",     ioctl_usb_get_module_info,
    IOCTL_USB_GET_ROOT_HUB_INFO,    "IOCTL_USB_GET_ROOT_HUB_INFO",   ioctl_usb_get_root_hub_info, 
    IOCTL_USB_SET_DEBUG_LEVEL,      "IOCTL_USB_SET_DEBUG_LEVEL",     ioctl_usb_set_debug_level,
    IOCTL_USB_GET_DEBUG_LEVEL,      "IOCTL_USB_GET_DEBUG_LEVEL",     ioctl_usb_get_debug_level,
    IOCTL_USB_GET_ROOT_HUB_STATUS,  "IOCTL_USB_GET_ROOT_HUB_STATUS", ioctl_usb_get_root_hub_status,
    IOCTL_USB_GET_DEVICE,           "IOCTL_USB_GET_DEVICE",          ioctl_usb_get_device,
    IOCTL_USB_GET_DEVICES_NUM,      "IOCTL_USB_GET_DEVICES_NUM",     ioctl_usb_get_devices_num,
    IOCTL_USB_CONTROL,              "IOCTL_USB_CONTROL",             ioctl_usb_not_implemented,
    IOCTL_USB_BULK,                 "IOCTL_USB_BULK",                ioctl_usb_not_implemented, /* 10 */
    IOCTL_USB_RESETEP,              "IOCTL_USB_RESETEP",             ioctl_usb_not_implemented,
    IOCTL_USB_SETINTF,              "IOCTL_USB_SETINTF",             ioctl_usb_not_implemented,
    IOCTL_USB_SETCONFIG,            "IOCTL_USB_SETCONFIG",           ioctl_usb_not_implemented,
    IOCTL_USB_GETDRIVER,            "IOCTL_USB_GETDRIVER",           ioctl_usb_not_implemented,
    IOCTL_USB_SUBMITURB,            "IOCTL_USB_SUBMITURB",           ioctl_usb_not_implemented,
    IOCTL_USB_DISCARDURB,           "IOCTL_USB_DISCARDURB",          ioctl_usb_not_implemented,
    IOCTL_USB_REAPURB,              "IOCTL_USB_REAPURB",             ioctl_usb_not_implemented,
    IOCTL_USB_REAPURBNDELAY,        "IOCTL_USB_REAPURBNDELAY",       ioctl_usb_not_implemented,
    IOCTL_USB_CLAIMINTF,            "IOCTL_USB_CLAIMINTF",           ioctl_usb_not_implemented,
    IOCTL_USB_RELEASEINTF,          "IOCTL_USB_RELEASEINTF",         ioctl_usb_not_implemented, /* 20 */
    IOCTL_USB_CONNECTINFO,          "IOCTL_USB_CONNECTINFO",         ioctl_usb_not_implemented,
    IOCTL_USB_IOCTL,                "IOCTL_USB_IOCTL",               ioctl_usb_not_implemented,
    IOCTL_USB_HUB_PORTINFO,         "IOCTL_USB_HUB_PORTINFO",        ioctl_usb_not_implemented,
    IOCTL_USB_RESET,                "IOCTL_USB_RESET",               ioctl_usb_not_implemented,
    IOCTL_USB_CLEAR_HALT,           "IOCTL_USB_CLEAR_HALT",          ioctl_usb_not_implemented,
    IOCTL_USB_DISCONNECT,           "IOCTL_USB_DISCONNECT",          ioctl_usb_not_implemented,
    IOCTL_USB_CONNECT,              "IOCTL_USB_CONNECT",             ioctl_usb_not_implemented, /* 27 */
    IOCTL_USB_SET_DEBUG_VALUES,     "IOCTL_USB_SET_DEBUG_VALUES",    ioctl_usb_set_debug_values, 
    IOCTL_USB_RUN_DEBUG_OP,         "IOCTL_USB_RUN_DEBUG_OP",        ioctl_usb_run_debug_op,  
	-1,                             "UNKNOWN IOCTL",                 ioctl_no_func,
    0, NULL, NULL
};

module_header_t usbcore_module_header={
	USB_MOD_CORE, 
	"usbcore", 
	"USBcore device driver", 
	"usbcore.o", 
	USB_DRIVER_IS_CORE
};




/*
*******************************************************************************************************
* Loadable modules stuff and global Variables                                                         *
*******************************************************************************************************
*/
char                                                *usbcore_mversion = M_VERSION;
int                                                 usbcore_devflag = D_MP;
unsigned int                                        usbcore_busy = 0;
gc_list_t                                           gc_list;
usbcore_instance_t                                  *global_soft = NULL;
#define MAX_HCD                                     8
module_header_t                                     *hcd_instances[MAX_HCD];
int                                                 hcd_instances_num = 0;
int                                                 all_modules_num = 0;
#define MAX_MODULES                                 32 
module_header_t                                     *modules[MAX_MODULES];

#define MAX_USB_DEVICES                             32
device_header_t                                     *usb_devices[MAX_USB_DEVICES];
int                                                 usb_devices_num = 0;

queue_t                                             msg_queue;
/*
*******************************************************************************************************
* Entry points for USBcore                                                                            *
*******************************************************************************************************
*/
void usbcore_init(void);
int usbcore_unload(void);
int usbcore_reg(void);
int usbcore_unreg(void);
int usbcore_attach(vertex_hdl_t conn);
int usbcore_detach(vertex_hdl_t conn);
int usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp);
int usbcore_close(dev_t dev);
int usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp);
int usbcore_write();
int usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp);
int usbcore_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int usbcore_unmap(dev_t dev, vhandl_t *vt);




/*
*******************************************************************************************************
* ioctl functions source code                                                                         *
*******************************************************************************************************
*/
int ioctl_usb_get_driver_info( void *hcd, void *arg){ 
    USB_driver_info_t *info = ( USB_driver_info_t *) arg;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL | TRC_START_END;

    TRACE( class, 10, "start", "");
    strcpy( info->long_name,     USBCORE_DRV_LONG_NAME);
    strcpy( info->short_name,    USBCORE_DRV_SHORT_NAME);
    strcpy( info->version,       USBCORE_DRV_VERSION);        	
    strcpy( info->short_version, USBCORE_DRV_SHORT_VERSION);
    strcpy( info->seqn,          USBCORE_DRV_SEQ);
    strcpy( info->build_date,    USBCORE_DRV_BUILD_DATE);        	

    TRACE( class, 10, "end", "");    
    return( 0);	
}



int ioctl_usb_not_implemented( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 6, "Not implemented", "");
    TRACE( class | TRC_START_END, 10, "end", "");    
	return( 0);
}



int ioctl_no_func( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class , 6, "ioctl not exist", "");
    TRACE( class | TRC_START_END, 10, "end", "");    
	
    return(0);
}



int ioctl_usb_set_debug_level( void *hcd, void *arg){
	usbcore_instance_t *soft = (usbcore_instance_t *) hcd;
	usb_generic_methods_t *methods;
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
	int i;
	char str[256];
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
	
	
    global_trace_class.class = trace_class->class;
    global_trace_class.level = trace_class->level;

    if( all_modules_num > 1){
        /* USBcore module is in modules[0], so we'll bypass it */
        for( i = 1 ; i < all_modules_num; i++){
    		methods = ( usb_generic_methods_t *) &modules[i]->methods;
			methods->set_trace_level( NULL,  (void *) &global_trace_class);
		}
	}

    TRACE( class, 12, "trace class settled; class=0x%x, level=%d", 
       global_trace_class.class, global_trace_class.level);
    TRACE( class | TRC_START_END, 10, "end", "");
	return(0);
} 



int ioctl_usb_get_debug_level( void *hcd, void *arg){
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "trace class settled; class=0x%x, level=%d", 
       global_trace_class.class, global_trace_class.level);
	
    trace_class->class = global_trace_class.class;
    trace_class->level = global_trace_class.level;
    
    TRACE( class | TRC_START_END, 10, "end", "");
	return(0);
} 



int ioctl_usb_get_num_modules( void *hcd, void *arg){
	int *num_modules = (int *) arg;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
	
	*num_modules = all_modules_num;

    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);
}



int ioctl_usb_get_module_info( void *instance, void *arg){ 
	USB_driver_module_info_t *info = (USB_driver_module_info_t *) arg;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
    
    TRACE( class, 6, "info->num_module=%d, all_modules_num=%d", 
       info->num_module, all_modules_num);

	if( info->num_module < 0){
        return( EINVAL);		
	}
	
	if( info->num_module >= all_modules_num){
		return( EINVAL);
    }
    
    info->module_id = modules[info->num_module]->module_id;
    info->type = modules[info->num_module]->type;
    bcopy( modules[info->num_module]->short_description, info->short_description, 12 );
    bcopy( modules[info->num_module]->long_description, info->long_description, 80 );
    bcopy( modules[info->num_module]->module_name, info->module_name, 32 );

    TRACE( class, 6, "module_id=%d", info->module_id );
    TRACE( class, 6, "type=%d", info->type );
    TRACE( class, 6, "short_description='%s'", info->short_description);
    TRACE( class, 6, "long_description='%s'", info->long_description);
    TRACE( class, 6, "module_name='%s'", info->module_name);
    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);	
}


int ioctl_usb_get_root_hub_info( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;
    USB_root_hub_info_t *info = (USB_root_hub_info_t *) arg;
    usbhub_instance_t *hub;
    int rc;
    TRACE( class | TRC_START_END, 10, "start", "");

    if( usb_devices[0] == NULL){
		/* device not available */
		rc = EUNATCH;
    }else{
	    hub = (usbhub_instance_t *) usb_devices[0];
	    info->hub_type = hub->hub_type; 
	    info->hub_index = hub->hub_index;
	    info->hub_parent = hub->hub_parent;
	    info->hcd_drivers = hub->hcd_drivers;
	    info->pci_bus = hub->pci_bus;
	    info->pci_function = hub->pci_function;
	    info->pci_slot = hub->pci_slot;
	    info->product_id = hub->device_header.device_id;
	    info->vendor_id = hub->device_header.vendor_id;
	    info->ports_number = hub->ports_number;
	    info->address = hub->address;
	    
	    strcpy( (char *) info->fs_device, (char *) hub->device_header.fs_device);
	    strcpy( (char *) info->hardware_description, 
	        (char *) hub->device_header.hardware_description);
	    rc = 0;
    }
    TRACE( class, 6, "end rc=%d", rc);    
    return(rc);
}


int ioctl_usb_get_root_hub_status( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;
    USB_root_hub_status_t *status = (USB_root_hub_status_t *) arg;
    usbhub_instance_t *hub;
    module_header_t *header;
    int i;
    int rc;
    if( usb_devices[0] == NULL){
		/* device not available */
		rc = EUNATCH;
    }else{
	    hub = (usbhub_instance_t *) usb_devices[0];
	    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
	        hub, USB_HUB_DEVICE_GET_STATUS, (void *) status, NULL, NULL);
	    for( i = 0; i < hub->ports_number; i++)
	        status->ports_status[i] = hub->ports_status[i];
	    rc = 0;
    }
    TRACE( class, 6, "end rc=%d", rc);    
    return(rc);
}


int ioctl_usb_get_devices_num( void *hcd, void *arg){
	int *num_devices = (int *) arg;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");
	
	*num_devices = usb_devices_num;

    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);
}


int ioctl_usb_get_device( void *hcd, void *arg){
	USB_device_t *device = (int *) arg;
	uint16_t i;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;
    char buffer[1024];

    TRACE( class | TRC_START_END, 10, "start", "");
	i = device->device_index;
	
	TRACE( class, 10, "device index=%d", i);
	if(( i < 0 ) || ( i >= usb_devices_num)){
	    return( EINVAL);
	}
	
    strcpy( device->hardware_description, usb_devices[i]->hardware_description);
    strcpy( device->module_name, usb_devices[i]->module_header->module_name);
    strcpy( device->fs_device, usb_devices[i]->fs_device);
    device->device_id = usb_devices[i]->device_id;
    device->vendor_id = usb_devices[i]->vendor_id;
    device->class_id = usb_devices[i]->class_id;
    device->interface_id = usb_devices[i]->interface_id;
    device->instance_id = usb_devices[i]->instance_id;
    device->module_id = usb_devices[i]->module_header->module_id;
    device->hub_index = usb_devices[i]->hub_index;
    device->port_index = usb_devices[i]->port_index;
    device->status = usb_devices[i]->module_header->methods.process_event_from_usbcore( 
	        usb_devices[i], USB_HUB_DEVICE_GET_STATUS, (void *) &buffer, NULL, NULL);
    
    TRACE( class | TRC_START_END, 10, "end", "");


    return(0);
}


int ioctl_usb_set_debug_values( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;
    usb_debug_values_t *v = (usb_debug_values_t *) arg;
	usbcore_instance_t *soft = (usbcore_instance_t *) hcd;
	usb_generic_methods_t *methods;
	usbhub_instance_t *hub;
	int rc;
    
    TRACE( class | TRC_START_END, 10, "start", "");
    
    if( usb_devices[0] == NULL){
		/* device not available */
		rc = EUNATCH;
    }else{
	    hub = (usbhub_instance_t *) usb_devices[0];
	    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
	        hub, USB_SET_DEBUG_VALUES, (void *) v, NULL, NULL);

	    rc = 0;
    }

    soft->debug_port = v->values[2];
    
    TRACE( class | TRC_START_END, 10, "end rc = %d", rc);
    return(0);

}


int ioctl_usb_run_debug_op( void *hcd, void *arg){
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;
    uint32_t *v = (uint32_t *) arg;
	usbcore_instance_t *soft = (usbcore_instance_t *) hcd;
    USB_root_hub_status_t status;
    usbhub_instance_t *hub;
    module_header_t *header;
    int i, rc;

    TRACE( class | TRC_START_END, 10, "start", "");
    
    if( usb_devices[0] == NULL){
		/* device not available */
		rc = EUNATCH;
    }else{
        /* get hub */
	    hub = (usbhub_instance_t *) usb_devices[0];
	    
	    /* check if the port has some device */
	    i = (int) soft->debug_port;
	    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
	        hub, USB_HUB_DEVICE_GET_STATUS, (void *) &status, NULL, NULL);
	    
	    if( ( hub->ports_status[i] & USB_PORT_IN_USE) == 0){
			rc = 1;
			goto exit_usb_run_debug_op;
		}
		
		/* ok, now reset the port */
		
			    
	    
    }
    

    

exit_usb_run_debug_op:
    TRACE( class | TRC_START_END, 10, "end rc = %d", rc);
    return( rc);
}


/*
*******************************************************************************************************
* FUNCTION:  get_ioctl_item                                                                           *
* BEHAVIOR:  Returns a pointer to the ioctl item table with the description of the ioctl passed as    *
*            argument. It will return NULL if ioctl table has no ioctl values at all. If the ioctl    *
*			 was not found, it'll return a pointer to a ioctl item descripting a Unknown IOCTL        *
* ARGUMENTS: A int value with the ioctl                                                               *
* RETURNS:   A pointer to a string descripting the ioctl passed as argument,                          *
*            "UNKNOWN IOCTL" if the ioctl passed as argument was not found                            *
*******************************************************************************************************
*/
ioctl_item_t *get_ioctl_item( int cmd){
    int i;


    for( i = 0; usbcore_ioctls[i].str != NULL; i++){
	    if( cmd == usbcore_ioctls[i].cmd)
		    return( &usbcore_ioctls[i] );
    }

    return( &usbcore_ioctls[i - 1]);
}

/*
*******************************************************************************************************
* Entry points for USBcore                                                                            *
*******************************************************************************************************
*/
void usbcore_init(void){
    vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbcore_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbdaemon_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t connv = GRAPH_VERTEX_NONE;
    graph_error_t ret = (graph_error_t) 0;
    usbcore_instance_t *soft, *soft_usbcore_device, *soft_usbdaemon_device; 
    soft = soft_usbcore_device = soft_usbdaemon_device = NULL;
    uint64_t class = TRC_MOD_CORE | TRC_INIT;
    int rc; 

  	TRACE( class | TRC_START_END, 10, "start", "");
  	printf( "**********************************************************\n");
  	printf( "* USBCORE Driver and stack for Silicon Graphics Irix 6.5 *\n");
  	printf( "* By bsdero at gmail dot org, 2011                       *\n");
  	printf( "* Version %s                                        *\n", USBCORE_DRV_VERSION);
  	printf( "* Sequence %s                                *\n", USBCORE_DRV_SEQ);
  	printf( "**********************************************************\n");
    printf( "usbcore kernel module loaded! \n");
    printf( "_diaginfo_: Kernel usbcore module base address :0x%x\n", module_address);


    
    /* initialize garbage collector list */
    gc_list_init( &gc_list);

    /* initialize this instance */
    soft = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft == NULL){
		TRACERR( class, "gc_malloc() returned NULL, quitting", "");
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    
    bzero( soft, sizeof( usbcore_instance_t));
    
    
    /* add /hw/usb path */
    ret = hwgraph_path_add( 
                GRAPH_VERTEX_NONE,      /* start at /hw */
                "/usb",                   /* this is the path */
                &masterv);              /* put vertex there */    
    TRACE( class, 12, "rc of hwgraph_path_add() = %d, master vertex", ret);

    /* couldn't add, goto done */
    if (ret != GRAPH_SUCCESS) 
        goto done;

    /* add char device /hw/usb/usbcore */
    ret = hwgraph_edge_get( masterv, "usbcore", &usbcore_device_vtx);
    TRACE( class, 12, "rc of hwgraph_edge_get() = %d, added usbcore edge", ret);
    ret = hwgraph_char_device_add( masterv, "usbcore", "usbcore_", &usbcore_device_vtx); 
    TRACE( class, 12, "rc of hwgraph_char_device_add() = %d, added usbcore edge (char device)", ret);

    /* add char device /hw/usb/usbdaemon */
    ret = hwgraph_edge_get( masterv, "usbdaemon", &usbdaemon_device_vtx);
    TRACE( class, 12, "rc of hwgraph_edge_get() = %d, added usbdaemon edge", ret);
    ret = hwgraph_char_device_add( masterv, "usbdaemon", "usbcore_", &usbdaemon_device_vtx); 
    TRACE( class, 12, "rc of hwgraph_char_device_add() = %d, added usbdaemon edge (char device)", ret);

    /* initialize usbcore instance */
    soft->device_header.module_header = &usbcore_module_header;
    soft->masterv = masterv; 
    soft->conn = connv;
    soft->usbdaemon = usbdaemon_device_vtx; /* note which vertex is "usbdaemon" */
    soft->usbcore = usbcore_device_vtx; /* note which vertex is "usbcore" */
    soft->register_module = usbcore_register_module;
    soft->unregister_module = usbcore_unregister_module;
    soft->register_device = usbcore_register_device;
    soft->unregister_device = usbcore_unregister_device;
    soft->process_event_from_hcd = process_event_from_hcd;
    
    soft->process_event_from_device = process_event_from_device;
    soft->mode = 0xff;
    MUTEX_INIT( &soft->mutex, MUTEX_DEFAULT, "usbcore");
    MUTEX_INIT( &soft->uhci_mutex, MUTEX_DEFAULT, "usbcore");
    MUTEX_INIT( &soft->ehci_mutex, MUTEX_DEFAULT, "usbcore");

    device_info_set( masterv, soft);

    /* add usbcore instance of device */
    soft_usbcore_device = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft_usbcore_device == NULL){
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    
    bcopy( soft, soft_usbcore_device, sizeof( usbcore_instance_t));
    soft_usbcore_device->mode = 0x00;
    device_info_set( usbcore_device_vtx, soft_usbcore_device);
    

    /* add daemon instance of device */
    soft_usbdaemon_device = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft_usbdaemon_device == NULL){
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    bcopy( soft, soft_usbdaemon_device, sizeof( usbcore_instance_t));
    soft_usbdaemon_device->mode = 0x01;    
    device_info_set( usbdaemon_device_vtx, soft_usbdaemon_device);

    /* clear list of host controller methods */    
    bzero( (void *) hcd_instances, sizeof( hcd_methods_t *) * MAX_HCD);
    hcd_instances_num = 0;   
    
    /* clear list of USB devices */    
    bzero( (void *) usb_devices, sizeof( void *) * MAX_USB_DEVICES);
   
    /* clear array of pointers to module descriptions */
    bzero( (void *) modules, sizeof( module_header_t *) * MAX_MODULES);
    
    /* the first module should be usbcore module */
    modules[0] = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    bcopy( (void *) &usbcore_module_header, (void *) modules[0], 
        sizeof( module_header_t));
    all_modules_num = 1;
    
    /* initialize queue */
    queue_init( &msg_queue);

    /* Create a system thread to do background work */
    rc = drv_thread_create("usbcore_main_thread", 0, 0, 0, usbcore_main_thread, 
         (void *) soft, NULL, NULL, NULL);

    if (rc ) {
        cmn_err(CE_WARN, "Creation of usbcore_main_thread() failed\n");
    }
    
done: /* If any error, undo all partial work */
    if (ret != 0)   {
        if ( usbdaemon_device_vtx != GRAPH_VERTEX_NONE) 
            hwgraph_vertex_destroy( usbdaemon_device_vtx);
            
        if ( usbcore_device_vtx != GRAPH_VERTEX_NONE) 
            hwgraph_vertex_destroy( usbcore_device_vtx);
            
        if ( soft){ 
            gc_list_destroy( &gc_list);  
        }
      
    }

    global_soft = soft;
    
    
    TRACE( class | TRC_START_END, 10, "end", "");
       
}

int usbcore_unload(void){
    vertex_hdl_t conn = GRAPH_VERTEX_NONE;
    vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbcore_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbdaemon_device_vtx = GRAPH_VERTEX_NONE;
    usbcore_instance_t *soft; 
    uint64_t class = TRC_MOD_CORE | TRC_UNLOAD;
    msg_t             message;

    TRACE( class | TRC_START_END, 10, "start", "");
    soft = global_soft;    

    message.msg_id = USB_EVENT_EXIT_MAIN_THREAD;
    
    
	MUTEX_LOCK( &soft->mutex, -1);
	queue_put( &msg_queue, &message); 
	MUTEX_UNLOCK( &soft->mutex);

    queue_print( &msg_queue);
    /* send event to thread */
    wakeup( (caddr_t) soft);
    
    USECDELAY(100000);
    
    MUTEX_DESTROY( &soft->mutex);
    MUTEX_DESTROY( &soft->uhci_mutex);
    MUTEX_DESTROY( &soft->ehci_mutex);
    masterv = soft->masterv;
    usbcore_device_vtx = soft->usbcore;
    usbdaemon_device_vtx = soft->usbdaemon;
    
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "/usb", &masterv))
        return -1;    
    
    hwgraph_edge_remove(masterv, "usbcore", &usbcore_device_vtx);    
    hwgraph_edge_remove(masterv, "usbdaemon", &usbdaemon_device_vtx);    
    
    hwgraph_vertex_destroy( usbcore_device_vtx);
    hwgraph_vertex_destroy( usbdaemon_device_vtx);

    hwgraph_edge_remove(conn, "/usb", &masterv);    
    hwgraph_vertex_destroy( masterv); 
    
    
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "USBCORE Driver unloaded!", "");
    TRACE( class | TRC_START_END, 10, "end", "");
    
    return 0;                /* always ok to unload */
}


int usbcore_reg(void){
    return 0;
}

int usbcore_unreg(void){
    if (usbcore_busy)
        return -1;

    return 0;
}

int usbcore_attach(vertex_hdl_t conn){
    return 0;
}

int usbcore_detach(vertex_hdl_t conn){
    return 0;

}

/*ARGSUSED */
int usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp){
    vertex_hdl_t                  vhdl;
    usbcore_instance_t            *soft;
    uint64_t class = TRC_MOD_CORE | TRC_OPEN;

    TRACE( class | TRC_START_END, 10, "start", "");
    
    vhdl = dev_to_vhdl(*devp);
    soft = (usbcore_instance_t *) device_info_get(vhdl);
    TRACE( class | TRC_START_END, 10, "end", "");

    return( 0);
}

/*ARGSUSED */
int usbcore_close(dev_t dev){
    uint64_t class = TRC_MOD_CORE | TRC_CLOSE;
    TRACE( class | TRC_START_END, 10, "start", "");
	
    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
}

/*ARGSUSED */
int usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp){
    return EOPNOTSUPP;
}

/*ARGSUSED */
int usbcore_write(){
    return EOPNOTSUPP;
}


int dump_ioctl( int n){
    int i = n;
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 6, "ioctl value %x", i);
    
    if(( i & _IOCTL_RMASK) != 0)
        TRACE( class, 6, " -read", "");
    if(( i & _IOCTL_WMASK) != 0)
        TRACE( class, 6, " -write", "");


    TRACE( class, 6, " -char = %c\n", _IOCTL_GETC(i));
    TRACE( class, 6, " -num = %d\n", _IOCTL_GETN( i));
    TRACE( class, 6, " -size = %d\n", _IOCTL_GETS( i));

    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);
}



/*ARGSUSED*/
int usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp){
    vertex_hdl_t               vhdl;
    usbcore_instance_t        *soft;
    void                      *ubuf;
    size_t                     size;
    int                          rc;
    ioctl_item_t              *item;
    uint64_t class = TRC_MOD_CORE | TRC_IOCTL;

    TRACE( class | TRC_START_END, 10, "start", "");

    vhdl = dev_to_vhdl(dev);
 
    soft = (usbcore_instance_t *) device_info_get(vhdl);
/*    TRACE( class, 4, "soft->mode = %x", soft->mode);
	dump_ioctl( cmd);*/
    
    item = get_ioctl_item( cmd);
    TRACE( class, 12, "cmd = %x, '%s', cmd_table=%d", cmd, item->str, item->cmd);
    
    if( item->cmd <= 0){
        rc = EINVAL;
        goto done;
    }
        
	/* get size defined in ioctl */
    size = _IOCTL_GETS( cmd);
    if( size > 0){ 
		if( uarg == NULL){ /* something is weird */
		    rc = EINVAL;
		    goto done; 	
		}
		
	    /* reserve kernel memory */
		ubuf = gc_malloc( &gc_list, size);
		if( ubuf == NULL){
		    TRACERR( class | TRC_ERROR, "Error in gc_malloc(), exit..", "");	
			rc = ENOMEM;
			goto done;
		}

        /* if we're writing data, copy from user space */
	    if( (cmd & _IOCTL_WMASK) != 0){	
	        copyin( uarg, ubuf, size);
        }
	}
	
    /* run ioctl function now */
	rc = item->func( (void *) soft, ubuf);

    if( size > 0){ 
	    if( (cmd & _IOCTL_RMASK) != 0){	
	        copyout( ubuf, uarg, size);
        }
		gc_mark( ubuf);
	}



    
done:        
    TRACE( class, 12, "exiting ioctl() entry point, rc=%d", rc);
    TRACE( class | TRC_START_END, 10, "end", "");

    return *rvalp = rc;
    
}

/*ARGSUSED*/
int usbcore_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    return( 0);
}

/*ARGSUSED*/
int usbcore_unmap(dev_t dev, vhandl_t *vt){
    return (0);
}


/* USB drivers call this function when something happens, so it should perform a pollwakeup() or
   take actions */
int process_event_from_device( void *p_usbcore, void *p_device, int event, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_MOD_CORE | TRC_POLL;
    usbcore_instance_t *usbcore = (usbcore_instance_t *) p_usbcore;
    device_header_t *device = ( device_header_t *) p_device;
    msg_t     message;
    uint32_t port_num = (uint32_t) arg0;
    
    message.msg_id = event;
    message.device = device;
    message.arg0 = arg0;
    message.arg1 = arg1;
    message.arg2 = arg2;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "device = '%s'", device->module_header->short_description);    
    /* lock queue */
	MUTEX_LOCK( &usbcore->mutex, -1);
	queue_put( &msg_queue, &message); 
	MUTEX_UNLOCK( &usbcore->mutex);
	
    /* send event to thread */
    wakeup( (caddr_t) global_soft);

    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);
   	
}


/* HCD drivers call this function when it needs something from USBcore */
int process_event_from_hcd( void *p_usbcore, void *p_hcd, int event, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_MOD_CORE | TRC_POLL;
    usbcore_instance_t *usbcore = (usbcore_instance_t *) p_usbcore;
    device_header_t *hcd = (device_header_t *) p_hcd;
    int rc;
    int r_arg, i;
    module_header_t *usbhub;


    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd_name = '%s'", hcd->module_header->short_description);

    for( i = 0; i < all_modules_num; i++){
		if( modules[i]->module_id == USB_MOD_HUB)
		    usbhub = modules[i];
    }
    
	switch( event){
		/* a detected HCD needs to know which instance id it will have. USBhub knows which instance id
		 * will be the next one */
        case USB_HCD_GET_INSTANCE_ID:
            rc = usbhub->methods.process_event_from_usbcore( NULL, USB_HCD_GET_INSTANCE_ID, arg0, (void *) &r_arg, NULL);
            
            return( r_arg);
        break;
        /* HCD wants to notify to usbcore that it has complete its initial setup. Forward this notification
         * to USBhub for HCD attach and device creation */
        case USB_HCD_ATTACHED_EVENT:
            rc = usbhub->methods.process_event_from_usbcore( NULL, USB_HCD_ATTACHED_EVENT, arg0, (void *) &r_arg, NULL);
        break;
    }
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);
}

int usbcore_register_module( void *arg){
	module_header_t *header = (module_header_t *) arg;
	int i, already_on_list = 0, rc;
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "Registering HCD module '%s' with ID='0x%x'", 
        header->short_description, header->module_id);
	    	
    /* look if this module info is already on modules list */
    already_on_list = 0;
    for( i = 0; i < all_modules_num; i++){
        if( modules[i]->module_id == header->module_id){
	        already_on_list = 1;
	        break;
   	    }    
        
    }    


    if( already_on_list == 0 && all_modules_num < MAX_MODULES){
	    TRACE( class, 12, "module info not on list, adding..", "");
        modules[all_modules_num] = (module_header_t *) gc_malloc( 
            &gc_list, sizeof( module_header_t));
        bcopy( (void *) arg, modules[all_modules_num], 
             sizeof( module_header_t));   
        all_modules_num++;
    }else{ 
	    TRACE( class, 12, "module info already on list, passing out..", "");
    }
    
    for( i = 0; i < all_modules_num; i++)
        TRACE( class, 12, "module list %d = %x, %s", i, modules[i]->module_id, 
            modules[i]->short_description);

    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
    
}




int usbcore_unregister_module( void *hcd_arg){
	module_header_t *header = (module_header_t *) hcd_arg;
    int i, j, already_on_list = 0;
    int delete_index, n;
    int rc;
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "Unregistering HCD module '%s' with ID='0x%x'", 
        header->short_description, header->module_id);

    
    /* now look if this module info is already on modules list */
    already_on_list = 0;
    for( i = 0; i < all_modules_num; i++){
        if( modules[i]->module_id == header->module_id){
	        already_on_list = 1;
	        delete_index = i;
	        break;
   	    }    
    }    

    /* no, it's not on the list */   
    if( already_on_list == 0 ){
		rc = 0;
        goto exit;
    }

    if( all_modules_num == 1) 
        delete_index = 0;


    gc_mark( (void *) modules[delete_index]);
    for( i = delete_index; i < (all_modules_num-1); i++)
        modules[i] = modules[i+1];
        
    modules[i] = NULL;   
    if( all_modules_num > 0) 
        all_modules_num--;


    rc = 0;

    n = 0;
    for( i = 0; i < usb_devices_num; i++){
        if( usb_devices[i]->module_header->module_id == header->module_id){
		    usb_devices[i] = NULL;
		    n++;
		}
	}
	
    for( i = 0; i < usb_devices_num; i++){
        if( usb_devices[i] == NULL){
			for( j = i; j < usb_devices_num; j++){
			    if( usb_devices[j] != NULL){
			        usb_devices[i] = usb_devices[j];
			        usb_devices[j] = NULL;
			    }
			}
		}
    }
    
    usb_devices_num = usb_devices_num - n;
		
exit:    

    for( i = 0; i < all_modules_num; i++)
        TRACE( class, 12, "module list %d = %x, %s", i, modules[i]->module_id, 
            modules[i]->short_description);

    for( i = 0; i < usb_devices_num; i++)
        TRACE( class, 12, "device list %d = %s", i, usb_devices[i]->hardware_description);
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return( rc);
    
}



int usbcore_register_device( void *arg){
	device_header_t *header = (module_header_t *) arg;
	int i, already_on_list = 0, rc, last;
    ulong_t ktime;

	
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "Registering device '%s'", header->hardware_description);
	    	
    if( usb_devices_num < MAX_USB_DEVICES){
	    TRACE( class, 12, "Adding device..", "");
        usb_devices[usb_devices_num] = header;
	    drv_getparm( TIME, &ktime);
	    header->instance_id = (uint32_t) krand( ktime, 65536);
        header->module_header->methods.set_trace_level( (void *) header->soft,  (void *) &global_trace_class);       
        
        usb_devices_num++;
    }
    
    for( i = 0; i < usb_devices_num; i++)
        TRACE( class, 12, "device list %d = %s", i, usb_devices[i]->hardware_description);

    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
    
}




int usbcore_unregister_device( void *arg){
	device_header_t *header = (module_header_t *) arg;
    int i, already_on_list = 0;
    int delete_index;
    int rc;
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "Unregistering device '%s'", header->hardware_description);

    
    for( i = 0; i < usb_devices_num; i++){
        if( usb_devices[i]->instance_id == header->instance_id){
	        delete_index = i;
	        break;
   	    }    
    }    

    /* no, it's not on the list */   
    if( already_on_list == 0 ){
		rc = 0;
        goto exit;
    }

    if( usb_devices_num == 1) 
        delete_index = 0;


    gc_mark( (void *) usb_devices[delete_index]);
    for( i = delete_index; i < (usb_devices_num-1); i++)
        usb_devices[i] = usb_devices[i+1];
        
    usb_devices[i] = NULL;   
    if( usb_devices_num > 0) 
        usb_devices_num--;

    rc = 0;
exit:    

    for( i = 0; i < usb_devices_num; i++)
        TRACE( class, 12, "device list %d = %s", i, usb_devices[i]->hardware_description);
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return( rc);
    
}


int usb_enumerate( device_header_t *device, uint32_t port_num){
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;
	int rc = 0; 
    usbhub_instance_t *hub = ( usbhub_instance_t *) device;
    usb_pipe_t *pipe;
    td_addr_t  tds[TD_ADDR_SIZE_VEC];
    usb_device_descriptor_t dinfo;

	/* Reset USB port here */
    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
           hub, USB_HUB_PORT_RESET, (void *) &port_num, NULL, NULL);
	            
    /* Add transfer descriptors for SET_ADDRESS */
    /* no data will be used for it */
    bzero( (void *) &tds, sizeof( td_addr_t)); 
	        
	        
    tds[0].data_size = 256; 
    tds[1].data_size = 256; 
    tds[2].data_size = -1; 
	        
    /* add just two transfer descriptors */
    rc = hub->device_header.module_header->methods.process_event_from_usbcore(
        hub, USB_HUB_ALLOC_PIPE, (void *) &port_num, (void *) &pipe, (void *) &tds);

    if( rc != 0){
        TRACE( class, 10, "AllocPipe failed", "");
	    goto drop_pipe_quit;        
	         
    }


    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
        hub, USB_GET_DEVICE_INFO8, (void *) pipe, (void *) &dinfo, NULL);
    if( rc != 0){
        TRACE( class, 10, "GetDeviceInfo8 failed", "");
	    goto drop_pipe_quit;        
	         
    }



    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
        hub, USB_HUB_SET_ADDRESS, (void *) &port_num, (void *) pipe, NULL);
    if( rc != 0){
        TRACE( class, 10, "SetAddress failed", "");
	    goto drop_pipe_quit;        
	         
    }
	            
    rc = hub->device_header.module_header->methods.process_event_from_usbcore( 
        hub, USB_GET_DESCRIPTOR, (void *) pipe, (void *) &dinfo, NULL);
    if( rc != 0){
        TRACE( class, 10, "GetDescriptor failed", "");
	    goto drop_pipe_quit;        
	         
    }
	            
  	            
    TRACE( class, 10, "device rev=%04x cls=%02x sub=%02x proto=%02x size=%02x\n"
        , dinfo.bcdUSB, dinfo.bDeviceClass, dinfo.bDeviceSubClass
        , dinfo.bDeviceProtocol, dinfo.bMaxPacketSize0);
            
    if (dinfo.bMaxPacketSize0 < 8 || dinfo.bMaxPacketSize0 > 64){
        TRACE( class, 10, "weird shit!!", "");
    }
    
drop_pipe_quit:
    rc = hub->device_header.module_header->methods.process_event_from_usbcore(
        hub, USB_HUB_DESTROY_PIPE, (void *) pipe, NULL, NULL);
            

    TRACE( class, 10, "goood!! pipes created with no crashes!!", "");
    return( rc);	
}

int run_event( usbcore_instance_t *usbcore, int event, device_header_t *device, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;
	int rc = 0; 
    uint32_t portnum = (uint32_t) arg0; 
    usbhub_instance_t *hub;
    usb_pipe_t *pipe;
    td_addr_t  tds[TD_ADDR_SIZE_VEC];
    usb_device_descriptor_t dinfo;


    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "device = '%s'", device->module_header->short_description);    
    
    switch( event){
	    case USB_EVENT_PORT_CONNECT:
			portnum = (uint32_t ) arg0;
			TRACE( class, 12, "event = USB_EVENT_PORT_CONNECT from '%s', portnum=%d", 
			    device->module_header->short_description, portnum);
			    
			/*rc = usb_enumerate( device, portnum);*/
			
			
		break;
		case USB_EVENT_PORT_DISCONNECT:
			portnum = (uint32_t ) arg0;
			TRACE( class, 12, "event = USB_EVENT_PORT_DISCONNECT from '%s', portnum=%d", 
			    device->module_header->short_description, portnum);
	    break;
	}	
    TRACE( class | TRC_START_END, 10, "end", "");
    return( rc);
}

void usbcore_main_thread( void *arg0, void *arg1, void *arg2, void *arg3){
    uint64_t class = TRC_MOD_CORE | TRC_HELPER;
    usbcore_instance_t *usbcore = ( usbcore_instance_t *) arg0;
    int i, rc;
    msg_t                         message;
    int exit = 0;
    
    TRACE( class | TRC_START_END, 10, "start", "");

        /*
         * Loop processing events and sleeping waiting for more
         */
    do{
              /*
               * Wait for more events to occur
               */
        TRACE( class, 12, "waiting for event", "");
        sleep( (caddr_t) arg0, PZERO);
        TRACE( class, 12, "waking up, checking queue", "");
        
        
        do{
    	    MUTEX_LOCK( &usbcore->mutex, -1);
	        rc = queue_get( &msg_queue, &message); 
	        MUTEX_UNLOCK( &usbcore->mutex);
	        
	        TRACE( class, 12, "event_id = %d", message.msg_id);
	        if( rc == 0){
				if( message.msg_id == USB_EVENT_EXIT_MAIN_THREAD){
                    TRACE( class, 12, "event = USB_EVENT_EXIT_MAIN_THREAD, end", "");
                    drv_thread_exit();
                    goto usbcore_main_thread_exit;
			    }else{
				    if( message.msg_id == USB_EVENT_PORT_CONNECT || message.msg_id == USB_EVENT_PORT_DISCONNECT){
                        TRACE( class, 12, "port = %d",     (uint32_t)(message.arg0)  );
                    }
				    rc = run_event( usbcore, message.msg_id, message.device, message.arg0, message.arg1, message.arg2);
				}
			}
                        		    
        }while( msg_queue.nelems > 0);
              /*
               * Do background work
               */
    }while( 1);

        /*
         * If we need to exit this thread for some reason
         * we call the below.  This is equivalent to just
         * calling return() from the base function.
         */
usbcore_main_thread_exit:         
    TRACE( class | TRC_START_END, 10, "end", "");
    drv_thread_exit();
}	
