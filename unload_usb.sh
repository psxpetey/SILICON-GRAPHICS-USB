#! /bin/sh
####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: unload_usb.sh                                              #
# Description: unload kernel modules script                        #
####################################################################

#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      07-05-2012     -For loop created with all the modules                         #
#                                                                                           #
# BSDero      10-10-2011     -Initial version                                               #
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

echo "Unloading USB drivers "
for a in usbehci usbuhci usbhub usbcore ; do
    echo "unloading $a"
    DRVID=`ml| grep $a| awk '{ print $2}'`
    /sbin/sync 
    /sbin/sync
    /sbin/sync 
    /sbin/sync
    /sbin/sync 
    /sbin/sync
    /sbin/ml unld -v $DRVID
    sleep 1
done


