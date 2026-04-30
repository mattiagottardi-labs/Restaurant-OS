// menu_builder.h
#ifndef MENU_BUILDER_H
#define MENU_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tool {
    char* name;
    int clean_time;
} tool;

typedef struct dish {
    char* name;
    double price;
    tool tools[4];
} dish;

typedef struct menu {
    dish** selection;
    int num_dishes;
} menu;

// ── Globals ────────────────────────────────────

extern char* tool_names[9];
extern menu Menu;

// ── Function Declarations ──────────────────────

int  get_clean_time(char* tool_name);
tool ctot(char* arg);
void make_menu(char* file_location, menu* Menu, int max_dishes);

#endif // MENU_BUILDER_H
