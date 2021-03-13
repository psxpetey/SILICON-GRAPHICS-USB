/*
********************************************************************
* USB Core Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbtool.c                                                  *
* Description: USB stack utility and test program                  *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-12-2012     -Added xml and scripts friendly options                                  *
*                                                                                                     *
* BSDero      07-19-2012     -Added help options                                                      *
*                                                                                                     *
* BSDero      07-16-2012     -Added options parser and actions for options                            *
*                                                                                                     *
* BSDero      07-05-2012     -Fix errors in trace options                                             *
*                                                                                                     *
* BSDero      06-15-2012     -Added get root hub status functionality                                 *
*                                                                                                     *
* BSDero      06-13-2012     -Added basic functionality, menus and options                            *
*                            -Added callback functions for menus                                      *
*                            -Added functionalities: get driver info, get trace level, set trace      *
*                             level, get driver modules, get root hub info and status                 *
*                                                                                                     *
* BSDero      05-07-2012     -Initial version (Parts rippen from previous tools)                      *
*                                                                                                     *
*******************************************************************************************************
*/
/*

MAIN_MENU 
    X.-Exit
	M.-Display Menu
	O.-Open
	H.-Help
If device open
    1.-Close
    2.-USBcore tools
	3.-USB HCD tools
	4.-USB Device tools
	5.-Libusb tools
       	


2.-USBcore tools
    R.-Return to previous menu
	M.-Display Menu	

	1.-Polling events test
    2.-ioctl1
    3.-ioctl2
    n.-ioctl..n 
    	
3.-USB HCD tools
    R.-Return to previous menu
	M.-Display Menu	

    1.-EHCI tools
    2.-OHCI tools
    3.-XHCI tools
    4.-UHCI tools

4.-USB Device tools
    R.-Return to previous menu
	M.-Display Menu	

    1.-Keyboard
    2.-Mouse
    3.-UMass
    4.-xxxx

5.-Libusb tools    
    R.-Return to previous menu
	M.-Display Menu	
    
*/


#ifndef _IRIX_
typedef unsigned char         uchar_t;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "usbioctl.h"
#include "kutils.c"
#include "kvarr.h"

#define USBTL_OPTION_HELP                          1
#define USBTL_OPTION_CORE                          2
#define USBTL_OPTION_ROOTHUB                       3
#define USBTL_OPTION_USBDEVICE                     4
#define USBTL_OPTION_API                           5
#define USBTL_OPTION_INTERACTIVE                   6

/* generic options, chars -v, -l, -i, -s, -d, -f, -a, -x, -e are reserved */
#define USBTL_OPTION_VERBOSE                       0x0001 
#define USBTL_OPTION_LIST                          0x0002
#define USBTL_OPTION_INFO                          0x0004
#define USBTL_OPTION_STATUS                        0x0008
#define USBTL_OPTION_DEVICE                        0x0010
#define USBTL_OPTION_FILE                          0x0020
#define USBTL_OPTION_ALL                           0x0040
#define USBTL_OPTION_XML                           0x0080
#define USBTL_OPTION_SHELL                         0x0100 /* shell friendly */


/* core options */
#define USBTL_OPTION_TRACELEVEL                    10
#define USBTL_OPTION_MODULES                       11
#define USBTL_OPTION_TREE                          13
#define USBTL_OPTION_STATS                         14
#define USBTL_OPTION_EVENTS                        15

/* root hub */
#define USBTL_OPTION_START                         20
#define USBTL_OPTION_RESET                         21
#define USBTL_OPTION_HALT                          22
#define USBTL_OPTION_SHUTDOWN                      23
#define USBTL_OPTION_DUMP                          24
#define USBTL_OPTION_SUSPEND                       25
#define USBTL_OPTION_WAKEUP                        26
#define USBTL_OPTION_PORT_RESET                    27
#define USBTL_OPTION_STOP                          28

/* libapi */
#define USBTL_OPTION_TEST                          30



/* ACTIONS */
#define USBTL_ACTION_INTERACTIVE                   1
#define USBTL_ACTION_HELP                          2
#define USBTL_ACTION_DRIVER_INFO                   3
#define USBTL_ACTION_DISPLAY_MODULES               4
#define USBTL_ACTION_ROOT_HUB_INFO                 5
#define USBTL_ACTION_ROOT_HUB_INFO_ALL             6
#define USBTL_ACTION_ROOT_HUB_STATUS	           7
#define USBTL_ACTION_DISPLAY_TRACE_LEVEL           8
#define USBTL_ACTION_SET_TRACE_LEVEL               9
#define USBTL_ACTION_SHOW_DEVICES                  10
#define USBTL_ACTION_SHOW_DEVICES_TREE             11



#define USBTL_DISPLAY_FMTHUMAN                     0
#define USBTL_DISPLAY_FMTSHELL                     1
#define USBTL_DISPLAY_FMTXML                       2

/*
*******************************************************************************************************
* Data structures                                                                                     *
*******************************************************************************************************
*/ 
typedef int(*func_t)(void *);

typedef struct{
    char                          *caption; /* length must be 19 */
	func_t                        func; 
	char                          key;
}menu_item_t;


/*
*******************************************************************************************************
* Menu functions                                                                                      *
*******************************************************************************************************
*/ 
int display_menu(void *items1, void *items2);
int display_menu_part( void *menu_items);
int menu_size( menu_item_t *menu);
int run_menu(menu_item_t *items1, menu_item_t *items2);
int parse_line_command( int argc, char **argv, char **strargs);
int parse_core_options( int argc, char **argv, char **strargs);
int search_generic_options( int argc, char **argv, int *num_options);
int is_option( char *str, char *str2, char *letter);
void show_trace_class();
int show_help();
int set_trace_class_args( char *strlevel, char *strclass);


/*
*******************************************************************************************************
* Menu Callback functions                                                                             *
*******************************************************************************************************
*/ 
int exit_function( void *p);
int i_open_device(void *p); /* interactive open device */
int open_device(void *p);   /* open device */
int close_device(void *p);
int usbcore_dummy(void *p);
int display_main_menu( void *p);
int return_function( void *p);
int display_gen_menu1( void *p);
int usbcore_tools( void *p);
int driver_info( void *p);
int show_modules( void *p);
int root_hub_info( void *p);
int set_trace_class( void *p);
int root_hub_status( void *p);
int poll_ports_test( void *p);
int show_devices( void *p);
int set_debug_values( void *p);
int run_debug_op( void *p);


/*
*******************************************************************************************************
* Menu, callbacks and key configurations                                                              *
*******************************************************************************************************
*/ 
char *entry_point_options[] = { 
    "init()/reg()     ",
	"unload()/unreg() ",
	"attach()         ",
	"detach()         ",
	"open()           ",
	"close()          ",
	"read()           ",
	"write()          ",
	"ioctl()          ",
	"strategy()       ",
	"poll()           ",
	"map()            ",
	"intr()           ",
	"errfunc()        ",
	"halt()           ",
	NULL
};
	
