/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: xusbnotifier.c                                             *
* Description: X11 USB notifier source code                        *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      10-05-2012     -Read from /var/adm/usblog instead stdin                                 *
*                                                                                                     *
* BSDero      09-10-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#define XK_LATIN1
#define XK_MISCELLANY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include "X11display.h"
#include "X11window.h"
#include "arrstr.h"

#define USBLOG                   "/var/adm/usblog"
typedef struct{
    X11_window_t *win;
	GC gc;
	XGCValues values;
	unsigned long value_mask;
	XFontStruct* font_info;
	char font_name[256];
}X11_gc_t;



typedef struct{
    int posx, posy;
	
#define MBUTTON_RIGHT           0x01
#define MBUTTON_CENTER          0x02
#define MBUTTON_LEFT            0x04          	
	int button_status;
}mouse_status_t;

FILE *flog;
arrstr_t strings;

int X11_gc_init( X11_gc_t *gc, X11_window_t *w);

int X11_gc_init( X11_gc_t *gc, X11_window_t *w){
    gc->win = w;
    gc->values.foreground = BlackPixel( w->display->dpy, w->display->screen_num);
	gc->values.line_width = 1;
	gc->values.line_style = LineSolid;
	gc->value_mask = GCForeground | GCLineWidth | GCLineStyle;
	gc->gc = XCreateGC( w->display->dpy, w->win, gc->value_mask, &gc->values);
    strcpy( gc->font_name, "*-courier-*-12-*");
    gc->font_info = XLoadQueryFont(w->display->dpy, gc->font_name);
	if ( !gc->font_info) {
        fprintf( stderr, "XLoadQueryFont: failed loading font '%s'\n", gc->font_name);
		return( -1);
	}

    XSetFont(w->display->dpy, gc->gc, gc->font_info->fid);	
	return(0);
}


int draw_status_bars( X11_gc_t *gc, char **arr){
#define COLRED      0
#define COLGREEN    1
#define COLYELLOW   2
#define COLCYAN     3
#define COLWHITE    4
    int i;
    char *name_colors[] = { "red", "green", "yellow", "cyan", "white" };
    Visual* default_visual;
    XColor X11Colors[5];
    XColor exact_color;
    Colormap my_colormap;
    int type;
    char *s;
    X11_window_t *w = gc->win;
   
    default_visual = DefaultVisual(w->display->dpy, DefaultScreen(w->display->dpy));   
    my_colormap = XCreateColormap(w->display->dpy, w->win, default_visual, AllocNone);   
   
    for( i = 0; i < 5; i++){
        /* allocate a color map entry. */
        XAllocNamedColor( w->display->dpy, my_colormap, name_colors[i], &X11Colors[i], &exact_color);
   
    }
    /* change the foreground color of this GC to white. */
    XSetForeground( w->display->dpy, gc->gc, BlackPixel( w->display->dpy, w->display->screen_num));
    XFillRectangle( w->display->dpy, w->win, gc->gc, 0, 0, w->width , w->height);
    /* change the background color of this GC to black. */
    XSetBackground( w->display->dpy, gc->gc, WhitePixel( w->display->dpy, w->display->screen_num));

    /* change the fill style of this GC to 'solid'. */
    XSetFillStyle(w->display->dpy, gc->gc, FillSolid);

    /* change the line drawing attributes of this GC to the given values. */
    /* the parameters are: Display structure, GC, line width (in pixels), */
    /* line drawing  style, cap (line's end) drawing style, and lines     */
    /* join style.                                                        */
    XSetLineAttributes(w->display->dpy, gc->gc, 3, LineSolid, CapRound, JoinRound);   
    XSetForeground( w->display->dpy, gc->gc, WhitePixel( w->display->dpy, w->display->screen_num));

    for( i = 0; i < 5; i++){
        if( arr[i] == NULL)
            continue;
   

        s = arr[i];
        s += 2;

        type = tolower( arr[i][0]);
        switch( type){
            case 'w':
                type = COLYELLOW;
            break;
            case 'e':
                type = COLRED;
            break;
            case 'o':
                type = COLGREEN;
            break;
            case 'd':
                type = COLCYAN;
            break;
            case 'i':
                type = COLWHITE;
            break;
           default: 
                type = COLWHITE;
                s = arr[i];
        }
       
       
        XSetForeground( w->display->dpy, gc->gc, X11Colors[type].pixel );
        XDrawRectangle( w->display->dpy, w->win, gc->gc, 5, (i * 30) + 5, w->width - 10, 25);   
        XDrawString( w->display->dpy, w->win, gc->gc, 10, (i * 30) + 22, s, strlen( s));   
    }
    return(0);
}


