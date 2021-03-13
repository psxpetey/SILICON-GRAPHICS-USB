/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbuhci.c                                                  *
* Description: UHCI host controller driver                         *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      14-02-2013     -Added UHCI priority queues Fixed pipe_t structure                       *
*                            -Added PIPE creation and basic functionality for USB CMD SetAddress      *
*                            -Added Queue head priorities lists                                       *
*                                                                                                     *
* BSDero      08-03-2012     -Fixed wrong class in trace messages                                     *
*                            -Added _diaginfo_ string in startup trace messages                       *
*                                                                                                     *
* BSDero      07-23-2012     -Usage usbhub mutex instead usbcore mutex                                *
*                            -Added advice to usbhub in port connect/disconnect                       *
*                            -Added log of kernel module address                                      *
*                                                                                                     *
* BSDero      07-12-2012     -Updated uhci_get_info(), uhci_info_get(), uhci_get_port()               *
*                            -Implemented uhci_port_action(), uhci_set_roothub()                      *
*                            -Deleted register_hcd_driver() and unregister_hcd_driver()               *
*                                                                                                     *
* BSDero      07-10-2012     -Added uhci_process_event() and uhci_process_event_from_usbcore()        *
*                                                                                                     *
* BSDero      07-05-2012     -Send events to hub implemented                                          *
*                                                                                                     *
* BSDero      06-26-2012     -Fixed issues in port reset, added mutex                                 *
*                                                                                                     *
* BSDero      06-21-2012     -Updated uhci_hub_info()                                                 *
*                                                                                                     *
* BSDero      06-15-2012     -Implemented get information from PCI bus. Added uhci_restart(),         *
*                            uhci_port_reset.                                                         *
*                            -Added port polling function                                             *
*                            -Fixed crashes by using a mutex in usbcore module during attach          *
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

#include "uhcireg.h"                                /* uhci register defines */
/*
*******************************************************************************************************
*   Supported UHCI devices                                                                            *
*******************************************************************************************************
*/

static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}uhci_descriptions[]={
    0x26888086, "Intel 631XESB/632XESB/3100 USB controller USB-1",
    0x26898086,"Intel 631XESB/632XESB/3100 USB controller USB-2",
    0x268a8086,"Intel 631XESB/632XESB/3100 USB controller USB-3",
    0x268b8086,"Intel 631XESB/632XESB/3100 USB controller USB-4",
    0x70208086,"Intel 82371SB (PIIX3) USB controller",
    0x71128086,"Intel 82371AB/EB (PIIX4) USB controller",
    0x24128086,"Intel 82801AA (ICH) USB controller",
    0x24228086,"Intel 82801AB (ICH0) USB controller",
    0x24428086,"Intel 82801BA/BAM (ICH2) USB controller USB-A",
    0x24448086,"Intel 82801BA/BAM (ICH2) USB controller USB-B",
    0x24828086,"Intel 82801CA/CAM (ICH3) USB controller USB-A",
    0x24848086,"Intel 82801CA/CAM (ICH3) USB controller USB-B",
    0x24878086,"Intel 82801CA/CAM (ICH3) USB controller USB-C",
    0x24c28086,"Intel 82801DB (ICH4) USB controller USB-A",
    0x24c48086,"Intel 82801DB (ICH4) USB controller USB-B",
    0x24c78086,"Intel 82801DB (ICH4) USB controller USB-C",
    0x24d28086,"Intel 82801EB (ICH5) USB controller USB-A",
    0x24d48086,"Intel 82801EB (ICH5) USB controller USB-B",
    0x24d78086,"Intel 82801EB (ICH5) USB controller USB-C",
    0x24de8086,"Intel 82801EB (ICH5) USB controller USB-D",
    0x26588086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-A",
    0x26598086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-B",
    0x265a8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-C",
    0x265b8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-D",
    0x27c88086,"Intel 82801G (ICH7) USB controller USB-A",
    0x27c98086,"Intel 82801G (ICH7) USB controller USB-B",
    0x27ca8086,"Intel 82801G (ICH7) USB controller USB-C",
    0x27cb8086,"Intel 82801G (ICH7) USB controller USB-D",
    0x28308086,"Intel 82801H (ICH8) USB controller USB-A",
    0x28318086,"Intel 82801H (ICH8) USB controller USB-B",
    0x28328086,"Intel 82801H (ICH8) USB controller USB-C",
    0x28348086,"Intel 82801H (ICH8) USB controller USB-D",
    0x28358086,"Intel 82801H (ICH8) USB controller USB-E",
    0x29348086,"Intel 82801I (ICH9) USB controller",
    0x29358086,"Intel 82801I (ICH9) USB controller",
    0x29368086,"Intel 82801I (ICH9) USB controller",
    0x29378086,"Intel 82801I (ICH9) USB controller",
    0x29388086,"Intel 82801I (ICH9) USB controller",
    0x29398086,"Intel 82801I (ICH9) USB controller",
    0x3a348086,"Intel 82801JI (ICH10) USB controller USB-A",
    0x3a358086,"Intel 82801JI (ICH10) USB controller USB-B",
    0x3a368086,"Intel 82801JI (ICH10) USB controller USB-C",
    0x3a378086,"Intel 82801JI (ICH10) USB controller USB-D",
    0x3a388086,"Intel 82801JI (ICH10) USB controller USB-E",
    0x3a398086,"Intel 82801JI (ICH10) USB controller USB-F",
    0x719a8086,"Intel 82443MX USB controller",
    0x76028086,"Intel 82372FB/82468GX USB controller",
    0x30381106,"VIA VT6212 UHCI USB 1.0 controller",
    0 ,NULL
};



/****************************************************************
 * uhci structs and flags
 ****************************************************************/
#define UHCI_CF_VENDOR_ID                           0x00
#define UHCI_CF_DEVICE_ID                           0x02
#define UHCI_CF_COMMAND                             0x04
#define UHCI_CF_STATUS                              0x06
#define UHCI_CF_REVISION_ID                         0x08
#define UHCI_CF_CLASS_CODE                          0x09
#define UHCI_CF_CACHE_LINE_SIZE                     0x0c
#define UHCI_CF_LATENCY_TIME                        0x0d
#define UHCI_CF_HEADER_TYPE                         0x0e
#define UHCI_CF_BIST                                0x0f
#define UHCI_CF_MMAP_IO_BASE_ADDR                   0x10
#define UHCI_CF_CIS_BASE_ADDR                       0x14
#define UHCI_CF_BASE_ADDR							0x20
#define UHCI_CF_CARDBUS_CIS_PTR                     0x28
#define UHCI_CF_SSID                                0x2c
#define UHCI_CF_PWR_MGMT_CAPS                       0x34
#define UHCI_CF_INTERRUPT_LINE                      0x3c
#define UHCI_CF_INTERRUPT_PIN                       0x3d
#define UHCI_NUM_CONF_REGISTERS                     0xc2
#define UHCI_NUM_IO_REGISTERS                       0x14
#define UHCICMD(sc, cmd)                            UWRITE2(sc, UHCI_CMD, cmd)
#define	UHCISTS(sc)                                 UREAD2(sc, UHCI_STS)

/*
*******************************************************************************************************
* Specific UHCI Data structures                                                                       *
*******************************************************************************************************
*/


#define TD_CTRL_SPD                                 (0x20000000)       /* Short Packet Detect */
#define TD_CTRL_C_ERR_MASK                          (0x18000000)       /* Error Counter bits */
#define TD_CTRL_C_ERR_SHIFT                         27
#define TD_CTRL_LS                                  (0x04000000)       /* Low Speed Device */
#define TD_CTRL_IOS                                 (0x02000000)       /* Isochronous Select */
#define TD_CTRL_IOC                                 (0x01000000)       /* Interrupt on Complete */
#define TD_CTRL_ACTIVE                              (0x00800000)       /* TD Active */
#define TD_CTRL_STALLED                             (0x00400000)       /* TD Stalled */
#define TD_CTRL_DBUFERR                             (0x00200000)       /* Data Buffer Error */
#define TD_CTRL_BABBLE                              (0x00100000)       /* Babble Detected */
#define TD_CTRL_NAK                                 (0x00080000)       /* NAK Received */
#define TD_CTRL_CRCTIMEO                            (0x00040000)       /* CRC/Time Out Error */
#define TD_CTRL_BITSTUFF                            (0x00020000)       /* Bit Stuff Error */
#define TD_CTRL_ACTLEN_MASK                         0x7FF   /* actual length, encoded as n - 1 */

#define TD_CTRL_ANY_ERROR                           (TD_CTRL_STALLED | TD_CTRL_DBUFERR | \
                                                     TD_CTRL_BABBLE | TD_CTRL_CRCTIMEO | \
                                                     TD_CTRL_BITSTUFF)
#define uhci_maxerr(err)                            ((err) << TD_CTRL_C_ERR_SHIFT)

#define TD_TOKEN_DEVADDR_SHIFT                      8
#define TD_TOKEN_TOGGLE_SHIFT                       19
#define TD_TOKEN_TOGGLE                             (1 << 19)
#define TD_TOKEN_EXPLEN_SHIFT                       21
#define TD_TOKEN_EXPLEN_MASK                        0x7FF   /* expected length, encoded as n-1 */
#define TD_TOKEN_PID_MASK                           0xFF

#define uhci_explen(len)                            ((((len) - 1) & TD_TOKEN_EXPLEN_MASK) << \
                                                    TD_TOKEN_EXPLEN_SHIFT)

#define uhci_expected_length(token)                 ((((token) >> TD_TOKEN_EXPLEN_SHIFT) + \
                                                    1) & TD_TOKEN_EXPLEN_MASK)

typedef struct{
    uint32_t links[1024];
} uhci_framelist_t;

typedef struct{
    uint32_t link;   
    uint32_t status; //8
    uint32_t token;
    uint32_t buffer; //16
    uint32_t r0, r1, r2, r3, r4; //36 bytes
    uint32_t this;//40
    uint32_t tags, flags; //48
    void     *klink;
    void     *kbuffer;
}uhci_td_t; /* size: 64 bytes */

