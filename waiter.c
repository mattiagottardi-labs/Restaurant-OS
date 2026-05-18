#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "customer.h"
#include "kitchen.h"
#include "clock.h"
#include "waiter.h"
#define MAX_DISHES_PER_QUEUE 10
#define CRITICAL_LOAD_THRESHOLD 8

//balancing and dynamic creation of new lists
void queue_balancing(queue_manager* qm){
    if(!qm) return;

    bool all_queues_overloaded = true;
    //controls if all the existing queues are full
    for(int i = 0; i < qm->num_queue; i++){
        if(qm->queue_balance[i] < CRITICAL_LOAD_THRESHOLD){
            all_queues_overloaded = false;
            break;
        }
    }

    //if all the queues are full, creates a new queue
    if(all_queues_overloaded && qm->num_queue < qm->max_queues){
        int new_index = qm->num_queue;
        qm->queue_array = realloc(qm->queue_array, sizeof(dish_queue*) * (new_index + 1));
        qm->queue_balance = realloc(qm->queue_balance, sizeof(int) * (new_index + 1));
     
        qm->queue_array[new_index] = create_new_queue(1);
        qm->queue_balance[new_index] = 0;

        qm->num_queue++;
    }
}

void schedule_order(queue_manager* qm, customer* nc){
    if(!qm || !nc || !nc->o || !nc->o->dishes) return;

    //creating the first array
    if(qm->num_queue == 0){
        qm->queue_array = malloc(sizeof(dish_queue*));
        qm->queue_balance = malloc(sizeof(int));
        qm->queue_array[0] = create_new_queue(1);
        qm->queue_balance[0] = 0;
        qm->num_queue = 1;
    }

    queue_balancing(qm);
    
}


int average_calculator(dish_queue* q){
    if(!q || q->num_dishes == 0) return 0;

    int sum = 0;
    for(int i = 0; i < q->num_dishes; i++){
        if(q->queue[i] && q->queue[i]->customer){
            sum += q->queue[i]->customer->patience;
        }
    }
    return sum / q->num_dishes;
}

dish_queue* create_new_queue(int priority){
    dishes_queue* q = malloc(sizeof(dish_queue));
    if(!q) return NULL;

    q->num_dishes = 0;
    q->max_capacity = MAX_DISHES_PER_QUEUE;
    q->priority = priority;
    q->avg_patience = 0;
    q->queue = malloc(sizeof(dish*)*q->max_capacity);
    return q;
}

