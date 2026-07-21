#ifndef KITCHEN_H
#define KITCHEN_H
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

#include "utils.h"

// forward declaration — customer defined in customer.h
typedef struct Customer customer;
typedef struct Order order;

typedef struct Tool {
    char*           name;
    int             clean_time;
    int             dirty_usages;
    _Atomic bool    in_use;
    pthread_mutex_t lock;
} Tool;

typedef struct ToolPool {
    int             in_use;
    int             quantity;
    char*           name;
    Tool*           tools;
    pthread_mutex_t lock;
    pthread_cond_t  cv;
} ToolPool;

typedef struct KitchenManager {
    ToolPool**      pools;
    int             num_pools;
    pthread_mutex_t sink;        // shared sink mutex for all cooks
} KitchenManager;

typedef struct Dish {
    char*           name;
    int             price;
    int             time;
    char**          tools;
    _Atomic bool    ready;
    _Atomic bool    cooking;
    bool            delivered;
    pthread_mutex_t lock;
} Dish;

typedef struct Menu {
    Dish**  selection;
    int     num_dishes;
} Menu;

char*   my_strdup(const char* s);
void    make_tools(const char* tools_location, KitchenManager* km, int max_tools);
void    make_menu(const char* menu_location, Menu* menu, const int max_dishes, const int max_tools_per_dish);

#endif