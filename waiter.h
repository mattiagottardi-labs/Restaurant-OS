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

typedef enum WaiterState {
    IDLE,
    ACCOMODATING_CUSTOMER,
    TAKING_ORDER,
    DELIVERING_FOOD,
    ENTERTAINING
} WaiterState;

typedef struct ListNode {
    Order*           o;
    int              prio;
    struct ListNode* next;
} ListNode;

typedef struct OrderList {
    ListNode*       head;
    int             size;
    pthread_mutex_t lock;
} OrderList;

typedef struct OrderManager {
    OrderList*      waitlist;
    OrderList*      priority;
    OrderList*      completed_orders;
    OrderList*      discarded_orders;
    pthread_mutex_t lock;
} OrderManager;

typedef struct WaiterArgs {
    int                 id;
    OrderManager*       om;
    CustomerQueue*      standing;
    CustomerQueue*      seated;
    CustomerQueue*      waiting_order;
    SimClock*           sc;
    bool*               running;
    sem_t*              rc;
    sem_t*              ea_bin;
    pthread_mutex_t*    print;
} WaiterArgs;

typedef struct Waiter {
    WaiterArgs*     arg;
    WaiterState     present;
    WaiterState     future;
} Waiter;

typedef struct EntertainmentActivity {
    char*           name;
    int             efficacy;
    int             duration;   // in microseconds
} EntertainmentActivity;

extern EntertainmentActivity ea[5];

int    get_prio(Order* o, int algorithm);
void   list_insert(OrderList* ol, Customer* cst, int algorithm);
Order* list_pop(OrderList* ol);
void   refill_priority(OrderManager* om);
void   waiter_loop(Waiter* wtr);
void   list_insert_order(OrderList* ol, Order* o, int algorithm);
void*  waiter_thread(void* arg);
void   om_init(OrderManager* om);
void   list_init(OrderList* ol);
int    customer_entertainment(EntertainmentActivity* ea);
//void   take_order(CustomerQueue* seated, CustomerQueue* ordered, OrderList* waiting);
void    print_wtr(Waiter* wtr);
#endif