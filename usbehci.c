/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbehci.c                                                  *
* Description: EHCI host controller driver                         *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      08-03-2012     -Fixed wrong class in trace messages                                     *
*                            -Added port reset in ehci_port_action()                                  *
*                            -Added _diaginfo_ string in startup trace messages                       *
*                                                                                                     *
* BSDero      07-26-2012     -Added EHCI data structures definition                                   *
*                            -Added initial EHCI list                                                 *
*                            -Added initial configuration for async list and periodic list            *
*                                                                                                     *
* BSDero      07-23-2012     -Fixed long time bug related to PortEnable in EHCI                       *
*                            -Improved port reset                                                     *
*                            -Added advice to usbhub in port connect/disconnect                       *
*                            -Added log of kernel module address                                      *
*                                                                                                     *
* BSDero      07-12-2012     -Updated ehci_get_info(), ehci_info_get(), ehci_get_port()               *
*                            -Implemented ehci_port_action(), ehci_set_roothub()                      *
*                            -Deleted register_hcd_driver() and unregister_hcd_driver()               *
*                                                                                                     *
* BSDero      07-10-2012     -Added ehci_process_event() and ehci_process_event_from_usbcore()        *
*                                                                                                     *
* BSDero      07-05-2012     -Get ports and controller ownership                                      *
*                            -Send events to hub implemented                                          *
*                                                                                                     *
* BSDero      06-21-2012     -Updated ehci_hub_info()                                                 *
*                                                                                                     *
* BSDero      06-15-2012     -Implemented get information from PCI bus                                *
*                                                                                                     *
* BSDero      06-03-2012     -Implemented get_usbcore(), register_hcd_driver(), unregister_hcd_driver *
*                                                                                                     *
* BSDero      05-03-2012     -Initial version, parts rippen from existing code from a previous file   *
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

#include "ehcireg.h"                                /* ehci register defines */

/*
*******************************************************************************************************
*   Supported EHCI devices                                                                            *
*******************************************************************************************************
*/
static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}ehci_descriptions[]={
    0x268c8086, "Intel 63XXESB USB 2.0 controller",
    0x523910b9, "ALi M5239 USB 2.0 controller",
    0x10227463, "AMD 8111 USB 2.0 controller",
    0x20951022, "(AMD CS5536 Geode) USB 2.0 controller",
    0x43451002, "ATI SB200 USB 2.0 controller",
    0x43731002, "ATI SB400 USB 2.0 controller",
    0x25ad8086, "Intel 6300ESB USB 2.0 controller",
    0x24cd8086, "(Intel 82801DB/L/M ICH4) USB 2.0 controller",
    0x24dd8086, "(Intel 82801EB/R ICH5) USB 2.0 controller",
    0x265c8086, "(Intel 82801FB ICH6) USB 2.0 controller",
    0x27cc8086, "(Intel 82801GB/R ICH7) USB 2.0 controller",
    0x28368086, "(Intel 82801H ICH8) USB 2.0 controller USB2-A",
    0x283a8086, "(Intel 82801H ICH8) USB 2.0 controller USB2-B",
    0x293a8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x293c8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x3a3a8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-A",
    0x3a3c8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-B",
    0x3b348086, "Intel PCH USB 2.0 controller USB-A",
    0x3b3c8086, "Intel PCH USB 2.0 controller USB-B",
    0x00e01033, "NEC uPD 720100 USB 2.0 controller",
    0x006810de, "NVIDIA nForce2 USB 2.0 controller",
    0x008810de, "NVIDIA nForce2 Ultra 400 USB 2.0 controller",
    0x00d810de, "NVIDIA nForce3 USB 2.0 controller",
    0x00e810de, "NVIDIA nForce3 250 USB 2.0 controller",
    0x005b10de, "NVIDIA nForce4 USB 2.0 controller",
    0x036d10de, "NVIDIA nForce MCP55 USB 2.0 controller",
    0x03f210de, "NVIDIA nForce MCP61 USB 2.0 controller",
    0x0aa610de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aa910de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aaa10de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x15621131, "Philips ISP156x USB 2.0 controller",
    0x31041106, "VIA VT6212 EHCI USB 2.0 controller",
    0 ,NULL
};

#define EHCI_CF_VENDOR_ID                           0x00
#define EHCI_CF_DEVICE_ID                           0x02
#define EHCI_CF_COMMAND                             0x04
#define EHCI_CF_STATUS                              0x06
#define EHCI_CF_REVISION_ID                         0x08
#define EHCI_CF_CLASS_CODE                          0x09
#define EHCI_CF_CACHE_LINE_SIZE                     0x0c
#define EHCI_CF_LATENCY_TIME                        0x0d
#define EHCI_CF_HEADER_TYPE                         0x0e
#define EHCI_CF_BIST                                0x0f
#define EHCI_CF_MMAP_IO_BASE_ADDR                   0x10
#define EHCI_CF_CIS_BASE_ADDR                       0x14
#define EHCI_CF_CARDBUS_CIS_PTR                     0x28
#define EHCI_CF_SSID                                0x2c
#define EHCI_CF_PWR_MGMT_CAPS                       0x34
#define EHCI_CF_INTERRUPT_LINE                      0x3c
#define EHCI_CF_INTERRUPT_PIN                       0x3d
#define EHCI_NUM_CONF_REGISTERS                     0x85
#define EHCI_NUM_IO_REGISTERS                       0x64

/*
*******************************************************************************************************
* Specific EHCI Data structures                                                                       *
*******************************************************************************************************
*/

typedef struct{
    uint32_t next;
    uint32_t info1;
    uint32_t info2;
    uint32_t current;

    uint32_t qtd_next;
    uint32_t alt_next;
    uint32_t token;
    uint32_t buf[5];
} ehci_qh_t;

#define QH_CONTROL       (1 << 27)
#define QH_MAXPACKET_SHIFT 16
#define QH_MAXPACKET_MASK  (0x7ff << QH_MAXPACKET_SHIFT)
#define QH_HEAD          (1 << 15)
#define QH_TOGGLECONTROL (1 << 14)
#define QH_SPEED_SHIFT   12
#define QH_SPEED_MASK    (0x3 << QH_SPEED_SHIFT)
#define QH_EP_SHIFT      8
#define QH_EP_MASK       (0xf << QH_EP_SHIFT)
#define QH_DEVADDR_SHIFT 0
#define QH_DEVADDR_MASK  (0x7f << QH_DEVADDR_SHIFT)

