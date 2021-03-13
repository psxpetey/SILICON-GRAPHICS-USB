/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbhub.c                                                   *
* Description: USB core kernel driver                              *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      14-02-2013     -Added PIPE creation and basic functionality for USB CMD SetAddress      *
*                                                                                                     * 
* BSDero      08-03-2012     -Fixed wrong class in trace messages                                     *
*                            -Added _diaginfo_ string in startup trace messages                       *
*                                                                                                     *
* BSDero      07-30-2012     -Fixes to send message to usbcore in port connect and port disconnect    *
*                                events                                                               *
*                                                                                                     *
* BSDero      07-17-2012     -Get status and port status from HCD and return this data to usbcore     *
*                            -bug fixes added and crashes solved                                      *
*                            -Hub detection from HCD modules added                                    *
*                            -hub_attach() and basic PCI hub detection added                          *
*                                                                                                     *
* BSDero      07-12-2012     -hub_methods_t data type added                                           *
*                            -bug fixes added                                                         *
*                            -Hub detection from HCD modules added                                    *
*                            -hub_attach() and basic PCI hub detection added                          *
*                            -HCD devices start on detect                                             *
*                            -Register hub and module with usbcore added                              *
*                                                                                                     *
* BSDero      07-10-2012     -Basic usbstack functions implementation                                 *
*                            -usbhub module header added                                              *
*                            -usbhub device creation added                                            *
*                            -Events handling functions added                                         *
*                                                                                                     *
* BSDero      07-03-2012     -Initial version                                                         *
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
* Utility function declaration and source code                                                        *
*******************************************************************************************************
*/
int hub_set_trace_level( void *hcd, void *arg);
int hub_process_event( void *origin, void *dest, int event_id, void *arg);
int hub_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2);

module_header_t hub_header={
	USB_MOD_HUB, 
	"usbhub", 
	"USBhub device driver", 
	"usbhub.o", 
	USB_DRIVER_IS_USB_DEVICE,
    {
		hub_set_trace_level,
		hub_process_event,
		hub_process_event_from_usbcore,
		0x0000
    }
};

int update_hub_status( void *hub, void *info);
int process_event_from_hcd( void *hcd, void *hub, int message_id, void *arg0, void *arg1);

hub_methods_t hub_methods={
	update_hub_status,
	process_event_from_hcd
};



/*
*******************************************************************************************************
* Loadable modules stuff and global Variables                                                         *
*******************************************************************************************************
*/
char                                                *usbhub_mversion = M_VERSION;
int                                                 usbhub_devflag = D_MP;
unsigned int                                        usbhub_busy = 0;
gc_list_t                                           gc_list;
usbhub_instance_t                                   *global_soft = NULL;
#define USB_HUB_MAX_STACK                           8
device_header_t                                     *hcd_stack[USB_HUB_MAX_STACK];
int                                                 num_hcds_in_stack = 0;
int                                                 spec_device_instance_id = 0;


/*
*******************************************************************************************************
* Entry points for USBhub                                                                            *
*******************************************************************************************************
*/
void usbhub_init(void);
int usbhub_unload(void);
int usbhub_reg(void);
int usbhub_unreg(void);
int usbhub_attach(vertex_hdl_t conn);
int usbhub_detach(vertex_hdl_t conn);
int usbhub_open(dev_t *devp, int flag, int otyp, struct cred *crp);
int usbhub_close(dev_t dev);
int usbhub_read(dev_t dev, uio_t * uiop, cred_t *crp);
int usbhub_write();
int usbhub_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp);
int usbhub_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int usbhub_unmap(dev_t dev, vhandl_t *vt);



/*
*******************************************************************************************************
* USB hub specific functions                                                                          *
*******************************************************************************************************
*/
/*
 PUT YOUR FUNCTIONS HERE
 * 
 * 
 * 
 * 
 * 
 * 
 */
usbcore_instance_t *get_usbcore(){
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;

    uint64_t class = TRC_MOD_HUB | TRC_INIT;	
	TRACE( class | TRC_START_END, 10, "start", "");


    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb/usbcore", &vhdl)){
 	    TRACEWAR( class | TRC_WARNING,  
 	        "hwgraph_traverse() could not follow /usb/usbcore path", "");
        return(NULL);
	}

	usbcore = (usbcore_instance_t *) device_info_get( vhdl);
	if( usbcore == NULL){
	    TRACEWAR( class | TRC_WARNING,  
	        "could not get usbcore information from device_info_get()", "");
	}
		
	
	TRACE( class | TRC_START_END, 10, "end", "");
	
	return( usbcore);
} 


