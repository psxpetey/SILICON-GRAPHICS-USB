/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbio.h                                                    *
* Description: macros for ioctl definitions                        *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      03-29-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#ifndef _USBIO_H_
#define _USBIO_H_




#define _IOCTL_RMASK                                (0x0100)
#define _IOCTL_WMASK                                (0x0200)

#define _IOCTL_(c,v)                                ((int)(c << 8 | v)) 
#define _IOCTL_R(c,v,n)                             ((int)( _IOCTL_RMASK | (sizeof( n) << 18) | _IOCTL_(c,v)))
#define _IOCTL_W(c,v,n)                             ((int)( _IOCTL_WMASK | (sizeof( n) << 18) | _IOCTL_(c,v)))
#define _IOCTL_WR(c,v,n)                            ((int)( _IOCTL_RMASK | _IOCTL_WMASK | (sizeof( n) << 18) | _IOCTL_(c,v)))

#define _IOCTL_GETC(c)                              (( c >> 8) & 0xff)
#define _IOCTL_GETN(n)                              (n & 0xff)
#define _IOCTL_GETS(s)                              (( s >> 18) & 0xffff)

#endif
