#include "kitchen.h"
#include "customer.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


order* make_order(int num_dishes, order_queue* order_q){
    srand(time(NULL));
    order o;
    o.dishes = malloc(num_dishes*sizeof(dish));
    for(int i = 0; i < num_dishes; i++){
        o.dishes[i] = Menu.selection[rand()%Menu.num_dishes];
    }
    int baseline = 0;
    for(int i = 0; i < num_dishes; i++){
        baseline += o.dishes[i]->time;
    }
    o.order_time = current_time;
    o.patience = baseline + rand()%50; //strictly greater than order time
    order_q->orders[order_q->current_index] = o;
    return &o;
}

int* get_order_price(order* o){
    int* total_price = malloc(sizeof(int));
    *total_price = 0;
    for(int i = 0; i < sizeof(o->dishes)/sizeof(dish); i++){
        *total_price += o->dishes[i]->price;
    }
    return total_price;
}

int customer_loop(){ //returns the score given if he recieves what he wants or not
    srand(time(NULL));
    int num_orders = rand()%5;
    order* temp = make_order(num_orders, &order_q);
    for(int i = 0; i < temp->patience; i++){
        if(temp->done){
            return 1; //got the order in time. TO BE UPDATED
        }
        sleep(1);
    }
    return 0;  //didnt get the order in time.
}