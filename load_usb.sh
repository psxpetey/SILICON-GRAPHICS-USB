#! /bin/sh
####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: load_usb.sh                                                #
# Description: Load kernel modules script                          #
####################################################################

#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      07-05-2012     -For loop created with all the modules                         #
#                                                                                           #
# BSDero      06-13-2012     -5 seconds delay removed                                       #
#                                                                                           #
# BSDero      10-10-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################

DESCRIPTION_FILE="usb.version"
SHORTNAME=`cat $DESCRIPTION_FILE|grep SHORTNAME|awk -F= '{print $2}'|sed "s/\n//g"`
LONGNAME=`cat $DESCRIPTION_FILE|grep LONGNAME|awk -F= '{print $2}'|sed "s/\n//g"`
VERSION=`cat $DESCRIPTION_FILE|grep VERSION|awk -F= '{print $2}'|sed "s/\n//g"`


echo "**************************************************************"
echo "* USB IRIX                                                   *"
echo "* USB stack for SGI Irix 6.5                                 *" 
echo "*                                                            *"
echo "* Programmed by bsdero                                       *"
echo "* <bsdero at gmail dot com> 2011/2012                        *"
echo "**************************************************************"
echo "* Version $VERSION                                           *"
echo "**************************************************************"

echo "Loading USB drivers "
for a in usbcore usbhub usbehci usbuhci; do
    echo "loading $a"
    /sbin/sync 
    /sbin/sync
    /sbin/sync 
    /sbin/sync
    /sbin/sync 
    /sbin/sync
    /sbin/ml ld -v -c $a".o" -p $a"_"
done