typedef struct{
    uint32_t link;
    uint32_t element;
    /* kernel address */
    uint32_t r0, r1;//16
    uint32_t this;   
    uint32_t prior_link; //24
    
    void     *kprior_link; //32
    void     *klink;       //40
    void     *kelement;    //48
    
}uhci_qh_t; /* size: 48 bytes */

struct usb_uhci_s {
    struct usb_s usb;
    uint16_t iobase;
    uhci_qh_t *control_qh, *bulk_qh;
    uhci_framelist_t *framelist;
};
struct usb_uhci_s usb_uhci_t;

struct uhci_pipe {
    uhci_qh_t qh;
    uhci_td_t *next_td;
    struct usb_pipe pipe;
    uint16_t iobase;
    uchar_t toggle;
};
struct uhci_pipe uhci_pipe_t;


typedef struct {
    uhci_qh_t qh;
    uhci_td_t td[6];
    uchar_t   buffer[512]; 
}uhci_control_chain_t;

#define UHCI_PTR_BITS           0x000F
#define UHCI_PTR_TERM           0x0001
#define UHCI_PTR_QH             0x0002
#define UHCI_PTR_DEPTH          0x0004
#define UHCI_PTR_BREADTH        0x0000


/*
*******************************************************************************************************
* UHCI transfer queues                                                                                *
*******************************************************************************************************
*/
typedef struct{
#define UHCI_QH1                                    0
#define UHCI_QH2                                    1
#define UHCI_QH4                                    2
#define UHCI_QH8                                    3
#define UHCI_QH16                                   4
#define UHCI_QH32                                   5
#define UHCI_QH64                                   6
#define UHCI_QH128                                  7
#define UHCI_QHCTL                                  UHCI_QH128

#define UHCI_QH1S                                   8
#define UHCI_QH2S                                   9
#define UHCI_QH4S                                   10
#define UHCI_QH8S                                   11
#define UHCI_QH16S                                  12
#define UHCI_QH32S                                  13
#define UHCI_QH64S                                  14
#define UHCI_QH128S                                 15
#define UHCI_QHCTLS                                 UHCI_QH128S

#define UHCI_QH1E                                   16
#define UHCI_QH2E                                   17
#define UHCI_QH4E                                   18
#define UHCI_QH8E                                   19
#define UHCI_QH16E                                  20
#define UHCI_QH32E                                  21
#define UHCI_QH64E                                  22
#define UHCI_QH128E                                 23
#define UHCI_QHCTLE                                 UHCI_QH128E

#define UHCI_QHNUM									8
    uhci_qh_t               queue_heads[UHCI_QHNUM*3]; /* we have 8 queues */
    uhci_qh_t               term_qh;
    uhci_td_t               term_td;
}uhci_transfer_queues_t;
	

/*
*******************************************************************************************************
* UHCI instance                                                                                       *
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
    uint32_t               sc_offs;
    pciio_piomap_t         ps_cmap;        /* piomap (if any) for ps_cfg */
    pciio_piomap_t         ps_rmap;        /* piomap (if any) for ps_regs */
    unsigned               ps_sst;         /* driver "software state" */
#define usbuhci_SST_RX_READY      (0x0001)
#define usbuhci_SST_TX_READY      (0x0002)
#define usbuhci_SST_ERROR         (0x0004)
#define usbuhci_SST_INUSE         (0x8000)
    pciio_intr_t            ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t          ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t          ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead         *ps_pollhead;  /* for poll() */
    int                     ps_blocks;  /* block dev size in NBPSCTR blocks */
    uint32_t                ps_event;
    uint8_t                 ps_noport;
    uint32_t                ps_eintrs;
    toid_t                  ps_itimeout;
    uchar_t                 ps_stopped_timeout;
    pciio_info_t            ps_pciio_info_device;
    usbcore_instance_t      *usbcore;
    usbhub_instance_t       *roothub;    
    uhci_framelist_t        *framelist;
    uhci_transfer_queues_t  *tq;
    uhci_qh_t               *control_qh;
    uhci_qh_t               *bulk_qh;
    int                     debug_port;
    uint32_t                debug_port_reset_delay;
    uint32_t                debug_port_reset_recovery_delay;
    uint32_t                debug_port_root_reset_delay;
    
}usbuhci_instance_t;

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/
int                                                 usbuhci_devflag = D_MP;
char                                                *usbuhci_mversion = M_VERSION;  /* for loadable modules */
int                                                 usbuhci_inuse = 0;     /* number of "usbuhci" devices open */
gc_list_t                                           gc_list;
usbuhci_instance_t                                  *global_soft = NULL; 
dma_list_t                                          dma_list; 
 
/*
*******************************************************************************************************
* Helper functions for usbcore                                                                        *
*******************************************************************************************************
*/
void uhci_ports_poll( usbuhci_instance_t *soft);
usbcore_instance_t *get_usbcore();

 
/*
*******************************************************************************************************
* Callback functions for usbcore                                                                      *
*******************************************************************************************************
*/
int uhci_reset( void *hcd);
int uhci_start( void *hcd);
int uhci_init( void *hcd);
int uhci_stop( void *hcd);
int uhci_shutdown( void *hcd);
int uhci_suspend( void *hcd);
int uhci_resume( void *hcd);
int uhci_status( void *hcd);
void uhci_free_pipe( void *hcd, struct usb_pipe *pipe);
usb_pipe_t *uhci_alloc_control_pipe( void *hcd, int port_num, td_addr_t *td_addr);
struct usb_pipe *uhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int uhci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int uhci_set_address( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int uhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize);
int uhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int uhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data);
int uhci_ioctl( void *hcd, int cmd, void *uarg);
int uhci_set_trace_level( void *hcd, void *trace_level);
int uhci_hub_info( void *hcd, void *info);
int uhci_port_action( void *hcd, int port, int action);
uint32_t uhci_get_port( void *hcd, int port);
int uhci_set_roothub(void *hcd, void *roothub);
int uhci_set_debug_values( void *hcd, void *pv);

/*
*******************************************************************************************************
* Methods for usbcore                                                                                 *
*******************************************************************************************************
*/
hcd_methods_t uhci_methods={
	uhci_reset,
	uhci_start,
	uhci_init, 
	uhci_stop,
	uhci_shutdown,
	uhci_suspend,
	uhci_resume,
	uhci_status,
	uhci_free_pipe,
	uhci_alloc_control_pipe, 
	uhci_alloc_bulk_pipe, 
	uhci_send_control, 
	uhci_set_address, 
	uhci_usb_send_bulk,
	uhci_alloc_intr_pipe,
	uhci_usb_poll_intr,
	uhci_ioctl,
	uhci_set_trace_level,
	uhci_hub_info,
	uhci_port_action,
	uhci_get_port,
	uhci_set_roothub,
	uhci_set_debug_values,
	0x00000000
};


int uhci_process_event( void *origin, void *dest, int event_id, void *arg);
int uhci_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2);

module_header_t uhci_header={
	USB_MOD_UHCI, 
	"usbuhci", 
	"USB Host Controller Interface (USB 1.0)", 
	"usbuhci.o", 
	USB_DRIVER_IS_HCD, 
	{
		uhci_set_trace_level,
	    uhci_process_event,
	    uhci_process_event_from_usbcore,
	    0x0000	
	}
};


