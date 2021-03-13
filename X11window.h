/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: X11window.h                                                *
* Description: X11 windows basic functions header                  *
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
#ifndef _X11WINDOW_H_
#define _X11WINDOW_H_

#include "X11display.h"



typedef struct{ 
    X11_display_t *display;
	Window win;
	int posx, posy, width, height;
    int sz_border;
    unsigned long cl_background, cl_border;
	unsigned long event_mask;
	XEvent ev;
}X11_window_t;


int X11_window_init( X11_display_t *display, X11_window_t *window);
int X11_window_set_geometry( X11_window_t *window, int width, int height, int posx, int posy, int border_width);
int X11_window_set_colors( X11_window_t *window, unsigned long cl_border, unsigned long int cl_background);
int X11_window_set_mask( X11_window_t *window, unsigned long mask);
int X11_window_display( X11_window_t *window);
int X11_window_close( X11_window_t *window);
int X11_window_destroy( X11_window_t *window);
unsigned long int X11_window_get_next_event( X11_window_t *window);
int X11_window_set_wm_name( X11_window_t *window, char *window_title);



#endif
