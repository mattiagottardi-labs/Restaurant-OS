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
        o->dishes[i] = Menu.selection[rand()%20];
    }
    o->patience = rand()%100;
    return o;
}