/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
void                  usbuhci_init(void);
int                   usbuhci_unload(void);
int                   usbuhci_reg(void);
int                   usbuhci_unreg(void);
int                   usbuhci_attach(vertex_hdl_t conn);
int                   usbuhci_detach(vertex_hdl_t conn);
static pciio_iter_f   usbuhci_reloadme;
static pciio_iter_f   usbuhci_unloadme;
int                   usbuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                   usbuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                   usbuhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                   usbuhci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbuhci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbuhci_strategy(struct buf *bp);
int                   usbuhci_poll(dev_t dev, short events, int anyyet, 
                               short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                   usbuhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                   usbuhci_unmap(dev_t dev, vhandl_t *vt);
void                  usbuhci_dma_intr(intr_arg_t arg);
static error_handler_f usbuhci_error_handler;
void                  usbuhci_halt(void);
int                   usbuhci_size(dev_t dev);
int                   usbuhci_print(dev_t dev, char *str);


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
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	
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


uchar_t uhci_restart( usbuhci_instance_t *soft){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    int rc;

    TRACE( class | TRC_START_END, 10, "start", "");
  	if ( UREAD2( soft, UHCI_CMD) & UHCI_CMD_RS) {
		TRACE( class, 10, "Already started", "");
		rc = 0;
		goto uhci_restart_exit;
	}

    TRACE( class | TRC_START_END, 10, "restarting", "");


	/* Reload fresh base address */
/*	UWRITE4(sc, UHCI_FLBASEADDR, buf_res.physaddr);*/

	/*
	 * Assume 64 byte packets at frame end and start HC controller:
	 */
	UHCICMD(soft, (UHCI_CMD_MAXP | UHCI_CMD_RS));

	/* wait 10 milliseconds */

	USECDELAY( 10000);

	/* check that controller has started */

	if (UREAD2(soft, UHCI_STS) & UHCI_STS_HCH) {
		TRACE( class, 10, "failed", "");
		rc = 1;
		goto uhci_restart_exit;
	}
     
    rc = 0; 
uhci_restart_exit:
    TRACE( class | TRC_START_END, 10, "end", "");
	return (rc);
}

int uhci_port_reset(usbuhci_instance_t *soft, unsigned int index){
	unsigned int port, x, lim, port_mapped;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    int rc;

    TRACE( class | TRC_START_END, 10, "start", "");
		
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);
    port_mapped = soft->device_header.spec_device_instance_id * 2 + (index % 2);
    TRACE( class, 12, "port mapped=%d", port_mapped);
    TRACE( class, 10, "soft=%x, uhciport=%d", soft, index);
     
	if (index == 1)
		port = UHCI_PORTSC1;
	else if (index == 2)
		port = UHCI_PORTSC2;
	else
		return ( 1);

    
	/*
	 * Before we do anything, turn on SOF messages on the USB
	 * BUS. Some USB devices do not cope without them!
	 */



	x = URWMASK(UREAD2(soft, port));
	UWRITE2(soft, port, x | UHCI_PORTSC_PR);
    USECDELAY( soft->debug_port_root_reset_delay);


	x = URWMASK(UREAD2(soft, port));
	UWRITE2(soft, port, x & ~UHCI_PORTSC_PR);

	/* 
	 * This delay needs to be exactly 100us, else some USB devices
	 * fail to attach!
	 */
    USECDELAY( soft->debug_port_reset_recovery_delay);


	x = URWMASK(UREAD2(soft, port));
/*	UWRITE2(soft, port, x & ~UHCI_PORTSC_CSC & ~UHCI_PORTSC_POEDC);*/
	UWRITE2(soft, port, x | UHCI_PORTSC_POEDC | UHCI_PORTSC_CSC);

	for (lim = 0; lim < 500; lim++) {
        x = UREAD2(soft, port);

		TRACE( class, 4, "uhci port %d iteration %u, status = 0x%04x",
		    index, lim, x);

        TRACE( class, 12, "port mapped=%d", port_mapped);
        TRACE( class, 10, "soft=%x, uhciport=%d", soft, index);

		if (!(x & UHCI_PORTSC_CCS)) {
			/*
			 * No device is connected (or was disconnected
			 * during reset).  Consider the port reset.
			 * The delay must be long enough to ensure on
			 * the initial iteration that the device
			 * connection will have been registered.  50ms
			 * appears to be sufficient, but 20ms is not.
			 */
			TRACE( class, 4, "uhci port %d loop %u, device detached",
			    index, lim);
			rc = 0;
			goto uhci_portreset_done;
		}
		if (x & (UHCI_PORTSC_POEDC | UHCI_PORTSC_CSC)) {
			/*
			 * Port enabled changed and/or connection
			 * status changed were set.  Reset either or
			 * both raised flags (by writing a 1 to that
			 * bit), and wait again for state to settle.
			 */
			UWRITE2(soft, port, URWMASK(x) |
			    (x & (UHCI_PORTSC_POEDC | UHCI_PORTSC_CSC)));
			TRACE( class, 4, "CSC still enabled, continue",
			    index, lim);
			USECDELAY(10);    
			continue;
		}
		if (x & UHCI_PORTSC_PE) {
			/* port is enabled */
			TRACE( class, 2, "port enabled", "");
			rc = 0;
			goto uhci_portreset_done;
		}
		
		UWRITE2(soft, port, URWMASK(x) | UHCI_PORTSC_PE);
        USECDELAY( soft->debug_port_reset_delay);
		
	}

	TRACE( class, 4, "uhci port %d reset timed out", index);
	rc = 1;

uhci_portreset_done:

    
    TRACE( class | TRC_START_END, 10, "end", "");
	return (rc);
}

 
void uhci_ports_poll( usbuhci_instance_t *soft){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    uint16_t reg;
    usbcore_instance_t *usbcore = soft->usbcore;
    usbhub_instance_t *roothub = soft->roothub;
    hub_methods_t           *hub_methods;
    uint32_t port_num; 
    int event = 0, rc = 0;
    
    if( soft == NULL) 
        return;
        
    if( soft->ps_itimeout == 0){
		soft->ps_stopped_timeout = 0;
        return;
    }


	MUTEX_LOCK( &roothub->uhci_mutex, -1);

    reg = UREAD2( soft, UHCI_PORTSC1);
    if( reg & UHCI_PORTSC_CSC){
        
		port_num = soft->device_header.spec_device_instance_id * 2 + 1;
		TRACE( class, 10, "portnum %d changed", port_num);
        
        UWRITE2( soft, UHCI_PORTSC1, reg);

        if( reg & UHCI_PORTSC_CCS){
            TRACE( class, 10, "device connected", "");
            event = USB_EVENT_PORT_CONNECT;
            uhci_port_reset( soft, 1); 
            
        }else{
            TRACE( class, 10, "device disconnected", "");
            event = USB_EVENT_PORT_DISCONNECT;
        }
        
        
    }    
    
    reg = UREAD2( soft, UHCI_PORTSC2);
    if( reg & UHCI_PORTSC_CSC){
		
		port_num = soft->device_header.spec_device_instance_id * 2;
		TRACE( class, 10, "portnum %d changed", port_num);

        UWRITE2( soft, UHCI_PORTSC2, reg);

        if( reg & UHCI_PORTSC_CCS){
            TRACE( class, 10, "device connected", "");
            event = USB_EVENT_PORT_CONNECT;
            uhci_port_reset( soft, 2);             
        }else{
            TRACE( class, 10, "device disconnected", "");
            event = USB_EVENT_PORT_DISCONNECT;
        }
        
    }    
   
	MUTEX_UNLOCK( &roothub->uhci_mutex);


    hub_methods = roothub->device_header.methods;

    if( event != 0){
		TRACE( class, 10, "soft=%x, port=%d", soft, port_num);
		hub_methods->process_event_from_hcd( (void *) soft, (void *) roothub, event, 
		 (void *) &port_num, NULL);
	}
/*
    if( soft->ps_itimeout == 0){
		soft->ps_stopped_timeout = 0;
        return;
    }else{
		*/
        soft->ps_itimeout = itimeout(uhci_ports_poll, soft, drv_usectohz(100000), 0);	
    /*}*/
        
}


 
/*
*******************************************************************************************************
* Host controller callback implementation for USBcore                                                 *
*******************************************************************************************************
*/
int uhci_reset( void *hcd){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    int rc = 0;
    int n;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);
	
	/* disable interrupts */
	UWRITE2(soft, UHCI_INTR, 0);

	/* global reset */
	UHCICMD(soft, UHCI_CMD_GRESET);

	/* wait */
	USECDELAY( 10000);

	/* terminate all transfers */
	UHCICMD(soft, UHCI_CMD_HCRESET);

	/* the reset bit goes low when the controller is done */
	n = 100;
	while (n--) {
		/* wait one millisecond */
		USECDELAY( 1000);

		if (!(UREAD2(soft, UHCI_CMD) & UHCI_CMD_HCRESET)) {
			rc = 0;
			goto done_1;
		}
	}
    rc = 1;
	TRACE( class, 8, "controller did not reset", "");

done_1:
 
    TRACE( class | TRC_START_END, 10, "end, rc = %d", rc);
	return( rc);
}


int uhci_start( void *hcd){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    int rc;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);

    soft->ps_stopped_timeout = 1;
    soft->ps_itimeout = itimeout(uhci_ports_poll, soft, drv_usectohz(200000), 0);
    TRACE( class, 12, "ps_itimeout = %d", soft->ps_itimeout);
	
    TRACE( class | TRC_START_END, 10, "end", "");
	return( rc);
}

