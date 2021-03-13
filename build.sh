#! /bin/sh
####################################################################
# USB stack and host controller driver for SGI IRIX 6.5            #
#                                                                  #
# Programmed by BSDero                                             #
# bsdero at gmail dot com                                          #
# 2011/2012                                                        #
#                                                                  #
#                                                                  #
# File: build.sh                                                   #
# Description: Build script                                        #
####################################################################

#############################################################################################
# Fixlist (Latest at top)                                                                   #
#############################################################################################
# Author      MM-DD-YYYY     Description                                                    #
#############################################################################################
# BSDero      07-23-2012     -Updated interpreter, now it should run with /bin/sh instead   #
#							    /usr/nekoware/bin/bash                                      #
#                                                                                           #
# BSDero      07-19-2012     -Added support for assembler and disassembled files. Now       #
#                                core dumps should be easier to read                        #
#							 -Added clean.sh script                                         #
#							 -Added Makefile support for usb tools                          #
#							 -Print warning on incorrect Irix version during builds         #
#                                                                                           #
# BSDero      07-05-2012     -Added usbhub module build suport                              #
#                            -Added support for build output to files                       #
# 						     -Fixes to on screen errors during builds. Errors               #
# 						      are easier to read now                                        #
#                                                                                           #
# BSDero      06-14-2012     -Removed old tools building                                    #
#                            -Added usbtool build support                                   #
#                                                                                           #
# BSDero      05-13-2012     -Added usbehci/usbuhci modules build                           #
#                                                                                           #
# BSDero      03-12-2012     -Added unit tests build                                        #
#                            -Added USBcore build                                           #
#                            -Added config.h generation							            #
#                                                                                           #
# BSDero      01-09-2012     -Will use c99 compiler                                         #
#                                                                                           #
# BSDero      01-15-2012     -Added config.h generation support                             #
#                                                                                           #
# BSDero      01-09-2012     -Will use c99 compiler                                         # 
#                                                                                           #
# BSDero      01-04-2012     -Initial version                                               #
#                                                                                           #
#############################################################################################

echo "**************************************************************"
echo "* USB IRIX                                                   *"
echo "* USB stack for SGI Irix 6.5                                 *" 
echo "*                                                            *"
echo "* Programmed by bsdero                                       *"
echo "* <bsdero at gmail dot com> 2011/2012                        *"
echo "**************************************************************"
 

echo "========================================================================"
echo "Configuring build"
OSNAME=`uname -s` 
OSVERSION=`uname -r` 
CPUARCH=`uname -m` 
echo "OS is "$OSNAME $OSVERSION
if [ "$OSVERSION" != "6.5" ]; then
    echo "Warning, $OSNAME $OSVERSION detected, these drivers has been"
    echo "tested in IRIX 6.5, some errors/crashes may happen..."
fi

echo "Detected "`/sbin/hinv -c processor|grep CPU`
CPUTYPE=`/sbin/hinv -c processor|head -1|awk '{ print $4}'`
export CPUBOARD=$CPUTYPE
echo "Building for "$CPUBOARD

CC=/usr/bin/c99
DIS=/usr/bin/dis
DISFLAGS="-f -I. -L -S -h -x -s -w"

###################################################################
#         GLOBAL - use these for all IRIX device drivers          #
###################################################################

CFLAGS="-D_KERNEL -DMP_STREAMS -D_MP_NETLOCKS -DMRSP_AS_MR \
-fullwarn -non_shared -G 0 -TARG:force_jalr \
-TENV:kernel -OPT:space -OPT:Olimit=0 -CG:unique_exit=on \
-TENV:X=1 align_aggregate -OPT:IEEE_arithmetic=1 -OPT:roundoff=0 \
-OPT:wrap_around_unsafe_opt=off -g"


###################################################################
#          32-bit - for device drivers on 32-bit systems          #
###################################################################

if [[ "$CPUBOARD" == "IP20" || "$CPUBOARD" == "IP22" || "$CPUBOARD" == "IP32" ]]; then
   CFLAGS+=" -n32 -D_PAGESZ=4096"
###################################################################
#          64-bit - for device drivers on 64-bit systems          #
###################################################################
#else /* !(IP20||IP22||IP32) */
else
   CFLAGS=$CFLAGS" -64 -D_PAGESZ=16384 -D_MIPS3_ADDRSPACE "
fi
#/* IP20||IP22||IP32 */


###################################################################
# Platform specific - for device drivers on the indicated machine #
###################################################################

if [ "$CPUBOARD" == "IP19" ]; then
   CFLAGS+=" -DIP19 -DEVEREST -DMP -DR4000"
fi

if [ "$CPUBOARD" == "IP20" ]; then
   CFLAGS+=" -DIP20 -DR4000 -DJUMP_WAR -DBADVA_WAR"
fi

if [ "$CPUBOARD" == "IP21" ]; then
   CFLAGS+=" -DIP21 -DEVEREST -DMP -DTFP -TARG:processor=r8000"
fi

if [ "$CPUBOARD" == "IP22" ]; then
   CFLAGS+=" -DIP22 -DR4000 -DJUMP_WAR -DBADVA_WAR -DTRITON"
fi

if [ "$CPUBOARD" == "IP25" ]; then
   CFLAGS+=" -DIP25 -DEVEREST -DMP -DR10000 -TARG:processor=r10000"
fi