char *module_options[] = {
    "usbuhci ", 
	"usbehci ", 
	"usbohci ", 
	"usbxhci ", 
	"usbcore ", 
	"usbhub  ", 
	NULL
};

char *misc_options[] = { 	
    "gc/dma      ",
	"helper      ",
	"messages    ",
	"core events ",
	"errors      ",
	"warning     ", 
	NULL
};

	
menu_item_t main_menu_top[]={
    "Exit usbutil       ", exit_function     , 'X',
	"Display menu       ", display_main_menu , 'M',
	"Open Device        ", i_open_device     , 'O',
	"Help               ", usbcore_dummy     , 'H',
	NULL, NULL, 0
};

menu_item_t main_menu_bottom[]={
    "Close device       ", close_device      , 0,
    "USBcore tools      ", usbcore_tools     , 0,
    "USB HCD tools      ", usbcore_dummy     , 0,
    "USB device tools   ", usbcore_dummy     , 0,
    "LibUSB tools       ", usbcore_dummy     , 0,
	NULL, NULL, 0
};


menu_item_t generic_menu_top[]={
    "Return to previous ", return_function   , 'R',
	"Display menu       ", display_gen_menu1 , 'M',
	NULL, NULL, 0
};

menu_item_t usbcore_menu_bottom[]={
    "Get driver info    ", driver_info       , 0,
    "Show USB modules   ", show_modules      , 0,
    "Get roothub info   ", root_hub_info     , 0,
	"Set trace cls/lvl  ", set_trace_class   , 0,
    "Get roothub status ", root_hub_status   , 0,
	"Poll ports         ", poll_ports_test   , 0,
	"Show USB devices   ", show_devices      , 0, 
	"Set debug Values   ", set_debug_values  , 0,
	"Run debug op       ", run_debug_op      , 0, 
	NULL, NULL, 0
};

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/ 
int device_fd = -1;


/*
*******************************************************************************************************
* Generic option flags                                                                                *
*******************************************************************************************************
*/ 
int flag_verbose = 0;
int flag_list = 0;
int flag_info = 0;
int flag_status = 0;
int flag_device = 0;
int flag_file = 0;
int flag_all = 0;
int flag_xml = 0;
int flag_shell = 0;


int print_fmt = USBTL_DISPLAY_FMTHUMAN;
int interactive_mode = 0;

/*
*******************************************************************************************************
* Utility functions                                                                                   *
*******************************************************************************************************
*/ 



/*
*******************************************************************************************************
* Line command options parser functions                                                               *
*******************************************************************************************************
*/
int is_option( char *str, char *str2, char *letter){
    char strss[80];
	char ch_op[80];
	
	strcpy( strss, "--");
	strcat( strss, str2);
	strcpy( ch_op, "-");
	strcat( ch_op, letter);
	
    if( (strcmp( str, strss) == 0) || (strcmp( str, ch_op) == 0)){
	    return( 1);
	}

	return( 0);
}

int search_generic_options( int argc, char **argv, int *num_options){
    int i, j =0;
	int selected = 0;
	
	for( i = 2; i < argc; i++){
        if( is_option( argv[i], "verbose", "v")){
            selected |= USBTL_OPTION_VERBOSE;
            flag_verbose = 1;
			j++;
		}else if( is_option( argv[i], "list", "l")){
            selected |= USBTL_OPTION_LIST; 	
            flag_list = 1; 			
			j++;
	    }else if( is_option( argv[i], "info", "i")){
            selected |= USBTL_OPTION_INFO; 	    
			flag_info = 1;
			j++;
	    }else if( is_option( argv[i], "status", "s")){
            selected |= USBTL_OPTION_STATUS; 	    
			flag_status = 1;
			j++;
	    }else if( is_option( argv[i], "device", "d")){
            selected |= USBTL_OPTION_DEVICE; 	    
			flag_device = 1;
			j++;
	    }else if( is_option( argv[i], "file", "f")){
            selected |= USBTL_OPTION_FILE; 	    
			flag_file = 1;
			j++;
	    }else if( is_option( argv[i], "all", "a")){
            selected |= USBTL_OPTION_ALL; 	    
			flag_all = 1;
			j++;
	    }else if( is_option( argv[i], "xml", "x")){
            selected |= USBTL_OPTION_XML; 	    
			flag_xml = 1;
			j++;
	    }else if( is_option( argv[i], "shell", "e")){
            selected |= USBTL_OPTION_SHELL; 	    
			flag_shell = 1;
			j++;
	    }
		
	}
	*num_options = j;
	return( selected);
}

/* parse core submenu options */
int parse_core_options( int argc, char **argv, char **strargs){
    int action = USBTL_ACTION_HELP;
	int generic;
	int generic_num_opts;
	
	if( argc < 3)
	    return( USBTL_ACTION_HELP);
		
	/* Look for generic options. */
	generic = search_generic_options( argc, argv, &generic_num_opts);
	if( generic & USBTL_OPTION_XML)
	    print_fmt = USBTL_DISPLAY_FMTXML;
		
	if( generic & USBTL_OPTION_SHELL)
		print_fmt = USBTL_DISPLAY_FMTSHELL;
		
	argc -= generic_num_opts;
	
	
	/* specified both XML output and shell-friendly output options. */
	if( (generic & USBTL_OPTION_XML) && (generic & USBTL_OPTION_SHELL)){
	    fprintf( stderr, "Error: Can not use XML or Shell friendly output format at same time.\n");
		return( USBTL_ACTION_HELP);
		
	/* got option driver info, number of arguments = 3     # usbtool -c -i        */	
	}else if( ( generic & USBTL_OPTION_INFO) && ( argc == 3)){
	    return( USBTL_ACTION_DRIVER_INFO);
		
	/* got roothub browse option, check for number of arguments */
	}else if( is_option( argv[2], "roothub", "r")){
	    if( generic == 0 && argc == 3){
		    return( USBTL_ACTION_ROOT_HUB_INFO);
		}else if( generic & USBTL_OPTION_STATUS && argc == 3){
		    return( USBTL_ACTION_ROOT_HUB_STATUS);
		}else if( generic & USBTL_OPTION_ALL && argc == 3){
		    return( USBTL_ACTION_ROOT_HUB_INFO_ALL);
		}
	}else if( is_option( argv[2], "tracelevel", "t")){
	    if( argc == 3){
		    return( USBTL_ACTION_DISPLAY_TRACE_LEVEL);
		}else if( argc == 5){
		    strargs[0] = argv[3];
			strargs[1] = argv[4];
			return( USBTL_ACTION_SET_TRACE_LEVEL);
		}
	}else if( is_option( argv[2], "usbdevices", "u")){
	    if( argc > 3){
	        if( is_option( argv[3], "tree", "t")){
		        return( USBTL_ACTION_SHOW_DEVICES_TREE);
			}
		}else{
		    return(USBTL_ACTION_SHOW_DEVICES);
		}
	}else if( is_option( argv[2], "modules", "m")){
	    if( argc == 3){
		    return(USBTL_ACTION_DISPLAY_MODULES);
		}
	}		
	return( action);
}