int uhci_init( void *hcd){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uhci_transfer_queues_t  *tq;    
    int                     rc, i;
    uint64_t                class = TRC_HELPER | TRC_MOD_UHCI;
    dma_node_t              *dma_node_tq;
    dma_node_t              *dma_node_fl;
    uhci_framelist_t        *fl;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);

	rc = uhci_reset( hcd);

    dma_list_init( &dma_list);
    
    /* dma alloc of transfer queues */
    dma_node_tq = dma_alloc( soft->ps_conn, &dma_list, sizeof(uhci_transfer_queues_t));
        
    /* dma alloc of framelist */
    dma_node_fl = dma_alloc( soft->ps_conn, &dma_list, sizeof(uhci_framelist_t));
    
    if( dma_node_tq == NULL || dma_node_fl == NULL){
		TRACE( class, 4, "Error in dma_alloc()!!!", "");
		return( -1);
	}
    
    TRACE( class, 10, "dma reserved", "");
    fl = ( uhci_framelist_t *) dma_node_fl->mem;
    tq = ( uhci_transfer_queues_t *) dma_node_tq->mem;
    
    memset( tq, 0, sizeof( uhci_transfer_queues_t ));
    memset( fl, 0, sizeof( uhci_framelist_t )); 
    /* Work around for PIIX errata */
    
    /* create the next terminator TD and QH 
     * term_td --> END OF QUEUE
     * term_qh --> term_td
     * At end we get a structure like this: term_qh->term_td->EOQ (END OF QUEUE)
     */ 
    tq->term_td.link = UHCI_PTR_TERM;
    tq->term_td.token = ( uhci_explen(0) | (0x7f << TD_TOKEN_DEVADDR_SHIFT) | USB_PID_IN);
    tq->term_td.klink = (void *) UHCI_PTR_TERM;

    tq->term_qh.link = UHCI_PTR_TERM;
    tq->term_qh.element = kaddr_2_daddr( dma_node_tq->daddr, dma_node_tq->kaddr, &tq->term_td);
    tq->term_qh.klink = (void *) UHCI_PTR_TERM;
    tq->term_qh.kprior_link = (void *) UHCI_PTR_TERM;
    tq->term_qh.this = kaddr_2_daddr( dma_node_tq->daddr, dma_node_tq->kaddr, &tq->term_qh);
    


    
    /* Build schedule queues 
     * 
     * make the first queue point to the term QH
     * queue_heads[UHCI_QH1].link->term_qh.link->EOQ
     * but the element field should point to our queue. 
     * A start and an end queue heads are added in each queue.
     * RQUEUE ---> start queue ---> end queue --> term.qh
     * 
     * */
    tq->queue_heads[UHCI_QH1].link = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->term_qh) | UHCI_PTR_QH;
    tq->queue_heads[UHCI_QH1].element = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->queue_heads[UHCI_QH1S]) | UHCI_PTR_QH;
    tq->queue_heads[UHCI_QH1].klink = (void *) &tq->term_qh;
    tq->queue_heads[UHCI_QH1].kelement = (void *) &tq->queue_heads[UHCI_QH1S];

    /* setup start qeueu head */    
    tq->queue_heads[UHCI_QH1S].link = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->queue_heads[UHCI_QH1E]) | UHCI_PTR_QH;
    tq->queue_heads[UHCI_QH1S].element = UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1S].klink = (void *) &tq->queue_heads[UHCI_QH1E];
    tq->queue_heads[UHCI_QH1S].kelement = (void *) UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1S].prior_link = UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1S].kprior_link = (void *) NULL;
    tq->queue_heads[UHCI_QH1S].this = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->queue_heads[UHCI_QH1S]) ;
    
    /* setup start end head */
    tq->queue_heads[UHCI_QH1E].link = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->term_qh) | UHCI_PTR_QH;
    tq->queue_heads[UHCI_QH1E].element = UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1E].klink = (void *) &tq->term_qh;
    tq->queue_heads[UHCI_QH1E].kelement = (void *) UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1E].prior_link = tq->queue_heads[UHCI_QH1S].this;
    tq->queue_heads[UHCI_QH1E].kprior_link = (void *) &tq->queue_heads[UHCI_QH1S];
    tq->queue_heads[UHCI_QH1E].this = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->queue_heads[UHCI_QH1E]) ;

    /* Create the next queue heads this way: each one should be pointing to the previous one
     * creating a pool of queues like this: 
     * queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     * queue_heads[UHCI_QH4]->queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     * queue_heads[UHCI_QH8]->queue_heads[UHCI_QH4]->queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     * ...
     * ...
     * until we got the last queue
     * queue_heads[UHCI_128]->queue_heads[UHCI_QH64]->....queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     */ 
    for( i = 1; i < UHCI_QHNUM; i++){
        tq->queue_heads[i].link = kaddr_2_daddr( dma_node_tq->daddr, 
            dma_node_tq->kaddr, &tq->queue_heads[i-1]) | UHCI_PTR_QH;
        tq->queue_heads[i].element = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->queue_heads[i+8]) | UHCI_PTR_QH;
        tq->queue_heads[i].klink = (void *) &tq->queue_heads[i-1];
        tq->queue_heads[i].kelement = (void *) &tq->queue_heads[i+8];
        

        tq->queue_heads[i+8].link = kaddr_2_daddr( dma_node_tq->daddr, 
            dma_node_tq->kaddr, &tq->queue_heads[i+16]) | UHCI_PTR_QH;
        tq->queue_heads[i+8].element = UHCI_PTR_TERM;
        tq->queue_heads[i+8].klink = (void *) &tq->queue_heads[i+16];
        tq->queue_heads[i+8].kelement = (void *)  UHCI_PTR_TERM;
        tq->queue_heads[i+8].prior_link = UHCI_PTR_TERM;
        tq->queue_heads[i+8].kprior_link = (void *) NULL;
        tq->queue_heads[i+8].this = kaddr_2_daddr( dma_node_tq->daddr, 
            dma_node_tq->kaddr, &tq->queue_heads[i+8]) ;

        tq->queue_heads[i+16].link = kaddr_2_daddr( dma_node_tq->daddr, 
            dma_node_tq->kaddr, &tq->term_qh) | UHCI_PTR_QH;
        tq->queue_heads[i+16].element = UHCI_PTR_TERM;
        tq->queue_heads[i+16].klink = (void *) &tq->term_qh;
        tq->queue_heads[i+16].kelement = (void *)  UHCI_PTR_TERM;
        tq->queue_heads[i+16].prior_link = tq->queue_heads[i+8].this;
        tq->queue_heads[i+16].kprior_link = (void *) &tq->queue_heads[i+8].this;
        tq->queue_heads[i+16].this = kaddr_2_daddr( dma_node_tq->daddr, 
            dma_node_tq->kaddr, &tq->queue_heads[i+16]);
    }
    
    
    /* Now assign pointers for each queue in list, this should be completed this way: 
     * 
     * links[0]->queue_heads[UHCI_QH1]
     * links[1]->queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     * links[3]->queue_heads[UHCI_QH1]
     * links[4]->queue_heads[UHCI_QH4]->queue_heads[UHCI_QH2]->queue_heads[UHCI_QH1]
     * 
     * thus, A structure like the next one should be created:
     * links pointer num      queue head num
     * 0                      1
     * 1                      2->1
     * 2                      1
     * 3                      4->2->1
     * 4                      1
     * 5                      2->1
     * 6                      1
     * 7                      8->4->2->1
     * 8                      1
     * ...
     * 15                     16->8->4->2->1
     * ...
     * ...
     * 127                    128->64->32->16->8->4->2->1
     * 
     * The UHCI_QH1 queue should be executed every microframe
     * The UHCI_QH2 queue should be executed every two microframes
     * The UHCI_QH4 queue should be executed every four microframes, and so on
     * 
     * The frame list has 1024 entries, so the entire queue is filled following the same
     * pattern.
     * 
     * QH1 should be executing in each microframe and QH128 each 128 microframes 
     */
    for ( i = 0; i < 1024; i++){
        fl->links[i] = (uint32_t) kaddr_2_daddr( dma_node_tq->daddr, 
          dma_node_tq->kaddr, &tq->queue_heads[0]) | UHCI_PTR_QH;
        
        if( (i % 2) == 1){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[1]) | UHCI_PTR_QH;			
	    }
        
        if( (i % 4) == 3){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[2]) | UHCI_PTR_QH;			
	    }

        if( (i % 8) == 7){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[3]) | UHCI_PTR_QH;			
	    }
	    
        if( (i % 16) == 15){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[4]) | UHCI_PTR_QH;			
	    }
        if( (i % 32) == 31){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[5]) | UHCI_PTR_QH;			
	    }
        if( (i % 64) == 63){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[6]) | UHCI_PTR_QH;			
	    }
        if( (i % 128) == 127){
            fl->links[i] = kaddr_2_daddr( dma_node_tq->daddr, 
              dma_node_tq->kaddr, &tq->queue_heads[7]) | UHCI_PTR_QH;			
	    }
    } 
    soft->framelist = fl;
    soft->tq = tq;
    

    // Set the frame length to the default: 1 ms exactly
    UWRITE1( soft, UHCI_SOF, 0x40);

    // Store the frame list base address
    UWRITE4( soft, UHCI_FLBASEADDR, (uint32_t ) dma_node_fl->daddr);

    // Set the current frame number
    UWRITE2( soft, UHCI_FRNUM, 0);
    

    // Mark as configured and running with a 64-byte max packet.
    UHCICMD(soft, (UHCI_CMD_MAXP | UHCI_CMD_RS | UHCI_CMD_CF));
    
	
    TRACE( class | TRC_START_END, 10, "end", "");
	return( rc);
}

int uhci_stop( void *hcd){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    int rc;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    rc = uhci_reset( hcd);
    UHCICMD(soft, 0);

    TRACE( class | TRC_START_END, 10, "end", "");
	return( rc);
}

int uhci_shutdown( void *hcd){
}

int uhci_suspend( void *hcd){
}

int uhci_resume( void *hcd){
}

int uhci_status( void *hcd){ /* just a demo */
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    uint16_t status;
    int rc; 
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);
    
    
    status = (UREAD2(soft, UHCI_STS) & ( UHCI_STS_HSE|UHCI_STS_HCPE));
    if( status == 0){
		rc = 0;
    }else{
		rc = 1;
	}
    TRACE( class | TRC_START_END, 10, "end", "");
    return( rc);
}

void uhci_free_pipe( void *hcd, struct usb_pipe *pipe){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);
    
    
    if( pipe->type == USB_PIPE_CONTROL){
        dma_drain( pipe->dma_node);
        dma_done( pipe->dma_node);
        dma_free( pipe->dma_node);
    }
    
    gc_mark( pipe);

    TRACE( class | TRC_START_END, 10, "end", "");
}

int uhci_insert_queue( dma_node_t *q, usbuhci_instance_t *soft, int numtq){
    uhci_transfer_queues_t  *tq = soft->tq; 
    uhci_control_chain_t *chain = (uhci_control_chain_t *) q->mem;
    uhci_qh_t *start = &tq->queue_heads[numtq+8];
    uhci_qh_t *end = &tq->queue_heads[numtq+8];
    
    chain->qh.link = start->link | UHCI_PTR_QH;
    chain->qh.klink = start->klink;
    chain->qh.prior_link = start->this;
    chain->qh.kprior_link = (void *) start;
    
    start->link = chain->qh.this| UHCI_PTR_QH;
    start->klink = (void *) &chain->qh;


    return(0);
}


usb_pipe_t *uhci_alloc_control_pipe( void *hcd, int port_num, td_addr_t *td_addr){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    usb_pipe_t *pipe;
    int mem_size, i, tdnum;
    dma_node_t *dma_chunk;
    uhci_control_chain_t *chain;

    
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    
    
    TRACE( class | TRC_START_END, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", soft->device_header.module_header->short_description);
    
    /* alloc a new pipe */
    pipe = (usb_pipe_t *) gc_malloc( &gc_list, sizeof( usb_pipe_t));
    bzero( ( void *) pipe, sizeof( usb_pipe_t));
  
    mem_size = sizeof( uhci_control_chain_t);

    dma_chunk = dma_alloc( soft->ps_conn, &dma_list, mem_size);
    
    /* fill in pipe structure */
    pipe->dma_node = (void *) dma_chunk;
    pipe->qh = (void *) dma_chunk->mem;
    pipe->single_dma_segment = 1;
    pipe->hcd = hcd;
    pipe->num_tds = 0;
    pipe->port_num = port_num;
    pipe->type = USB_PIPE_CONTROL;
    pipe->devaddr = 0;

    /* Build queue */
    chain = ( uhci_control_chain_t *) dma_chunk->mem;

    chain->qh.this = dma_chunk->daddr;
    chain->qh.link = UHCI_PTR_TERM;
    chain->qh.klink = (void *) chain->qh.link;
    chain->qh.element = chain->qh.this + sizeof( uhci_qh_t);
    chain->qh.kelement = &chain->td[0];

    
    
    /* XXX check datasize and set pointers to data buffer */
    for( i = 0; i < 6; i++){
		chain->td[i].this = chain->qh.this + sizeof( uhci_qh_t) + (i * sizeof( uhci_td_t ) );
        chain->td[i].link = chain->td[i].this + sizeof( uhci_td_t );
        chain->td[i].klink = &chain->td[i+1];
        chain->td[i].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[i*16]);
        chain->td[i].kbuffer = &chain->buffer[i*16];
        
    }
    
    chain->td[i].link = UHCI_PTR_TERM;
    chain->td[i].klink = NULL;
    
    bcopy( (void *) td_addr, (void *) &pipe->tds, sizeof( td_addr_t) * TD_ADDR_SIZE_VEC);
       
    TRACE( class | TRC_START_END, 10, "end", "");        
    return( (usb_pipe_t *) pipe);
}




