#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "customer.h"
#include "kitchen.h"
#include "clock.h"

order* make_order(int num_dishes) {
    order* o = malloc(sizeof(order));
    if (!o) {
        fprintf(stderr, "make_order: out of memory\n");
        return NULL;
    }
    o->dishes = calloc(num_dishes, sizeof(dish*));
    if (!o->dishes) {
        fprintf(stderr, "make_order: out of memory for dishes\n");
        free(o);
        return NULL;
    }
    o->patience = 0;
    o->order_time = 0;
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
    if (!c || !cq || !cq->start) return;

    node* cur  = cq->start;
    node* prev = NULL;
    pthread_mutex_lock(&cq->lock);
    while (cur) {
        if (cur->customer == c) {
            if (prev) {
                prev->next = cur->next;
            } else {
                cq->start = cur->next;
            }
            free(cur);
            cq->num_customers--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&cq->lock);
    fprintf(stderr, "remove_customer: customer not found in queue\n");
}

float customer_loop(customer* c, customer_queue* cq, sim_clock* sim) {
    if (!c || !c->o) return 0;

    int last_tick = sim->tick;
    int elapsed   = 0;

    pthread_mutex_lock(&sim->lock);
    while (!is_done(c->o)) {
        pthread_cond_wait(&sim->tick_cv, &sim->lock);

        if (sim->tick == last_tick) continue;
        elapsed += sim->tick - last_tick;
        last_tick = sim->tick;

        if (elapsed > c->patience) {
            pthread_mutex_unlock(&sim->lock);
            remove_customer(c, cq);
            return -get_order_price(c->o) * log2(1 + ((float)c->patience / (1 + count_done(c->o))));
        }
    }
    pthread_mutex_unlock(&sim->lock);
    remove_customer(c, cq);
    return get_order_price(c->o) * (1 - ((float)elapsed / c->patience));
}

int get_prep_time(dish** dishes){
    int x = 0;
    int i = 0;
    while(dishes[i]){
        x+= dishes[i]->time;
        i++;
    }
    return x;
}