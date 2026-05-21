/*
this file should serve as outline for the development of the waiter mechanics
feel free to adjust anything and comment on things you dont get.
please notify before making structural changes.
*/
#ifndef WAITER_H
#define WAITER_H
#include "customer.h"
#include "kitchen.h"

typedef struct dish_queue{
    int num_dishes;
    int max_capacity;
    int priority;
    int avg_patience; //should keep track of this particular queue's avg patience to fit the order to it's best placement
    dish** queue;
}dish_queue;

typedef struct queue_manager{
    int num_queue;
    int max_queues;
    int* queue_balance; //will keep track of how many dishes are already in each queue to balance it eventually.
    dish_queue** queue_array;
}queue_manager;

//should take the top customer, remove it from the cq tre and place his order in the best queue;
void schedule_order(queue_manager* qm, customer* nc);
void queue_balancing(queue_manager* qm);
int average_calculator(dish_queue* q);
dish_queue* create_new_queue(int priority); 

#endif