/*
this file should serve as outline for the development of the waiter mechanics
feel free to adjust anything and comment on things you dont get.
please notify before making structural changes.
*/
#ifndef WAITER_H
#define WAITER_H
#include "customer.h"
#include "kitchen.h"

typedef struct order_queue{
    int num_orders;
    int max_capacity;
    int avg_patience; //should keep track of this particular queue's avg patience to fit the order to it's best placement
    order** queue;
    order_queue* next_queue; //pointer to the next queue, if we need to create more than one
    pthread_mutex_t lock; //enables thread safety, one must acquire the lock before modifying
}order_queue;

typedef struct queue_manager{
    int num_queue;
    int max_queues;
    int* queue_balance; //will keep track of how many dishes are already in each queue to balance it eventually.
    order_queue* queue_start; //pointer to the first queue in the list
    pthread_mutex_t lock; //thr safety
}queue_manager;

//should take the top customer, remove it from the cq tre and place his order in the best queue;
void schedule_order(queue_manager* qm, customer* nc);
void queue_balancing(queue_manager* qm);
int average_calculator(order_queue* q);
order_queue* create_new_queue(int priority); 

void give_order_to_customer(order* o, order_queue* oq);


#endif