int wait_qh(usbuhci_instance_t *soft, uhci_qh_t *qh){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	int i, rc;
    TRACE( class | TRC_START_END, 10, "start", "");

    /* XXX - 500ms just a guess */
    int complete = 0;
    
    for( i = 0; i < 100; i++){
	    USECDELAY( 5000);	

        if (qh->element & UHCI_PTR_TERM){
            rc = 0;
            goto wait_qh_quit;  
        } 
        
	}
    rc = 1;	
	TRACE( class, 12, "Timeout waiting for queue head", "");
	
wait_qh_quit:
    TRACE( class | TRC_START_END, 10, "end", "");
	
    return( rc);	
}


int wait_for_transfer(usbuhci_instance_t *soft, uhci_control_chain_t *chain){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	int i, rc;
	uint32_t   v;
    TRACE( class | TRC_START_END, 10, "start", "");

    /* XXX - 500ms just a guess */
    int complete = 0;
    
    for( i = 0; i < 10; i++){
	    USECDELAY( 5000);	

        if ( chain->qh.element & UHCI_PTR_TERM){
            rc = 0;
            goto wait_for_transfer_quit;  
        }
     
        v = chain->td[0].status & 0x07fe0000;
        if( v & TD_CTRL_ACTIVE) {
		        TRACE( 	class | TRC_START_END, 10, "TD status: ACTIVE", "");
		        continue;
		}
		
		if( v & TD_CTRL_STALLED)
		        TRACE( 	class, 10, "TD status: STALLED", "");
	    if( v & TD_CTRL_DBUFERR)
		        TRACE( 	class, 10, "TD status: DBUFER ERR", "");
		if( v & TD_CTRL_BABBLE) 
		        TRACE( 	class, 10, "TD status: BABBLE", "");
		if( v & TD_CTRL_NAK) 
		        TRACE( 	class, 10, "TD status: NAK", "");
		if( v & TD_CTRL_CRCTIMEO) 
		        TRACE( 	class, 10, "TD status: CRC_TIMEO", "");
		if( v & TD_CTRL_BITSTUFF) 
		        TRACE( 	class, 10, "TD status: BITSTUFF", "");
		        
		if( v == 0) 
		        TRACE( 	class | TRC_START_END, 10, "TD status: COMPLETE", "");
		
		rc = 0;
		goto wait_for_transfer_quit;
	}
    rc = 1;	
	TRACE( class, 12, "Timeout waiting for queue head", "");
	
wait_for_transfer_quit:
    TRACE( class | TRC_START_END, 10, "end", "");
	
    return( rc);	
}


/* Wait for next USB frame to start - for ensuring safe memory release.*/
int uhci_waittick( usbuhci_instance_t *soft){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	int i, rc;
	uint16_t start_frame, v;
	
    TRACE( class | TRC_START_END, 10, "start", "");
    start_frame = UREAD2( soft, UHCI_FRNUM);
	for( i = 0; i < 100; i++){
	    USECDELAY( 1000);
	    v = UREAD2( soft, UHCI_FRNUM);
	    if( start_frame != v){
	        rc = 0;
	        goto quit_uhci_waittick;
	    } 
	}
	rc = 1;
	
    quit_uhci_waittick:
    TRACE( class | TRC_START_END, 10, "end", "");
	
    return( rc);	
}



struct usb_pipe *uhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}


void uhci_chain_dump( uhci_control_chain_t *chain){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    int i;
    	
    TRACE( class, 12, "----- chain dump", "");
    TRACE( class, 12, "QH:", "");
    dump_uint32( class, 12, (uint32_t *) &chain->qh, sizeof( uhci_qh_t));
    USECDELAY( 4000);    
    
    
    for( i = 0; i < 3; i++){
        TRACE( class, 12, "TD%d:", i);
        dump_uint32( class, 12, (uint32_t *) &chain->td[i], sizeof( uhci_td_t));
        USECDELAY( 40000);    
    }

    for( i = 0; i < 3; i++){
        TRACE( class, 12, "BUF%d:", i);
        dump_uint32( class, 12, (uint32_t *) &chain->buffer[i*16], 16);
        USECDELAY( 40000);    
    }
    

}

int uhci_send_control( void *hcd, struct usb_pipe *p, int dir, void *cmd, int cmdsize, void *data, int datasize){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    uhci_control_chain_t *chain;
    int rc, i;
    int len, toggle;
    int timeout = 50;
    uint16_t regv;
    usb_ctrlrequest_t req;    
    usb_device_descriptor_t *dinfo; 
    usb_ctrlrequest_t *r;
    dma_node_t *dma_chunk;
    uint32_t  addr;   
    
    char      str[32];
    uchar_t   buffie[8] = { 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uchar_t   buffie2[8] ={ 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00 };
    
    
    TRACE( class | TRC_START_END, 10, "start", "");


    UWRITE2(soft, UHCI_INTR, 0x000f);  // acknowledge the interrupt
    UWRITE2(soft, UHCI_FRNUM, 0x0000);  // acknowledge the interrupt
    UWRITE2(soft, UHCI_SOF, 0x0040);  // acknowledge the interrupt

    dma_chunk = (dma_node_t *) p->dma_node;
    chain = ( uhci_control_chain_t *) p->qh;

    chain->qh.link = soft->tq->term_qh.this;
    chain->qh.element = chain->td[0].this;
    
    chain->td[0].link = chain->td[1].this | 0x00000004;
    chain->td[0].status = 0x1c800000;
    chain->td[0].token = 0x00e0002d;
    chain->td[0].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[0]);
    
    chain->td[1].link = chain->td[2].this | 0x00000004;
    chain->td[1].status = 0x1c800000;
    chain->td[1].token = 0x00e80069;
    chain->td[1].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[16]);

    chain->td[2].link = 0x00000001;
    chain->td[2].status = 0x1d800000;
    chain->td[2].token = 0xffe800e1;
    chain->td[2].buffer = 0x00000000;


    bcopy( (void *) buffie2, (void *) chain->td[0].kbuffer, 8);


    UWRITE2(soft, UHCI_STS, 0x0001);  // acknowledge the interrupt

    
    r = (usb_ctrlrequest_t *) cmd;
    addr = 1;
/*    
    uhci_chain_dump( chain); 
    
    TRACE( class, 12, "FRAMELIST", "");
    dump_uint32( class, 12, (uint32_t *) soft->framelist->links, 16);
    USECDELAY( 10000);
    TRACE( class, 12, "REGISTERS", "");
    regv = UREAD2( soft, UHCI_STS);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "status = %s", str);
    
    regv = UREAD2( soft, UHCI_CMD);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "cmd = %s", str);
    
    regv = UREAD2( soft, UHCI_INTR);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "intr = %s", str);

    regv = UREAD2( soft, UHCI_PORTSC1);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    
    regv = UREAD2( soft, UHCI_PORTSC2);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port2 = %s", str);
    */
    soft->framelist->links[0] = chain->qh.this | UHCI_PTR_QH;
     
    /*USECDELAY(2000000);*/
    /*dma_drain( p->dma_node);*/
    
    timeout = 100;
    while (!( UREAD2( soft, UHCI_STS) & 1) && (timeout > 0)) {
        timeout--;
        USECDELAY(100000);
        
    }
    
    if (timeout == 0) {
        TRACE( class, 12, "UHCI timeout in transfers1", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok1 %d", timeout);
	}
	

    UWRITE2(soft, UHCI_STS, 1);  // acknowledge the interrupt
    
    timeout = 100;
    while( ((chain->td[0].status & (0xFF<<16)) != 0) && (timeout > 0)){ 
        timeout--;
        USECDELAY(100000);
        dma_drain( p->dma_node);
	}	
    if (timeout == 0){
        TRACE( class, 12, "UHCI error in transfers2", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok2", "");
    }
    
    soft->framelist->links[0] = soft->framelist->links[2];

    uhci_chain_dump( chain);        
    TRACE( class, 12, "FRAMELIST", "");
    dump_uint32( class, 12, (uint32_t *) soft->framelist->links, 16);
    
    USECDELAY( 10000);
    
    TRACE( class, 12, "REGISTERS", "");
    regv = UREAD2( soft, UHCI_STS);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "status = %s", str);
    
    regv = UREAD2( soft, UHCI_CMD);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "cmd = %s", str);
    
    regv = UREAD2( soft, UHCI_INTR);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "intr = %s", str);

    regv = UREAD2( soft, UHCI_PORTSC1);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    
    regv = UREAD2( soft, UHCI_PORTSC2);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    TRACE( class, 12, "UHCI GET8 end", "");
 

    USECDELAY( 50000);
    
            req.bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
            req.bRequest = USB_REQ_GET_DESCRIPTOR;
            req.wValue = B2LENDIAN((uint16_t)USB_DT_DEVICE<<8);
            req.wIndex = 0;
            req.wLength = 8;    


    chain->qh.link = soft->tq->term_qh.this;
    chain->qh.element = chain->td[0].this;

    chain->td[0].link = chain->td[1].this | 0x00000004;
    chain->td[0].status = 0x1c800000;
    chain->td[0].token = 0x00e0002d;
  /*  chain->td[0].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[0]);*/

    chain->td[1].link = 0x00000001;
    chain->td[1].status = 0x1d800000;
    chain->td[1].token =  0xffe80069;
 /*   chain->td[1].buffer = 0x00000000;*/

    bcopy( (void *) buffie, (void *) chain->td[0].kbuffer, 8);

    uhci_chain_dump( chain);    
    


    soft->framelist->links[0] = chain->qh.this | UHCI_PTR_QH;