int hub_attach( void *psoft, int hub_type){
	usbhub_instance_t *soft;
    uint64_t class = TRC_MOD_HUB | TRC_ATTACH;	
    int i, hcd_info = -1;
    hcd_methods_t *hcd_methods;
    int t = 0;
    vertex_hdl_t  vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t  charv = GRAPH_VERTEX_NONE;
    graph_error_t ret = (graph_error_t) 0;
    usbcore_instance_t *usbcore; 
    char str[80];
    
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    
    soft = (usbhub_instance_t *) gc_malloc( &gc_list, sizeof( usbhub_instance_t));
    ASSERT(soft != NULL);
    
    bzero( (void *) soft, sizeof( usbhub_instance_t));


    if( hub_type == USB_HUB_PCI){
		/* try to get information from EHCI first */
        for( i = 0; i < num_hcds_in_stack; i++){
            if( hcd_stack[i]->module_header->module_id == USB_MOD_EHCI){
				hcd_info = i; 
				break;
		    }
		}

		
		/* no? get information from UHCI then */
		if( hcd_info == -1){
			for( i = 0; i < num_hcds_in_stack; i++){
				if( hcd_stack[i]->module_header->module_id == USB_MOD_UHCI){
					hcd_info = i; 
					break;
				}
			}
	    }
	    
	    /* fill in usbhub soft structure */
        soft->device_header.module_header = &hub_header;
	    bcopy( hcd_stack[hcd_info]->hardware_description, 
		    soft->device_header.hardware_description, 80);
		soft->device_header.device_id = hcd_stack[hcd_info]->device_id;
		soft->device_header.vendor_id = hcd_stack[hcd_info]->vendor_id;
		soft->device_header.class_id = hcd_stack[hcd_info]->class_id;
		soft->device_header.interface_id = hcd_stack[hcd_info]->interface_id;
        soft->device_header.hub_index = -1;
        soft->device_header.port_index = -1;
		soft->hub_index = 0; /* this is the root hub */
		soft->hub_parent = -1;
		soft->address = 0;
        soft->hcd_owner = hcd_info;
        soft->cd_events_num = 0;
        soft->cd_itimeout = 0;
        soft->maxaddr = 0;
        
        /* fill in ports number and pci information from hcd owner */
        hcd_methods = (hcd_methods_t *) hcd_stack[hcd_info]->methods;
        hcd_methods->hcd_hub_info( (void *) hcd_stack[hcd_info], (void *) soft);
        
        /* fill in ports status also */
        for( i = 0; i < soft->ports_number; i++){
		    soft->ports_status[i] = hcd_methods->hcd_get_port( 
		        (void *) hcd_stack[hcd_info], i);
		}
       
		for( i = 0; i < num_hcds_in_stack; i++){
			
			soft->hcds[i] = hcd_stack[i];
			TRACE( class, 10, "hcd[%d] = %x", i, soft->hcds[i]);
			/* set usb roothub in HCD devices */
			hcd_methods = (hcd_methods_t *) hcd_stack[i]->methods;
			hcd_methods->hcd_set_roothub( (void *) hcd_stack[i], (void *) soft);
			
			if( hcd_stack[i]->module_header->module_id == USB_MOD_UHCI){
				soft->hcd_drivers |= USB_UHCI;
			}else if( hcd_stack[i]->module_header->module_id == USB_MOD_EHCI){
				soft->hcd_drivers |= USB_EHCI;
			}
		}
		soft->hcd_instances_num = num_hcds_in_stack;
		
		
	} 	/* if( hub_type == USB_HUB_PCI)*/
	
	
	
	

	soft->device_header.info_size = sizeof( usbhub_instance_t);
	soft->device_header.instance_id = 0; /* usbcore will assign this value */
	soft->device_header.spec_device_instance_id = spec_device_instance_id++;

    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
     	TRACEWAR( class | TRC_WARNING, "error in hwgraph_traverse(), quitting..", ""); 
        return( -1);
    }
    
    ret = hwgraph_edge_get( vhdl, soft->device_header.module_header->short_description, 
        &charv);

                    
    sprintf( soft->device_header.fs_device, USB_FSPATH "%s%d", 
        soft->device_header.module_header->short_description,
        soft->device_header.spec_device_instance_id);
    TRACE( class, 12, "fsname = '%s'", soft->device_header.fs_device);
        
    strcpy( str, soft->device_header.module_header->short_description);
    strcat( str, "_");
    ret = hwgraph_char_device_add( vhdl, soft->device_header.fs_device, str, &charv); 
        
    TRACE( class, 12, 
        "rc of hwgraph_char_device_add() = %d, added usbehci edge (char device)", ret);    

    device_info_set(vhdl, (void *)soft);
    soft->ps_vhdl = vhdl;
    soft->ps_conn = vhdl;
    soft->ps_charv = charv;
    soft->device_header.methods = ( void *) &hub_methods;
	
	soft->device_header.soft = soft;
    soft->hub_type = hub_type;
    MUTEX_INIT( &soft->mutex, MUTEX_DEFAULT, "usbhub");
    MUTEX_INIT( &soft->uhci_mutex, MUTEX_DEFAULT, "usbhub");

    
    usbcore = get_usbcore();
    soft->usbcore = usbcore;
    
    usbcore->register_device( (void *) soft);
        
    if( hub_type == USB_HUB_PCI){
	    for( i = 0; i < soft->hcd_instances_num; i++){
			TRACE( class, 12, "starting %d", i);
			
			
			/* set usb roothub in HCD devices */
			hcd_methods = (hcd_methods_t *) soft->hcds[i]->methods;
			hcd_methods->hcd_start( (void *) soft->hcds[i] );
		    	
		}	
	}
    TRACE( class | TRC_START_END, 10, "end", "");
    return(0);                /* always ok to unload */
}