#define QH_SMASK_SHIFT   0
#define QH_SMASK_MASK    (0xff << QH_SMASK_SHIFT)
#define QH_CMASK_SHIFT   8
#define QH_CMASK_MASK    (0xff << QH_CMASK_SHIFT)
#define QH_HUBADDR_SHIFT 16
#define QH_HUBADDR_MASK  (0x7f << QH_HUBADDR_SHIFT)
#define QH_HUBPORT_SHIFT 23
#define QH_HUBPORT_MASK  (0x7f << QH_HUBPORT_SHIFT)
#define QH_MULT_SHIFT    30
#define QH_MULT_MASK     (0x3 << QH_MULT_SHIFT)

#define EHCI_PTR_BITS           0x001F
#define EHCI_PTR_TERM           0x0001
#define EHCI_PTR_QH             0x0002


#define EHCI_QTD_ALIGN 32

typedef struct{
    uint32_t qtd_next;
    uint32_t alt_next;
    uint32_t token;
    uint32_t buf[5];
    //uint32_t buf_hi[5];
}ehci_qtd_t;

#define QTD_TOGGLE      (1 << 31)
#define QTD_LENGTH_SHIFT 16
#define QTD_LENGTH_MASK (0x7fff << QTD_LENGTH_SHIFT)
#define QTD_CERR_SHIFT  10
#define QTD_CERR_MASK   (0x3 << QTD_CERR_SHIFT)
#define QTD_IOC         (1 << 15)
#define QTD_PID_OUT     (0x0 << 8)
#define QTD_PID_IN      (0x1 << 8)
#define QTD_PID_SETUP   (0x2 << 8)
#define QTD_STS_ACTIVE  (1 << 7)
#define QTD_STS_HALT    (1 << 6)
#define QTD_STS_DBE     (1 << 5)
#define QTD_STS_BABBLE  (1 << 4)
#define QTD_STS_XACT    (1 << 3)
#define QTD_STS_MMF     (1 << 2)
#define QTD_STS_STS     (1 << 1)
#define QTD_STS_PING    (1 << 0)

#define ehci_explen(len) (((len) << QTD_LENGTH_SHIFT) & QTD_LENGTH_MASK)

#define ehci_maxerr(err) (((err) << QTD_CERR_SHIFT) & QTD_CERR_MASK)


typedef struct{
    uint32_t links[1024];
} ehci_framelist_t;


/*
*******************************************************************************************************
* EHCI instance                                                                                       *
*******************************************************************************************************
*/
typedef struct {
	device_header_t        device_header;
    vertex_hdl_t           ps_conn;        /* connection for pci services  */
    vertex_hdl_t           ps_vhdl;        /* backpointer to device vertex */
    vertex_hdl_t           ps_charv;       /* backpointer to device vertex */
    USB_func_t             ps_event_func;  /* function to USBcore event    */ 
    uchar_t                *ps_cfg;        /* cached ptr to my config regs */
    uchar_t                *ps_regs;        /* cached ptr to my regs */
    uchar_t                *pci_io_caps;
    pciio_piomap_t         ps_cmap;        /* piomap (if any) for ps_cfg */
    pciio_piomap_t         ps_rmap;        /* piomap (if any) for ps_regs */
    unsigned               ps_sst;         /* driver "software state" */
#define usbehci_SST_RX_READY      (0x0001)
#define usbehci_SST_TX_READY      (0x0002)
#define usbehci_SST_ERROR         (0x0004)
#define usbehci_SST_INUSE         (0x8000)
    pciio_intr_t            ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t          ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t          ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead         *ps_pollhead;  /* for poll() */
    uint16_t                sc_offs;
    int                     ps_blocks;  /* block dev size in NBPSCTR blocks */
    uint32_t                ps_event;
    uint8_t                 ps_noport;
    uint32_t                ps_eintrs;
    pciio_info_t            ps_pciio_info_device;
    usbcore_instance_t      *usbcore;    
    usbhub_instance_t       *roothub;
    ehci_qh_t               *async_list_addr;
    ehci_framelist_t        *periodic_list_base;
}usbehci_instance_t;

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/
int                                                 usbehci_devflag = D_MP;
char                                                *usbehci_mversion = M_VERSION;  /* for loadable modules */
int                                                 usbehci_inuse = 0;     /* number of "usbehci" devices open */
gc_list_t                                           gc_list;
usbehci_instance_t                                  *global_soft = NULL; 
dma_list_t                                          dma_list; 

/*
*******************************************************************************************************
* Helper functions for usbcore                                                                        *
*******************************************************************************************************
*/
usbcore_instance_t *get_usbcore();
 
/*
*******************************************************************************************************
* Callback functions for usbcore                                                                      *
*******************************************************************************************************
*/
int ehci_reset( void *hcd);
int ehci_start( void *hcd);
int ehci_init( void *hcd);
int ehci_stop( void *hcd);
int ehci_shutdown( void *hcd);
int ehci_suspend( void *hcd);
int ehci_resume( void *hcd);
int ehci_status( void *hcd);
void ehci_free_pipe( void *hcd, struct usb_pipe *pipe);
usb_pipe_t *ehci_alloc_control_pipe( void *hcd, int tdnum, td_addr_t *td_addr);
usb_pipe_t *ehci_alloc_bulk_pipe( void *hcd, usb_pipe_t *pipe, struct usb_endpoint_descriptor *descriptor);
int ehci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int ehci_set_address( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int ehci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize);
int ehci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int ehci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data);
int ehci_ioctl( void *hcd, int cmd, void *uarg);
int ehci_set_trace_level( void *hcd, void *trace_level);
int ehci_hub_info( void *hcd, void *info);
int ehci_port_action( void *hcd, int port, int action);
uint32_t ehci_get_port( void *hcd, int port);
int ehci_set_roothub(void *hcd, void *roothub);
int ehci_port_reset( usbehci_instance_t *soft, int port);
int ehci_set_debug_values( void *hcd, void *pv);


/*
*******************************************************************************************************
* Methods for usbcore                                                                                 *
*******************************************************************************************************
*/
hcd_methods_t ehci_methods={
	ehci_reset,
	ehci_start,
	ehci_init, 
	ehci_stop,
	ehci_shutdown,
	ehci_suspend,
	ehci_resume,
	ehci_status,
	ehci_free_pipe,
	ehci_alloc_control_pipe, 
	ehci_alloc_bulk_pipe, 
	ehci_send_control, 
	ehci_set_address, 
	ehci_usb_send_bulk,
	ehci_alloc_intr_pipe,
	ehci_usb_poll_intr,
	ehci_ioctl,
	ehci_set_trace_level,
	ehci_hub_info,
	ehci_port_action,
	ehci_get_port,
	ehci_set_roothub,
	ehci_set_debug_values,
	0x00000000
};


int ehci_process_event( void *origin, void *dest, int event_id, void *arg);
int ehci_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2);

