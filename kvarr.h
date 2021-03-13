/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: kvarr.h                                                    *
* Description: Key-value array basic functions header              *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-13-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#ifndef _KVARR_H_
#define _KVARR_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KVARR_MAX                64


typedef struct{
    char *key[KVARR_MAX];
	char *val[KVARR_MAX];
	int nelem;
}kvarr_t;

void kvarr_init( kvarr_t *kvarr);
void kvarr_destroy( kvarr_t *kvarr);
void kvarr_add( kvarr_t *kvarr, char *key, char *val);
void kvarr_dump( kvarr_t *kvarr);
void kvarr_display_val( kvarr_t *kvarr, char *title);
void kvarr_display_xml( kvarr_t *kvarr, char *docdescr);


#endif


