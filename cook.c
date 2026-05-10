#include<stdlib.h>
#include<pthread.h>
#include<stdbool.h>
#include<string.h>
#include "customer.h"
#include "kitchen.h"
#include "cook.h"
#include "clock.h"
#include<math.h>
#define DIRTY_TRESHOLD 5 //initial approach is static, we'll adjust later.
#define GAME_SPEED 1

static pthread_mutex_t sink_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for sink access

void cook_dish(dish* d, sim_clock* clock, kitchen_manager* km, pthread_mutex_t sink) {
    tool** used = acquire_tools(d, km);
    if (!used) return;

    pthread_mutex_lock(&clock->lock);
    int ticks_remaining = d->time;
    while (ticks_remaining > 0) {
        pthread_cond_wait(&clock->tick_cv, &clock->lock);
        ticks_remaining--;
    }
    d->ready = true;
    pthread_mutex_unlock(&clock->lock);

    release_tools(used, d, km, clock, sink);
    free(used);
}

tool* acquire_pool(tool_pool* pool) {
    pthread_mutex_lock(&pool->lock);
    while (pool->in_use >= pool->quantity) {
        pthread_cond_wait(&pool->cv, &pool->lock);
    }
    // find a free tool slot
    tool* picked = NULL;
    for (int i = 0; i < pool->quantity; i++) {
        if (!pool->tools[i].in_use) {
            picked = &pool->tools[i];
            picked->in_use = true;
            break;
        }
    }
    if (!picked) {
        /* should never happen if in_use tracking is correct */
        fprintf(stderr, "acquire_pool: no free tool found despite in_use < quantity\n");
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }
    pool->in_use++;
    pthread_mutex_unlock(&pool->lock);
    return picked;
}

void release_pool(tool_pool* pool, tool* t, sim_clock* clock, pthread_mutex_t sink) {
    t->dirty_usages++;
    if (t->dirty_usages >= DIRTY_TRESHOLD ){
        // acquire sink for washing
        pthread_mutex_lock(&sink);
        // cook the cleaning ticks before releasing
        pthread_mutex_lock(&clock->lock);
        int clean_ticks = t->clean_time;
        while (clean_ticks > 0) {
            pthread_cond_wait(&clock->tick_cv, &clock->lock);
            clean_ticks--;
        }
        t->dirty_usages = 0;
        pthread_mutex_unlock(&clock->lock);
        pthread_mutex_unlock(&sink_mutex);
    }

    pthread_mutex_lock(&pool->lock);
    t->in_use = false; 
    pool->in_use--;
    pthread_cond_signal(&pool->cv);
    pthread_mutex_unlock(&pool->lock);
}

tool** acquire_tools(dish* d, kitchen_manager* km) {
    int n = count_tools(d);
    tool** used = calloc(n + 1, sizeof(tool*));  // NULL-terminated
    if (!used) {
        fprintf(stderr, "acquire_tools: out of memory\n");
        return NULL;
    }
    for (int i = 0; d->tools[i] != NULL; i++) {
        tool_pool* pool = find_pool(d->tools[i], km);
        if (!pool) continue;
        tool* t = acquire_pool(pool);
        if (!t) {
            fprintf(stderr, "acquire_tools: acquire_pool returned NULL for '%s'\n", d->tools[i]);
            continue;
        }
        used[i] = t;
    }
    return used;
}

void release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* clock, pthread_mutex_t sink) {
    if (!used) return;
    for (int i = 0; d->tools[i] != NULL; i++) {
        if (!used[i]) continue; // if failed aquire skip.
        tool_pool* pool = find_pool(d->tools[i], km);
        if (pool) release_pool(pool, used[i], clock, sink);
    }
}

int count_tools(dish* d){
    int i = 0;
    while(d->tools[i] != NULL) i++;
    return i;
}

tool_pool* find_pool(const char* tool_name, kitchen_manager* kitchen){
    for(int i = 0; i < kitchen->num_pools; i++){
        if(strcmp(kitchen->pools[i]->name, tool_name)==0)
        return kitchen->pools[i];
    }
    fprintf(stderr, "tool not found");
    return NULL;
}

/*dish* cook_loop(dish* d, order* o){
    if(d == NULL){
        printf("No dish to cook\n");
        return 0;
    }

    for(int i = 0; i < d->num_tools_required; i++){
        if(is_tool_available(d->tools) && !is_tool_dirty(d->tools)){ //tool available and clean
            //use tool
            printf("Tool %s used for %s, available\n", d->tools, d->name);
        }
    }else if(is_tool_available(d->tools) && is_tool_dirty(d->tools)){ //tool available but dirty
     //I need to understand if I should clean/use the tool dirty or if I should skip the client
        if(o->patience > (t->clean_time + d->time)){ //clean the tool and the cook the dish
            printf("Cleaning %s before cooking %s\n", d->tools, d->name);
            sink_cleaning(d->tools);
            printf("Tool %s used for  %s, available\n", d->tools, d->name);
        }else{ //I need to understand if it's worth for the restaurant cooking with dirty tools or skipping the client
            if(){
                //DA ANDARE AVANTI!!!
            }
        }

    }else{ //tool not available --> I need to search for another dish to make until I wait for the tool to be available
        printf("Tool not available, searching for another dish to cook\n");
        return NULL;
    }
    return d;  
}*/