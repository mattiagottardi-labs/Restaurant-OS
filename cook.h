#ifndef COOK_H
#define COOK_H

#include "kitchen.h"
#include "clock.h"

#define DIRTY_THRESHOLD 5
#define GAME_SPEED      1

/* cooking */
void    cook_dish(dish* d, sim_clock* sim, kitchen_manager* km);

/* tool pool */
tool*   acquire_pool(tool_pool* pool);
void    release_pool(tool_pool* pool, tool* t, sim_clock* sim);

/* tool set for a dish */
tool**  acquire_tools(dish* d, kitchen_manager* km);
void    release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* sim);

/* helpers */
int     count_tools(dish* d);
tool_pool* find_pool(const char* name, kitchen_manager* km);

#endif