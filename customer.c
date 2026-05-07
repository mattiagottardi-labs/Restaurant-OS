#include "kitchen.h"
#include "customer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


order make_order(int num_dishes){
    srand(time(NULL));
    order o;
    o.dishes = malloc(num_dishes*sizeof(dish));
    for(int i = 0; i < num_dishes; i++){
        o.dishes[i] = Menu.selection[rand()%Menu.num_dishes];
    }
    int baseline;
    for(int i = 0; i < num_dishes; i++){
        baseline += o.dishes[i]->time;
    }
    o.patience = rand()%30 + baseline; //strictly greater than order time
    return o;
}

int* get_order_price(order* o){
    int* total_price = malloc(sizeof(int));
    *total_price = 0;
    for(int i = 0; i < sizeof(o->dishes)/sizeof(dish); i++){
        *total_price += o->dishes[i]->price;
    }
    return total_price;
}