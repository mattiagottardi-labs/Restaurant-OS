#ifndef KITCHEN_H
#define KITCHEN_H
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include "utils.h"

// forward declaration — customer defined in customer.h
typedef struct customer customer;
typedef struct order order;

typedef struct tool {
    char*           name;
    int             clean_time;
    int             dirty_usages;
    _Atomic bool    in_use;
    pthread_mutex_t lock;
} tool;

typedef struct tool_pool {
    int             in_use;
    int             quantity;
    char*           name;
    tool*           tools;
    pthread_mutex_t lock;
    pthread_cond_t  cv;
} tool_pool;

typedef struct kitchen_manager {
    tool_pool** pools;
    int         num_pools;
    pthread_mutex_t sink;        // shared sink mutex for all cooks
} kitchen_manager;

typedef struct dish {
    char*           name;
    int             price;
    int             time;
    struct order*   o;
    char**          tools;
    _Atomic bool    ready;
    _Atomic bool    cooking;
    pthread_mutex_t lock;
} dish;

typedef struct menu {
    dish**  selection;
    int     num_dishes;
} menu;

char* my_strdup(const char* s);
void make_tools(const char* tools_location, kitchen_manager* km, int max_tools);
void make_menu(const char* menu_location, menu* Menu, const int max_dishes, const int max_tools_per_dish);

#endif