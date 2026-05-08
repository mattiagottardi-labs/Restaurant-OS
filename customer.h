#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <stdbool.h>
#include "kitchen.h"

typedef struct order {
    dish**  dishes;
    int     patience;
    int     order_time;
    bool    done;
} order;

typedef struct customer {
    order* o;
    int    patience;
} customer;

typedef struct node {
    customer* customer;
    struct node* next;
    int extra_patience; // patience - prep_time; lower = served first
} node;

typedef struct customer_queue {
    node* start;
    int   num_customers;
} customer_queue;

order* make_order(int num_dishes);
int    get_order_price(order* o);
bool   is_done(order* o);
int    count_done(order* o);

int    get_prep_time(dish** dishes);

void   add_customer(customer* c, customer_queue* cq);
void   remove_customer(customer* c, customer_queue* cq);

// lifecycle — returns a score (see customer.c for scoring formula) 
float  customer_loop(customer* c, customer_queue* cq); //THIS is what we call in main.

#endif