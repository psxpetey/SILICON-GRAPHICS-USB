# SGI-USB
This code was made by BSDHERO liscence is open and he accepts no responsibility for damage to your device.

You will need mips pro and cscope to make this package.
For the build, you should use the script build.sh

If everything went fine, you will have the next files: usbcore.o usbhub.o usbehci.o usbuhci.o


once the modules are built , you should try to load them into the kernel, by using: load_usb.sh

check hinv to see if the modules were loaded successfully.

Also, detect for plug/unplug devices to the USB host should be shown.

You can add devices and controllers in usbehci.c and usbuhci.c,

Maybe the drivers has a lot of code for such simple operations. But I designed them for further implementation of USB calls. 
