#ifndef CUSTOMER_H
#define CUSTOMER_H
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include "kitchen.h"
#include <semaphore.h>

typedef struct order {
    dish**           dishes;
    _Atomic int      remaining_time;
    _Atomic bool     expired;
    _Atomic bool     completed;
    struct customer* c;
    pthread_mutex_t  lock;
} order;

typedef struct customer {
    order*          o;
    int             patience;
    _Atomic bool    served;
    _Atomic bool    discarded;
    pthread_mutex_t lock;
} customer;

typedef struct queue_node {
    customer*          c;
    struct queue_node* next;
} queue_node;

typedef struct customer_queue {
    queue_node*     head;
    queue_node*     tail;
    int             size;
    int             max_size;
    pthread_mutex_t lock;
} customer_queue;

// queue for the customers standing outside the restaurant
typedef struct standing_customer_queue {
    customer_queue* stq;
} standing_customer_queue;

typedef struct customer_args{
  customer_queue*   q;
  sim_clock*        sc;
  menu*             Menu;
  _Atomic float*    score;
  bool*             running;
  sem_t*            restaurant_capacity;
} customer_args;

// order creation
order*  make_order(customer* c, menu* Menu, int num_dishes);
dish*   copy_dish(dish* src);
void    free_order(order* o);

// queue
bool      is_empty(customer_queue* q);
void      enqueue(customer* c, customer_queue* q);
void      dequeue(customer_queue* q);
customer* pop(customer_queue* q);
customer* peek(customer_queue* q);
void      clean(customer_queue* q);
int       get_prep_time(order* o);
// customer lifecycle
void    customer_loop(customer* c, customer_queue* q, sim_clock* sc, _Atomic float* score, sem_t* restaurant_capacity);
void*   customer_thread(void* arg);


#endif
