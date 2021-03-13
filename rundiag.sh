#! /bin/sh

####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: rundiag.sh                                                 #
# Description: Diagnostics script                                  #
####################################################################

#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      08-01-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################


if [ -e diaginfo ]; then
    rm -rf diaginfo
fi 

if [ -e diaginfo.tar ]; then
    rm -f diaginfo.tar
fi

if [ -e diaginfo.tar.Z ]; then
    rm -f diaginfo.tar.Z
fi

if [ -e diaginfo.tar.bz2 ]; then
    rm -f diaginfo.tar.bz2
fi

mkdir -p diaginfo/analysis
mkdir -p diaginfo/disam

##########################################################################################
# Get the last 5000 lines for SYSLOG 
##########################################################################################
tail -8000 /var/adm/SYSLOG > diaginfo/SYSLOG




##########################################################################################
# Get analysis files
##########################################################################################
cp /var/adm/crash/analysis* diaginfo/analysis

##########################################################################################
# Also get kernel modules base address
##########################################################################################
grep "diaginfo" diaginfo/SYSLOG > diaginfo/analysis/kmodules_base_address




##########################################################################################
# Disassembled files add for core analysis
##########################################################################################
cp *.disam diaginfo/disam/
cp diaginfo/analysis/kmodules_base_address diaginfo/disam/



##########################################################################################
# Retrieve info 
##########################################################################################
OUTPUT=diaginfo/machine.info

echo "#date">$OUTPUT 
date>>$OUTPUT 
echo>>$OUTPUT 

echo "#uname -a">>$OUTPUT 
uname -a>>$OUTPUT
echo >>$OUTPUT

echo "#hinv -v">>$OUTPUT
hinv -v>>$OUTPUT
echo>>$OUTPUT

echo "#hinv">>$OUTPUT
hinv >>$OUTPUT
echo>>$OUTPUT

echo "#df -h">>$OUTPUT
df -h>>$OUTPUT
echo>>$OUTPUT

echo "#ml">>$OUTPUT
ml>>$OUTPUT
echo>>$OUTPUT

echo "#versions | grep -i compiler">>$OUTPUT 
versions | grep -i compiler>>$OUTPUT 
echo>>$OUTPUT 

tar cvf diaginfo.tar diaginfo
compress diaginfo.tar
echo "Done. Take file diaginfo.tar.Z and send to maintainer"     
