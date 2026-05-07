#ifndef ORDER_H
#define ORDER_H

#include "kitchen.h"

typedef struct order {
    dish** dishes;
    int patience;
    int order_time;
    bool done;
} order;

typedef struct order_queue{
    order* orders;
    int current_index; //at which point of the queue are we? 
}order_queue; 

extern order_queue order_q;
// Creates an order with a random selection of dishes and calculates patience
order* make_order(int num_dishes, order_queue* order_q);
int* get_order_price(order* o);
#endif /* ORDER_H */