int hub_detach( void *soft){
	usbhub_instance_t *usbhub = (usbhub_instance_t *) soft;
    uint64_t class = TRC_MOD_HUB  | TRC_ATTACH;	
    
    TRACE( class | TRC_START_END, 10, "start", "");
    
    if( usbhub->hub_type == USB_HUB_PCI){
		/* usbcore will delete this device */
		
		
	}
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return NULL;                /* always ok to unload */
}


int hub_info( void *soft){
	usbhub_instance_t *usbhub = (usbhub_instance_t *) soft;
    uint64_t class = TRC_MOD_HUB  | TRC_ATTACH;	
    
    TRACE( class | TRC_START_END, 10, "start", "");
    
    if( usbhub->hub_type == USB_HUB_PCI){
		/* usbcore will delete this device */
		
		
	}
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return NULL;                /* always ok to unload */
}


int hub_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_MOD_HUB | TRC_HELPER;
	char str[256];    	
	TRACE( class | TRC_START_END, 10, "start", "");


	global_trace_class.class = trace_arg->class;
	global_trace_class.level = trace_arg->level;
	INT2HEX64X( str, global_trace_class.class);
    TRACE( class, 12, "trace class settled; class=%s, level=%d", 
       str, global_trace_class.level);
	TRACE( class | TRC_START_END, 10, "end", "");	

	return(0);
}

int update_hub_status( void *hub, void *info){
    uint64_t class = TRC_MOD_HUB | TRC_HELPER;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	
	return(0);
}


void check_for_event( void *hub){
    uint64_t class = TRC_MOD_HUB | TRC_HELPER;
    usbhub_instance_t *roothub = (usbhub_instance_t *) hub;
    uint32_t port_num;
    int message_id; 
    usbcore_instance_t *usbcore;
    

	TRACE( class | TRC_START_END, 10, "start", "");
    MUTEX_LOCK( &roothub->mutex, -1);	

    TRACE( class | TRC_START_END, 10, "events_num=%d", roothub->cd_events_num);
    if( roothub->cd_events_num >= 1){
		
		port_num = roothub->cd_port_event[0];
		message_id = roothub->cd_event_id[0];
        usbcore = roothub->usbcore;
        TRACE( class, 10, "sending event to usbcore, port_num = %d, message_id=%d", 
            port_num, message_id);
        usbcore->process_event_from_device( (void *)usbcore, (void *) roothub,
            message_id, (void *) port_num, NULL, NULL);
		
		roothub->cd_events_num = 0;
	}	

	MUTEX_UNLOCK( &roothub->mutex);

	TRACE( class | TRC_START_END, 10, "end", "");	
	
}