int parse_line_command( int argc, char **argv, char **strargs){
	int action = USBTL_ACTION_HELP;

		
	if( is_option( argv[1], "help", "h")){
	    return( USBTL_ACTION_HELP);
	}else if( is_option( argv[1], "core", "c")){
	    action = parse_core_options( argc, argv, strargs);
		return( action);
	}else if( is_option( argv[1], "roothub", "r")){
	    action = USBTL_ACTION_HELP;
	}else if( is_option( argv[1], "usbdevice", "u")){
	    action = USBTL_ACTION_HELP;
	}else if( is_option( argv[1], "interactive", "i") && argc == 2){
	    action = USBTL_ACTION_INTERACTIVE;
	}
	
	return( action);
}


int show_help(){
    int i;
    char *help[]={
        "Usage: usbtool <main-options>",
        "",
        "main-options:",
        "-h| --help                              Display this help",
        "-c| --core <core-options>               Use core options ",
        "-r| --roothub <hub-options>             Use roothub options",
        "",
        "",
        "core-options: ",
        "-i,--info                               Display driver information",
        "-r,--roothub                            Display root hub information ",
        "    -a,--all                            Display all the root hub information ",
        "    -s,--status                         Display root hub status",
        "-t,--tracelevel                         Display trace level and class",
        "       <level> <class>                     Set trace level and class",
        "",
        "       level                              Valid values are 0-12. ",
        "                                            0:  minimal quantity of messages",
        "                                            12: trace all messages",
        "",
        "       class                              Specify which functionality to trace",
        "                                           64 bits value for mask. Bits in mask in hex are:",
        "                                           Entry points in kernel modules:",
        "                                            00000000 00000001   : init/register",
        "                                            00000000 00000002   : unload/unregister",
        "                                            00000000 00000004   : attach",
        "                                            00000000 00000008   : dettach",
        "                                            00000000 00000010   : open",
        "                                            00000000 00000020   : close",
        "                                            00000000 00000040   : read",
        "                                            00000000 00000080   : write",
        "                                            00000000 00000100   : ioctl",
        "                                            00000000 00000200   : strategy",
        "                                            00000000 00000400   : poll",
        "                                            00000000 00000800   : map/unmap",
        "                                            00000000 00001000   : intr",
        "                                            00000000 00002000   : err",
        "                                            00000000 00008000   : halt",
        "",
        "                                           Helper functionalities tracing:",
        "                                            00000000 00010000   : memory lists/dma",
        "                                            00000000 00020000   : helper functions",
        "                                            00000000 00040000   : intermodule function calls",
        "                                            00000000 00080000   : usb stack core events",
        "                                            00000000 00100000   : error flag",
        "                                            00000000 00200000   : warning flag",
        "",
        "                                           Module tracing: ",
        "                                            00000001 00000000   : usbuhci",
        "                                            00000002 00000000   : usbehci    ",
        "                                            00000010 00000000   : usbcore",
        "                                            00000020 00000000   : usbhub ",
        "",
        "                                        Examples: ",
        "                                          Trace usbcore module only, load and unload",
        "                                            0x0000001000000003",
        "                                          Trace all in usbehci and usbuhci",
        "                                            0x00000003003fffff",
        "                                          Trace all in all modules ",
        "                                            0xffffffffffffffff",
        "                                          Trace usb core events only",
        "                                            0x0000000000080000",
        "",
        "",
        "-m,--modules                            Display modules list",
        "-u,--usbdevices                         List all the detected USB devices",
        "       -a,--all                            Display full information for USB devices",
        "    -r,--tree                           Display tree of usb devices",
        "-t,--test <testnumber>                  Run test number testnumber",
        "",
        "",
		NULL
	};
	

	for( i = 0; help[i]; i++)
	    puts( help[i]);
	   
    return( 0);
}

/*
*******************************************************************************************************
* Callback functions                                                                                  *
*******************************************************************************************************
*/ 
int exit_function( void *p){
   close_device( p);
   printf("Quitting..\n");
   exit( 0);
   
   return(0); /* should not happen */
}


int open_device( void *p){

#ifdef _EMUIOCTL_
    device_fd = 999;
#else
    device_fd = open( (char *) p, O_RDWR);
    if( device_fd < 0){
        perror("error opening device");
   	    exit( 1);
    }
	
#endif

    return( device_fd);
}

int i_open_device(void *p){
    char device_name[256];
	char option[5];
	int iopt;
	
	if( device_fd != -1){
	    printf("ERROR, Device already open, close it first\n");
		return(0);
	}
	
	printf("Select your USB device file. (Enter to Cancel)\n");
	printf("   1.-Open /hw/usb/usbcore\n");
	printf("   2.-Open /hw/usb/usbdaemon\n");
	
	gets( option);
	iopt = atoi( option);
	
	if( iopt == 1)
	    strcpy( device_name, "/hw/usb/usbcore");
	else if( iopt == 2)
	    strcpy( device_name, "/hw/usb/usbdaemon");
    else{
	    printf("No device file selected. \n");
		return( 0);
    }
	    
    printf("Opening device '%s' \n", device_name);
    device_fd = open_device( (void *) device_name);

    if( device_fd < 0){
        perror("error opening device");
    }else{
        printf("device '%s' opened, fd=%d \n", device_name, device_fd);
    }    
	return(0);
}


int close_device(void *p){
#ifdef _EMUIOCTL_
    device_fd = -1;
#endif
	if( device_fd < 0){
		return(0);
	}
	
    close( device_fd);
	device_fd = -1;
	printf("Device closed successfully\n");
	return(0);
}

int usbcore_dummy(void *p){ 
	return(0);
}

int display_main_menu( void *p){
    display_menu( main_menu_top, main_menu_bottom);
	return(0);
}

int return_function( void *p){
    return( -100);
}

int display_gen_menu1( void *p){
    display_menu( generic_menu_top, usbcore_menu_bottom);
	return(0);
}

int usbcore_tools( void *p){
    run_menu( generic_menu_top, usbcore_menu_bottom);
    return(0);
}

int driver_info(void *p){
    USB_driver_info_t info;
	int rc; 

	
	#ifndef _EMUIOCTL_
        rc = ioctl( device_fd, IOCTL_USB_GET_DRIVER_INFO, &info);	
	#else
	    rc = 0;
		strcpy( info.short_name, "USBCORE");
		strcpy( info.long_name,  "USB stack and host controller driver for SGI IRIX 6.5 <bsdero at gmail dot org>");
		strcpy( info.version,    "0.0.1.5");
		strcpy( info.short_version, "0015");
		strcpy( info.seqn, "14092012162130");
		strcpy( info.build_date, "14092012");
	#endif
	
	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_DRIVER_INFO executed, rc=%d\n", rc);
	}
	
	
	
    if( rc == 0){
   	    printf("<<<<< Driver information >>>>>\n");
        printf("Driver Name: %s\n", info.short_name);
   	    printf("Driver Description: %s\n", info.long_name);
        printf("Driver Version: %s\n", info.version);
        printf("Driver Short Version: %s\n", info.short_version);
        printf("Driver Seq Number: %s\n", info.seqn);
        printf("Driver Build Date: %s\n", info.build_date);	
    }else{
	    printf("ERROR\n");
	}
	return(0);
}  

