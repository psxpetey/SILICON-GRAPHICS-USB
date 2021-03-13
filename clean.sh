#! /bin/sh
####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: clean.sh                                                   #
# Description: Clean script                                        #
####################################################################
#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      09-12-2012     -Added diaginfo directory removal                              #
#                            -Added help.txt file removal                                   #
#                                                                                           #
# BSDero      07-23-2012     -Updated interpreter, now it should run with /bin/sh instead   #
#							    /usr/nekoware/bin/bash                                      #
#                                                                                           #
# BSDero      07-19-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################


##################################################################################
# Clean files
##################################################################################
echo "Cleaning files..."
if [ -e config.h ]; then
    rm config.h
fi

make clean
rm -rf *.o *.disam *.s *.build.output
rm -rf diaginfo
rm -rf help.txt
echo "done"