module_header_t ehci_header={
	USB_MOD_EHCI, 
	"usbehci", 
	"Enhanced Host Controller Interface (USB 2.0)", 
	"usbehci.o", 
	USB_DRIVER_IS_HCD, 
	{
		ehci_set_trace_level,
	    ehci_process_event,
	    ehci_process_event_from_usbcore,
	    0x0000	
	}
};
    
	

/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
void                  usbehci_init(void);
int                   usbehci_unload(void);
int                   usbehci_reg(void);
int                   usbehci_unreg(void);
int                   usbehci_attach(vertex_hdl_t conn);
int                   usbehci_detach(vertex_hdl_t conn);
static pciio_iter_f   usbehci_reloadme;
static pciio_iter_f   usbehci_unloadme;
int                   usbehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                   usbehci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                   usbehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                   usbehci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbehci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbehci_strategy(struct buf *bp);
int                   usbehci_poll(dev_t dev, short events, int anyyet, 
                               short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                   usbehci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                   usbehci_unmap(dev_t dev, vhandl_t *vt);
void                  usbehci_dma_intr(intr_arg_t arg);
static error_handler_f usbehci_error_handler;
void                  usbehci_halt(void);
int                   usbehci_size(dev_t dev);
int                   usbehci_print(dev_t dev, char *str);


/*
*******************************************************************************************************
* Host controller specific hardware functions                                                         *
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
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
	
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
 
int ehci_port_reset( usbehci_instance_t *soft, int port){
    int rc, i, j, reset = 0;
	uint32_t v;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
	
	TRACE( class | TRC_START_END, 10, "Starting port %d reset", port);
	
    /* clear connect status change */
    v = EOREAD4(soft, EHCI_PORTSC(port))  & ~EHCI_PS_CSC;
    
    for( j = 0; j < 2; j++){
    	/* enable reset bit */
	    v |= EHCI_PS_PR;
	
	    /* clear port enable bit */
        v &= ~EHCI_PS_PE; 
    
    	EOWRITE4( soft, EHCI_PORTSC(port), v);
	    USECDELAY(50000);  /* wait 50 miliseconds */
	
	    /* clear reset bit */
	    v &= ~EHCI_PS_PR; 
	    EOWRITE4( soft, EHCI_PORTSC(port), v );
	    USECDELAY(10000);  /* wait 10 miliseconds */
	
	    for( i = 0; i < 100;  i++){ /* loop until port reset is complete */
        
            v = EOREAD4( soft, EHCI_PORTSC(port)) ;
            if( ( v & EHCI_PS_PR) == 0){
		        TRACE( class, 12, "port reset complete", "");
		        reset = 1; 
		        break;	
		    }
		    USECDELAY(1000);
	    }
    
        if( reset == 0){ 
	        TRACE( class, 12, "port didn't reset!!! EHCI_PS_PR still enable... quitting with rc=-1", ""); 	
	        return( -1);
	    }
	    
	    USECDELAY(2000);  /* wait 2 milisecs */
	    v = EOREAD4( soft, EHCI_PORTSC(port)) ;

	    
	    if( !( v & EHCI_PS_PE)){
			if( j != 0){ 
				/* this is the second reset and this is 
				 * not a high speed device, give up ownership.*/
				TRACE( class, 12, "Not a high speed device, give up ownership here", "");
				v = EOREAD4( soft, EHCI_PORTSC(port)) & ~EHCI_PS_CLEAR;
				EOWRITE4(soft, EHCI_PORTSC(port), v | EHCI_PS_PO);
				TRACE( class | TRC_START_END, 10, "end", "");
				return( 0);
			}
		}else{
            TRACE( class, 12, "High speed device!! YES!!", "");
            TRACE( class | TRC_START_END, 10, "end", "");
            return( 1);
        }
        
    }
    
    
    TRACE( class | TRC_START_END, 10, "end", "");
	return(0);
	
	
}


 
/*
*******************************************************************************************************
* Host controller callback implementation for USBcore                                                 *
*******************************************************************************************************
*/
int ehci_reset( void *hcd){
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);
        
        
    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
}

int ehci_start( void *hcd){
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);

    /* enable interrupts */
    TRACE( class, 12, "enabling interrupts", "");
/*    soft->ps_eintrs = EHCI_NORMAL_INTRS;*/
    soft->ps_eintrs = (EHCI_STS_PCD | EHCI_STS_ERRINT | EHCI_STS_INT);
    EOWRITE4(soft, EHCI_USBINTR, soft->ps_eintrs);
        
    TRACE( class, 10 | TRC_START_END, "end", "");
    return( 0);
}

int ehci_hcreset( void *hcd){
    uint32_t hcr;
    int i;
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);

    EOWRITE4(soft, EHCI_USBCMD, 0);/* Halt controller */
    for (i = 0; i < 100; i++) {
        USECDELAY(10000);
        hcr = EOREAD4(soft, EHCI_USBSTS) & EHCI_STS_HCH;
        if (hcr){
		    TRACE( class, 10, "Host controller halted (HCH = 1)", "");
		    TRACE( class | TRC_START_END, 10, "end", "");
            return(0);
		}
    }

    TRACE( class | TRC_START_END, 10, "end", "");
	return( -1);
 }


