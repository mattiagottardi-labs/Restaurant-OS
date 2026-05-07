#ifndef KITCHEN_H
#define KITCHEN_H

#include <stdio.h>
#include <stdbool.h>

typedef struct tool {
    char* name;
    int clean_time;
    int dirty_usages;
} tool;

typedef struct tool_pool {
    bool available;
    int in_use;
    int quantity;
    char* name;
    tool* tools; 
} tool_pool;

typedef struct kitchen_manager {
    tool_pool** pools;
    int num_pools;
} kitchen_manager;

typedef struct dish {
    char* name;
    int price;
    int time;
    char** tools; // Points to an array of strings
    int num_tools_required;
} dish;

typedef struct menu {
    dish** selection;
    int num_dishes;
} menu;

void make_tools(char* tools_location, kitchen_manager* my_kitchen, int max_tools);
void make_menu(char* menu_location, menu* MyMenu, int max_dishes);

#endif