#ifndef CUSTOMER_H
#define CUSTOMER_H
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include "kitchen.h"
#include <semaphore.h>
#include <unistd.h>

typedef struct CustomerQueue CustomerQueue;

typedef enum CustomerState {
    STANDING,
    SEATED,
    WAITING_ORDER,
    EATING,
    FINISHED,
    TIRED
} CustomerState;

typedef struct Order {
    Dish**              dishes;
    _Atomic int         remaining_time;
    _Atomic bool        expired;
    _Atomic bool        completed;
    _Atomic int         total_price;
    struct Customer*    c;
    pthread_mutex_t     lock;
} Order;

typedef struct CustomerArgs {
    int                 id;
    SimClock*           sc;
    Menu*               menu;
    _Atomic float*      score;
    bool*               running;
    sem_t*              rc;
    CustomerQueue*      standing;
    pthread_mutex_t*    print;
} CustomerArgs;

typedef struct Customer {
    Order*                  o;
    int                     patience;
    _Atomic bool            served;
    _Atomic int             order_made;
    _Atomic int             order_received;
    pthread_mutex_t         lock;
    _Atomic CustomerState   present;
    _Atomic CustomerState   future;
    CustomerArgs*           arg;
} Customer;

typedef struct QueueNode {
    Customer*         c;
    struct QueueNode* next;
} QueueNode;

typedef struct CustomerQueue {
    QueueNode*      head;
    QueueNode*      tail;
    int             size;
    int             max_size;
    pthread_mutex_t lock;
} CustomerQueue;

// Order creation
Order*      make_order(Customer* c, Menu* menu, int num_dishes);
Dish*       copy_dish(Dish* src);
void        free_order(Order* o);

// queue
bool        is_empty(void* q, Casting cast);
void        enqueue(Customer* c, CustomerQueue* q);
void        dequeue(CustomerQueue* q);
Customer*   pop(CustomerQueue* q);
Customer*   peek(CustomerQueue* q);
void        clean(CustomerQueue* q);
int         get_prep_time(Order* o);
int         get_price(Order* o);
float       atomic_float_add(_Atomic float *target, float amount);
// customer lifecycle
void        customer_loop(Customer* c);
void*       customer_thread(void* arg);
void        print_cst(Customer* cst);
#endif
