#ifndef KITCHEN_H
#define KITCHEN_H

#include <stdio.h>
#include <stdbool.h>

// --- Structures ---

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
    tool* tools; // Array of tool objects
} tool_pool;

typedef struct dish {
    char* name;
    int price;
    int time;
    char** tools; // Changed to char** to store multiple tool names
    int num_tools_required;
} dish;

typedef struct menu {
    dish** selection;
    int num_dishes;
} menu;

// --- Function Prototypes ---

/**
 * Initializes the tool pools based on a CSV file.
 * Returns a pointer to an array of tool_pool pointers.
 */
tool_pool** make_tools(char* tools_location, int max_tools);

/**
 * Initializes the menu based on a CSV file.
 */
void make_menu(char* menu_location, menu* Menu, char* tools_location, int max_dishes);

/**
 * Helper to find cleaning time from the tools CSV.
 */
int get_clean_time(const char* tool_name, const char* tools_location);

#endif