int ehci_init( void *hcd){
    uint32_t                version;
    uint32_t                sparams;
    uint32_t                cparams;
    uint16_t                i;
    uint32_t                cmd;
    int                     hcr;  
    usbehci_instance_t      *soft = ( usbehci_instance_t *) hcd;
    dma_node_t              *dma_node_framelist;
    dma_node_t              *dma_node_intr_qh;
    dma_node_t              *dma_node_async_qh;
    ehci_framelist_t        *fl;
    ehci_qh_t               *intr_qh;
    ehci_qh_t               *async_qh;
    
    uint64_t class = TRC_ATTACH | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);
	
    soft->sc_offs = EHCI_CAPLENGTH(EREAD4(soft, EHCI_CAPLEN_HCIVERSION));
    TRACE( class, 12, "sc_offs=0x%x", soft->sc_offs);

    soft->ps_event = 0x0000;


    version = EHCI_HCIVERSION(EREAD4( soft, EHCI_CAPLEN_HCIVERSION));
    TRACE( class, 12,  "EHCI version %x.%x", version >> 8, version & 0xff);
    sparams = EREAD4( soft, EHCI_HCSPARAMS);
    TRACE( class, 12, "sparams=0x%x", sparams);
    soft->ps_noport = EHCI_HCS_N_PORTS( sparams);
    TRACE( class, 12, "ports number=%d", soft->ps_noport);

    cparams = EREAD4( soft, EHCI_HCCPARAMS);

    if ( EHCI_HCC_64BIT( cparams)) {
        TRACE( class, 12, "HCC uses 64-bit structures", "");
        /* MUST clear segment register if 64 bit capable */
        EWRITE4( soft, EHCI_CTRLDSSEGMENT, 0);
    }
    
    /* Reset the controller */
    TRACE( class, 12, "EHCI controller resetting", "");
    ehci_hcreset(soft);
    
    dma_list_init( &dma_list);
    dma_node_framelist = dma_alloc( soft->ps_conn, &dma_list, sizeof(ehci_framelist_t));
    dma_node_intr_qh = dma_alloc( soft->ps_conn, &dma_list, sizeof(ehci_qh_t));
    dma_node_async_qh = dma_alloc( soft->ps_conn, &dma_list, sizeof(ehci_qh_t));
    
    
    if( dma_node_framelist == NULL || dma_node_intr_qh == NULL || dma_node_async_qh == NULL){
		TRACE( class, 4, "Error in dma_alloc()!!!", "");
		return( -1);
	}
    
    TRACE( class, 10, "dma reserved", "");
    fl = ( ehci_framelist_t *) dma_node_framelist->mem;
    intr_qh = (ehci_qh_t *) dma_node_intr_qh->mem;
    async_qh = (ehci_qh_t *) dma_node_async_qh->mem;
    
    memset( intr_qh, 0, sizeof( ehci_qh_t));
    TRACE( class, 10, "written1", "");
    
    intr_qh->next = EHCI_PTR_TERM;
    intr_qh->info2 = (0x01 << QH_SMASK_SHIFT);
    intr_qh->token = QTD_STS_HALT;
    intr_qh->qtd_next = intr_qh->alt_next = EHCI_PTR_TERM;
    TRACE( class, 10, "written2", "");
    

    for ( i = 0; i < 1024; i++)
        fl->links[i] = (uint32_t)( dma_node_intr_qh->daddr) | EHCI_PTR_QH;

    TRACE( class, 10, "written3", "");
        
    EOWRITE4(soft, EHCI_PERIODICLISTBASE, (uint32_t) dma_node_framelist->daddr);
    TRACE( class, 10, "periodiclistbase register updated!!", "");
    soft->periodic_list_base = fl;

    /* Set async list to point to primary async queue head */
    memset( async_qh, 0, sizeof( ehci_qh_t) );
    TRACE( class, 10, "written4", "");
    
    async_qh->next = ( uint32_t) ( (  (uint32_t) dma_node_async_qh->daddr) | EHCI_PTR_QH);
    async_qh->info1 = QH_HEAD;
    async_qh->token = QTD_STS_HALT;
    async_qh->qtd_next = async_qh->alt_next = EHCI_PTR_TERM;
    TRACE( class, 10, "written5", "");

    soft->async_list_addr = async_qh;
    TRACE( class, 10, "written6", "");
    
    EOWRITE4(soft, EHCI_ASYNCLISTADDR, (uint32_t) dma_node_async_qh->daddr);    
    TRACE( class, 10, "asynclistaddr register updated!!", "");
    
/*    EOWRITE4(sc, EHCI_ASYNCLISTADDR, (uint32_t)0);
    EOWRITE4(sc, EHCI_ASYNCLISTADDR, (uint32_t)0);
	USECDELAY(200000);*/
	
    TRACE( class, 12, "Turn on controller", "");
    /* turn on controller */
    /* as the PCI bus in the O2 is too slow (33 mhz). we need to slowdown the interrupts max rate to 64 microframes (8 ms).
       default is 1 microframe (0x08)
    */

    /*
    EOWRITE4(sc, EHCI_USBCMD,
                 EHCI_CMD_ITC_1 | 
                (EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_FLS_M) |
                 EHCI_CMD_ASE |
                 EHCI_CMD_PSE |
                 EHCI_CMD_RS);
*/
    EOWRITE4(soft, EHCI_USBCMD,
                 EHCI_CMD_ITC_64 | /* 1 microframes interrupt delay */
                (EOREAD4(soft, EHCI_USBCMD)) |
                 EHCI_CMD_ASE  |
                 EHCI_CMD_PSE  |
			     EHCI_CMD_IAAD |
                 EHCI_CMD_RS);

				 
    TRACE( class, 12, "Enable interrupts", "");
    /* enable interrupts */
   /* soft->ps_eintrs = EHCI_NORMAL_INTRS;
    EOWRITE4(soft, EHCI_USBINTR, soft->ps_eintrs);*/


    /* Take over port ownership */
    TRACE( class, 12, "EHCI driver now has ports ownership", "");
    EOWRITE4(soft, EHCI_CONFIGFLAG, EHCI_CONF_CF);

    for (i = 0; i < 10000; i++) {
        USECDELAY(100);
        hcr = EOREAD4(soft, EHCI_USBSTS) & EHCI_STS_HCH;
        if (!hcr) {
            break;
        }
    }

    if (hcr) {
        TRACE( class, 12, "run timeout", "");
    }
    
    TRACE( class, 12, "EHCI driver taking property of all ports over another *HCI", "");
    for (i = 1; i <= soft->ps_noport; i++) {
        cmd = EOREAD4(soft, EHCI_PORTSC(i));
        EOWRITE4(soft, EHCI_PORTSC(i), (cmd & ~EHCI_PS_PO) | EHCI_PS_PP );
    }
    

    TRACE( class | TRC_START_END, 10, "end", "");
    return( 0);
}


int ehci_stop( void *hcd){
}

int ehci_shutdown( void *hcd){
}

int ehci_suspend( void *hcd){
}

int ehci_resume( void *hcd){
}

int ehci_status( void *hcd){ /* just a demo */
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    uint32_t status;
    int rc;
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);
        
    status = ( EOREAD4(soft, EHCI_USBSTS) & EHCI_STS_HSE);
    if( status == 0){
		rc = 0;
    }else{
		rc = 1;
	}
    
    TRACE( class | TRC_START_END, 10, "end", "");
    return(rc);
}

void ehci_free_pipe( void *hcd, struct usb_pipe *pipe){
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);

    TRACE( class | TRC_START_END, 10, "end", "");
}

usb_pipe_t *ehci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *desc){
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);

    TRACE( class | TRC_START_END, 10, "end", "");

}

usb_pipe_t *ehci_alloc_control_pipe( void *hcd, int tdnum, td_addr_t *td_addr){
}

int ehci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize){
}

int ehci_set_address( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize){
}


int ehci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize){
}

int ehci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int ehci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data){
}

int ehci_ioctl( void *hcd, int cmd, void *uarg){
    	
}



int ehci_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
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