/*    USECDELAY(2000000);*/
/*    dma_drain( p->dma_node);*/
    
    timeout = 100;
    while (!( UREAD2( soft, UHCI_STS) & 1) && (timeout > 0)) {
        timeout--;
        USECDELAY(100000);

    }
    
    if (timeout == 0) {
        TRACE( class, 12, "UHCI timeout in transfers1", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok1 %d", timeout);
	}
	

    UWRITE2(soft, UHCI_STS, 1);  // acknowledge the interrupt
    
    timeout = 100;
    while( ((chain->td[0].status & (0xFF<<16)) != 0) && (timeout > 0)){ 
        timeout--;
        USECDELAY(100000);
        dma_drain( p->dma_node);
	}	
    if (timeout == 0){
        TRACE( class, 12, "UHCI error in transfers2", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok2", "");
    }
    
    soft->framelist->links[0] = soft->framelist->links[2];

    uhci_chain_dump( chain);    
    TRACE( class, 12, "UHCI SET ADDRESS completed", "");

    USECDELAY( 1000);
 
        
    chain->qh.link = soft->tq->term_qh.this;
    chain->qh.element = chain->td[0].this;
    
    chain->td[0].link = chain->td[1].this | 0x00000004;
    chain->td[0].status = 0x1c800000;
    chain->td[0].token = 0x00e0012d;
    /*chain->td[0].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[0]);*/
    
    chain->td[1].link = chain->td[2].this | 0x00000004;
    chain->td[1].status = 0x1c800000;
    chain->td[1].token = 0x00e80169;
  /*  chain->td[1].buffer = chain->td[1].buffer + 16*/
    
    chain->td[2].link = chain->td[3].this | 0x00000004;
    chain->td[2].status = 0x1c800000;
    chain->td[2].token = 0x00e00169;
 /*   chain->td[2].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[24]);*/

    chain->td[3].link = chain->td[4].this | 0x00000004;
    chain->td[3].status = 0x1c800000;
    chain->td[3].token = 0x00280169;
  /*  chain->td[3].buffer = kaddr_2_daddr( dma_chunk->daddr, dma_chunk->kaddr, &chain->buffer[32]);*/

    chain->td[4].link = 0x00000001;
    chain->td[4].status = 0x1d800000;
    chain->td[4].token = 0xffe801e1;
 /*   chain->td[4].buffer = 0x00000000;*/


    bcopy( (void *) buffie2, (void *) chain->td[0].kbuffer, 8);



    
    r = (usb_ctrlrequest_t *) cmd;
    addr = 1;
    uhci_chain_dump( chain); 
    
    TRACE( class, 12, "FRAMELIST", "");
    dump_uint32( class, 12, (uint32_t *) soft->framelist->links, 16);
    USECDELAY( 10000);
    TRACE( class, 12, "REGISTERS", "");
    regv = UREAD2( soft, UHCI_STS);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "status = %s", str);
    
    regv = UREAD2( soft, UHCI_CMD);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "cmd = %s", str);
    
    regv = UREAD2( soft, UHCI_INTR);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "intr = %s", str);

    regv = UREAD2( soft, UHCI_PORTSC1);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    
    regv = UREAD2( soft, UHCI_PORTSC2);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port2 = %s", str);
    USECDELAY(2000000);
    soft->framelist->links[0] = chain->qh.this | UHCI_PTR_QH;
    
    timeout = 100;
    while (!( UREAD2( soft, UHCI_STS) & 1) && (timeout > 0)) {
        timeout--;
        USECDELAY(100000);
        
    }
    
    if (timeout == 0) {
        TRACE( class, 12, "UHCI timeout in transfers1", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok1 %d", timeout);
	}
	

    UWRITE2(soft, UHCI_STS, 1);  // acknowledge the interrupt
    
    timeout = 100;
    while( ((chain->td[0].status & (0xFF<<16)) != 0) && (timeout > 0)){ 
        timeout--;
        USECDELAY(200000);
        dma_drain( p->dma_node);
	}	
    if (timeout == 0){
        TRACE( class, 12, "UHCI error in transfers2", "");
    }else{
		TRACE( class, 12, "UHCI transfer ok2", "");
    }
    
    soft->framelist->links[0] = soft->framelist->links[2];
    
    uhci_chain_dump( chain);        
    TRACE( class, 12, "FRAMELIST", "");
    dump_uint32( class, 12, (uint32_t *) soft->framelist->links, 16);
    USECDELAY( 10000);
    TRACE( class, 12, "REGISTERS", "");
    regv = UREAD2( soft, UHCI_STS);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "status = %s", str);
    
    regv = UREAD2( soft, UHCI_CMD);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "cmd = %s", str);
    
    regv = UREAD2( soft, UHCI_INTR);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "intr = %s", str);

    regv = UREAD2( soft, UHCI_PORTSC1);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    
    regv = UREAD2( soft, UHCI_PORTSC2);
    INT2HEX16X( str, regv);
    TRACE( class, 12, "port1 = %s", str);
    USECDELAY( 10000);

    TRACE( class, 12, "UHCI GET DESCRIPTOR end", "");


  /*  dinfo = (usb_device_descriptor_t *) &chain->buffer[16];
    TRACE( class, 10, "device rev=%04x cls=%02x sub=%02x proto=%02x size=%02x\n"
                , dinfo->bcdUSB, dinfo->bDeviceClass, dinfo->bDeviceSubClass
                , dinfo->bDeviceProtocol, dinfo->bMaxPacketSize0);*/
    USECDELAY(5000000);
    return(1);

    /* XXX copy data to data buffer */
/*    chain->td[0].link &= 0xfffffff0;   
    chain->td[0].link |= UHCI_PTR_DEPTH;
    chain->td[0].status = uhci_maxerr( 3) | ( p->speed ? TD_CTRL_LS : 0) | TD_CTRL_ACTIVE;
    chain->td[0].token = (uhci_explen(cmdsize) | (p->devaddr << TD_TOKEN_DEVADDR_SHIFT) | USB_PID_SETUP);
    bcopy( (void *) cmd, (void *) chain->td[0].kbuffer, cmdsize);


    toggle = TD_TOKEN_TOGGLE;
    chain->td[1].link &= 0xfffffff0;
    chain->td[1].link |= UHCI_PTR_DEPTH;
    chain->td[1].status = (uhci_maxerr(3) | (p->speed ? TD_CTRL_LS : 0) | TD_CTRL_ACTIVE);
    len = p->maxpacket;
    chain->td[1].token = (uhci_explen(len) | toggle 
                        | (p->devaddr << TD_TOKEN_DEVADDR_SHIFT)
                        | (dir ? USB_PID_IN : USB_PID_OUT));
    
    toggle ^= TD_TOKEN_TOGGLE;

    chain->td[2].link &= 0xfffffff0;    
    chain->td[2].link |= UHCI_PTR_TERM;
    chain->td3.status = (uhci_maxerr(0) | (p->speed ? TD_CTRL_LS : 0) | TD_CTRL_ACTIVE);
    chain->td3.token = (uhci_explen(len) | TD_TOKEN_TOGGLE
                        | (p->devaddr << TD_TOKEN_DEVADDR_SHIFT)
                        | (dir ? USB_PID_IN : USB_PID_OUT));
    chain->td3.buffer = 0;
    chain->td3.kbuffer = NULL;

    
    uhci_chain_dump( chain);
     
    uhci_insert_queue( p->dma_node, soft,  UHCI_QHCTL);

         
    rc = wait_for_transfer( soft, &chain);

    dma_drain( p->dma_node);
    if (rc) {
        chain->qh.element = UHCI_PTR_TERM;
        uhci_waittick( p->hcd);
    }
    
    
    uhci_chain_dump( chain);
    
    if( datasize > 0)
        bcopy( (void *) chain->td2.kbuffer, (void *) data,datasize);    
  */      
    TRACE( class | TRC_START_END, 10, "end", "");
	
    return( rc);
	
}

int uhci_set_address( void *hcd, struct usb_pipe *p, int dir, void *cmd, int cmdsize, void *data, int datasize){
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
}

int uhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize){
}

int uhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int uhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data){
}

int uhci_ioctl( void *hcd, int cmd, void *uarg){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;

    
	TRACE( class | TRC_START_END, 10, "start", "");
   	TRACE( class | TRC_START_END, 10, "end", "");	
   	return( 0);
  	
}



int uhci_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
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

int uhci_hub_info( void *hcd, void *info){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	usbhub_instance_t *usb_hub = (usbhub_instance_t *) info;
	usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	usb_hub->pci_bus = (uchar_t) pciio_info_bus_get( soft->ps_pciio_info_device);
	usb_hub->pci_slot = (uchar_t) pciio_info_slot_get( soft->ps_pciio_info_device);
	usb_hub->pci_function = (int) pciio_info_function_get( soft->ps_pciio_info_device);
    usb_hub->ports_number = soft->ps_noport;
	
	TRACE( class | TRC_START_END, 10, "end", "");	
	return( 0); 
}

uint32_t uhci_get_port( void *hcd, int port){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint32_t pa, rport;

	TRACE( class | TRC_START_END, 10, "start", "");
    port = port % 2;
    
    if( port == 0)
        rport = UREAD2( soft, UHCI_PORTSC2);
    else
        rport = UREAD2( soft, UHCI_PORTSC1);

	pa = 0;
	if( rport & UHCI_PORTSC_CCS)
	    pa |= USB_PORT_CONNECTED;
	if( rport & UHCI_PORTSC_PE)
	    pa |= USB_PORT_ENABLED | USB_PORT_IN_USE;
	if( rport & UHCI_PORTSC_SUSP)
	    pa |= USB_PORT_SUSPENDED;
	
    if( rport & UHCI_PORTSC_LSDA)
		pa |= USB_PORT_LOW_SPEED;
	else 
	    pa |= USB_PORT_FULL_SPEED;
	        
    pa |= USB_PORT_STATUS_OK;	    
    pa |= USB_PORT_SET_HCD_OWNER(USB_UHCI);
    	
	TRACE( class | TRC_START_END, 10, "end", "");	
	return( pa);
}

int uhci_set_roothub(void *hcd, void *roothub){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    usbhub_instance_t *usbhub = ( usbhub_instance_t *) roothub;
    
	TRACE( class | TRC_START_END, 10, "start", "");
    soft->roothub = roothub;
   	TRACE( class | TRC_START_END, 10, "end", "");	
   	return( 0);
}

int uhci_port_action( void *hcd, int port, int action){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    usbuhci_instance_t *soft = (usbuhci_instance_t *) hcd;
    int rc; 
    usbhub_instance_t *roothub = soft->roothub;

	TRACE( class | TRC_START_END, 10, "start", "");
	switch(action){
		case USB_HUB_PORT_RESET:
	        MUTEX_LOCK( &roothub->uhci_mutex, -1);    
	        TRACE( class, 10, "uhci reset port = %d", port);
	        port = ( port % 2);
	        
	        rc = uhci_port_reset( soft, port);
	        MUTEX_UNLOCK( &roothub->uhci_mutex);
	    break;
	}

	TRACE( class | TRC_START_END, 10, "end rc=%d", rc);	
	return(rc);	
}


