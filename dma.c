/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: dma.c                                                      *
* Description: DMA utilities                                       *
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
* BSDero      02-26-2012     -Separated node information from DMA chunk                               *
*                                                                                                     *
* BSDero      02-24-2012     -Fixed crashed passing a invalid pointer to pciio_dmamap_done()          *
*                                                                                                     *
* BSDero      01-23-2012     -Removed delays for trace calls                                          *
*                                                                                                     *
* BSDero      01-19-2012     -Changed printf calls for TRACE calls                                    *
*                                                                                                     *
* BSDero      01-15-2012     -Fixed bugs and crashes                                                  *
*                                                                                                     *
* BSDero      01-05-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

typedef uint64_t        pciaddr_t;


#define dma_list_init(ptr)                          gc_list_init(ptr)
#define dma_list_empty(l)                           gc_list_empty(l)
#define dma_node_get_data_addr(node)                (node + sizeof(dma_node_t))         
#define offset_of(st, m)                            ((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#define kaddr_2_daddr( daddr,kaddr,ofs)             (uint32_t) (daddr + ((uint64_t)ofs - (uint64_t)kaddr))


typedef gc_list_t           dma_list_t;



typedef struct{
    iopaddr_t               paddr;         /* starting phys addr       */
    caddr_t                 kaddr;         /* starting kern addr       */
    pciio_dmamap_t          map;           /* mapping resources (ugh!) */
    pciaddr_t               daddr;         /* starting pci addr        */
    size_t                  bytes;         /* size of block in bytes   */
    uchar_t                 *mem;          /* memory address           */
    char                    spaces[12];    /* fill up memory space     */
}dma_node_t;

extern int        pci_user_dma_max_pages;


dma_node_t *dma_alloc( vertex_hdl_t vhdl, dma_list_t *dma_list, size_t size);
void dma_drain( dma_node_t *dma_node);
void dma_done( dma_node_t *dma_node);
void dma_mark( dma_node_t *dma_node);
void dma_free( dma_node_t *dma_node);
void dma_list_sweep( dma_list_t *dma_list);
void dma_list_destroy( dma_list_t *dma_list);
void dma_list_dump( dma_list_t *dma_list);
void dma_node_dump( dma_node_t *dma_node);



dma_node_t *dma_alloc( vertex_hdl_t vhdl, dma_list_t *dma_list, size_t size){
    dma_node_t *dma_node;
    void                   *kaddr = 0;
    iopaddr_t               paddr;
    pciio_dmamap_t          dmamap = 0;
    size_t                  bytes = size;
    pciaddr_t               daddr;
    int                     err = 0;
    uchar_t                 *mem;       /* memory address */
    unsigned                flags = 0;

    TRACE( TRC_GC_DMA, 12, "enter", NULL);


    /* alloc DMA memory descriptor */
    TRACE( TRC_GC_DMA, 12, "user wants 0x%x bytes of DMA, flags 0x%x, sizeof(dma_node_t)=%d", 
       bytes, flags, sizeof( dma_node_t));
    TRACE( TRC_GC_DMA, 12, "allocating descriptor", "");
    dma_node = (dma_node_t *) gc_malloc( dma_list, sizeof(dma_node_t));
    if( dma_node == NULL){
        TRACE( TRC_GC_DMA, 12, "dma descriptor not allocated, return NULL", NULL);
        TRACE( TRC_GC_DMA, 12, "exit dma_node=0x%x", dma_node);    
        
        return( NULL);
    }     

    bzero( (void *) dma_node, sizeof( dma_node_t));

    /* alloc DMA chunk */
    mem = ( uchar_t *) kmem_zalloc( size, KM_CACHEALIGN | KM_PHYSCONTIG | KM_SLEEP);
    if( mem == NULL){
	    goto dma_alloc_exit;	
	}
    kaddr = (void *) mem;
    TRACE( TRC_GC_DMA, 12, "kaddr = 0x%x, mem = 0x%x", kaddr, mem);
    
    /* conver to physical address */
    paddr = kvtophys( kaddr);
    
    /* translate to a pciio_map */
    daddr = pciio_dmatrans_addr( vhdl, 0, paddr, size, PCIIO_WORD_VALUES);

    /* daddr is the address to pass to device */
    if (daddr == 0) {    /* "no direct path available" */
        TRACE( TRC_GC_DMA, 12, "dmatrans failed, trying dmamap", NULL);
        
        dmamap = pciio_dmamap_alloc( vhdl, 0, size, PCIIO_WORD_VALUES);
        if (dmamap == 0) {
            TRACE( TRC_GC_DMA, 12, "unable to allocate dmamap", NULL);
            kmem_free( (void *) mem, size);
            err = 1;
            goto dma_alloc_exit;    /* "out of mapping resources" */
        }
        
        daddr = pciio_dmamap_addr(dmamap, paddr, size);
        if (daddr == 0) {
            TRACE( TRC_GC_DMA, 12, "dmamap_addr failed", NULL);
            kmem_free( (void *) mem, size);
            err = 1;
            goto dma_alloc_exit; /* "can't get there from here" */
        }
    }
    TRACE( TRC_GC_DMA, 12, "daddr is 0x%08x", daddr);

        
    dma_node->bytes = size;
    dma_node->paddr = paddr;
    dma_node->kaddr = kaddr;
    dma_node->map = dmamap;
    dma_node->daddr = daddr;
    dma_node->mem = mem;

    TRACE( TRC_GC_DMA, 12, "dmap=0x%x kaddr=0x%x bytes=0x%x paddr=0x%x daddr=0x%x",
    dmamap, kaddr, bytes, paddr, daddr);
    
    dma_alloc_exit:
    if( err == 1){
        gc_mark( (void *) dma_node);
        dma_node = NULL;
    }

    TRACE( TRC_GC_DMA, 12, "exit dma_node=0x%x", dma_node);    
    return( dma_node);
}


