#ifndef COOK_H
#define COOK_H
#include <pthread.h>
#include <stdatomic.h>
#include "kitchen.h"
#include "waiter.h"
#include "utils.h"

#define DIRTY_THRESHOLD 3

// cook arguments structure passed to the thread
typedef struct cook_args {
    order_manager* m;
    sim_clock* sc;
    kitchen_manager* km;
} cook_args;

// tool helpers
int        count_tools(dish* d);
tool_pool* find_pool(const char* name, kitchen_manager* km);
tool*      acquire_pool(tool_pool* pool);
void       release_pool(tool_pool* pool, tool* t, sim_clock* sc, kitchen_manager* km);
tool**     acquire_tools(dish* d, kitchen_manager* km);
void       release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* sc);

// order/dish selection
order* get_next_order(order_manager* m);
dish*  pick_dish(order* o);

// cook lifecycle
void cook_dish(dish* d, order* o, order_manager* m, sim_clock* sc, kitchen_manager* km);
void cook_loop(order_manager* m, sim_clock* sc, kitchen_manager* km);
float get_pressure(order_list* l); //will estimate how hard the kitchen must work
void* cook_thread(void* arg);
#endif