int show_modules( void *p){
    int i;
	int num_modules;
    USB_driver_module_info_t info;
	int rc; 
	char *module_desc[]={ "HCD", "Core", "DD", NULL };
    #ifdef _EMUIOCTL_
	    USB_driver_module_info_t p_info[] = {
		    {  1, USB_MOD_CORE, "usbcore", "USB Core device driver", "usbcore", USB_DRIVER_IS_CORE },
			{  2, USB_MOD_EHCI, "usbehci", "USB ehci device driver", "usbehci", USB_DRIVER_IS_HCD  },
			{  3, USB_MOD_UHCI, "usbuhci", "USB uhci device driver", "usbuhci", USB_DRIVER_IS_HCD  },
			{  4, USB_MOD_HUB,  "usbhub ", "USB hubb device driver", "usbhub ", USB_DRIVER_IS_USB_DEVICE }
	    };
	#endif

	
	printf("<<<<< USB Kernel Modules >>>>>\n");

	#ifndef _EMUIOCTL_
	    rc = ioctl( device_fd, IOCTL_USB_GET_NUM_MODULES, &num_modules);
	#else
	    rc = 0;
		num_modules = 4;
	#endif
	
	if( flag_verbose == 1){
	    printf("ioctl IOCTL_USB_GET_NUM_MODULES executed, rc=%d, num_modules=%d\n", rc, num_modules);
	}
	
	
	
	if( rc != 0 ){
	    printf("ERROR\n");
	    return( 1);
	}
	
	if( num_modules == 0){
	    printf("No modules found. \n");
		return(0);
	}
	
	printf("Modules list\n");
	printf("ID     Short Name   Module Name        Type     Long Description\n");
	printf("------ ------------ ------------------ -------- ------------------------------\n");
	for( i = 0 ; i < num_modules; i++){
		info.num_module = i;

		#ifndef _EMUIOCTL_
            rc = ioctl( device_fd, IOCTL_USB_GET_MODULE_INFO, &info);	
		#else
	        memcpy( ( void *) &info, (void *) &p_info[i], sizeof( USB_driver_module_info_t));
            rc = 0;
	    #endif
		
		if( flag_verbose == 1){
	        printf("ioctl IOCTL_USB_GET_MODULE_INFO executed, rc=%d\n", rc);
        }
		
	    if( rc == 0){
		    printf("0x%-4x %-12s %-18s %-8s %-30.30s\n", 
			    info.module_id, info.short_description, info.module_name, 
				module_desc[info.type], info.long_description);
        }else{
		    printf("ERROR\n");
			return( -1);
		}
	}
	return(0);
	
}

int root_hub_info( void *p){
#ifndef _EMUIOCTL_
    USB_root_hub_info_t info;
#else
    USB_root_hub_info_t info = {
	    USB_HUB_PCI, 
		0, 
		0, 
		USB_UHCI|USB_EHCI,
		0, 0, 0,
		0xdead,
		0xbeef,
		"Emulated USB host controller",
		4,
		12,
		"/hw/usb/roothub0"
	}; 
#endif	
	int rc; 
	
	printf("<<<<< USB root hub info >>>>>\n");
	#ifndef _EMUIOCTL_
        rc = ioctl( device_fd, IOCTL_USB_GET_ROOT_HUB_INFO, &info);	
	#else
	    rc = 0;
	#endif	
	
	if( flag_verbose == 1){
	    printf("ioctl IOCTL_USB_GET_ROOT_HUB_INFO executed, rc=%d\n", rc);
	}
	
	if( rc == 0){
		printf("Device Name: %s\n", info.hardware_description);
		printf("Product ID: 0x%x\n", info.product_id);
		printf("Vendor ID: 0x%x\n", info.vendor_id);
		printf("Bus type: ");
		if( info.hub_type == USB_HUB_PCI){
		    printf("PCI\n");
    		printf("PCI Bus: 0x%x\n", info.pci_bus);
	    	printf("PCI Slot: 0x%x\n", info.pci_slot);
		    printf("PCI Function: 0x%x\n", info.pci_function);
		}else{
		    printf("USB\n");
		}
		printf("Filesystem device name: %s\n", info.fs_device);
		printf("Number of ports: %d\n", info.ports_number);
		printf("Address: 0x%x\n", info.address);
		printf("Supported host controllers: ");
		if( info.hcd_drivers & USB_UHCI) 
		    printf("UHCI ");
		if( info.hcd_drivers & USB_EHCI)
		    printf("EHCI ");
		if( info.hcd_drivers & USB_OHCI)
		    printf("OHCI ");
		if( info.hcd_drivers & USB_XHCI)
		    printf("XHCI ");
		printf("\n");
		
    }else{
	    printf("ERROR\n");
	}
    return(0);
}


void show_trace_class(){
    USB_trace_class_t trace;
    uint64_t i;
    int rc;
    char str[128];
    
#ifndef _EMUIOCTL_	
    rc = ioctl( device_fd, IOCTL_USB_GET_DEBUG_LEVEL, &trace);	
#else
    rc = 0;
    trace.class = 0xffffffffffffffff;
	trace.level = 5;
#endif
	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_DEBUG_LEVEL executed, rc=%d\n", rc);	
	}
    
	if( rc != 0){
	    printf("ERROR\n");
		return;
	}
	
    INT2HEX64X( str, trace.class);
    printf("Trace values, level=%d, class=%s\n", trace.level, str);
    printf(">> trace of entry points\n");
    for( i = 0; entry_point_options[i] != NULL; i++){
	    printf( "%s : ", entry_point_options[i]);
		if( trace.class & (1 << i))
		    printf("1");
		else
		    printf("0");
		printf("\n");
	}

	printf(">> trace of usb stack modules: \n");
    for( i = 0; module_options[i] != NULL; i++){
        printf( "%s : ", module_options[i]);
		if( trace.class & (TRC_HCD_SET ( ((uint64_t)1) << i)) )
			    printf("1");
			else
			    printf("0");
			printf("\n");		
	}
	
	printf(">> trace of miscelaneous functionality: \n");
    for( i = 0; misc_options[i] != NULL; i++){
        printf( "%s : ", misc_options[i] );
		if( trace.class & ( 1 << (i + 20) ))
			    printf("1");
			else
			    printf("0");
			printf("\n");		
	}
	
}