void dma_drain( dma_node_t *dma_node){
/*   pciio_dmamap_drain(  pciio_dmamap_t *map) */

   /* TRACE( TRC_GC_DMA, 12, "enter", NULL);*/
    pciio_dmamap_drain( &dma_node->map);
   /* TRACE( TRC_GC_DMA, 12, "exit ", NULL);   */ 

}

void dma_done( dma_node_t *dma_node){
/*      pciio_dmamap_done(pciio_dmamap_t map)*/ 
    TRACE( TRC_GC_DMA, 12, "enter", NULL);
    pciio_dmamap_done( dma_node->map);
    TRACE( TRC_GC_DMA, 12, "exit ", NULL);    
}

void dma_mark( dma_node_t *dma_node){
    TRACE( TRC_GC_DMA, 12, "enter ", NULL);
    dma_drain( dma_node);
    dma_done( dma_node);
    gc_mark( (void *) dma_node);
    TRACE( TRC_GC_DMA, 12, "exit ", NULL);    
}

void dma_node_dump( dma_node_t *dma_node){
    TRACE( TRC_GC_DMA, 07, "enter", NULL);
    
    TRACE( TRC_GC_DMA, 7, "dma_node=0x%x", dma_node);
    TRACE( TRC_GC_DMA, 7, " paddr = 0x%x", dma_node->paddr);
    TRACE( TRC_GC_DMA, 7, " kaddr = 0x%x", dma_node->kaddr);
    TRACE( TRC_GC_DMA, 7, " mem   = 0x%x", dma_node->mem);
    TRACE( TRC_GC_DMA, 7, " map   = 0x%x", dma_node->map);
    TRACE( TRC_GC_DMA, 7, " daddr = 0x%x", dma_node->daddr);
    TRACE( TRC_GC_DMA, 7, " bytes = 0x%x", dma_node->bytes ); 
    TRACE( TRC_GC_DMA, 7, "exit ", NULL);
}

void dma_free( dma_node_t *dma_node){
 /*void      pciio_dmamap_free(pciio_dmamap_t map) */
    TRACE( TRC_GC_DMA, 12, "enter", NULL);
    
    if( dma_node->map != NULL){
       pciio_dmamap_free( dma_node->map);
    }
    kmem_free( (void *) dma_node->mem, dma_node->bytes);
    gc_free( (void *) dma_node);
    TRACE( TRC_GC_DMA, 12, "exit ", NULL);
    
}

void dma_list_sweep( dma_list_t *dma_list){
    list_t *l, *i, *tmp;

    TRACE( TRC_GC_DMA, 12, "enter ", NULL);
    l = (list_t *) dma_list;

    list_for_each_safe( i, tmp, l){
        if( (  (gc_node_t *) i)->mark == 1)
            dma_free( (void *) gc_get_data_ptr(  (void *)i)   );
    }    

    TRACE( TRC_GC_DMA, 12, "exit ", NULL);    
}

void dma_list_dump( dma_list_t *dma_list){
    list_t *l, *i, *tmp;
    dma_node_t *dma_node;
    TRACE( TRC_GC_DMA, 12, "enter ", NULL);
    

        l = (list_t *) dma_list;

        list_for_each_safe( i, tmp, l){
            dma_node = gc_get_data_ptr(  (void *)i);
            TRACE( TRC_GC_DMA, 7, "dma_node = 0x%x", dma_node);
            dma_node_dump( dma_node);
        }

    TRACE( TRC_GC_DMA, 12, "exit ", NULL);

}

void dma_list_destroy( dma_list_t *dma_list){
    list_t *l, *i, *tmp;
    dma_node_t *dma_node;   
    TRACE( TRC_GC_DMA, 12, "enter", NULL);
    
   
    l = (list_t *) dma_list;
    
    list_for_each_safe( i, tmp, l){
        dma_node = gc_get_data_ptr(  (void *)i);
        TRACE( TRC_GC_DMA, 12, "dma_node = 0x%x", dma_node);
        
        dma_free( dma_node);         
    }   

    dma_list_init( dma_list);    
    TRACE( TRC_GC_DMA, 12, "exit ", NULL);
}


