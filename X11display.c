/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: X11display.c                                               *
* Description: X11 display basic functions code                    *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-10-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include "X11display.h"


int X11_display_init( X11_display_t *display){
	/* First connect to the display server, as specified in the DISPLAY 
	environment variable. */
	
	display->dpy = XOpenDisplay(NULL);
	if (!display->dpy) {
	    fprintf(stderr, "unable to connect to display ");
		return( -1);
	}
		
    
    
	/* these are macros that pull useful data out of the display object */
	/* we use these bits of info enough to want them in their own variables */
	display->screen_num = DefaultScreen(display->dpy);
	display->width = DisplayWidth( display->dpy, DefaultScreen( display->dpy)); 
	display->height = DisplayHeight( display->dpy, DefaultScreen( display->dpy)); 
    display->cl_border = WhitePixel( display->dpy, display->screen_num);
	display->cl_background = BlackPixel( display->dpy, display->screen_num);

    return( 0);
}

int X11_display_close( X11_display_t *display){
    XCloseDisplay(display->dpy);
	return(0);
}

