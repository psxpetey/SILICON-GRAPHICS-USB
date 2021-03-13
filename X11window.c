/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: X11window.c                                                *
* Description: X11 windows basic functions code                    *
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

#include "X11window.h"
#include <string.h>

int X11_window_init( X11_display_t *display, X11_window_t *window){
    memset( (void *) window, 0, sizeof( X11_window_t));
	
	window->display = display; 
	window->cl_border = display->cl_border;
	window->cl_background = display->cl_background;
	return(0);
}



int X11_window_set_geometry( X11_window_t *window, int width, int height, int posx, int posy, int border_width){
    window->posx = posx;
	window->posy = posy;
	window->width = width;
	window->height = height;
    window->sz_border = border_width;
	return(0);
}

int X11_window_set_colors( X11_window_t *window, unsigned long cl_border, unsigned long int cl_background){
    window->cl_border = cl_border;
	window->cl_background = cl_background;
	return(0);
}

int X11_window_set_mask( X11_window_t *window, unsigned long mask){
    window->event_mask = mask;
	return(0);
}

int X11_window_display( X11_window_t *window){
	window->win = XCreateSimpleWindow( window->display->dpy, DefaultRootWindow( window->display->dpy), /* display, parent */
		window->posx, window->posy, /* x, y: the window manager will place the window elsewhere */
		window->width, window->height, /* width, height */
		window->sz_border, window->cl_border, /* border width & colour, unless you have a window manager */
		window->cl_background); /* background colour */

	/* tell the display server what kind of events we would like to see */
	XSelectInput( window->display->dpy, window->win, window->event_mask );

	/* okay, put the window on the screen, please */
	XMapWindow( window->display->dpy, window->win);
	XMoveWindow( window->display->dpy, window->win, window->posx, window->posy);

    XFlush(  window->display->dpy);
	return(0);
    
}

int X11_window_close( X11_window_t *window){
    XUnmapWindow( window->display->dpy , window->win);
	return(0);
}

int X11_window_destroy( X11_window_t *window){
    XDestroyWindow( window->display->dpy , window->win);
	return(0);
}

unsigned long int X11_window_get_next_event( X11_window_t *window){
    XNextEvent( window->display->dpy, &window->ev);
	return( window->ev.type);
}


int X11_window_set_wm_name( X11_window_t *window, char *window_title){
    XSizeHints  xsh;		/* Size hints for window manager */
    
    xsh.flags = (PPosition|PSize);
    xsh.height = window->height;
    xsh.width = window->width;
    xsh.x = window->posx;
    xsh.y = window->posy;
    
    XSetStandardProperties(window->display->dpy, window->win, window_title, window_title, None, NULL, 0, &xsh);	
	return(0);
	
}