int ehci_hub_info( void *hcd, void *info){
	usbhub_instance_t *usb_hub = (usbhub_instance_t *) info;
	usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
     	
	TRACE( class | TRC_START_END, 10, "start", "");
	
	usb_hub->pci_bus = (uchar_t) pciio_info_bus_get( soft->ps_pciio_info_device);
	usb_hub->pci_slot = (uchar_t) pciio_info_slot_get( soft->ps_pciio_info_device);
	usb_hub->pci_function = (int) pciio_info_function_get( soft->ps_pciio_info_device);
    usb_hub->ports_number = soft->ps_noport;
/*
    for (i = 1; i <= soft->ps_noport; i++){
		port = EOREAD4( soft, EHCI_PORTSC(i));
		pa = 0;
		if( port & EHCI_PS_CS)
		    pa |= USB_PORT_CONNECTED;
		if( port & EHCI_PS_PE)
		    pa |= USB_PORT_ENABLED | USB_PORT_IN_USE;
		if( port & EHCI_PS_SUSP)
		    pa |= USB_PORT_SUSPENDED;
	
	    if( EHCI_PS_IS_LOWSPEED( port))
			pa |= USB_PORT_LOW_SPEED  | USB_PORT_SET_HCD_OWNER(USB_EHCI);
	    else 
	        pa |= USB_PORT_HIGH_SPEED | USB_PORT_SET_HCD_OWNER(USB_EHCI);
	        
        pa |= USB_PORT_STATUS_OK;	    
	    
		
	    usb_hub->ports_status[i-1] = pa;
	}
*/    
	    
	TRACE( class | TRC_START_END, 10, "end", "");	
	return(0);
}



int ehci_port_action( void *hcd, int port, int action){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    usbehci_instance_t *soft = (usbehci_instance_t *) hcd;
    uint32_t v;
    
	TRACE( class | TRC_START_END, 10, "start", "");
	switch( action){
	    case USB_HCD_SET_OWNER:
	        TRACE( class, 12, "ehci now owns port", "");
	        port++;  	                    
	        v = EOREAD4( soft, EHCI_PORTSC(port));
            EOWRITE4(soft, EHCI_PORTSC(port), v & ~EHCI_PS_PO);
	    break;	
	    case USB_HUB_PORT_RESET:
	        TRACE( class, 12, "ehci reset port", "");
	        port++;  	                    
	        ehci_port_reset( soft, port);
	    break;
	}
	TRACE( class | TRC_START_END, 10, "end", "");	
	return(0);
}




uint32_t ehci_get_port( void *hcd, int port){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
	usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint32_t pa, rport;

	TRACE( class | TRC_START_END, 10, "start", "");
    port++;
	rport = EOREAD4( soft, EHCI_PORTSC(port));
	pa = 0;
	if( rport & EHCI_PS_CS)
	    pa |= USB_PORT_CONNECTED;
	if( rport & EHCI_PS_PE)
	    pa |= USB_PORT_ENABLED | USB_PORT_IN_USE;
	if( rport & EHCI_PS_SUSP)
	    pa |= USB_PORT_SUSPENDED;
	
    if( EHCI_PS_IS_LOWSPEED( rport))
		pa |= USB_PORT_LOW_SPEED;
	else 
	    pa |= USB_PORT_HIGH_SPEED;
	        
    pa |= USB_PORT_STATUS_OK;	    
    pa |= USB_PORT_SET_HCD_OWNER(USB_EHCI);
    	
	TRACE( class | TRC_START_END, 10, "end", "");	
	return( pa);
}


int ehci_set_roothub(void *hcd, void *roothub){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    usbhub_instance_t *usbhub = ( usbhub_instance_t *) roothub;
    
	TRACE( class | TRC_START_END, 10, "start", "");
    soft->roothub = roothub;
   	TRACE( class | TRC_START_END, 10, "end", "");	
   	return( 0);
}



int ehci_process_event( void *origin, void *dest, int event_id, void *arg){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	
   	return( 0);
}

int ehci_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	
   	return( 0);
}

int ehci_set_debug_values( void *hcd, void *pv){
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
    usbehci_instance_t *soft = (usbehci_instance_t *) hcd;
    int rc; 
    usb_debug_values_t *v = (usb_debug_values_t *) pv;

	TRACE( class | TRC_START_END, 10, "start", "");

	TRACE( class | TRC_START_END, 10, "end rc=%d", rc);	
	return(rc);	
}


/*
*******************************************************************************************************
* USBcore calls functions                                                                             *
*******************************************************************************************************
*/
int register_ehci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbehci_instance_t          *soft;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
	char str[45];
	
	soft = (usbehci_instance_t *) hcd;
	TRACE( class | TRC_START_END, 10, "start", "");



	usbcore = (usbcore_instance_t *) soft->usbcore;
	if( usbcore == NULL){
	    TRACEWAR( class | TRC_WARNING,  
	        "could not get usbcore information from device_info_get()", "");
	    return( EFAULT);
	}
		
	TRACE( class | TRC_START_END, 10, "end", "");
	return( 0);
}


int unregister_ehci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbehci_instance_t          *soft;
    uint64_t class = TRC_HELPER | TRC_MOD_EHCI;
	char str[45];
	
	soft = (usbehci_instance_t *) hcd;
	TRACE( class | TRC_START_END, 10, "start", "");



	usbcore = (usbcore_instance_t *) soft->usbcore;
	if( usbcore == NULL){
	    TRACEWAR( class | TRC_WARNING,  
	        "could not get usbcore information from device_info_get()", "");
	    return( EFAULT);
	}

	TRACE( class | TRC_START_END, 10, "end", "");
	return( 0);
}



/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
    
/*
*******************************************************************************************************
* usbehci_init: called once during system startup or                                                  *
* when a loadable driver is loaded.                                                                   *
*******************************************************************************************************
*/
void usbehci_init(void){
    usbcore_instance_t *usbcore;
    uint64_t class = TRC_INIT | TRC_MOD_EHCI;
	
  	TRACE( class | TRC_START_END, 10, "start", "");
  	printf( "**********************************************************\n");
  	printf( "* EHCI USB Driver for Silicon Graphics Irix 6.5          *\n");
  	printf( "* By bsdero at gmail dot org, 2011                       *\n");
  	printf( "* Version %s                                        *\n", USBCORE_DRV_VERSION);
  	printf( "* Sequence %s                                *\n", USBCORE_DRV_SEQ);
  	printf( "**********************************************************\n");
    printf( "usbehci kernel module loaded! \n");
    printf( "_diaginfo_: Kernel usbehci module base address:0x%x\n", module_address);

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */

    usbcore = get_usbcore();
    usbcore->register_module( ( void *) &ehci_header);
    pciio_iterate("usbehci_", usbehci_reloadme);	
    gc_list_init( &gc_list);
	TRACE( class | TRC_START_END, 10, "end", "");
}


