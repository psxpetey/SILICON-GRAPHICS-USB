UHCI_QHCTL

    uhci_transfer_queues_t  *tq;    
    
soft->tq = tq;
soft->framelist = fl;


    tq->queue_heads[UHCI_QH1].element = UHCI_PTR_TERM;
    tq->queue_heads[UHCI_QH1].link = kaddr_2_daddr( dma_node_tq->daddr, 
        dma_node_tq->kaddr, &tq->term_qh) | UHCI_PTR_QH;

int uhci_insert_queue( dma_node_t *q, usbuhci_instance_t *soft, int numtq){
    uhci_transfer_queues_t  *tq = soft->tq; 
    uhci_control_chain_t *chain = (uhci_control_chain_t *) q->mem;
    
    
    chain->qh.link = tq->queue_heads[numtq].element;
    chain->qh.klink = tq->queue_heads[numtq].kelement;
    chain->qh.kprior_link = &tq->queue_heads[numtq];
    
    tq->queue_heads[numtq].element = q->daddr | UHCI_PTR_QH;
    tq->queue_heads[numtq].kelement = q->mem;
    return(0);
}


int uhci_remove_queue( dma_node_t *q, usbuhci_instance_t *soft, int numqt){
    uhci_transfer_queues_t  *tq = soft->tq; 
    uhci_control_chain_t *chain = (uhci_control_chain_t *) q->mem;
    uhci_qh_t *endqh = soft->tq->term_qh;
    uint32_t aux;
    
    tq->queue_heads[numtq].element = chain->qh.link;
    tq->queue_heads[numtq].kelement = chain->qh.klink;
    
    
    
 
}

