# SGI-USB
License:

This code was made by BSDHERO license is open and he accepts no responsibility for damage to your device.

Prerequisites:

You will need mips pro and cscope to make this package.


Building the package:

For the build, you should use the script build.sh

If everything went fine, you will have the next files: usbcore.o usbhub.o usbehci.o usbuhci.o


once the modules are built , you should try to load them into the kernel, by using: load_usb.sh

check hinv to see if the modules were loaded successfully.

Also, detect for plug/unplug devices to the USB host should be shown.

You can add devices and controllers in usbehci.c and usbuhci.c,

Maybe the drivers has a lot of code for such simple operations. But I designed them for further implementation of USB calls. 


Adding your cards controller chips:
In usbehci.c you will ad your usb 2.0 controller chip, to get the vendor and device code type hinv -vvm, the first 4 numbers 
in these two files are the vendor code and the second 4 are device mine was 0x00351033 and 0x0e001033. To find out which of your 
devices under pci unknown is the usb controllers take the codes you got from hinv 
And enter them into pcilookup.com

To get a n64 64 bit kernel build build as normal. To get a 32 bit build (n32) for kernel you must (will be added when bsd hero tells me)
Normally it should detect the processor and build your kernel according to that but sometimes it doesn't like in my case.