int set_trace_class_args( char *strlevel, char *strclass){
	int level, rc; 
	uint64_t class, cc;
	USB_trace_class_t trace;
	char str[256];
	
	level = atoi( strlevel);
	rc = str2hex( strclass, &class);
	
	if( rc != 0){
	    printf( "ERROR, invalid values\n");
	    return( 1);
	}
	
	trace.class = class;
	trace.level = level;
	
	cc = class;
	INT2HEX64X( str, cc);


	if( flag_verbose == 1){
    	printf("level = %d, class=%s, strclass=%s\n", level, str, strclass);
	}
	
	
#ifndef _EMUIOCTL_		
    rc = ioctl( device_fd, IOCTL_USB_SET_DEBUG_LEVEL, &trace);	
#else
    rc = 0;
#endif

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_SET_DEBUG_LEVEL executed, rc=%d\n", rc);
    }
	
	if( rc != 0){
	    printf("ERROR\n");
		return(-1);
	}
     
    
	return(0);
}

int set_trace_class( void *p){
	int rc; 		
    USB_trace_class_t trace;
	uint64_t i;
	char option[64];
    int update_trace = 0;
    char str[128];
	
	
#ifndef _EMUIOCTL_	
    rc = ioctl( device_fd, IOCTL_USB_GET_DEBUG_LEVEL, &trace);	
#else
    rc = 0;
    trace.class = 0xffffffffffffffff;
	trace.level = 5;
#endif
	
	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_DEBUG_LEVEL executed, rc=%d\n", rc);	
	}
    
	if( rc != 0){
	    printf("ERROR\n");
		return(-1);
	}
	
   	printf("\n>>>>> Current trace configuration <<<<<\n");
	if( flag_verbose == 1){
        INT2HEX64X( str, trace.class);
        printf("Trace values, level=%d, class=%s\n", trace.level, str);
	} 
	
	if( trace.class == TRC_ALL){
	    printf("All flags enabled\n");
		goto select_trace_opts;
	}else if( trace.class == TRC_NO_MESSAGES){
        printf("All flags disabled\n");
		goto select_trace_opts;
    }else{
	    printf(">> trace of entry points\n");
        for( i = 0; entry_point_options[i] != NULL; i++){
		    printf( "%s : ", entry_point_options[i]);
			if( trace.class & (1 << i))
			    printf("1");
			else
			    printf("0");
			printf("\n");
		}
    }
	
	printf("Press enter to continue"); 
	getchar();
	printf(">> trace of usb stack modules: \n");
    for( i = 0; module_options[i] != NULL; i++){
        printf( "%s : ", module_options[i]);
		if( trace.class & (TRC_HCD_SET ( ((uint64_t)1) << i)) )
			    printf("1");
			else
			    printf("0");
			printf("\n");		
	}
	
	printf(">> trace of miscelaneous functionality: \n");
    for( i = 0; misc_options[i] != NULL; i++){
        printf( "%s : ", misc_options[i] );
		if( trace.class & ( 1 << (i + 20) ))
			    printf("1");
			else
			    printf("0");
			printf("\n");		
	}
	

select_trace_opts:
    printf("\nUpdate trace level? (y/n)[n]:");
	
    gets( option);
	if( tolower(option[0] ) != 'y')
	    goto select_trace_level;
	
    printf("\nUpdate trace class with hex digit?\n");
	printf(" -Yes, I already know the class bit flags\n");
	printf(" -No, I prefer specify each bit of the mask\n");
	printf(" Chosse option (y/n)[y]:");
	
    gets( option);
	if( tolower(option[0] ) != 'n'){
	    printf("Capture the trace class in hexadecimal format: 0x");
	    gets( option);
		sscanf( option, "%x", (unsigned long *)(&trace.class));
	    update_trace = 1;
	    goto select_trace_level;
	}


	
    printf("\nSelect your entry point to trace:\n");
	trace.class = 0;
	for( i = 0; entry_point_options[i] != NULL; i++){
	    printf(" Enable trace %s (current is %d)? (0/1):", entry_point_options[i], ((trace.class & (1 << i)) == 1));
		gets( option);
		if( option[0] == '1') 
	        trace.class |= (uint64_t) (1 << i);
	}


    printf("\nSelect your miscelaneous options to trace:\n");
	for( i = 0; misc_options[i] != NULL; i++){
	    printf(" Enable trace %s (current is %d)? (0/1):", misc_options[i], ((trace.class & ( 1 << (i + 16) )) == 1) );
		gets( option);
		if( option[0] == '1') 
	        trace.class |= (uint64_t) ( 1 << (i + 16) );
	}

    printf("\nSelect your USB stack modules options to trace:\n");
	for( i = 0; module_options[i] != NULL; i++){
	    printf(" Enable trace %s (current is %d)? (0/1):", module_options[i], ((trace.class & ( (uint64_t)1 << (i + 32) )) == (uint64_t)1) );
		gets( option);
		if( option[0] == '1') 
	        trace.class |= (uint64_t) ( (uint64_t)1 << (i + 32) );
	}
	
select_trace_level:
    printf("\nUpdate trace class? (y/n)[n]:");
    gets( option);
	if( tolower(option[0] ) != 'y')
	    goto exit_option;

    printf("Enter your trace level (current: %d): ", trace.level);
	gets( option);
	trace.level = atoi( option);
	update_trace = 1;

    INT2HEX64X( str, trace.class);
    printf("Trace values, level=%d, class=%s\n", trace.level, str);
exit_option:

    if( update_trace == 1){
#ifndef _EMUIOCTL_		
        rc = ioctl( device_fd, IOCTL_USB_SET_DEBUG_LEVEL, &trace);	
#else
        rc = 0;
#endif

	    if( flag_verbose == 1){
            printf("ioctl IOCTL_USB_SET_DEBUG_LEVEL executed, rc=%d\n", rc);	        
		}
		
		if( rc != 0){
		    printf("ERROR\n");
			return( -1);
		}
    }	
	
	
	return(0);
}

