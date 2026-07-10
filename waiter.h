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

EntertainmentActivity ea[5] = {
    {"chatting", 1, 1000000},
    {"singing", 2, 2000000},
    {"dancing", 3, 3000000},
    {"performing magic tricks", 5, 2500000},
    {"making puns", 2, 500000}
};

typedef struct ListNode {
    Order*            o;
    int               prio;
    struct ListNode* next;
} ListNode;

typedef struct OrderList {
    ListNode*      head;
    int             size;
    pthread_mutex_t lock;
} OrderList;

typedef struct OrderManager {
    OrderList*     waitlist;
    OrderList*     priority;
    OrderList*     completed_orders;
    OrderList*     discarded_orders;
    pthread_mutex_t lock;
} OrderManager;

typedef struct WaiterArgs {
    OrderManager* m;
    CustomerQueue* standing;
    CustomerQueue* seated;
    SimClock* sc;
    bool* running;
    sem_t* ea_bin;
} WaiterArgs;

typedef struct EntertainmentActivity {
    char* name;
    int efficacy;
    int duration;   // in microseconds
} EntertainmentActivity;

int    get_prio(Order* o, int algorithm);
void   list_insert(OrderList* l, Customer* c, int algorithm);
Order* list_pop(OrderList* l);
Order* peek(OrderList* l);
void   refill_priority(OrderManager* m);
void   waiter_loop(OrderManager* m, CustomerQueue* standing, CustomerQueue* seated, SimClock* sc, sem_t* ea_bin, bool* running);
void   list_insert_order(OrderList* l, Order* o, int algorithm);
void*  waiter_thread(void* arg);
void   om_init(OrderManager* om);
void   list_init(OrderList* ol);
int    customer_entertainment(EntertainmentActivity* ea);
void   take_order(CustomerQueue* seated, CustomerQueue* ordered, OrderList* waiting);

#endif