/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: X11display.h                                               *
* Description: X11 display basic functions header                  *
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
#ifndef _X11DISPLAY_H_
#define _X11DISPLAY_H_


/* first include the standard headers that we're likely to need */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>

#define MOUSE_ALL_EVENTS_MASK       ( ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask)
#define WINDOW_ALL_EVENTS_MASK      ( StructureNotifyMask | ExposureMask | SubstructureNotifyMask)
#define KEYBOARD_ALL_EVENTS_MASK    ( KeyPressMask | KeyReleaseMask )

typedef struct{
    Display *dpy;
	int width, height;
	int screen_num;
	unsigned long cl_background, cl_border;
}X11_display_t;

int X11_display_init( X11_display_t *display);
int X11_display_close( X11_display_t *display);

#endif