int MainWindowInit(){
	int rc;
    X11_display_t display;
	X11_window_t window;
	X11_gc_t gc;
	KeySym key_symbol;
	int quit = 0;
	unsigned long int event;
    int ascii_key, event_fd;
	int x11_fd, stdin_fd;
	fd_set in_fds;
	struct timeval tv;
	char input_str[80];
	char* window_title = "X USB Notifier";
	int c, i;
	
	flog = fopen( USBLOG, "r");
	if( flog == NULL){
	    perror("Could not open file:");
		exit( -1);
	}
	
	/*fseek( flog, 0, SEEK_END);*/
	
	rc = X11_display_init( &display);
	if( rc != 0){
	    exit(1);
	}

	X11_window_init( &display, &window);
	X11_window_set_geometry( &window, 350, 150, display.width - ( 350 + 5), display.height - (150 + 40), 5);
	X11_window_set_mask( &window, WINDOW_ALL_EVENTS_MASK  );
	
	X11_window_display( &window);
	X11_window_set_wm_name( &window, window_title);
	rc = X11_gc_init( &gc, &window);
    
        Atom wmDelete = XInternAtom(display.dpy, "WM_DELETE_WINDOW", True);
        XSetWMProtocols(display.dpy, window.win, &wmDelete, 1);
	

	/* as each event that we asked about occurs, we respond.  In this
	 * case we note if the window's shape changed, and exit if a button
	 * is pressed inside the window */

    
    
	stdin_fd = fileno( flog);
	x11_fd = ConnectionNumber( display.dpy);

    lseek( stdin_fd, 0, SEEK_END);
	
    draw_status_bars( &gc, strings.str);	
	while( quit == 0){

    	FD_ZERO( &in_fds);
	/*	FD_SET( stdin_fd, &in_fds);*/
		FD_SET( x11_fd, &in_fds);
        tv.tv_usec = 0;
        tv.tv_sec = 1;

        
        /* Wait for X Event or a Timer*/
        event_fd = select( x11_fd + 1, &in_fds, 0, 0, &tv);
        /* if( FD_ISSET( stdin_fd, &in_fds) ){
		    i = 0;
            while( read( stdin_fd, &c, 1) > 0){
	            printf("%d %c\n", c, c);
	            if( c == EOF || c == 10 || c == 13)
		            break;

	            input_str[i++] = (char ) c; 		
	        }
    
	        input_str[i++] = 0;
		
			
			printf("input_str='%s'\n", input_str);
			arrstr_add( &strings, input_str);
			draw_status_bars( &gc, strings.str);	
		}else */
		if( event_fd == 0){
		    printf("timeout\n");
			strcpy( input_str, "");
			
            while( read( stdin_fd, &c, 1) > 0){
			    c = (char) c;
                if( c == '\n' || c == '\r'){
				    input_str[i++] = 0;
					
					if( strlen( input_str) != 0){
	                    printf("input_str='%s'\n", input_str);
		    			arrstr_add( &strings, input_str);
					}
					i = 0;
                }else if(c == EOF){
                    break;
          		}else{	
	                input_str[i++] = (char ) c; 		
				}
	        }
			
			draw_status_bars( &gc, strings.str);
	    }else if( FD_ISSET( x11_fd, &in_fds)){ 
	    
	        while( XPending( display.dpy)){
				event = X11_window_get_next_event( &window);
		
		
				switch( event){
					case ConfigureNotify:
						if (window.width != window.ev.xconfigure.width || window.height != window.ev.xconfigure.height) {
							window.width = window.ev.xconfigure.width;
							window.height = window.ev.xconfigure.height;
							printf("Size changed to: %d by %d\n", window.width, window.height);
						}
					break;
					case UnmapNotify:
						printf("UnmapNotify\n");
					break;
					case MapNotify:
						printf("MapNotify\n");
					break;
                                        case ClientMessage:
					case DestroyNotify:
						printf("DestroyNotify \n");
						/*X11_display_close( &display);*/
                                                quit = 1;
				
					break;
					case Expose:
                                                draw_status_bars( &gc, strings.str);	
						printf("Expose\n");
					break;
					case KeyPress:
						printf("KeyPress\n");
						key_symbol = XKeycodeToKeysym(display.dpy, window.ev.xkey.keycode, 0);
				
				
						if (key_symbol >= XK_A && key_symbol <= XK_Z) {
							ascii_key = key_symbol - XK_A + 'A';
							printf("Key pressed - '%c'\n", ascii_key);
						}
						if (key_symbol >= XK_a && key_symbol <= XK_z) {
							ascii_key = key_symbol - XK_a + 'a';
							printf("Key pressed - '%c'\n", ascii_key);
						}
				
				
					break;

					     
				} 

			}
		}
		XFlush(display.dpy);
		fflush( stdout);
	}

        X11_window_close( &window);
	X11_window_destroy( &window);
	fclose( flog);
	return( 0);
	
}


int main( int argc, char **argv){

    arrstr_init( &strings, 5);
    MainWindowInit();
    arrstr_destroy( &strings);
    return(0);
}
