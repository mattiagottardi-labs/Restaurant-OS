#ifndef COOK_H
#define COOK_H

#include "kitchen.h"
#include "utils.h"
#include "waiter.h"

#define DIRTY_THRESHOLD1 3
#define DIRTY_THRESHOLD2 5
#define GAME_SPEED      1

void    cook_dish(dish* d, sim_clock* sim, kitchen_manager* km, pthread_mutex_t sink);

tool*   acquire_pool(tool_pool* pool);
void    release_pool(tool_pool* pool, tool* t, sim_clock* sim, pthread_mutex_t sink);

tool**  acquire_tools(dish* d, kitchen_manager* km);
void    release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* sim, pthread_mutex_t sink);

int     count_tools(dish* d);
tool_pool* find_pool(const char* name, kitchen_manager* km);

void push_finished(order* o, order_queue* oq) //sends the finished order to the order queue

dish* pick_dish(order_queue* oq);
order_queue* pick_queue(queue_manager* qm);

#endif