/* HCD call this function when needs to advice something to its HUB */
int process_event_from_hcd( void *hcd, void *hub, int message_id, void *arg0, void *arg1){
    uint64_t class = TRC_MOD_HUB | TRC_HELPER;
    device_header_t *hcdp = ( device_header_t *) hcd;
    usbhub_instance_t *roothub = (usbhub_instance_t *) hub;
    hcd_methods_t *hcd_methods;
    usbcore_instance_t *usbcore;
    uint32_t *port_num = (uint32_t *) arg0;
    usb_pipe_t *pipe;
    uint32_t port_value = 0;
    int send_event = 0;
    int speed;
    ulong_t ktime, kdif;    
    
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class, 6, "hcd_name = '%s'", hcdp->module_header->short_description);
    TRACE( class, 10, "Port : %d", *port_num);
	
	MUTEX_LOCK( &roothub->mutex, -1);
	usbcore = roothub->usbcore;
	switch( message_id){
	    case USB_EVENT_PORT_CONNECT:
	        TRACE( class, 10, "Port connected : %d", *port_num);
	        hcd_methods = (hcd_methods_t *) hcdp->methods;
	        port_value = hcd_methods->hcd_get_port( (void *) hcdp, (int) *port_num);
	        roothub->ports_status[*port_num] = port_value;
	        roothub->ports_hcds[*port_num] = hcdp;
	        drv_getparm( TIME, &ktime);
	        ktime -= roothub->cd_time_event[0];
	        TRACE( class, 10, "ktime diff = %d, events_num = %d, event0=%d, current_port=%d",
	            ktime, roothub->cd_events_num, roothub->cd_port_event[0], *port_num);
	            
	        if((roothub->cd_events_num == 1) &&
	           (roothub->cd_port_event[0] == *port_num) &&
	           (ktime < 2) ){
				roothub->cd_events_num = 0;
				untimeout( roothub->cd_itimeout);
				roothub->cd_itimeout = 0;
		    }
	        
	        TRACE( class, 10, "sending event to usbcore", "");
	        usbcore->process_event_from_device( (void *)usbcore, (void *) roothub,
                        USB_EVENT_PORT_CONNECT, (void *) *port_num, NULL, NULL);
	     /*   pipe = set_address( roothub, hcdp, *port_num, speed);*/
	    break;
	    case USB_EVENT_PORT_DISCONNECT:
	        TRACE( class, 10, "Port disconnected : %d", *port_num);
	        TRACE( class, 10, "owner = %d", roothub->hcd_owner);
            TRACE( class, 10, "Port : %d", *port_num);
	        
	        /* get usb root hub hcd owner methods */
	        hcd_methods = (hcd_methods_t *) roothub->hcds[roothub->hcd_owner]->methods;
	        
	        /* if disconnected from not-pci card owner 
	         * (Ex. if uhci generated the event and pci card owner is ehci) */
	        if( hcdp != roothub->hcds[roothub->hcd_owner]){
				TRACE( class, 10, "recover ownership of port ", "");
	            /* set hcd owner as port owner
	             *  (ex. disconnected port from uhci, so set ehci as port owner )*/
		        hcd_methods->hcd_port_action( (void *) roothub->hcds[roothub->hcd_owner], 
		            *port_num, USB_HCD_SET_OWNER);
		    }
			
		    /* now get port information */
		    roothub->ports_status[*port_num] = hcd_methods->hcd_get_port( 
		        (void *) roothub->hcds[roothub->hcd_owner], *port_num);

            drv_getparm( TIME, &ktime);
            TRACE( class, 10, "Port : %d", *port_num);
	    	TRACE( class, 10, "ktime=%d, port_num=%d, evnum=%d", ktime, *port_num,
		        roothub->cd_events_num);
            roothub->cd_port_event[roothub->cd_events_num] = *port_num;
            roothub->cd_time_event[roothub->cd_events_num] = ktime;
            roothub->cd_event_id[roothub->cd_events_num] = message_id;
            roothub->cd_events_num++;
            roothub->cd_itimeout = itimeout( check_for_event, hub, drv_usectohz(1000000), 0);
            
	    break;
		
	}
	
	MUTEX_UNLOCK( &roothub->mutex);
	
	TRACE( class | TRC_START_END, 10, "end", "");	
	return(0);
}


