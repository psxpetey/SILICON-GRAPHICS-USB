/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: gc.c                                                       *
* Description: Simple garbage collector for kernel memory          *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-29-2013     -Use kmem_zalloc instead kmem_alloc                                      *
*                                                                                                     *
* BSDero      01-16-2012     -Fixes in memory dumps                                                   *
*                                                                                                     *
* BSDero      01-04-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/



#define DMALLOC(b)                          kmem_zalloc( b, KM_SLEEP)
#define DFREE(p,b)                          kmem_free(p, b)
#define INIT_GC_LIST(m)                     INIT_LIST_HEAD(m)
#define gc_list_init(l)                     INIT_GC_LIST(l)
#define gc_list_empty(l)                    list_empty(l)
typedef struct{
    list_t            list;
    unsigned char     mark;
    size_t            size;
}gc_node_t;

typedef list_t                    gc_list_t;


gc_node_t *gc_get_node_ptr( void *data);
void *gc_get_data_ptr( gc_node_t *node);
void *gc_malloc( gc_list_t *list, size_t size);
void gc_free( void *ptr);
void gc_mark( void *data);
void gc_list_sweep( gc_list_t *gc_list);
void gc_list_destroy( gc_list_t *gc_list);
void gc_list_dump( gc_list_t *gc_list);
char *gc_strdup( gc_list_t *gc_list, char *p);
void gc_dump( void *data);


gc_node_t *gc_get_node_ptr( void *data){
    char *ptr = data;
    ptr -= sizeof( gc_node_t);
    return( (gc_node_t *) ptr);
}

void *gc_get_data_ptr( gc_node_t *node){
    char *p = (char *)node;
    return( p + sizeof(gc_node_t));
}

void *gc_malloc( gc_list_t *list, size_t size){
    void *ptr;
    char *ret_ptr;
    gc_node_t *node;
    
    ptr = DMALLOC( size + sizeof(gc_node_t));
    if( ptr == NULL)
        return( NULL);

        
    node = (gc_node_t *) ptr;
    list_add( (list_t *) node, list);
    node->mark = 0;    
    node->size = size;
    
    ret_ptr = ((char *) node) + sizeof( gc_node_t);
    return( (void *) ret_ptr);
}


void gc_free( void *ptr){
    gc_node_t *node; 
    size_t size;
    node = gc_get_node_ptr( ptr);    
    list_del( (list_t *) node);
    
    size = node->size;
    DFREE( node, size);
    
}




void gc_mark( void *data){
    gc_node_t *node;
    node = gc_get_node_ptr( data);
    node->mark = 1;
}


void gc_list_sweep( gc_list_t *gc_list){
    list_t *l, *i, *tmp;
       
    l = (list_t *) gc_list;
    
    list_for_each_safe( i, tmp, l){
        if( (  (gc_node_t *) i)->mark == 1)
            gc_free( (void *) gc_get_data_ptr(  (void *)i)   );
    }    
}


void gc_list_destroy( gc_list_t *gc_list){
    list_t *l, *i, *tmp;
       
    l = (list_t *) gc_list;
    
    list_for_each_safe( i, tmp, l){
        gc_free( (void *) gc_get_data_ptr(  (void *)i)   );
    }   

    gc_list_init( gc_list);    
}

void gc_list_dump( gc_list_t *gc_list){
    list_t *l, *i, *tmp;
    gc_node_t *node;   
       
    l = (list_t *) gc_list;
    
    printf("list->next=0x%x, list->prev=0x%x\n", l->next, l->prev);
    list_for_each_safe( i, tmp, l){
        node = (gc_node_t *) i;
        gc_dump( (void *) gc_get_data_ptr( node));
    }   
}


void gc_dump( void *data){
    gc_node_t *node;
    node = gc_get_node_ptr( data);
    TRACE( TRC_GC_DMA, 7, "node = 0x%08x", node);
    TRACE( TRC_GC_DMA, 7, "node->next = 0x%08x", node->list.next);
    TRACE( TRC_GC_DMA, 7, "node->prev = 0x%08x", node->list.prev);
    TRACE( TRC_GC_DMA, 7, "node->mark = 0x%08x", node->mark);
    TRACE( TRC_GC_DMA, 7, "node->size = 0x%08x", node->size);
    TRACE( TRC_GC_DMA, 7, "node data: ", NULL);
    dumphex( TRC_GC_DMA, 7, (unsigned char *) gc_get_data_ptr( node), node->size);
}

char *gc_strdup( gc_list_t *gc_list, char *p){
    char *s;
    long int n = strlen( p) + 1;
    
    s = gc_malloc( gc_list, n);
    if( s == NULL) 
       return( NULL);
       
    strncpy( s, p, n);
    s[n] = '\0';
    return( s);
}

