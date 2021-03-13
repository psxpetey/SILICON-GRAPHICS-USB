/*
********************************************************************
* USB Core Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbd.c                                                     *
* Description: USB core daemon and monitor                         *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      10-05-2012     -Added writing to log /var/adm/usblog                                    *
*                                                                                                     *
* BSDero      09-12-2012     -Initial version (Parts rippen from previous tools)                      *
*                                                                                                     *
*******************************************************************************************************
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
#include <sys/time.h>

#include "usbioctl.h"
#include "kutils.c"
#include "kvarr.h"

int device_fd = -1;
int verbose_flag = 0;
FILE *flog;

#define USBLOG                   "/var/adm/usblogs"
#ifdef _EMUIOCTL_
   int flag = 0;
#endif


int open_device( void *p);
int close_device( void *p);

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
    if( verbose_flag != 0)
        printf("Device opened successfully\n");
    return( device_fd);
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
	
    if( verbose_flag != 0)
        printf("Device closed successfully\n");
	return(0);
}

int usb_suscribe_events(){
    uint32_t events = (uint32_t) USB_EVENT_PORT_CONNECT | USB_EVENT_PORT_DISCONNECT;
    int rc = 0;

#ifndef _EMUIOCTL_
    rc = ioctl( device_fd, IOCTL_USB_SUSCRIBE_EVENTS, &events);	
#else
    rc = 0;
#endif	
	
    if( verbose_flag != 0)
        printf("rc = %d, ioctl IOCTL_USB_SUSCRIBE_EVENTS executed.\n", rc);
	if( rc != 0){
	    exit(-1);
	}    
	
	return( rc);
}

int usb_poll_start(){
    int rc;
#ifndef _EMUIOCTL_
    rc = ioctl( device_fd, IOCTL_USB_POLL_START, NULL);	
#else
    rc = 0;
#endif	
    if( verbose_flag != 0)
        printf("rc = %d, ioctl IOCTL_USB_POLL_START executed.\n", rc);
		
	if( rc != 0){
	    exit(-1);
	}
	
	return( rc);	
}


int usb_poll_end(){
    int rc;
#ifndef _EMUIOCTL_
    rc = ioctl( device_fd, IOCTL_USB_POLL_END, NULL);	
#else
    rc = 0;
#endif	
    if( verbose_flag != 0)
        printf("rc = %d, ioctl IOCTL_USB_POLL_END executed.\n", rc);
	if( rc != 0){
	    exit(-1);
	}
	
	return( rc);	
}

int usb_get_pending_events_num(){
    int n, rc;
#ifndef _EMUIOCTL_
    rc = ioctl( device_fd, IOCTL_USB_POLL_GET_EVENTS_NUM, &n );	
#else
    rc = 0;
	n = 2;
#endif	
    if( verbose_flag != 0)
        printf("rc = %d, ioctl IOCTL_USB_POLL_GET_EVENTS_NUM executed, num of events = %d.\n", rc, n);
	if( rc != 0){
	    exit(-1);
	}
	
	return( n);	
}



int usb_get_next_event(){
    int rc;
	usb_event_t usb_event;
	
#ifndef _EMUIOCTL_
    rc = ioctl( device_fd, IOCTL_USB_POLL_GET_NEXT_EVENT, &usb_event );	
#else
    rc = 0;
	flag = !flag;
	
	if( flag == 0){
	    usb_event.event_id = USB_EVENT_PORT_CONNECT;
		usb_event.param = 1;
	}else{
	    usb_event.event_id = USB_EVENT_PORT_DISCONNECT;
		usb_event.param = 2;
	}
#endif
    if( verbose_flag != 0)
	    printf("rc = %d, ioctl IOCTL_USB_POLL_GET_NEXT_EVENT executed, event id = %d.\n", rc, usb_event.event_id);

		if( rc != 0){
	    exit(-1);
	}

		
	if( usb_event.event_id == USB_EVENT_PORT_CONNECT)
	    fprintf( flog, "I:Port %d connected\n", usb_event.param);
	else if ( usb_event.event_id == USB_EVENT_PORT_DISCONNECT)
	    fprintf( flog, "W:Port %d disconnected\n", usb_event.param);
		
	fflush( flog);
	
	return( rc);	
}


int main( int argc, char **argv){
    int i, event_fd;
    int num_events;
	int quit = 0;
	int stdin_fd = fileno( stdin);
	fd_set in_fds;
	struct timeval tv;
	int error = 1;

	
	if( argc == 1)
	    error = 0;
	
    if( argc == 2){
	    if( strcmp( argv[1], "-v") == 0){
		    verbose_flag = 1;
			error = 0;
	    }
	}
	
	if( error != 0){
	    printf("Usage: usbd [-v]\n");
		exit( -1);
	}
	
    open_device( "/hw/usb/usbdaemon");
	flog = fopen( USBLOG, "a");
	
	if( flog == NULL){
	    perror("Could not open file:");
		exit( -1);
	}
	
    usb_suscribe_events();
    usb_poll_start();
    	
    do{
        num_events = usb_get_pending_events_num();
		for( i = 0; i < num_events; i++){
		    usb_get_next_event();
		}

    	FD_ZERO( &in_fds);
		FD_SET( stdin_fd, &in_fds);
		FD_SET( device_fd, &in_fds);
        tv.tv_usec = 0;
        tv.tv_sec = 1;

#ifndef _EMUIOCTL_
        
        event_fd = select( device_fd + 1, &in_fds, 0, 0, &tv);
        if( FD_ISSET( stdin_fd, &in_fds) ){
		    quit = 1;
			break;
		}else if( FD_ISSET( device_fd, &in_fds) ){
            continue;   
        }else if( event_fd == 0){
		    if( verbose_flag != 0)
                printf(" timeout\n");
		}
#else
        sleep(2);
#endif
    }while( quit == 0);

    usb_poll_end();

    close_device( NULL);
	fclose( flog);
    return(0);
}
