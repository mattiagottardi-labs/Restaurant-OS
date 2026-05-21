#ifndef KITCHEN_H
#define KITCHEN_H

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

extern menu Menu;
extern kitchen_manager kitchen;

typedef struct tool {
    char* name;
    int clean_time;
    int dirty_usages;
    bool in_use;
} tool;

typedef struct tool_pool {
    int             in_use;
    int             quantity;
    char*           name;
    tool*           tools;
    pthread_mutex_t lock;
    pthread_cond_t  cv;
} tool_pool;

typedef struct kitchen_manager{
    tool_pool** pools;
    int num_pools;
} kitchen_manager;

typedef struct dish {
    char* name;
    int price;
    int time;
    order* o;
    char** tools; // Points to an array of strings
    bool ready;
    bool cooking;
    pthread_mutex_t lock;
} dish;

typedef struct menu {
    dish** selection;
    int num_dishes;
} menu;

void make_tools(const char* tools_location, kitchen_manager* my_kitchen, const int max_tools);
void make_menu(const char* menu_location, menu* MyMenu, const int max_dishes);

#endif