int root_hub_status(void *p){
    USB_root_hub_status_t info;
	int rc; 
	int i;
	uint32_t port, n;
	
    printf("<<<<< Root Hub Status >>>>>\n");
#ifndef _EMUIOCTL_			
    rc = ioctl( device_fd, IOCTL_USB_GET_ROOT_HUB_STATUS, &info);	
#else
    rc = 0;
	info.ports_number = 4;
	info.ports_status[0] = USB_PORT_CONNECTED | USB_PORT_ENABLED | USB_PORT_IN_USE | USB_PORT_STATUS_OK | USB_PORT_SET_HCD_OWNER(USB_PORT_EHCI);
	info.ports_status[1] = USB_PORT_STATUS_OK | USB_PORT_SET_HCD_OWNER(USB_PORT_EHCI);
	info.ports_status[2] = USB_PORT_STATUS_OK | USB_PORT_SET_HCD_OWNER(USB_PORT_UHCI);
	info.ports_status[3] = USB_PORT_STATUS_OK | USB_PORT_SET_HCD_OWNER(USB_PORT_EHCI);
	info.hc_condition = USB_HC_OK;
#endif

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_ROOT_HUB_STATUS executed, rc=%d\n", rc);
	}
		
	if( rc == 0){
	    printf("Status :");
	    if( info.hc_condition == USB_HC_OK)
		    printf("OK\n");
		else
		    printf("Error\n");
		
		printf("Number of ports : %d\n", info.ports_number);
	    printf("  ------------ Ports status ------------------\n");
		printf("  <C=Connected E=Enabled S=Suspended I=In use P=Port power>\n");
        printf("  Port No.\tOK\tStatus\tSpeed\tHC Owner\n");
        for( i = 0; i < info.ports_number; i++){
            port = info.ports_status[i];
			printf("  #%d\t\t", i);
			printf("%c\t", ((port & USB_PORT_STATUS_OK) == 0) ? 'N' : 'Y'); 
            if( ( port & USB_PORT_CONNECTED) != 0)
			    printf("C");
            if( ( port & USB_PORT_ENABLED) != 0)
			    printf("E");
            if( ( port & USB_PORT_SUSPENDED) != 0)
			    printf("S");
            if( ( port & USB_PORT_IN_USE) != 0)
			    printf("I");
            if( ( port & USB_PORT_POWER) != 0)
			    printf("P");
			
			printf("\t");
			n = USB_PORT_GET_SPEED( port);
			switch( n){
			    case USB_PORT_FULL_SPEED: printf("Full"); break;
				case USB_PORT_LOW_SPEED:  printf("Low");  break;
				case USB_PORT_HIGH_SPEED: printf("High"); break;
			}
			printf("\t");
			
			n = USB_PORT_GET_HCD_OWNER( port);
			switch( n){
			    case USB_PORT_UHCI: printf("UHCI"); break;
			    case USB_PORT_EHCI: printf("EHCI"); break;
			    case USB_PORT_OHCI: printf("OHCI"); break;
			    case USB_PORT_XHCI: printf("XHCI"); break;
			}
			printf("\n");
        }
    }else{
	    printf("ERROR\n");
		return( -1);
	}
	
	return(0);
}  

int poll_ports_test(void *p){

	
    printf("<<<<< Poll ports test >>>>>\n");
 /*   rc = ioctl( device_fd, IOCTL_USB_GET_ROOT_HUB_STATUS, &info);	
	printf("ioctl IOCTL_USB_GET_ROOT_HUB_STATUS executed, rc=%d\n", rc);
*/

	
	return(0);
}  


int show_devices( void *p){
    int usb_devices_num, i, rc;
	USB_device_t USB_device;
#ifdef _EMUIOCTL_		
	USB_device_t USB_device_f[] = {
	    { 0, "USB roothub VIA VT6212", "/hw/usb/roothub0", 0x0bad, 0xbabe, 
		0, 0, 0x10, -1, -1, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"VIA VT6212", 0, "VIA", "VIA VT6212 4 ports PCI roothub", "usbhub.o", 
		5, 0, 0 },
	    { 1, "USB roothub conexant", "/hw/usb/roothub1", 0xdead, 0xbeef, 
		5, 0, 0x11, 0, 1, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Conexant 1234", 0, "Conexant", "Conexant 4 ports USB roothub", "usbhub.o", 
		5, 0, 1 },
	    { 2, "Generic USB mouse", "/hw/usb/mouse0", 0x0bad, 0xcafe, 
		6, 0, 0x12, 0, 2, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Generic mouse", 0, "Generic", "Generic USB Mouse", "usbmouse.o", 
		6, 0, 0 },
	    { 3, "Generic USB keyboard", "/hw/usb/keyboard0", 0xb16b, 0x00b5, 
		7, 0, 0x13, 0, 3, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Generic keyboard", 0, "Generic", "Generic USB Keyboard", "usbkeyb.o", 
		7, 0, 0 },
	    { 4, "Kingston Data Traveller 2.0", "/hw/usb/umass0", 0xface, 0xb00c, 
		8, 0, 0x14, 1, 1, 
		USB_DEVICE_STATUS_ERROR ,0 ,0 ,0 ,
		"Kingston umass", 0, "Kingston", "Kingston Data Traveller 2.0 storage", "umass.o", 
		8, 0, 0 }		
	};
#endif

	char *t[10] = {
	    "", "", "", "", "", "hub", "mouse", "keyboard", "umass", "" };
		
    
	printf("<<<<< Devices list >>>>>\n");
#ifndef _EMUIOCTL_		
    rc = ioctl( device_fd, IOCTL_USB_GET_DEVICES_NUM, &usb_devices_num);	
#else
    rc = 0;
	usb_devices_num = 5;
#endif
	
	

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_DEVICES_NUM executed, rc=%d, num_modules=%d\n", rc, usb_devices_num);
	}
		

	if( rc != 0 || usb_devices_num == 0){
	    printf("ERROR\n");
	    return( 1);
	}
	

	if( flag_all == 0){
	    printf("No.  Device ID  Short Name   Module Name  Sts Filesystem device name\n");
	    printf("---- ---------- ------------ ------------ --- ------------------------------------\n");
	}
	for( i = 0; i < usb_devices_num; i++){  
	#ifndef _EMUIOCTL_
		rc = ioctl( device_fd, IOCTL_USB_GET_DEVICE, &USB_device);	
	#else
	    memcpy( (void *) &USB_device, (void *) &USB_device_f[i], sizeof(USB_device_t ));
	#endif
    	if( flag_verbose == 1){
            printf("ioctl IOCTL_USB_GET_DEVICE executed, rc=%d\n", rc);
	    }
	
		if( flag_all == 0){
 	        printf("0x%-2x 0x%-8x %-12s %-12s %s %-33.33s\n", 
		        USB_device.device_index, USB_device.instance_id, t[USB_device.module_id],
			    USB_device.module_name, 
				((USB_device.status == USB_DEVICE_STATUS_OK) ? "OK " : "ERR" ), 
				USB_device.fs_device    );
		}else{
		    printf("Device #%d:\n", USB_device.device_index);
			printf("    InstanceID: 0x%x, Devicetype: %s #%d, ModuleName: %s\n", 
			    USB_device.instance_id, t[USB_device.module_id], 
				USB_device.spec_device_instance_id, USB_device.module_name); 
			printf("    FileSystemDeviceName: %s\n", USB_device.fs_device);
		    printf("    Long device name: %s\n", USB_device.hardware_description);
			printf("    Vendor: %s, DeviceID: 0x%x, VendorID: 0x%x, ClassID: 0x%x\n", 
			     USB_device.vendor_str, USB_device.device_id, USB_device.vendor_id, USB_device.class_id);
		    printf("    Status: %s\n", ((USB_device.status == USB_DEVICE_STATUS_OK) ? "OK " : "ERROR" ));
			if( USB_device.module_id == 5 && USB_device.hub_index == -1){
				    printf("    RootHub device #0\n");
			}else{	
                    printf("    Device is %s, connected to hub #%d, port %d\n", 
					    t[USB_device.module_id], USB_device.hub_index, USB_device.port_index);
			
			}
			
		}
	}
	
    return(0);
}


