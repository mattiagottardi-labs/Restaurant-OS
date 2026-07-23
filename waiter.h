/*
this file should serve as outline for the development of the waiter mechanics
feel free to adjust anything and comment on things you dont get.
please notify before making structural changes.
*/
#ifndef WAITER_H
#define WAITER_H
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#include "customer.h"
#include "kitchen.h"

typedef enum WaiterState {
    IDLE,
    ACCOMODATING_CUSTOMER,
    TAKING_ORDER,
    DELIVERING_DISH,
    ENTERTAINING
} WaiterState;

typedef struct OrderManager {
    DishList*       completed_dishes;
    OrderList*      waitlist;
    OrderList*      priority;
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
    WaiterArgs* arg;
    WaiterState present;
    WaiterState future;
} Waiter;

typedef struct EntertainmentActivity {
    char*           name;
    int             efficacy;
} EntertainmentActivity;

extern EntertainmentActivity ea[5];

int     get_prio(Order* o, int algorithm);
void    list_insert(OrderList* ol, Customer* cst, int algorithm);
void    list_insert_dish(DishList* dl, Dish* d);
void    list_insert_order(OrderList* ol, Order* o, int algorithm);
Order*  list_peek(OrderList* ol);
Order*  list_remove_order(OrderList* ol);
Dish*   list_remove_dish(DishList* dl);
void    refill_priority(OrderManager* om);
void    waiter_loop(Waiter* wtr);
void*   waiter_thread(void* arg);
void    om_init(OrderManager* om);
void    list_init(OrderList* ol);
void    customer_entertainment(Waiter* wtr, EntertainmentActivity* ea);
//void   take_order(CustomerQueue* seated, CustomerQueue* ordered, OrderList* waiting);
#endif

