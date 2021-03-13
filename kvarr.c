/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: kvarr.c                                                    *
* Description: Key-value array basic functions code                *
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
#include <string.h>
#include "kvarr.h"


void kvarr_init( kvarr_t *kvarr){
    kvarr->nelem = 0;
    memset( (void *) kvarr, 0, sizeof( kvarr_t) );

}



void kvarr_destroy( kvarr_t *kvarr){
    int i;

    for( i = 0; i < kvarr->nelem; i++){
        if( kvarr->key[i] != NULL)
            free( kvarr->key[i]);

        if( kvarr->val[i] != NULL)
            free( kvarr->val[i]);
    }
	
    kvarr->nelem = 0;
}



void kvarr_add( kvarr_t *kvarr, char *key, char *val){
    kvarr->key[kvarr->nelem] = strdup( key);
    kvarr->val[kvarr->nelem] = strdup( val);
    kvarr->nelem++;
}



void kvarr_dump( kvarr_t *kvarr){
    int i;

    for( i = 0; i < kvarr->nelem; i++){
        printf("%d: %s: %s\n", i, kvarr->key[i], kvarr->val[i]);
    }
}



void kvarr_display_val( kvarr_t *kvarr, char *title){
    int i;
	
	printf("-------- %s\n", title);
    for( i = 0; i < kvarr->nelem; i++){
        printf("%s:%s\n", kvarr->key[i], kvarr->val[i]);
    }
}



void kvarr_display_xml( kvarr_t *kvarr, char *docdescr){
    int i;
	
    printf("<?xml version=\"1.0\"?>\n");
    printf("<%s>\n", docdescr);
    for( i = 0; i < kvarr->nelem; i++){
        printf("    <%s>%s</%s>\n",  kvarr->key[i], kvarr->val[i], kvarr->key[i]);
    }
	
    printf("</%s>\n", docdescr);
}


/******************************************************* 
Test, uncomment this code for test

int main(){
    kvarr_t kvarr;
	
	kvarr_init( &kvarr);
    kvarr_add( &kvarr, "Key1" , "xyz"); 
    kvarr_add( &kvarr, "Key2" , "12345"); 
    kvarr_add( &kvarr, "Key3" , "value"); 
    kvarr_add( &kvarr, "RC" , "0"); 
	
	kvarr_dump( &kvarr);
	kvarr_display_val( &kvarr);
	kvarr_display_xml( &kvarr, "Document");
	kvarr_destroy( &kvarr);
	
	
}
*******************************************************/



