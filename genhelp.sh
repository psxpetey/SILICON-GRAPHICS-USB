#! /usr/nekoware/bin/bash
####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: genhelp.sh                                                 #
# Description: Help strings generator for usbtool.c                #
#              copy and paste the output in usbtool.c              #
####################################################################
#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      07-19-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################


cat > help.txt << "EOF"
Usage: usbtool <main-options>

main-options:
-h| --help                              Display this help
-c| --core <core-options>               Use core options 
-r| --roothub <hub-options>             Use roothub options


core-options: 
-i,--info                               Display driver information
-r,--roothub                            Display root hub information 
    -a,--all                            Display all the root hub information 
    -s,--status                         Display root hub status
-t,--tracelevel                         Display trace level and class
	<level> <class>                     Set trace level and class

	level                               Valid values are 0-12. 
                                            0:  minimal quantity of messages
                                            12: trace all messages

    class                               
                                        Specify which functionality to trace
                                           64 bits value for mask. Bits in mask in hex are:
                                           Entry points in kernel modules:
                                            00000000 00000001   : init/register
                                            00000000 00000002   : unload/unregister
                                            00000000 00000004   : attach
                                            00000000 00000008   : dettach
                                            00000000 00000010   : open
                                            00000000 00000020   : close
                                            00000000 00000040   : read
                                            00000000 00000080   : write
                                            00000000 00000100   : ioctl
                                            00000000 00000200   : strategy
                                            00000000 00000400   : poll
                                            00000000 00000800   : map/unmap
                                            00000000 00001000   : intr
                                            00000000 00002000   : err
                                            00000000 00008000   : halt

                                           Helper functionalities tracing:
                                            00000000 00010000   : memory lists/dma
                                            00000000 00020000   : helper functions
                                            00000000 00040000   : intermodule function calls
                                            00000000 00080000   : usb stack core events
                                            00000000 00100000   : error flag
                                            00000000 00200000   : warning flag

                                           Module tracing: 
                                            00000001 00000000   : usbuhci
                                            00000002 00000000   : usbehci    
                                            00000010 00000000   : usbcore
                                            00000020 00000000   : usbhub 

                                        Examples: 
                                          Trace usbcore module only, load and unload
                                            0x0000001000000003
                                          Trace all in usbehci and usbuhci
                                            0x00000003003fffff
                                          Trace all in all modules 
                                            0xffffffffffffffff
                                          Trace usb core events only
                                            0x0000000000080000


-m,--modules                            Display modules list
-u,--usbdevices                         List all the detected USB devices
	-a,--all                            Display full information for USB devices
    -r,--tree                           Display tree of usb devices
-t,--test <testnumber>                  Run test number testnumber


EOF
cat help.txt | sed "s/^/        \"/g" |  sed "s/$/\",/g"
