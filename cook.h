#ifndef COOK_H
#define COOK_H

#include "kitchen.h"
#include "clock.h"

#define DIRTY_THRESHOLD 3
#define GAME_SPEED      1

void    cook_dish(dish* d, sim_clock* sim, kitchen_manager* km, pthread_mutex_t sink);

tool*   acquire_pool(tool_pool* pool);
void    release_pool(tool_pool* pool, tool* t, sim_clock* sim, pthread_mutex_t sink);

tool**  acquire_tools(dish* d, kitchen_manager* km);
void    release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* sim, pthread_mutex_t sink);

int     count_tools(dish* d);
tool_pool* find_pool(const char* name, kitchen_manager* km);

#endif