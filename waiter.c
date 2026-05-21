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
        qm->queue_array = realloc(qm->queue_array, sizeof(order_queue*) * (new_index + 1));
        qm->queue_balance = realloc(qm->queue_balance, sizeof(int) * (new_index + 1));
     
        qm->queue_array[new_index] = create_new_queue(1);
        qm->queue_balance[new_index] = 0;

        qm->num_queue++;
    }
}

void schedule_order(queue_manager* qm, customer* nc){
    if(!qm || !nc || !nc->o || !nc->o->dishes) return;

    //creating the first queue, if there are noone yet
    if(qm->num_queue == 0){
        qm->queue_array = malloc(sizeof(dish_queue*));
        qm->queue_balance = malloc(sizeof(int));
        qm->queue_array[0] = create_new_queue(1);
        qm->queue_balance[0] = 0;
        qm->num_queue = 1;
    }

    //controls if the system need new queues
    queue_balancing(qm);

    //find the best queue for the order
    int best_queue_index = -1; //index of the best queue for the order
    int min_score = INT_MAX;

    //analyzes each queue and fint the best match
    for(int i = 0; i < qm->num_queue; i++){
        dish_queue* current_queue = qm->queue_array[i];
        //if the queue is full, skip it
        if(current_queue->num_dishes >= current_queue->max_capacity) continue;

        int current_avg = current_queue->avg_patience;

        if(current_queue->num_orders == 0){ //this is a superb match
            current_avg = nc->patience; //if the queue is empty, use the new order's patience as the average
        }

        int patience_diff = abs(current_avg - nc->patience); //abs to evitate negative values
        int queue_load_penalty = qm->queue_balance[i]; //the more loaded the queue is, the higher the penalty
        int score = patience_diff + queue_load_penalty; //this will give us a global score for the queue

        if(score < min_score){
            min_score = score;
            best_queue_index = i;
        }
    }

    if(best_queue_index != -1){
        order_queue* target_queue = qm->queue_array[best_queue_index];

        target_queue->queue[target_queue->num_orders] = nc->o; //place the order in the queue
        target_queue->num_orders++;
        qm->queue_balance[best_queue_index]++;//update the queue balance
        target_queue->avg_patience = average_calculator(target_queue); //update the average patience of the queue
    
    }else{
        fprintf(stderr, "Error: No suitable queue found for the order.\n");
    }
}


int average_calculator(order_queue* q){
    if(!q || q->num_orders == 0) return 0;

    int sum = 0;
    for(int i = 0; i < q->num_orders; i++){
        if(q->queue[i] && q->queue[i]->o){
            sum += q->queue[i]->o->patience;
        }
    }
    return sum / q->num_orders;
}

order_queue* create_new_queue(int priority){
    order_queue* q = malloc(sizeof(*q));
    if(!q) return NULL;

    q->num_orders = 0;
    q->max_capacity = MAX_ORDERS_PER_QUEUE;
    q->priority = priority;
    q->avg_patience = 0;
    q->queue = malloc(sizeof(*q->queue) * q->max_capacity);
    if(!q->queue){
        free(q);
        return NULL;
    }
    return q;
}

