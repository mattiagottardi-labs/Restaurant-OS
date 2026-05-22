#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "customer.h"
#include "kitchen.h"
#include "clock.h"

order* make_order(int num_dishes) {
    srand(seed++);
    order* o = malloc(sizeof(order));
    if (!o) return NULL;

    // Allocate num_dishes + 1 to leave a guaranteed NULL sentinel at the end
    o->dishes = calloc(num_dishes + 1, sizeof(dish*));
    if (!o->dishes) {
        free(o);
        return NULL;
    }
    
    for(int i = 0; i < num_dishes; i++){
        o->dishes[i] = Menu[rand() % 20];
    }
    o->dishes[num_dishes] = NULL; // Explicitly enforce NULL termination
    
    o->time_to_finish = get_prep_time(o);
    return o;
}

bool is_done(order* o){
    for(int i = 0; o->dishes[i] != NULL; i++){
        if(!o->dishes[i]->ready) return false;
    }
    return true;
}

int count_done(order* o){
    int res = 0;
    for(int i = 0; o->dishes[i] != NULL; i++){
        if(o->dishes[i]->ready) res++;
    }
    return res;
}

int get_order_price(order* o) {
    if (!o || !o->dishes) return 0;
    int total = 0;
    /* iterate until a NULL pointer */
    for (int i = 0; o->dishes[i] != NULL; i++) {
        total += get_dish_price(o->dishes[i]);
    }
    return total;
}

static node* make_node(customer* c) {
    node* n = malloc(sizeof(node));
    if (!n) {
        fprintf(stderr, "make_node: out of memory\n");
        return NULL;
    }
    n->customer = c;
    n->extra_patience = c->patience - get_prep_time(c->o->dishes);
    n->next = NULL;
    return n;
}

void add_customer(customer* c, customer_queue* cq) {
    if (!c || !cq) return;
    node* new_node = make_node(c);
    if (!new_node) return;
    pthread_mutex_lock(&cq->lock);
    // Insert in sorted (ascending extra_patience) position
    if (!cq->start || new_node->extra_patience <= cq->start->extra_patience) {
        /* goes at the front */
        new_node->next = cq->start;
        cq->start = new_node;
    } else {
        node* cur = cq->start;
        while (cur->next && cur->next->extra_patience < new_node->extra_patience) {
            cur = cur->next;
        }
        new_node->next = cur->next;
        cur->next = new_node;
    }

    cq->num_customers++;
    pthread_mutex_unlock(&cq->lock);
}

void remove_customer(customer* c, customer_queue* cq) {
    if (!c || !cq) return;

    pthread_mutex_lock(&cq->lock); //lock
    node* cur  = cq->start;
    node* prev = NULL;

    while (cur) {
        if (cur->customer == c) {
            if (prev) {
                prev->next = cur->next;
            } else {
                cq->start = cur->next;
            }
            free(cur);
            cq->num_customers--;
            pthread_mutex_unlock(&cq->lock); //Unlock before returning
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&cq->lock); // Unlock if not found
}

float customer_loop(customer* c, customer_queue* cq, sim_clock* sim) {
    if (!c || !c->o) return 0;

    int last_tick = sim->tick;
    int elapsed   = 0;
    bool ran_out_of_patience = false;
    add_customer(c, cq);

    while (true) {
        if (c->served) {
            break;
        }

        pthread_mutex_lock(&sim->lock);
        while (sim->tick == last_tick) {
            pthread_cond_wait(&sim->tick_cv, &sim->lock);
        }
        elapsed += sim->tick - last_tick;
        last_tick = sim->tick;
        pthread_mutex_unlock(&sim->lock); 
        if (elapsed > c->patience) {
            ran_out_of_patience = true;
            break;
        }
    }

    if (ran_out_of_patience) {
        // Customer leaves queue entirely on their own terms
        remove_customer(c, cq); 
        return -get_order_price(c->o) * log2(1 + ((float)c->patience / (1 + count_done(c->o))));
    }

    return get_order_price(c->o) * (1 - ((float)elapsed / c->patience));
}

int get_prep_time(dish** dishes){
    int x = 0;
    int i = 0;
    while(dishes[i]){
        if(!dishes[i]->ready && !dishes[i]->cooking) x+= dishes[i]->time;
        i++;
    }
    return x;
}