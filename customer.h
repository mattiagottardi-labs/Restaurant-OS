#ifndef ORDER_H
#define ORDER_H

#include "menu_builder.h"

typedef struct order {
    dish** dishes;
    int patience;
} order;

// Creates an order with a random selection of dishes and calculates patience
order* make_order(int num_dishes);

#endif /* ORDER_H */