int uhci_set_debug_values( void *hcd, void *pv){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    usbuhci_instance_t *soft = (usbuhci_instance_t *) hcd;
    int rc; 
    usb_debug_values_t *v = (usb_debug_values_t *) pv;

	TRACE( class | TRC_START_END, 10, "start", "");

    switch( v->id){
	    case 0:
	        soft->debug_port = v->values[0];
	        soft->debug_port_root_reset_delay = v->values[1];
	        soft->debug_port_reset_recovery_delay = v->values[2];
	        soft->debug_port_reset_delay = v->values[3];
		break;	   	
	}
	TRACE( class | TRC_START_END, 10, "end rc=%d", rc);	
	return(rc);	
}

/*
*******************************************************************************************************
* USBcore calls functions                                                                             *
*******************************************************************************************************
*/
int register_uhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbuhci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	char str[45];
	
	soft = (usbuhci_instance_t *) hcd;
	TRACE( class | TRC_START_END, 10, "start", "");
/*
    usbcore = soft->usbcore;	
	rc = usbcore->register_hcd_driver( (void *) &soft->device_header);
	TRACE( class, 6,"rc = %d", rc);
    soft->ps_event_func = usbcore->hcd_event_func;*/
	
	TRACE( class | TRC_START_END, 10, "end", "");
	
	return( rc);
}


int unregister_uhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbuhci_instance_t          *soft;
	int rc = 0; 
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
	char str[45];
	
	soft = (usbuhci_instance_t *) hcd;
	TRACE( class, 10, "start", "");


/*    usbcore = soft->usbcore;
	func = usbcore->unregister_hcd_driver; 
    rc = func( (void *) &soft->device_header);
	TRACE( class, 6,"rc = %d", rc);*/
	TRACE( class, 10, "end", "");
	
	return( rc);
}


int uhci_process_event( void *origin, void *dest, int event_id, void *arg){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	
}

int uhci_process_event_from_usbcore( void *dest, int event_id, void *arg0, void *arg1, void *arg2){
    uint64_t class = TRC_HELPER | TRC_MOD_UHCI;
    	
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class | TRC_START_END, 10, "end", "");	

}


/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
    
/*
*******************************************************************************************************
* usbuhci_init: called once during system startup or                                                  *
* when a loadable driver is loaded.                                                                   *
*******************************************************************************************************
*/
void usbuhci_init(void){
    usbcore_instance_t *usbcore;
    uint64_t class = TRC_INIT | TRC_MOD_UHCI;

  	TRACE( class | TRC_START_END, 10, "start", "");
  	printf( "**********************************************************\n");
  	printf( "* UHCI USB Driver for Silicon Graphics Irix 6.5          *\n");
  	printf( "* By bsdero at gmail dot org, 2011-2013                  *\n");
  	printf( "* Version %s                                        *\n", USBCORE_DRV_VERSION);
  	printf( "* Sequence %s                                *\n", USBCORE_DRV_SEQ);
  	printf( "**********************************************************\n");
    printf( "usbuhci kernel module loaded! \n");
    printf( "_diaginfo_: Kernel usbuhci module base address:0x%x\n", module_address);
    USECDELAY(100000);

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */

    usbcore = get_usbcore();
    usbcore->register_module( ( void *) &uhci_header);
    pciio_iterate("usbuhci_", usbuhci_reloadme);	
    gc_list_init( &gc_list);
	TRACE( class | TRC_START_END, 10, "end", "");
}


/*
*******************************************************************************************************
*    usbuhci_unload: if no "usbuhci" is open, put us to bed                                           *
*      and let the driver text get unloaded.                                                          *
*******************************************************************************************************
*/
int usbuhci_unload(void){
    usbcore_instance_t *usbcore;
    uint64_t class = TRC_UNLOAD | TRC_MOD_UHCI;

	TRACE( class | TRC_START_END, 10, "start", "");
    if (usbuhci_inuse)
        return EBUSY;

    usbcore = get_usbcore();
    usbcore->unregister_module( ( void *) &uhci_header);
    pciio_iterate("usbuhci_", usbuhci_unloadme);
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "UHCI Driver unloaded", "");
	TRACE( class | TRC_START_END, 10, "end", "");
    
    return( 0);
}



/*
*******************************************************************************************************
*    usbuhci_reg: called once during system startup or                                                *
*      when a loadable driver is loaded.                                                              *
*    NOTE: a bus provider register routine should always be                                           *
*      called from _reg, rather than from _init. In the case                                          *
*      of a loadable module, the devsw is not hooked up                                               *
*      when the _init routines are called.                                                            *
*******************************************************************************************************
 */
int usbuhci_reg(void){
	int i;
	uint16_t device_id, vendor_id;
    uint64_t class = TRC_INIT | TRC_MOD_UHCI;
	
	TRACE( class | TRC_START_END, 10, "start", "");
	
    /* registering UHCI devices */
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        device_id = ((uhci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (uhci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "usbuhci_", 0);  /* usbuhci_attach and usbuhci_detach entry points */
    }

	TRACE( class | TRC_START_END, 10, "end", "");
    return 0;
}



/*
*******************************************************************************************************
* usbuhci_unreg: called when a loadable driver is unloaded.                                           *
*******************************************************************************************************
*/
int usbuhci_unreg(void){
	TRACE(  TRC_UNLOAD | TRC_MOD_UHCI | TRC_START_END, 10, "start", "");
    pciio_driver_unregister("usbuhci_");
	TRACE(  TRC_UNLOAD | TRC_MOD_UHCI | TRC_START_END, 10, "end", "");
    return 0;
}


/*
*******************************************************************************************************
*    usbuhci_attach: called by the pciio infrastructure                                               *
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
int usbuhci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t            charv = GRAPH_VERTEX_NONE;       
    uchar_t                 *cfg;
    usbuhci_instance_t      *soft;
    uchar_t                 *regs;    
    pciio_piomap_t          cmap = NULL;
    pciio_piomap_t          rmap = NULL;
    uint32_t                ps_io_base_addr = 0;
    graph_error_t           ret = (graph_error_t) 0;
    uint16_t                vendor_id;
    uint16_t                device_id;
    uint32_t                ssid;
    uchar_t                 rev_id;
    uint16_t                val;    
    int                     i;
    uint32_t                device_vendor_id;    
    int                     rc;
    device_desc_t           usbuhci_dev_desc;    
    uint64_t                class =  TRC_ATTACH | TRC_MOD_UHCI;
    usbcore_instance_t      *usbcore;
    
	TRACE( class | TRC_START_END, 10, "start", "");
	TRACE( class, 0, "UHCI Host Controller Device Detected!", "");
    
    usbcore = get_usbcore();
    if( usbcore == NULL){ 
		TRACEWAR( class | TRC_WARNING, "usbcore not loaded, quitting..", ""); 
		return( -1);
	} 
	
	MUTEX_LOCK( &usbcore->uhci_mutex, -1);
    
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
		TRACEWAR( class | TRC_WARNING, "error in hwgraph_traverse(), quitting..", ""); 
        rc = -1;
        goto exit_attach;
    }
    

    ret = hwgraph_edge_get( vhdl, "usbuhci", &charv);

    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    soft = (usbuhci_instance_t *) gc_malloc( &gc_list, sizeof( usbuhci_instance_t));
    
    ASSERT(soft != NULL);
    device_info_set(vhdl, (void *)soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_charv = charv;
    
    
    cfg = (uchar_t *) pciio_pio_addr
        (conn, 0,                     /* device and (override) dev_info */
         PCIIO_SPACE_CFG,             /* select configuration addr space */
         0,                           /* from the start of space, */
         UHCI_NUM_CONF_REGISTERS,     /* ... up to vendor specific stuff */
         &cmap,                       /* in case we needed a piomap */
         0);                          /* flag word */

    vendor_id = (uint16_t) PCI_CFG_GET16(conn, UHCI_CF_VENDOR_ID);
    device_id = (uint16_t) PCI_CFG_GET16(conn, UHCI_CF_DEVICE_ID);
    ssid = (uint32_t) PCI_CFG_GET32( conn, UHCI_CF_SSID);
    rev_id = (uchar_t) PCI_CFG_GET8( conn, UHCI_CF_REVISION_ID);
    ps_io_base_addr = (uint32_t) PCI_CFG_GET32( conn, UHCI_CF_BASE_ADDR);
    PCI_CFG_SET16( conn, PCI_LEGSUP, 0x8f00);
    USECDELAY(1000);
    PCI_CFG_SET16( conn, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN);
    
    TRACE( class, 10, "UHCI supported device found", "");
    TRACE( class, 10, "Vendor ID: 0x%x, Device ID: 0x%x", vendor_id, device_id);
    TRACE( class, 10, "SSID: 0x%x, Rev ID: 0x%x, base addr: 0x%x",ssid, rev_id,
         ps_io_base_addr);
    
    
    soft->device_header.module_header = &uhci_header;
    
    device_vendor_id = device_id << 16 | vendor_id;
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        if( uhci_descriptions[i].device_id == device_vendor_id){
            soft->device_header.vendor_id = vendor_id;
            soft->device_header.device_id = device_id;
            strcpy( soft->device_header.hardware_description, 
                  (char *)uhci_descriptions[i].controller_description);
            TRACE( class, 10, "Device Description: %s", 
                uhci_descriptions[i].controller_description);
            break;
        }
    }

    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);
    
    /*
     * Get a pointer to our DEVICE registers
     */
     
    TRACE( class, 8, "Trying PCIIO_SPACE_IO", regs);
    regs = (uchar_t *) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_SPACE_IO,                /* in my primary decode window,   */
         0, 0x400010,                   /* base and size                  */
         &rmap,                         /* in case we needed a piomap     */
         0);                            /* flag word                      */

    TRACE( class, 12, "regs = 0x%x", regs);
    if( regs == NULL){
		TRACE( class, 12, "failed", ""); 
		rc = -1;
		goto exit_attach;
    }else
        goto solved;