/*
*******************************************************************************************************
*    usbehci_unload: if no "usbehci" is open, put us to bed                                           *
*      and let the driver text get unloaded.                                                          *
*******************************************************************************************************
*/
int usbehci_unload(void){
    usbcore_instance_t *usbcore;
    uint64_t class = TRC_UNLOAD | TRC_MOD_EHCI;

	TRACE( class | TRC_START_END, 10, "start", "");
    if (usbehci_inuse)
        return EBUSY;

    usbcore = get_usbcore();
    usbcore->unregister_module( ( void *) &ehci_header);
    pciio_iterate("usbehci_", usbehci_unloadme);
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "EHCI Driver unloaded", "");
	TRACE( class | TRC_START_END, 10, "end", "");
    
    return 0;
}



/*
*******************************************************************************************************
*    usbehci_reg: called once during system startup or                                                *
*      when a loadable driver is loaded.                                                              *
*    NOTE: a bus provider register routine should always be                                           *
*      called from _reg, rather than from _init. In the case                                          *
*      of a loadable module, the devsw is not hooked up                                               *
*      when the _init routines are called.                                                            *
*******************************************************************************************************
 */
int usbehci_reg(void){
	int i;
	uint16_t device_id, vendor_id;
    uint64_t class = TRC_INIT | TRC_MOD_EHCI;
	
	TRACE( class | TRC_START_END, 10, "start", "");
	
    /* registering EHCI devices */
    for( i = 0; ehci_descriptions[i].device_id != 0; i++){
        device_id = ((ehci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (ehci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "usbehci_", 0);  /* usbehci_attach and usbehci_detach entry points */
    }

	TRACE( class | TRC_START_END, 10, "end", "");
    return 0;
}



/*
*******************************************************************************************************
* usbehci_unreg: called when a loadable driver is unloaded.                                           *
*******************************************************************************************************
*/
int usbehci_unreg(void){
	TRACE(  TRC_UNLOAD | TRC_MOD_EHCI | TRC_START_END, 10, "start", "");
    pciio_driver_unregister("usbehci_");
	TRACE(  TRC_UNLOAD | TRC_MOD_EHCI | TRC_START_END, 10, "end", "");
    return 0;
}


/*
*******************************************************************************************************
*    usbehci_attach: called by the pciio infrastructure                                               *
*      once for each vertex representing a crosstalk widget.                                          *
*      In large configurations, it is possible for a                                                  *
*      huge number of CPUs to enter this routine all at                                               *
*      nearly the same time, for different specific                                                   *
*      instances of the device. Attempting to give your                                               *
*      devices sequence numbers based on the order they                                               *
*      are found in the system is not only futile but may be                                          *
*      dangerous as the order may differ from run to run.                                             *
*******************************************************************************************************
*/
int usbehci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t            charv = GRAPH_VERTEX_NONE;       
    uchar_t                 *cfg;
    usbehci_instance_t      *soft;
    volatile uchar_t        *regs;
    pciio_piomap_t          cmap = NULL;
    pciio_piomap_t          rmap = NULL;
    graph_error_t           ret = (graph_error_t) 0;
    uint16_t                vendor_id;
    uint16_t                device_id;
    uint32_t                ssid;
    uchar_t                 rev_id;
    uchar_t                 val;
    int                     i;
    uint32_t                device_vendor_id;    
    int                     rc;
    device_desc_t           usbehci_dev_desc;    
    uint64_t                class =  TRC_ATTACH | TRC_MOD_EHCI;
    usbcore_instance_t      *usbcore;
    
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class, 0, "EHCI Host Controller Device Detected!", "");

    usbcore = get_usbcore();
    if( usbcore == NULL){ 
		TRACEWAR( class | TRC_WARNING, "usbcore not loaded, quitting..", ""); 
		return( -1);
	} 

    
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
		TRACEWAR( class | TRC_WARNING, "error in hwgraph_traverse(), quitting..", ""); 
        return( -1);
    }
    

    ret = hwgraph_edge_get( vhdl, "usbehci", &charv);
    TRACE( class, 8, "rc of hwgraph_edge_get() = %d, added usbehci edge", ret);



    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    soft = (usbehci_instance_t *) gc_malloc( &gc_list, sizeof( usbehci_instance_t));
    
    ASSERT(soft != NULL);
    device_info_set(vhdl, (void *)soft);
    
    /* fill in soft structure */
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_charv = charv;
    soft->usbcore = usbcore;
    
    
    
    cfg = (uchar_t *) pciio_pio_addr
        (conn, 0,                     /* device and (override) dev_info */
         PCIIO_SPACE_CFG,             /* select configuration addr space */
         0,                           /* from the start of space, */
         EHCI_NUM_CONF_REGISTERS,     /* ... up to vendor specific stuff */
         &cmap,                       /* in case we needed a piomap */
         0);                          /* flag word */

    vendor_id = (uint16_t) PCI_CFG_GET16(conn, EHCI_CF_VENDOR_ID);
    device_id = (uint16_t) PCI_CFG_GET16(conn, EHCI_CF_DEVICE_ID);
    ssid = (uint32_t) PCI_CFG_GET32( conn, EHCI_CF_SSID);
    rev_id = (uchar_t) PCI_CFG_GET8( conn, EHCI_CF_REVISION_ID);
    
    TRACE( class, 0, "EHCI supported device found", "");
    TRACE( class, 0, "Vendor ID: 0x%x, Device ID: 0x%x", vendor_id, device_id);
    TRACE( class, 0, "SSID: 0x%x, Rev ID: 0x%x",ssid, rev_id);
    

    soft->device_header.module_header = &ehci_header;
    
    device_vendor_id = device_id << 16 | vendor_id;
    for( i = 0; ehci_descriptions[i].device_id != 0; i++){
        if( ehci_descriptions[i].device_id == device_vendor_id){
            soft->device_header.vendor_id = vendor_id;
            soft->device_header.device_id = device_id;
            strcpy( soft->device_header.hardware_description, 
                  (char *)ehci_descriptions[i].controller_description);
            TRACE( class, 0, "Device Description: %s", 
                ehci_descriptions[i].controller_description);
            break;
        }
    }
    
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);

    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (volatile uchar_t *) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),            /* in my primary decode window, */
         0, EHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                         /* in case we needed a piomap */
         0);                            /* flag word */
         
    TRACE( class, 8, "regs = 0x%x", regs);
    if( regs == NULL){
        return( EACCES);
    }

    soft->pci_io_caps = regs;
    soft->ps_regs = regs;               /* save for later */
    soft->ps_rmap = rmap;
    soft->ps_cfg = cfg;               /* save for later */
    soft->ps_cmap = cmap;
    soft->ps_pciio_info_device = pciio_info_get(conn);
	soft->device_header.soft = (void *) soft;
	soft->device_header.class_id = 0;
	soft->device_header.interface_id = 0;
	soft->device_header.info_size = sizeof( usbehci_instance_t);
	soft->device_header.methods = (void *) &ehci_methods;
	soft->usbcore = usbcore;
	global_soft = soft;
    
  
    ehci_init( (void *) soft);

    usbehci_dev_desc = device_desc_dup(vhdl);
    device_desc_intr_name_set(usbehci_dev_desc, "usbehci");
    device_desc_default_set(vhdl, usbehci_dev_desc);
    
    TRACE( class, 4, "Creating PCI INTR", "");
    soft->ps_intr = pciio_intr_alloc
        (conn, usbehci_dev_desc,
         PCIIO_INTR_LINE_C,   /* VIA EHCI works with line C only */
         vhdl);

    TRACE( class, 4, "about to connect..", "");
    rc = pciio_intr_connect(soft->ps_intr, usbehci_dma_intr, soft,(void *) 0);
    TRACE( class, 4, "rc = %d", rc);

    
    soft->device_header.spec_device_instance_id = usbcore->process_event_from_hcd( usbcore, 
       soft, USB_HCD_GET_INSTANCE_ID, soft, NULL, NULL);

    sprintf( soft->device_header.fs_device, "usbehci%d", 
        soft->device_header.spec_device_instance_id );
        
    ret = hwgraph_char_device_add( vhdl, soft->device_header.fs_device, 
        "usbehci_", &charv); 
        
    TRACE( class, 8, 
        "rc of hwgraph_char_device_add() = %d, added usbehci edge (char device)", ret);    

    device_info_set(vhdl, (void *)soft);

    rc = usbcore->process_event_from_hcd( usbcore, soft, USB_HCD_ATTACHED_EVENT, soft, NULL, NULL);



    /* now we're ready to launch events to root hub */
    /* FIXME add notification to HCDs when roothub is COMPLETE */
    
	TRACE( class | TRC_START_END, 10, "end", "");

    return 0;                      /* attach successsful */
}




