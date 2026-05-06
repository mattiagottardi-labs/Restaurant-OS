#include "menu_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct order{
    dish** dishes;
    int patience;
}order;

order* make_order(int num_dishes){
    srand(time(NULL));
    order *o;
    o->dishes = malloc(num_dishes*sizeof(dish));
    for(int i = 0; i < num_dishes; i++){
        o->dishes[i] = Menu.selection[rand()%Menu.num_dishes];
    }
    int baseline;
    for(int i = 0; i < num_dishes; i++){
        baseline += o->dishes[i]->time;
    }
    o->patience = rand()%30 + baseline; //strictly greater than order time
    return o;
}
