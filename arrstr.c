/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: arrstr.c                                                   *
* Description: String array basic functions                        *
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
#include <stdio.h>
#include <stdlib.h>
#include "arrstr.h"


void arrstr_init( arrstr_t *arr, int max){
    memset( (void *) arr, 0, sizeof( arrstr_t));
    if( max >= MAX_STRINGS)
        max = MAX_STRINGS;
        
	arr->max = max;
}

void arrstr_destroy( arrstr_t *arr){
    int i;
	
	for( i = 0; i < arr->max; i++){
		if( arr->str[i] != NULL)
	        free( arr->str[i]);
    }
    
    arrstr_init( arr, MAX_STRINGS);	
	
}


void arrstr_add( arrstr_t *arr, char *str){
    int i;

    if( arr->str[0] != NULL)
        free( arr->str[0]);
        
		
	for( i = 0; i < ( arr->max - 1); i++){
		arr->str[i] = arr->str[i + 1];
	}
		
	arr->str[i] = strdup( str);
}

void arrstr_dump( arrstr_t *arr){
    int i;
	printf("DUMP\n");
    for( i = 0; i < arr->max; i++){
		if( arr->str[i] == NULL)
		    printf("%d : NULL\n", i);
		else
	        printf("%d : %s\n", i, arr->str[i]);
	    
	}
		
}