/*
*******************************************************************************************************
*    usbehci_detach: called by the pciio infrastructure                                               *
*      once for each vertex representing a crosstalk                                                  *
*      widget when unregistering the driver.                                                          *
*                                                                                                     *
*      In large configurations, it is possible for a                                                  *
*      huge number of CPUs to enter this routine all at                                               *
*      nearly the same time, for different specific                                                   *
*      instances of the device. Attempting to give your                                               *
*      devices sequence numbers based on the order they                                               *
*      are found in the system is not only futile but may be                                          *
*      dangerous as the order may differ from run to run.                                             *
*******************************************************************************************************
*/
int usbehci_detach(vertex_hdl_t conn){
    vertex_hdl_t                  vhdl, blockv, charv;
    usbehci_instance_t            *soft;
    uint64_t                      class =  TRC_DETACH | TRC_MOD_EHCI;
    int rc;
    
	TRACE( class | TRC_START_END, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl))
        return( -1);


    soft = global_soft;

    TRACE( class, 12, "soft=0x%x, global_soft=0x%x", soft, global_soft);    
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);

    ehci_hcreset( soft);
    EOWRITE4( soft, EHCI_USBINTR, (uint32_t) 0);
    
    
  /*  rc = unregister_ehci_with_usbcore((void *) soft);
    TRACE( class, 4, "rc = %d", rc);
    */

    TRACE( class, 12, "disconnect interrupt", rc);
/*    
    pciio_error_register(conn, 0, 0);
    */
    pciio_intr_disconnect(soft->ps_intr);

    TRACE( class, 12, "free interrupt", rc);
    pciio_intr_free(soft->ps_intr);
    
/*    
    phfree(soft->ps_pollhead);
    if (soft->ps_ctl_dmamap)
        pciio_dmamap_free(soft->ps_ctl_dmamap);
    if (soft->ps_str_dmamap)
        pciio_dmamap_free(soft->ps_str_dmamap);*/

    dma_list_destroy( &dma_list);
    TRACE( class, 12, "dropping cmap", "");
    if (soft->ps_cmap != NULL)
        pciio_piomap_free(soft->ps_cmap);

/*        
    TRACE( class, 6, "dropping rmap", "");
    if (soft->ps_rmap != NULL)
        pciio_piomap_free(soft->ps_rmap);
*/
        

    TRACE( class, 12, "edge remove", rc);
    hwgraph_edge_remove(vhdl, soft->device_header.fs_device, &charv);
    /*
     * we really need "hwgraph_dev_remove" ...
     */
     
    if (GRAPH_SUCCESS == hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {

        TRACE( class, 12, "device_info_set", rc);
        device_info_set(charv, NULL);

        TRACE( class, 12, "vertex destroy", rc);
        hwgraph_vertex_destroy(charv);
    }
    

    TRACE( class, 12, "device_info_set2", rc);
    device_info_set(vhdl, NULL);
    

    TRACE( class, 12, "hwgraph_vertex_destroy", rc);
    hwgraph_vertex_destroy(vhdl);

    TRACE( class, 12, "gc_list_destroy", rc);
    gc_list_destroy( &gc_list);      

	TRACE( class | TRC_START_END, 10, "end", "");
    
    return( 0);
}





/*
*******************************************************************************************************
*    usbehci_reloadme: utility function used indirectly                                               *
*      by usbehci_init, via pciio_iterate, to "reconnect"                                             *
*      each connection point when the driver has been                                                 *
*      reloaded.                                                                                      *
*******************************************************************************************************
*/
static void usbehci_reloadme(vertex_hdl_t conn){
    vertex_hdl_t            vhdl;
    usbehci_instance_t      *soft;
    uint64_t                class = TRC_INIT | TRC_MOD_EHCI;

	TRACE( class | TRC_START_END, 10, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "usbehci", &vhdl))
        return;
        
    soft = (usbehci_instance_t *) device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbehci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbehci_dma_intr, soft, 0);
	TRACE( class | TRC_START_END, 10, "end", "");
}





/*
*******************************************************************************************************
 *    usbehci_unloadme: utility function used indirectly by
 *      usbehci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void usbehci_unloadme(vertex_hdl_t pconn){
    vertex_hdl_t            vhdl;
    usbehci_instance_t      *soft;
    uint64_t                class = TRC_UNLOAD | TRC_MOD_EHCI;
    
	TRACE( class | TRC_START_END, 10, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(pconn, "usbehci", &vhdl))
        return;
        
    soft = (usbehci_instance_t *) device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
	TRACE( class | TRC_START_END, 10, "end", "");
}



/*
*******************************************************************************************************
*    pciehci_open: called when a device special file is                                               * 
*      opened or when a block device is mounted.                                                      *
*******************************************************************************************************
*/ 
int usbehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return( EINVAL);
}




/*
*******************************************************************************************************
*    usbehci_close: called when a device special file
*      is closed by a process and no other processes
*      still have it open ("last close").
*******************************************************************************************************
*/ 
int usbehci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return 0;
}


