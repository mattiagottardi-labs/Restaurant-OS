#ifndef CUSTOMER_H
#define CUSTOMER_H
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include "kitchen.h"
#include <semaphore.h>

typedef enum CustomerState {
    STANDING,
    SEATED,
    ORDERING,
    WAITING_ORDER,
    EATING,
    FINISHED
} CustomerState;

typedef struct Order {
    Dish**           dishes;
    _Atomic int      remaining_time;
    _Atomic bool     expired;
    _Atomic bool     completed;
    struct Customer* c;
    pthread_mutex_t  lock;
} Order;

typedef struct CustomerArgs {
  CustomerQueue*    q;
  SimClock*         sc;
  Menu*             menu;
  _Atomic float*    score;
  bool*             running;
} CustomerArgs;

typedef struct Customer {
    Order*          o;
    int             patience;
    _Atomic bool    served;
    _Atomic bool    discarded;
    pthread_mutex_t lock;
    CustomerState   present;
    CustomerState   future;
    CustomerArgs*   arg;
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
bool        is_empty(CustomerQueue* q);
void        enqueue(Customer* c, CustomerQueue* q);
Customer*   dequeue(CustomerQueue* q);
Customer*   pop(CustomerQueue* q);
Customer*   peek(CustomerQueue* q);
void        clean(CustomerQueue* q);
int         get_prep_time(Order* o);
// customer lifecycle
void        customer_loop(Customer* c);
void*       customer_thread(void* arg);
#endif