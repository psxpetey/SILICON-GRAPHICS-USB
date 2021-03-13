####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: Makefile                                                   #
# Description: User binaries makefile                              #
#                                                                  #
####################################################################

#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      01-25-2013     -Added robodoc auto documentation and cscope                   #   
#                                 database generation                                       #
#                            -Added distclean target                                        #
#                                                                                           #
# BSDero      10-08-2012     -Added usbd                                                    #
#                            -Fixed bug with xusbnotifier                                   #
#                                                                                           #
# BSDero      09-12-2012     -Added xusbnotifier                                            #
#                                                                                           #
# BSDero      07-19-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################

CC=/usr/bin/c99
DIS=/usr/bin/dis
DISFLAGS=-f -I. -L -S -h -x -s -w
CFLAGS=-D_IRIX_ -g -I./ 
CLIBS=-L/usr/lib/X11 -lX11
ALL=allfiles
#############################################################

all: $(ALL)


allfiles: usbtoolfiles xusbnotifierfiles usbdfiles cscopedb 



##########################################################
# usbd
usbdfiles: usbd usbd.s usbd.disam
        
usbd: usbd.o
	$(CC) -o usbd usbd.o

usbd.s:
	$(CC) $(CFLAGS) -o usbd.s usbd.c -S

usbd.disam: usbd.o
	$(DIS) $(DISFLAGS) usbd.o>usbd.disam

##########################################################
# usbtool 
usbtoolfiles: usbtool usbtool.s usbtool.disam
        
usbtool: usbtool.o kvarr.o
	$(CC) -o usbtool usbtool.o kvarr.o

usbtool.s:
	$(CC) $(CFLAGS) -o usbtool.s usbtool.c -S

usbtool.disam: usbtool.o
	$(DIS) $(DISFLAGS) usbtool.o>usbtool.disam

##########################################################
# xusbnotifier 
xusbnotifierfiles: xusbnotifier xusbnotifier.s xusbnotifier.disam

xusbnotifier: xusbnotifier.o X11display.o X11window.o arrstr.o
	$(CC) -o xusbnotifier xusbnotifier.o X11display.o X11window.o arrstr.o $(CLIBS)

xusbnotifier.s: 
	$(CC) -o xusbnotifier.s xusbnotifier.c -S

xusbnotifier.disam: xusbnotifier.o
	$(DIS) $(DISFLAGS) xusbnotifier.o>xusbnotifier.disam

##########################################################
# object files

arrstr.o: arrstr.c
	$(CC) -c arrstr.c

X11display.o: X11display.c
	$(CC) -c X11display.c $(CFLAGS)

X11window.o: X11window.c
	$(CC) -c X11window.c $(CFLAGS)

xusbnotifier.o: xusbnotifier.c
	$(CC) -c xusbnotifier.c $(CFLAGS)

usbtool.o:
	$(CC) $(CFLAGS) -c usbtool.c

kvarr.o:
	$(CC) $(CFLAGS) -c kvarr.c
        
usbd.o:
	$(CC) $(CFLAGS) -c usbd.c

api.html:
	robodoc --src ./ --doc api --singledoc --html

cscopedb:
	cscope -R -b -q

distclean: clean
	@rm -rf cscope.*
	@rm -rf filelist

clean:
	@rm -rf *.o
	@rm -rf *.s
	@rm -rf *.disam
	@rm -rf usbtool
	@rm -rf xusbnotifier
	@rm -rf usbd
	@rm -rf *.bck