/*
*******************************************************************************************************
*    usbehci_ioctl: a user has made an ioctl request                                                  *
*      for an open character device.                                                                  *
*      Arguments cmd and arg are as specified by the user;                                            *
*      arg is probably a pointer to something in the user's                                           *
*      address space, so you need to use copyin() to                                                  *
*      read through it and copyout() to write through it.                                             *
*******************************************************************************************************
*/ 
int usbehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
    /* user should not open this module, it should be used through usbcore module */        
    *rvalp = -1;
    return ENOTTY;          /* TeleType® is a registered trademark */
}





/*
*******************************************************************************************************
*          DATA TRANSFER ENTRY POINTS                                                                 *
*      Since I'm trying to provide an example for both                                                *
*      character and block devices, I'm routing read                                                  *
*      and write back through strategy as described in                                                *
*      the IRIX Device Driver Programming Guide.                                                      *
*      This limits our character driver to reading and                                                *
*      writing in multiples of the standard sector length.                                            *
*******************************************************************************************************
*/ 
int usbehci_read(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_write(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_strategy(struct buf *bp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
    return 0;
}

/*
*******************************************************************************************************
* Poll Entry points                                                                                   *
*******************************************************************************************************
*/
int usbehci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Memory map entry points                                                                             *
*******************************************************************************************************
*/
int usbehci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_unmap(dev_t dev, vhandl_t *vt){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Interrupt entry point                                                                               *
* We avoid using the standard name, since our prototype has changed.                                  *
*******************************************************************************************************
*/
void usbehci_dma_intr(intr_arg_t arg){
    usbehci_instance_t      *soft = (usbehci_instance_t *) arg;
    uint64_t                class = TRC_INTR | TRC_MOD_EHCI;
    uint64_t                status, port_status, port_connect, v; 
    uint32_t                portc;
    int                     i, event = 0, ehci_is_owner = 0;
    usbhub_instance_t       *usbhub; 
    hub_methods_t           *hub_methods;
    
	TRACE( class | TRC_START_END, 10, "start", "");
	
	
    soft = global_soft;
    
    TRACE( class, 12, "checking if interrupt was for us", "");
    status = EHCI_STS_INTRS(EOREAD4(soft, EHCI_USBSTS));
    if (status == 0) {
        /* the interrupt was not for us */
        return;
    }    
    

    usbhub = ( usbhub_instance_t *) soft->roothub;
    hub_methods = usbhub->device_header.methods;

    TRACE( class, 12, "ack, status=0x%08x", status);

    EOWRITE4(soft, EHCI_USBSTS, status);/* acknowledge */

    status &= soft->ps_eintrs;

    
    if (status & EHCI_STS_HSE) {
        TRACE( class, 12, "unrecoverable error, controller halted", "");
		
    }
    
	if( status & EHCI_STS_IAA){
	    TRACE( class, 12, "Got USB data from ASYNC, ready for transfers", "");
		
	}else if( status & EHCI_STS_PCD){
    
        TRACE( class, 12, "port change detected!", "");
	    for (i = 1; i <= soft->ps_noport; i++){
	        port_status = EOREAD4( soft, EHCI_PORTSC(i));
	        
	        port_connect = port_status & EHCI_PS_CLEAR;
	        /* pick out CHANGE bits from the status register */
	        if( port_connect){
                TRACE( class, 12, "activity in port %d", i);
	            if( ( port_status & EHCI_PS_CS) == 0){
	                TRACE( class, 12, "Device disconnected", "");
                    event = USB_EVENT_PORT_DISCONNECT;
                    portc = i;   
                    /* ack, clear change bits in port */
                    EOWRITE4( soft, EHCI_PORTSC(i),  EOREAD4(soft, EHCI_PORTSC(i)) | EHCI_PS_CLEAR);
                }else{
                    TRACE( class, 12, "Device connected", "");
	                portc = i;
	                if( EHCI_PS_IS_LOWSPEED(port_status)){
	                    TRACE( class, 12, "No High speed device, passing to companion controller", "");
	                    
	                    /* ack, clear change bits in port, and set companion controller as owner */
	                    v = EOREAD4( soft, EHCI_PORTSC(i)) & ~EHCI_PS_CLEAR;
	                    EOWRITE4(soft, EHCI_PORTSC(i), v | EHCI_PS_PO);
	                    
                    }else{
  		                TRACE( class, 12, "Device is High speed, resetting", "");
	                    ehci_is_owner = ehci_port_reset( soft, i);
	                    
	                    if( ehci_is_owner == 1)
                            event = USB_EVENT_PORT_CONNECT;
                        
	                    
		            }
	            }
	            
                
	        }
        }
    }	
   
    portc--;
    if( event != 0){
		/* advice to usbhub that we got activity */
		hub_methods->process_event_from_hcd( (void *) soft, (void *) usbhub, event, 
		 (void *) &portc, NULL);
	}
    
     /*
     * for each buf our hardware has processed,
     *      set buf->b_resid,
     *      call pciio_dmamap_done,
     *      call bioerror() or biodone().
     *
     * XXX - would it be better for buf->b_iodone
     * to be used to get to pciio_dmamap_done?
     */
    /*
     * may want to call pollwakeup.
     */
	TRACE( class | TRC_START_END, 10, "end", "");
     
}



/*
*******************************************************************************************************
* Error handling entry point                                                                          *
*******************************************************************************************************
*/
static int usbehci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror){

/* not used right now */


/*    usbehci_instance_t            *soft = (usbehci_instance_t *) einfo;
    vertex_hdl_t                  vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbehci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    ioerror_dump("usbehci", error_code, mode, ioerror);
*/



    return IOERROR_HANDLED;
}



/*
*******************************************************************************************************
* Support entry points                                                                                *
*******************************************************************************************************
*    usbehci_halt: called during orderly system                                                       *
*      shutdown; no other device driver call will be                                                  *
*      made after this one.                                                                           *
*******************************************************************************************************
*/
void usbehci_halt(void){
   	TRACE(  TRC_HELPER | TRC_MOD_EHCI | TRC_START_END, 2, "start", "");
	TRACE(  TRC_HELPER | TRC_MOD_EHCI | TRC_START_END, 2, "end", "");

}



/*
*******************************************************************************************************
*    usbehci_size: return the size of the device in                                                   *
*      "sector" units (multiples of NBPSCTR).                                                         *
*******************************************************************************************************
*/
int usbehci_size(dev_t dev){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
*    usbehci_print: used by the kernel to report an                                                   *
*      error detected on a block device.                                                              *
*******************************************************************************************************
*/
int usbehci_print(dev_t dev, char *str){
	/* not used */
    return 0;
}


