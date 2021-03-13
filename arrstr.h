/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: arrstr.h                                                   *
* Description: String array basic functions header                 *
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

#ifndef _ARRSTR_H_
#define _ARRSTR_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRINGS                8

typedef struct{
    int max;
	char *str[MAX_STRINGS];
}arrstr_t;

void arrstr_init( arrstr_t *arr, int max);
void arrstr_destroy( arrstr_t *arr);
void arrstr_add( arrstr_t *arr, char *str);
void arrstr_dump( arrstr_t *arr);

#endif
