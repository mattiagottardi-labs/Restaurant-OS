#ifndef ORDER_H
#define ORDER_H

#include "kitchen.h"

typedef struct order {
    dish** dishes;
    int patience;
    int order_time;
    bool done;
} order;

typedef struct customer{
    order *o;
    int patience;
}customer;

typedef struct node{
    customer* customer;
    node* next;
    int extra_patience; //customer are ranked by the amount of extra patience they have, so as to do the impatient first
}node;

typedef struct customer_queue{ //defined as a linked list, so as to make removal of customers easy upon delivering the goods
    node* start;
    int num_customers;
}customer_queue;
//we could implement different queues with different scheduling algorithms

order* make_order(int num_dishes);
int get_order_price(order* o); //price can be taken by value/copied, no need for a pointer
int customer_loop(); // returns the score, negative if he didnt get the food, positive if he got it !! INCOMPLETE
void add_customer(customer* c, customer_queue* cq); //should put customer into queue
void remove_customer(customer* c, customer_queue* cq); 

#endif 