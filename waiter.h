/*
this file should serve as outline for the development of the waiter mechanics
feel free to adjust anything and comment on things you dont get.
please notify before making structural changes.
*/

#include "customer.h"
#include "kitchen.h"

typedef struct dish_queue{
    int num_dishes; // if a queue is too populated it should create a new queue and split the dishes
                    // if no dishes in queue queue should removed
    int priority;
    int avg_patience; //should keep track of this particular queue's avg patience to fit the order to it's best placement
    dish** queue;
}dish_queue;

typedef struct queue_manager{
    int num_queue;
    int* queue_balance; //will keep track of how many dishes are already in each queue to balance it eventually.
    dish_queue** queue_array;
}queue_manager;

//should take the top customer, remove it from the cq tre and place his order in the best queue;
void schedule_order(customer_queue* cq, queue_manager* qm);  