int hub_process_event( void *origin, void *dest, int event_id, void *arg){
    uint64_t class = TRC_HELPER | TRC_MOD_HUB;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	
	return(0);
}


/* usbcore.run_event() thread sends events and they're received here. 
 * After that, this function forwards the requests to the adequate HCD */
int hub_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_HELPER | TRC_MOD_HUB;
    device_header_t *hcd;
    usbhub_instance_t *hub;
    USB_root_hub_status_t *status;
    hcd_methods_t *hcd_methods;
    usb_ctrlrequest_t req;
    int hcd_status, port_num, speed, rc;
    usb_pipe_t *pipe, **ppipe;
    int *r_code = (int *) arg1;
    int i, j = 0;
    int uhci_num, ehci_num, td_num;
    usb_device_descriptor_t *dinfo;
    td_addr_t  *tds;
    usb_debug_values_t *dvalues;
    uint32_t           *debugop;

    hub = ( usbhub_instance_t *) dest;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	switch( event_id){
		
		
		case USB_HUB_PORT_RESET: 
		    hub = ( usbhub_instance_t *) dest;
		    port_num = (int) arg0;

			TRACE( class, 6, "event = USB_HUB_PORT_RESET from '%s', portnum=%d", 
			    hub->device_header.module_header->short_description, port_num);
		    
		    hcd = hub->ports_hcds[port_num];
		    hcd_methods = hcd->methods;
	        rc = hcd_methods->hcd_port_action( (void *) hcd, port_num,
	            USB_HUB_PORT_RESET);
		    return( rc);
		break;
		
		case USB_HUB_ALLOC_PIPE:
		    hub = ( usbhub_instance_t *) dest;
		    port_num = (int) arg0;
	        ppipe = (usb_pipe_t **) arg1;    
            tds = ( td_addr_t *) arg2;
	        
	        
	     	TRACE( class, 6, "event = USB_HUB_ALLOC_PIPE from '%s'", hub->device_header.module_header->short_description);


		    hcd_methods = hub->ports_hcds[port_num]->methods;
		    hcd = hub->ports_hcds[port_num];
	        pipe = hcd_methods->hcd_alloc_control_pipe( (void *) hcd, port_num, tds);
            
	        TRACE( class, 10, "arg=0x%x", pipe);
			*ppipe = pipe;
	        
		    return( rc);
		break;
		
		case USB_HUB_DESTROY_PIPE:
		    hub = ( usbhub_instance_t *) dest;
	        pipe = (usb_pipe_t *) arg0;    
		    hcd = ( device_header_t *) pipe->hcd;
	        
	        
	     	TRACE( class, 6, "event = USB_HUB_DESTROY_PIPE from '%s'", hub->device_header.module_header->short_description);
	        TRACE( class, 10, "arg=0x%x", pipe);

		    hcd = hub->ports_hcds[pipe->port_num];
		    hcd_methods = hub->ports_hcds[pipe->port_num]->methods;
	        hcd_methods->hcd_free_pipe( (void *) hcd,  pipe);
		    return( rc);
		break;
		
		case USB_HUB_SET_ADDRESS:
		    hub = ( usbhub_instance_t *) dest;
		    port_num = (int) arg0;
	        pipe = ( struct usb_pipe_t *) arg1;

            pipe->maxpacket = 8;
	        pipe->speed = USB_PORT_GET_SPEED( hub->ports_status[port_num]);
	        pipe->transfer_type = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
	        hcd = pipe->hcd; 
	        
            hcd_methods = hcd->methods;
	        USECDELAY( 10000);

            req.bRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
            req.bRequest = USB_REQ_SET_ADDRESS;
            req.wValue = B2LENDIAN((uint16_t)(hub->maxaddr + 1));
            req.wIndex = 0;
            req.wLength = 0;
            
            
		    
		    rc = hcd_methods->hcd_send_control( 
		        (void *) hcd, pipe, req.bRequestType & USB_DIR_IN, 
		        (void *) &req, sizeof( usb_ctrlrequest_t), NULL, req.wLength );

      
		   /* if( rc != 0) 
		        arg1 = NULL;*/
		        
		    TRACE( class, 6, "SET ADDRESS IS UP AND RUNNING!!", "");
		    
		    USECDELAY( 2000);    
		    hub->maxaddr++; 
            pipe->devaddr = hub->maxaddr;
		    return( rc);
		break;
		case USB_GET_DEVICE_INFO8:
		    hub = ( usbhub_instance_t *) dest;
	        pipe = ( struct usb_pipe_t *) arg0;
		    dinfo = ( usb_device_descriptor_t *) arg1;
		    
		    bzero( (void *) dinfo, sizeof( usb_device_descriptor_t));
            req.bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
            req.bRequest = USB_REQ_GET_DESCRIPTOR;
            req.wValue = B2LENDIAN((uint16_t)USB_DT_DEVICE<<8);
            req.wIndex = 0;
            req.wLength = 8;

		    rc = hcd_methods->hcd_send_control( 
		        (void *) hcd, pipe, req.bRequestType & USB_DIR_IN, 
		        (void *) &req, sizeof( usb_ctrlrequest_t), (void *) dinfo, req.wLength );
		    
		    return( rc);
		break;
		case USB_GET_DESCRIPTOR:
		    hub = ( usbhub_instance_t *) dest;
	        pipe = ( struct usb_pipe_t *) arg0;
		    dinfo = ( usb_device_descriptor_t *) arg1;
		    
		    bzero( (void *) dinfo, sizeof( usb_device_descriptor_t));
            req.bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
            req.bRequest = USB_REQ_GET_DESCRIPTOR;
            req.wValue = B2LENDIAN((uint16_t)USB_DT_DEVICE<<8);
            req.wIndex = 0;
            req.wLength = 18;

		    rc = hcd_methods->hcd_send_control( 
		        (void *) hcd, pipe, req.bRequestType & USB_DIR_IN, 
		        (void *) &req, sizeof( usb_ctrlrequest_t), (void *) dinfo, req.wLength );
		    
		    return( rc);
		break;
        case USB_HCD_GET_INSTANCE_ID:
            hcd = ( device_header_t *) arg0;
            TRACE( class, 6, "hcd_name = '%s'", hcd->module_header->short_description);
            
            if( num_hcds_in_stack == 0){
			    *r_code = 0;
			    return( 0);
			}
            
			j = 0;
            for( i = 0; i < num_hcds_in_stack; i++){
				if( hcd->module_header->module_id == hcd_stack[i]->module_header->module_id)
				    j++;
		    }

			*r_code = j;
			return(0);
		break;
		case USB_HCD_ATTACHED_EVENT:
            hcd = ( device_header_t *) arg0;
            TRACE( class, 6, "hcd_name = '%s'", hcd->module_header->short_description);

            if( num_hcds_in_stack >= USB_HUB_MAX_STACK)
                return( -1);
                
                
            hcd_stack[num_hcds_in_stack++] = hcd;
            /* add this HCD to the stack */
            if( num_hcds_in_stack == 1){
				*r_code = 1; /* hub needs extra HCDs */
				return(0);
		    }
			   
			if( num_hcds_in_stack == 3){
                ehci_num = uhci_num = 0;
                
                for( i = 0; i < num_hcds_in_stack; i++){
                    if( hcd_stack[i]->module_header->module_id == USB_MOD_EHCI)
                        ehci_num++;
                    else if( hcd_stack[i]->module_header->module_id == USB_MOD_UHCI)
                        uhci_num++;
                }
                
                if( ehci_num == 1 && uhci_num == 2){
					/* FIXME: check here if detected HCDs belongs to the same device */
					TRACE( class, 10, "Device is complete", "");
					hub_attach( NULL, USB_HUB_PCI);
					
					*r_code = 2;
			    }
		    }
		    
		    *r_code = 2;
		break;
		case USB_HUB_DEVICE_GET_STATUS: 
		    hub = ( usbhub_instance_t *) dest;
		    status = (USB_root_hub_status_t *) arg0;
		    
		    
		    status->ports_number = hub->ports_number;
		    hcd_status = 0;
		    
		    /* pass through all the hcds and get status for each */
		    for( i = 0; i < hub->hcd_instances_num; i++){
			    hcd_methods = (hcd_methods_t *) hub->hcds[i]->methods;
			    hcd_status |= hcd_methods->hcd_status( (void *) hub->hcds[i] );
		    }
		    if( hcd_status == 0){
		        status->hc_condition = USB_HC_OK;
		    }else{
		        status->hc_condition = USB_HC_ERROR;
		    }
		    
		break;
		case USB_SET_DEBUG_VALUES:
		    hub = ( usbhub_instance_t *) dest;
		    dvalues = (USB_root_hub_status_t *) arg0;

            for( i = 0; i < num_hcds_in_stack; i++){
                if( hcd_stack[i]->module_header->module_id == USB_MOD_UHCI){
	                rc = hcd_methods->hcd_set_debug_values( (void *) hcd, (void *) dvalues);
                }
		    }
		break;
    
		    
		
    }
	TRACE( class | TRC_START_END, 10, "end", "");	
    return(0);
}

