/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: list.c                                                     *
* Description: Double linked list utilities                        *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-04-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#define LIST(l)                       ((list_t *)(l))
#define INIT_LIST_HEAD(ptr)           (ptr)->next = (ptr); (ptr)->prev = (ptr)
#define LIST_HEAD_INIT(ptr)           { &(ptr), &(ptr) }
#define list_init( ptr)               INIT_LIST_HEAD(ptr)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

    
struct list_entry {
    struct list_entry *next;
    struct list_entry *prev;
};

typedef struct list_entry list_t;
typedef struct list_entry list_entry_t;


int list_empty (list_t *list);
void list_add (list_t *new_entry, list_t *head);
void list_add_tail (list_t *new_entry, list_t *head);
void list_del (list_t *entry);






int list_empty (list_t *list){
    return (list->next == list);
}

void list_add (list_t *new_entry, list_t *head){
    new_entry->next  = head->next;
    new_entry->prev  = head;
    head->next->prev = new_entry;
    head->next       = new_entry;
}

void list_add_tail (list_t *new_entry, list_t *head){
    new_entry->next  = head;
    new_entry->prev  = head->prev;
    head->prev->next = new_entry;
    head->prev       = new_entry;
}

void list_del (list_t *entry){
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

