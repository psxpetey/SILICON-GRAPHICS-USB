/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: kmaddr.c                                                   *
* Description: Returns module base address                         *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      07-19-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

/* This function do anything but it's address will allow us to get the 
kernel module base address!! Nice for crash dumps analysis!! */
unsigned int module_address(){
    return(0x0BADBABE);
}