void display_hub( USB_device_t *buf, int usb_devices_num, int hub_index, int level){
    int i;
	int j;
	char *t[10] = { "", "", "", "", "", "hub", "mouse", "keyboard", "umass", "" };
	
			
	for( i = 0; i < usb_devices_num; i++){
	
	    if( buf[i].hub_index == hub_index){
        	for( j = 0; j < level; j++)
    	        printf("    ");

		    printf("hub #%d port %d: %s #%d, 0x%x, %s\n", 
			    hub_index,
			    buf[i].port_index,
			    t[buf[i].module_id], 
				buf[i].spec_device_instance_id, 
				buf[i].instance_id, 
				buf[i].hardware_description); 
		}

        if( buf[i].module_id == 5 && buf[i].hub_index == hub_index){
		    display_hub( buf, usb_devices_num, hub_index + 1, level + 1);
		}
		
	}
	
/*
		if( flag_all == 0){
 	        printf("0x%-2x 0x%-8x %-12s %-12s %s %-33.33s\n", 
		        buf[i].device_index, buf[i].instance_id, t[buf[i].module_id],
			    buf[i].module_name, 
				((buf[i].status == USB_DEVICE_STATUS_OK) ? "OK " : "ERR" ), 
				buf[i].fs_device    );
		}else{
		    printf("Device #%d:\n", buf[i].device_index);
			printf("    InstanceID: 0x%x, Devicetype: %s #%d, ModuleName: %s\n", 
			    buf[i].instance_id, t[buf[i].module_id], 
				buf[i].spec_device_instance_id, buf[i].module_name); 
			printf("    FileSystemDeviceName: %s\n", buf[i].fs_device);
		    printf("    Long device name: %s\n", buf[i].hardware_description);
			printf("    Vendor: %s, DeviceID: 0x%x, VendorID: 0x%x, ClassID: 0x%x\n", 
			     buf[i].vendor_str, buf[i].device_id, buf[i].vendor_id, buf[i].class_id);
		    printf("    Status: %s\n", ((buf[i].status == USB_DEVICE_STATUS_OK) ? "OK " : "ERROR" ));
			if( buf[i].module_id == 5 && buf[i].hub_index == -1){
				    printf("    RootHub device #0\n");
			}else{	
                    printf("    Device is %s, connected to hub #%d, port %d\n", 
					    t[buf[i].module_id], buf[i].hub_index, buf[i].port_index);
			
			}
			
		}	

*/	   
}


int show_devices_tree( void *p){
    int usb_devices_num, i, rc;
	
	USB_device_t USB_device;

#define USBTL_MAX_DEVICES_TREE                     64
	USB_device_t *p_devices[USBTL_MAX_DEVICES_TREE];     
    USB_device_t *buf;
	int hubs_num;

	

#ifdef _EMUIOCTL_		
	USB_device_t USB_device_f[] = {
	    { 0, "USB roothub VIA VT6212", "/hw/usb/roothub0", 0x0bad, 0xbabe, 
		0, 0, 0x10, -1, -1, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"VIA VT6212", 0, "VIA", "VIA VT6212 4 ports PCI roothub", "usbhub.o", 
		5, 0, 0 },
	    { 1, "USB roothub conexant", "/hw/usb/roothub1", 0xdead, 0xbeef, 
		5, 0, 0x11, 0, 1, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Conexant 1234", 0, "Conexant", "Conexant 4 ports USB roothub", "usbhub.o", 
		5, 0, 1 },
	    { 2, "Generic USB mouse", "/hw/usb/mouse0", 0x0bad, 0xcafe, 
		6, 0, 0x12, 0, 2, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Generic mouse", 0, "Generic", "Generic USB Mouse", "usbmouse.o", 
		6, 0, 0 },

	    { 3, "Generic USB keyboard", "/hw/usb/keyboard0", 0xb16b, 0x00b5, 
		7, 0, 0x13, 0, 3, 
		USB_DEVICE_STATUS_OK ,0 ,0 ,0 ,
		"Generic keyboard", 0, "Generic", "Generic USB Keyboard", "usbkeyb.o", 
		7, 0, 0 },
		
	    { 4, "Kingston Data Traveller 2.0", "/hw/usb/umass0", 0xface, 0xb00c, 
		8, 0, 0x14, 1, 1, 
		USB_DEVICE_STATUS_ERROR ,0 ,0 ,0 ,
		"Kingston umass", 0, "Kingston", "Kingston Data Traveller 2.0 storage", "umass.o", 
		8, 0, 0 },
		
	    { 4, "Kingston Data Traveller 1.0", "/hw/usb/umass1", 0xface, 0xb00c, 
		8, 0, 0x19, 1, 2, 
		USB_DEVICE_STATUS_ERROR ,0 ,0 ,0 ,
		"Kingston umass", 0, "Kingston", "Kingston Data Traveller 1.0 storage", "umass.o", 
		8, 0, 1 }		
	};
#endif

	char *t[10] = {
	    "", "", "", "", "", "hub", "mouse", "keyboard", "umass", "" };
		
    
	printf("<<<<< Devices list >>>>>\n");
#ifndef _EMUIOCTL_		
    rc = ioctl( device_fd, IOCTL_USB_GET_DEVICES_NUM, &usb_devices_num);	
#else
    rc = 0;
	usb_devices_num = 6;
#endif
	
	

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_GET_DEVICES_NUM executed, rc=%d, num_modules=%d\n", rc, usb_devices_num);
	}
		

	if( rc != 0 || usb_devices_num == 0){
	    printf("ERROR\n");
	    return( 1);
	}
	
	buf = (USB_device_t *) malloc( sizeof(USB_device_t) * usb_devices_num);
	
	memset( (void *) buf, 0, sizeof( USB_device_t) * usb_devices_num);
	memset( (void *) &p_devices, 0, sizeof( USB_device_t *) * USBTL_MAX_DEVICES_TREE);
	
	hubs_num = 0;
	for( i = 0; i < usb_devices_num; i++){  
	#ifndef _EMUIOCTL_
		rc = ioctl( device_fd, IOCTL_USB_GET_DEVICE, &USB_device);	
	#else
	    memcpy( (void *) &USB_device, (void *) &USB_device_f[i], sizeof(USB_device_t ));
	#endif
	
    	if( flag_verbose == 1){
            printf("ioctl IOCTL_USB_GET_DEVICE executed, rc=%d\n", rc);
	    }

	    memcpy( (void *) &buf[i], (void *) &USB_device, sizeof(USB_device_t ));
		if( buf[i].module_id == 5)
		    hubs_num++;
		
	}


	for( i = 0; i < hubs_num; i++){  
        if( buf[i].module_id == 5 && buf[i].hub_index == -1){
		    printf("RootHub -");
	        printf("%s #%d, 0x%x, %s\n", 
			    t[buf[i].module_id], 
				buf[i].spec_device_instance_id, 
				buf[i].instance_id, 
				buf[i].hardware_description); 
			
			display_hub( buf, usb_devices_num, 0, 1);
		}
		
	}
	
	
	free( buf);
	
    return(0);
}





