if [ "$CPUBOARD" == "IP26" ]; then
   CFLAGS+=" -DIP26 -DTFP -TARG:sync=off -TARG:processor=r8000"
fi

if [ "$CPUBOARD" == "IP27" ]; then
   CFLAGS+=" -DIP27 -DR10000 -DMP -DSN0 -DSN \
 -DMAPPED_KERNEL -DLARGE_CPU_COUNT -DPTE_64BIT -DULI -DCKPT -DMIPS4_ISA \
 -DR10K_LLSC_WAR -DNUMA_BASE -DNUMA_PM  -DNUMA_TBORROW -DNUMA_MIGR_CONTROL \
 -DNUMA_REPLICATION -DNUMA_REPL_CONTROL -DNUMA_SCHED -DCELL_PREPARE \
 -DBHV_PREPARE -TARG:processor=r10000"
fi

if [ "$CPUBOARD" == "IP28" ]; then
# All Indigo2 10000 kernel modules must be built with the t5_no_spec_stores
# option to the C compiler and assembler.
   CFLAGS+=" -DIP28 -DR10000 -DSCACHE_SET_ASSOC=2 -D_NO_UNCACHED_MEM_WAR \
 -DR10000_SPECULATION_WAR \
 -TARG:processor=r10000 -TARG:t5_no_spec_stores "
fi

if [ "$CPUBOARD" == "IP30" ]; then
   CFLAGS+=" -DIP30 -DR10000 -DMP -DCELL_PREPARE -DBHV_PREPARE \
 -TARG:processor=r10000 "
fi

#O2
if [ "$CPUBOARD" == "IP32" ]; then
   CFLAGS+=" -DIP32 -DR4000 -DR10000 -DTRITON -DUSE_PCI_PIO -c99 "
fi



if [ "$CPUBOARD" == "IP35" ]; then
   CFLAGS=$CFLAGS" -DIP35 -DR10000 -DMP -DSN1 -DSN \
-DMAPPED_KERNEL -DLARGE_CPU_COUNT -DPTE_64BIT -DULI -DCKPT -DMIPS4_ISA \
-DNUMA_BASE -DNUMA_PM -DNUMA_TBORROW -DNUMA_MIGR_CONTROL \
-DNUMA_REPLICATION -DNUMA_REPL_CONTROL -DNUMA_SCHED -DCELL_PREPARE \
-DBHV_PREPARE -TARG:processor=r10000 "
fi


##################################################################################
# Clean files
##################################################################################
./clean.sh


##################################################################################
# Creation of config.h
##################################################################################
DESCRIPTION_FILE="usb.version"
CONFIG_H_IN_FILE="config.h.in"
CONFIG_H_FILE="config.h"
SHORTNAME=`cat $DESCRIPTION_FILE|grep SHORTNAME|awk -F= '{print $2}'|sed "s/\n//g"`
LONGNAME=`cat $DESCRIPTION_FILE|grep LONGNAME|awk -F= '{print $2}'|sed "s/\n//g"`
VERSION=`cat $DESCRIPTION_FILE|grep VERSION|awk -F= '{print $2}'|sed "s/\n//g"`
SHORTVERSION=`echo $VERSION|sed "s/\.//g"|sed "s/\n//g"`
SEQ=`date "+%H%M%S%m%d%Y"`
BUILDDATE=`date "+%c"`

cat $CONFIG_H_IN_FILE>$CONFIG_H_FILE

echo "#define USBCORE_DRV_SHORT_NAME                \"$SHORTNAME\"">>$CONFIG_H_FILE
echo "#define USBCORE_DRV_LONG_NAME                 \"$LONGNAME\"">>$CONFIG_H_FILE
echo "#define USBCORE_DRV_VERSION                   \"$VERSION\"">>$CONFIG_H_FILE
echo "#define USBCORE_DRV_SHORT_VERSION             \"$SHORTVERSION\"">>$CONFIG_H_FILE
echo "#define USBCORE_DRV_SEQ                       \"$SEQ\"">>$CONFIG_H_FILE
echo "#define USBCORE_DRV_BUILD_DATE                    \"$BUILDDATE\"">>$CONFIG_H_FILE
echo >>$CONFIG_H_FILE
echo >>$CONFIG_H_FILE


echo "========================================================================"
echo "Building kernel modules"
DRIVER_MODULES="usbcore usbhub usbehci usbuhci"
for MODULE in $DRIVER_MODULES; do
    SRC=$MODULE".c"
    echo "========================================================================"
    echo "Building $MODULE"
#    echo "$CC $CFLAGS -c $SRC"
    echo "$CC $CFLAGS -c $SRC" > $MODULE".build.output"
    $CC $CFLAGS -c $SRC >> $MODULE".build.output" 2>&1
    ERRORS=`grep "detected in the compilation of" $MODULE".build.output"`
    if [ "$ERRORS" != "" ]; then
        echo $ERRORS
        grep -n ERROR $MODULE".build.output"| awk -F: '{ print $1+5}' | while read n; do
            head -n $n $MODULE".build.output"| tail -6
            echo "---------------------------------------------------"
        done
    else
        echo $MODULE".o" "created"
        $CC $CFLAGS -S $SRC 2>/dev/null
        echo $MODULE".s" "created"
        $DIS $DISFLAGS $MODULE".o" > $MODULE."disam"
        echo $MODULE".disam" "created"
        
    fi
done

echo "========================================================================"
echo "Building user programs"
make
echo "Build complete"