/*
*******************************************************************************************************
* Entry points for USBhub                                                                            *
*******************************************************************************************************
*/
void usbhub_init(void){
    uint64_t class = TRC_MOD_HUB  | TRC_INIT;
    usbcore_instance_t *usbcore;

  	TRACE( class | TRC_START_END, 0, "start", "");
  	printf( "**********************************************************\n");
  	printf( "* USBHUB Driver for Silicon Graphics Irix 6.5            *\n");
  	printf( "* By bsdero at gmail dot org, 2012                       *\n");
  	printf( "* Version %s                                        *\n", USBCORE_DRV_VERSION);
  	printf( "* Sequence %s                                *\n", USBCORE_DRV_SEQ);
  	printf( "**********************************************************\n", "");
    printf( "usbhub kernel module loaded! \n", "");
    printf( "_diaginfo_: Kernel usbhub module base address: 0x%x\n", module_address);
  	
    
    /* initialize garbage collector list */
    gc_list_init( &gc_list);
    
    /* register module with USBcore */
    usbcore = get_usbcore();
    usbcore->register_module( ( void *) &hub_header);
    

    /* initialize this instance */
    /*
    soft = ( usbhub_instance_t *) gc_malloc( &gc_list, sizeof( usbhub_instance_t));
    if( soft == NULL){
		TRACERR( class, "gc_malloc() returned NULL, quitting", "");
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    
    bzero( soft, sizeof( usbhub_instance_t));
    */
    TRACE( class, 0, "USBHUB Driver loaded!", "");
    TRACE( class | TRC_START_END, 10, "end", "");
}