solved:
    soft->pci_io_caps = regs;
    soft->ps_regs = regs;               /* save for later */
    soft->ps_rmap = rmap;
    soft->ps_cfg = cfg;               /* save for later */
    soft->ps_cmap = cmap;
    soft->ps_pciio_info_device = pciio_info_get(conn);
    soft->sc_offs = ps_io_base_addr;
    soft->ps_noport = 2;
	soft->device_header.soft = (void *) soft;
	soft->device_header.class_id = 0;
	soft->device_header.interface_id = 0;
	soft->device_header.info_size = sizeof( usbuhci_instance_t);
	soft->device_header.methods = (void *) &uhci_methods;	
	soft->debug_port_root_reset_delay = 50;
	soft->debug_port_reset_recovery_delay = 100;
	soft->debug_port_reset_delay = 10; 
	soft->usbcore = usbcore;
	global_soft = soft;

  
    uhci_init( (void *) soft);

    /* enable interrupts */
    TRACE( class, 12, "enabling interrupts", "");
    usbuhci_dev_desc = device_desc_dup(vhdl);
    device_desc_intr_name_set(usbuhci_dev_desc, "usbuhci");
    device_desc_default_set(vhdl, usbuhci_dev_desc);

    global_soft = soft;

    /* send message to usbcore, getting this HCD instance number */
    soft->device_header.spec_device_instance_id = usbcore->process_event_from_hcd( usbcore, 
       soft, USB_HCD_GET_INSTANCE_ID, soft, NULL, NULL);

    TRACE( class, 12, "Creating PCI INTR, spec_device_instance_id=%d", 
        soft->device_header.spec_device_instance_id);
    
   
    sprintf( soft->device_header.fs_device, "usbuhci%d", 
        soft->device_header.spec_device_instance_id );
        
    ret = hwgraph_char_device_add( vhdl, soft->device_header.fs_device, 
        "usbehci_", &charv); 

    device_info_set(vhdl, (void *)soft);

    /*now we have the roothub configured, we can send messages to it!! */
    /*
    if( soft->device_header.spec_device_instance_id == 0){
		
        soft->ps_intr = pciio_intr_alloc(conn, usbuhci_dev_desc, 
            PCIIO_INTR_LINE_A|PCIIO_INTR_LINE_B, vhdl);

        if( soft->ps_intr != NULL){
            TRACE( class, 4, "about to connect..", "");
            rc = pciio_intr_connect(soft->ps_intr, usbuhci_dma_intr, soft,(void *) 0);    
            TRACE( class, 4, "rc = %d", rc);
        }
			
    }else{
		soft->ps_intr = NULL;
	}*/

    rc = 0;
exit_attach:
    /* send message tu USBcore, this HCD is ready and configured */
    rc = usbcore->process_event_from_hcd( usbcore, soft, USB_HCD_ATTACHED_EVENT, soft, NULL ,NULL);
    

    /* ok*/
    MUTEX_UNLOCK( &usbcore->uhci_mutex);

    TRACE( class, 12, "rc = %d", rc);
	TRACE( class | TRC_START_END, 10, "end", "");

    return( rc);                 
}




/*
*******************************************************************************************************
*    usbuhci_detach: called by the pciio infrastructure                                               *
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
int usbuhci_detach(vertex_hdl_t conn){
    vertex_hdl_t                  vhdl, blockv, charv;
    usbuhci_instance_t            *soft;
    uint64_t                      class =  TRC_DETACH|TRC_MOD_UHCI;
    int                           i, rc;
    usbcore_instance_t            *usbcore;
    usbhub_instance_t             *roothub;
    
    
	TRACE( class | TRC_START_END, 10, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl))
        return( -1);

    usbcore = get_usbcore();
    if( usbcore == NULL){ 
		TRACEWAR( class | TRC_WARNING, "usbcore not loaded, quitting..", ""); 
		return( -1);
	} 
    

	
	
    soft = global_soft;
    roothub = soft->roothub;
    MUTEX_LOCK( &roothub->uhci_mutex, -1);
    TRACE( class, 12, "soft=0x%x, global_soft=0x%x", soft, global_soft);    
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->device_header.module_header->short_description);

    TRACE( class, 12, "untimeout", "");
/*    if( soft->ps_itimeout != 0)*/
    untimeout( soft->ps_itimeout);
    soft->ps_itimeout = 0;

    uhci_stop( soft);
/*
    for( i = 0; i < 100; i++){
        USECDELAY(100000); 
        if( soft->ps_stopped_timeout == 0)
            break;
    }
   
    if( soft->ps_stopped_timeout == 0){
		TRACE( class, 12, "timeout function stopped successfully", "");
	}else{
		TRACE( class, 12, "timeout could not stop, crashing...!!", "");
	}	*/
    USECDELAY(1000000); 
    
    
    TRACE( class, 12, "disconnect interrupt", rc);
    dma_list_destroy( &dma_list);    
/*    
    pciio_error_register(conn, 0, 0);
    */
    /*
    if( soft->ps_intr != NULL){
        pciio_intr_disconnect(soft->ps_intr);
        TRACE( class, 4, "free interrupt", rc);
        pciio_intr_free(soft->ps_intr);
    }
    
    */
    
/*    
    phfree(soft->ps_pollhead);
    if (soft->ps_ctl_dmamap)
        pciio_dmamap_free(soft->ps_ctl_dmamap);
    if (soft->ps_str_dmamap)
        pciio_dmamap_free(soft->ps_str_dmamap);*/
 /*   rc = unregister_uhci_with_usbcore((void *) soft);
    TRACE( class, 4, "rc = %d", rc);
*/

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

	MUTEX_UNLOCK( &roothub->uhci_mutex);
     
	TRACE( class | TRC_START_END, 10, "end", "");
    
    return 0;
}





/*
*******************************************************************************************************
*    usbuhci_reloadme: utility function used indirectly                                               *
*      by usbuhci_init, via pciio_iterate, to "reconnect"                                             *
*      each connection point when the driver has been                                                 *
*      reloaded.                                                                                      *
*******************************************************************************************************
*/
static void usbuhci_reloadme(vertex_hdl_t conn){
    vertex_hdl_t            vhdl;
    usbuhci_instance_t      *soft;
    uint64_t                class = TRC_INIT | TRC_MOD_UHCI;

	TRACE( class | TRC_START_END, 10, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "usbuhci", &vhdl))
        return;
        
    soft = (usbuhci_instance_t *) device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbuhci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbuhci_dma_intr, soft, 0);
	TRACE( class | TRC_START_END, 10, "end", "");
}





/*
*******************************************************************************************************
 *    usbuhci_unloadme: utility function used indirectly by
 *      usbuhci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void usbuhci_unloadme(vertex_hdl_t pconn){
    vertex_hdl_t            vhdl;
    usbuhci_instance_t      *soft;
    uint64_t                class = TRC_UNLOAD | TRC_MOD_UHCI;
    
	TRACE( class | TRC_START_END, 10, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(pconn, "usbuhci", &vhdl))
        return;
        
    soft = (usbuhci_instance_t *) device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
	TRACE( class | TRC_START_END, 10, "end", "");
}



/*
*******************************************************************************************************
*    pciuhci_open: called when a device special file is                                               * 
*      opened or when a block device is mounted.                                                      *
*******************************************************************************************************
*/ 
int usbuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return( EINVAL);
}




/*
*******************************************************************************************************
*    usbuhci_close: called when a device special file
*      is closed by a process and no other processes
*      still have it open ("last close").
*******************************************************************************************************
*/ 
int usbuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return 0;
}


/*
*******************************************************************************************************
*    usbuhci_ioctl: a user has made an ioctl request                                                  *
*      for an open character device.                                                                  *
*      Arguments cmd and arg are as specified by the user;                                            *
*      arg is probably a pointer to something in the user's                                           *
*      address space, so you need to use copyin() to                                                  *
*      read through it and copyout() to write through it.                                             *
*******************************************************************************************************
*/ 
int usbuhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
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
int usbuhci_read(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_write(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_strategy(struct buf *bp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
    return 0;
}

/*
*******************************************************************************************************
* Poll Entry points                                                                                   *
*******************************************************************************************************
*/
int usbuhci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Memory map entry points                                                                             *
*******************************************************************************************************
*/
int usbuhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_unmap(dev_t dev, vhandl_t *vt){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Interrupt entry point                                                                               *
* We avoid using the standard name, since our prototype has changed.                                  *
*******************************************************************************************************
*/
void usbuhci_dma_intr(intr_arg_t arg){
    usbuhci_instance_t        *soft = (usbuhci_instance_t *) arg;
    uint64_t                class = TRC_INTR | TRC_MOD_UHCI;
    
	TRACE( class | TRC_START_END, 10, "start", "");
    
    if( soft == NULL){
        soft = global_soft;
    }    /*
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
static int usbuhci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror){

/* not used right now */


/*    usbuhci_instance_t            *soft = (usbuhci_instance_t *) einfo;
    vertex_hdl_t                  vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbuhci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    ioerror_dump("usbuhci", error_code, mode, ioerror);
*/



    return IOERROR_HANDLED;
}



/*
*******************************************************************************************************
* Support entry points                                                                                *
*******************************************************************************************************
*    usbuhci_halt: called during orderly system                                                       *
*      shutdown; no other device driver call will be                                                  *
*      made after this one.                                                                           *
*******************************************************************************************************
*/
void usbuhci_halt(void){
   	TRACE(  TRC_HELPER | TRC_MOD_UHCI | TRC_START_END, 10, "start", "");
	TRACE(  TRC_HELPER | TRC_MOD_UHCI | TRC_START_END, 10, "end", "");

}



/*
*******************************************************************************************************
*    usbuhci_size: return the size of the device in                                                   *
*      "sector" units (multiples of NBPSCTR).                                                         *
*******************************************************************************************************
*/
int usbuhci_size(dev_t dev){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
*    usbuhci_print: used by the kernel to report an                                                   *
*      error detected on a block device.                                                              *
*******************************************************************************************************
*/
int usbuhci_print(dev_t dev, char *str){
	/* not used */
    return 0;
}


