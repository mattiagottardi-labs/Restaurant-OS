#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include<pthread.h>
#include "customer.h"
#include "kitchen.h"
#include "clock.h"
#include "waiter.h"
#define MAX_ORDERS_PER_QUEUE 5
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

void select_customer(customer_queue* cq){
//ALDO MORO E MATTEOTTI GRANDI FIGURE DELLA STORIA
}

void schedule_order(queue_manager* qm, customer* nc){
    if(!qm || !nc || !nc->o || !nc->o->dishes) return;

    //creating the first queue, if there are noone yet
    if(qm->num_queue == 0){
        qm->queue_start = create_new_queue(MAX_ORDERS_PER_QUEUE);
        qm->queue_balance = malloc(sizeof(int));
        qm->queue_balance[0] = 0;
        qm->num_queue = 1;
    }

    //controls if the system need new queues
    queue_balancing(qm);

    //find the best queue for the order
    order_queue* best_queue = NULL;
    int best_queue_index = -1; //index of the best queue for the order
    int min_score = INT_MAX; //is set to max because we're trying to find the lowest penalty score

    order_queue* current_queue = qm->queue_start;
    int i = 0;

    while(current_queue != NULL && i < qm->num_queue){
        //skip the queue if it's full
        if(current_queue->num_orders >= current_queue->max_capacity){
            current_queue = current_queue->next_queue;
            i++;
            continue;
        }

        int current_avg = current_queue->avg_patience;
        if(current_queue->num_orders == 0){
            current_avg = nc->patience; //if the queue is empty, we consider the new order's patience as the average
        }

        int patience_diff = abs(current_avg - nc->patience);
        int queue_load_penalty = qm->queue_balance[i]; //penalty for the current load of the queue
        int score = patience_diff + queue_load_penalty;

        if(score < min_score){
            min_score = score;
            best_queue = current_queue;
            best_queue_index = i;
        }

        current_queue = current_queue->next_queue;
        i++;
    }

    //Assign the order to the best queue found
    if(best_queue != NULL && best_queue_index != -1){
        pthread_mutex_lock(&best_queue->lock);
        //Recalculate avg patience before index push
        best_queue->avg_patience = average_calculator(best_queue, nc->patience);

        best_queue->queue[best_queue->num_orders] = nc->o; //add the order to the queue
        best_queue->num_orders++;
        qm->queue_balance[best_queue_index] ++;

        pthread_mutex_unlock(&best_queue->lock);
    } else {
        fprintf(stderr, "schedule_order: no suitable queue found for the order\n");
    }
}


int average_calculator(order_queue* q, int new_patience){
    if(!q || q->num_orders == 0) return new_patience;
    return (q->avg_patience * q->num_orders + new_patience) / (q->num_orders + 1);
}

order_queue* create_new_queue(int max_capacity){
    order_queue* q = malloc(sizeof(order_queue));
    if(!q) return NULL;

    q->num_orders = 0;
    q->max_capacity = max_capacity;
    q->avg_patience = 0;
    q->queue = malloc(sizeof(order*) * max_capacity);
    if(!q->queue){
        free(q);
        return NULL;
    }
    q->next_queue = NULL;
    pthread_mutex_init(&q->lock, NULL);
    return q;
}

void give_order_to_customer(order* o, order_queue* oq){
    if(!o || !oq) return; // the queue is empty, there isn't any order ready
    pthread_mutex_lock(&oq->lock);

    while(oq->num_orders > 0){
        oq->queue[oq->num_orders - 1]->customer->served = true; //mark the customer as served
        oq->num_orders--; //remove the order from the queue
    }
    pthread_mutex_unlock(&oq->lock);
    //we implemented this in a way that the waiter empties all the queue, if there is more than a order ready
}