int usbhub_unload(void){
    usbcore_instance_t *usbcore;
    uint64_t class = TRC_MOD_HUB  | TRC_UNLOAD;


    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 0, "USBHUB Driver unloaded!", "");


    /* unregister module with USBcore */
    usbcore = get_usbcore();
    usbcore->unregister_module( ( void *) &hub_header);

    gc_list_destroy( &gc_list);      
    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);                /* always ok to unload */
}


int usbhub_reg(void){
    return( 0);
}

int usbhub_unreg(void){
    if (usbhub_busy)
        return -1;

    return 0;
}

int usbhub_attach(vertex_hdl_t conn){
    return 0;
}

int usbhub_detach(vertex_hdl_t conn){
    return 0;

}

/*ARGSUSED */
int usbhub_open(dev_t *devp, int flag, int otyp, struct cred *crp){
    vertex_hdl_t                  vhdl;
    usbhub_instance_t            *soft;
    uint64_t class = TRC_MOD_HUB  | TRC_OPEN;

    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class | TRC_START_END, 10, "end", "");

    return( 0);
}

/*ARGSUSED */
int usbhub_close(dev_t dev){
    uint64_t class = TRC_MOD_HUB  | TRC_CLOSE;
    TRACE( class | TRC_START_END, 10, "start", "");
	
    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
}

/*ARGSUSED */
int usbhub_read(dev_t dev, uio_t * uiop, cred_t *crp){
    return EOPNOTSUPP;
}

/*ARGSUSED */
int usbhub_write(){
    return EOPNOTSUPP;
}


/*ARGSUSED*/
int usbhub_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp){
    uint64_t class = TRC_MOD_HUB | TRC_IOCTL;
    int rc = 0;
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class | TRC_START_END, 10, "end", "");

    return *rvalp = rc;
    
}

/*ARGSUSED*/
int usbhub_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    return 0;
}

/*ARGSUSED*/
int usbhub_unmap(dev_t dev, vhandl_t *vt){
    return (0);
}



