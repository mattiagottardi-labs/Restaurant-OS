/*
this file should serve as outline for the development of the waiter mechanics
feel free to adjust anything and comment on things you dont get.
please notify before making structural changes.
*/
#ifndef WAITER_H
#define WAITER_H
#include <pthread.h>
#include <stdatomic.h>
#include "customer.h"
#include "kitchen.h"

typedef struct list_node {
    order*            o;
    int               prio;
    struct list_node* next;
} list_node;

typedef struct order_list {
    list_node*      head;
    int             size;
    pthread_mutex_t lock;
} order_list;

typedef struct order_manager {
    order_list*     waitlist;
    order_list*     priority;
    int             priority_size;
    _Atomic bool    running;
    order_list*     completed_orders;
    order_list*     discarded_orders;
    pthread_mutex_t lock;
} order_manager;

typedef struct waiter_args{
    order_manager* m;
    customer_queue* q;
    sim_clock* sc;
} waiter_args;

int    get_prio(order* o, int algorithm);
void   list_insert(order_list* l, customer* c, int algorithm);
order* list_pop(order_list* l);
void   refill_priority(order_manager* m);
void   waiter_loop(order_manager* m, customer_queue* q, sim_clock* sc);
void   list_insert_order(order_list* l, order* o, int algorithm);
void* waiter_thread(void* arg);
#endif