int set_debug_values(void *p){
    usb_debug_values_t v;
	int rc; 
	char option[80];

	
    printf("<<<<< Set Debug Values >>>>>\n");
    printf("Type debug values case id : ");
    gets( option);
	sscanf( option, "%d", (int *)(&v.id));
	
	printf("\nType port number : ");
    gets( option);
	sscanf( option, "%d", (int *)(&v.values[0]));
	
	printf("\nType port root reset delay [50]: ");
    gets( option);
	sscanf( option, "%d", (int *)(&v.values[1]));
	
	printf("\nType port reset recovery delay [10]: ");
    gets( option);
	sscanf( option, "%d", (int *)(&v.values[2]));
	
	printf("\nType port reset delay [50]: ");
    gets( option);
	sscanf( option, "%d", (int *)(&v.values[3]));
	

#ifndef _EMUIOCTL_			
    rc = ioctl( device_fd, IOCTL_USB_SET_DEBUG_VALUES, &v);	
#else
    rc = 0;
#endif

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_SET_DEBUG_VALUES executed, rc=%d\n", rc);
	}
		
	if( rc == 0){
		printf("OK\n");
    }else{
	    printf("ERROR\n");
		return( -1);
	}
	
	return(0);
}  




int run_debug_op(void *p){
    uint32_t op;
	int rc, i; 
	char option[80];

	
    printf("<<<<< Run Debug Op >>>>>\n");
    printf("Type debug op id : ");
    gets( option);
	sscanf( option, "%d", (int *)(&op));
	
	
    printf("************************ WARNING *************************\n");
    printf("This runs debug operations that may hang or crash the\n"); 
    printf("machine. sync() calls will be executed now.\n");
    printf("Continue? (y/n)[n]:");
	
    gets( option);
	if( tolower(option[0] ) != 'y')
	    goto exit_debug_op;
	

    for( i = 0; i < 10; i++){
	    printf("syncing...\n");
	    usleep( 200000);
	    sync(); 	
	}
    
    sleep(1);
    
#ifndef _EMUIOCTL_			
    rc = ioctl( device_fd, IOCTL_USB_RUN_DEBUG_OP, &op);	
#else
    rc = 0;
#endif

	if( flag_verbose == 1){
        printf("ioctl IOCTL_USB_RUN_DEBUG_OP executed, rc=%d\n", rc);
	}
		
	if( rc == 0){
		printf("OK\n");
    }else{
	    printf("ERROR\n");
		return( -1);
	}

exit_debug_op:    	
	return(0);
}  

/*
*******************************************************************************************************
* Menu functions                                                                                      *
*******************************************************************************************************
*/
int display_menu(void *items1, void *items2){
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|                    USBCORE FOR SGI IRIX 6.5 TEST UTILITY                    |\n");
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|                           << GENERAL  COMMANDS >>                           |\n");
    display_menu_part( items1);
	if( device_fd != -1){
        printf("+-------------------------+-------------------------+-------------------------+\n");
	    display_menu_part( items2);
    }
    printf("+-------------------------+-------------------------+-------------------------+\n");
    return(0);	
}
 
int display_menu_part( void *menu_items){
    menu_item_t *item;
    int i;
	
	item = ( menu_item_t *) menu_items;
	i = 0;
	
	while( item[i].caption != NULL){
	    if( item[i].key == 0)
	        printf("| [%02d]%s  ", i, item[i].caption);
		else
		    printf("| [%c] %s  ", item[i].key, item[i].caption);
		i++;
		
		if( item[i].caption == NULL){
		    printf("                                                   |\n");
			break;
		}else{
  	        if( item[i].key == 0)
		        printf(" [%02d]%s  ", i, item[i].caption);
 		    else
		        printf(" [%c] %s  ", item[i].key, item[i].caption);
		}
		i++;
		
		if( item[i].caption == NULL){
		    printf("                         |\n");
			break;
		}else{
		    if( item[i].key == 0)
		        printf(" [%02d]%s |\n", i, item[i].caption);
			else
			    printf(" [%c] %s |\n", item[i].key, item[i].caption);
		}
		i++;
		
	}
	
    return(0);
}

int menu_size( menu_item_t *menu){
    int i = 0;
	menu_item_t *m = menu;
	
	for( ; m[i].caption != NULL; i++);
	return(i);
}



int run_menu(menu_item_t *items1, menu_item_t *items2){
	char s[80];
	int i, opt;
	int rc;
	
	display_menu( items1, items2);	
	do{
        
	    printf("\n Option select ('M' for menu) >> ");
	    gets( s);
	    if( isalpha( s[0])){
	        for( i = 0; items1[i].caption != NULL; i++){
			    if( items1[i].key == toupper(s[0]))
				    rc = items1[i].func( NULL);
	        }
	    }else{
            if( device_fd != -1){		
                if( isdigit(s[0])){
		            opt = atoi( s);
			        if( opt < menu_size( items2))
			            rc = items2[opt].func( NULL);
				}
			}
		}
	    
		if( rc == -100)
		    break;
	}while(1);
	return(0);
}



int main(int argc, char **argv){
    int action; 
	int i;
	char *str_args[8];
	
	
	if( argc > 1){ 
	    for( i = 0; i < argc; i++)
            action = parse_line_command( argc, argv, str_args);
    }else{
	    action = USBTL_ACTION_HELP;	
	}

    switch( action){
	    case USBTL_ACTION_INTERACTIVE:
		    run_menu( main_menu_top, main_menu_bottom);
			interactive_mode = 1;
		break;
		case USBTL_ACTION_HELP:
		    show_help();
		break;
		case USBTL_ACTION_DRIVER_INFO:
	        open_device( "/hw/usb/usbcore");
			driver_info(NULL);
		break;
		case USBTL_ACTION_DISPLAY_MODULES:
	        open_device( "/hw/usb/usbcore");
			show_modules(NULL);
		break;
		case USBTL_ACTION_ROOT_HUB_INFO_ALL:
		case USBTL_ACTION_ROOT_HUB_INFO:
	        open_device( "/hw/usb/usbcore");
			root_hub_info( NULL);
		break;
		case USBTL_ACTION_ROOT_HUB_STATUS:
	        open_device( "/hw/usb/usbcore");
			root_hub_status( NULL);
		break;
		case USBTL_ACTION_DISPLAY_TRACE_LEVEL:
	        open_device( "/hw/usb/usbcore");
            show_trace_class();
		break;
		case USBTL_ACTION_SET_TRACE_LEVEL:
	        open_device( "/hw/usb/usbcore");
		    set_trace_class_args( str_args[0], str_args[1]);
		break;
		case USBTL_ACTION_SHOW_DEVICES:
	        open_device( "/hw/usb/usbcore");
			printf("show devices selected\n");
			show_devices(NULL);
		break;
		case USBTL_ACTION_SHOW_DEVICES_TREE:
	        open_device( "/hw/usb/usbcore");
			printf("show devices tree selected\n");
			show_devices_tree( NULL);
		break;
	}
		/*
	if( action == USBTL_ACTION_SET_TRACE_LEVEL)
	    printf("arg0 = %s, arg1 = %s\n", str_args[0], str_args[1]);
*/
    if( device_fd >= 0)
	    close( device_fd);
}

