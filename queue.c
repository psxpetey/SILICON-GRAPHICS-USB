/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: queue.c                                                    *
* Description: Circular queue functions                            *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      07-30-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#define QUEUE_MAX_SIZE				128

typedef struct{
    int                   msg_id;
    device_header_t       *device;
    void                  *arg0;
    void                  *arg1;
    void                  *arg2;
}msg_t;

typedef struct{
    int start, end, nelems;
    msg_t q[QUEUE_MAX_SIZE];
}queue_t;


void queue_init( queue_t *q){
    q->start = 0;
    q->end = 0;
    q->nelems = 0;
}


int queue_put( queue_t *q, msg_t *v){
    if( q->nelems >= QUEUE_MAX_SIZE){
        TRACE( TRC_HELPER, 12, "Queue is full!", "");
        return( 1);
    }
    
    
    bcopy( (void *) v, (void *) &q->q[q->end], sizeof( msg_t) );
    q->end = (q->end + 1) % QUEUE_MAX_SIZE;
    q->nelems++;
    return(0);
} 


int queue_get( queue_t *q, msg_t *v){
    if( q->nelems <= 0){
        TRACE( TRC_HELPER, 12, "Queue is empty!", "");
        return( 1);
    }
    
    bcopy( (void *) &q->q[q->start], (void *) v, sizeof( msg_t) );
    q->start = ( q->start + 1) % QUEUE_MAX_SIZE;
    q->nelems--;
    return( 0);
}



void queue_print( queue_t *q){
    int i, j;
    
    
    
    TRACE( TRC_HELPER, 12, "QUEUE:", "");
    if( q->nelems == 0){
        TRACE( TRC_HELPER, 12, "Queue is empty!", "");
        return;
    }

    for( i = q->start, j = 0; j < q->nelems; j++, i = (i+1) % QUEUE_MAX_SIZE ){ 
        TRACE( TRC_HELPER, 12, "Queue element[%d] = %d", j, q->q[i].msg_id);
    }
}

