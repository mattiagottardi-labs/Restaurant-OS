#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "cook.h"
#include "customer.h"

/* --------------------------------------------------------------------------
 * Tool helpers
 * -------------------------------------------------------------------------- */
int count_tools(Dish* d) {
    int i = 0;
    while (d->tools[i] != NULL) i++;
    return i;
}

void remove_order(OrderList* ol, Order* target){
    pthread_mutex_lock(&ol->lock);

    OrderListNode* temp = ol->head;
    OrderListNode* prev = NULL;

    while(temp && temp->o != target){
        prev = temp;
        temp = temp->next;
    }

    if(!temp){
        pthread_mutex_unlock(&ol->lock);
        return;
    }

    if(prev){
        prev->next = temp->next;
    } else {
        ol->head = temp->next;
    }

    ol->size--;
    free(temp);

    pthread_mutex_unlock(&ol->lock);
}


float get_pressure(OrderList* ol){
  int tot_prio;
  int tot_orders;
  pthread_mutex_lock(&ol->lock);
  OrderListNode* current = ol->head;
  for(int i = 0; i < ol->size; i++){
    if(atomic_load(&current->o->completed) || atomic_load(&current->o->expired)){
      current = current->next;
      continue;
    }
    tot_prio += get_prio(current->o, 0);
    tot_orders++;
  }
  return (float) tot_prio/tot_orders;
}

ToolPool* find_pool(const char* tool_name, KitchenManager* km) {
    for (int i = 0; i < km->num_pools; i++) {
        if (strcmp(km->pools[i]->name, tool_name) == 0)
            return km->pools[i];
    }
    fprintf(stderr, "find_pool: Tool '%s' not found\n", tool_name);
    return NULL;
}

Tool* acquire_pool(ToolPool* pool) {
    //printf("\tAcquiring pool %s\n", pool->name);
    pthread_mutex_lock(&pool->lock);
    while (pool->in_use >= pool->quantity)
        pthread_cond_wait(&pool->cv, &pool->lock);

    Tool* picked = NULL;
    for (int i = 0; i < pool->quantity; i++) {
        if (!atomic_load(&pool->tools[i].in_use)) {
            picked = &pool->tools[i];
            atomic_store(&picked->in_use, true);
            pool->in_use++;
            break;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return picked;
}

void wash_tool(Tool* t, KitchenManager* km, SimClock* sc) {
    pthread_mutex_lock(&km->sink);
    pthread_mutex_lock(&sc->lock);

    int ticks = t->clean_time;
    while(ticks > 0) {
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
        ticks--;
    }
    t->dirty_usages = 0;

    pthread_mutex_unlock(&sc->lock);
    pthread_mutex_unlock(&km->sink);
}

void release_pool(ToolPool* pool, Tool* t, SimClock* sc, KitchenManager* km) {
    pthread_mutex_lock(&pool->lock);
    atomic_store(&t->in_use, false);
    pool->in_use--;
    pthread_cond_signal(&pool->cv);
    pthread_mutex_unlock(&pool->lock);
}

Tool** acquire_tools(Dish* d, KitchenManager* km) {
    int n = count_tools(d);
    Tool** used = calloc(n + 1, sizeof(Tool*));
    if (!used) return NULL;
    for (int i = 0; d->tools[i] != NULL; i++) {
        ToolPool* pool = find_pool(d->tools[i], km);
        if (!pool) continue;
        used[i] = acquire_pool(pool);
    }
    used[n] = NULL;
    return used;
}

void release_tools(Tool** used, Dish* d, KitchenManager* km, SimClock* sc) {
    if (!used) return;
    //printf("\tReleasing tools\n");
    for (int i = 0; d->tools[i] != NULL; i++) {
        if (!used[i]) continue;
        ToolPool* pool = find_pool(d->tools[i], km);
        if (pool) release_pool(pool, used[i], sc, km);
    }
    free(used);
}

/* --------------------------------------------------------------------------
 * get_next_order — walk priority list from head, return first Order
 *                  that has at least one Dish available to claim.
 *                  Skips fully claimed and expired orders.
 *                  Returns NULL if nothing is available.
 * -------------------------------------------------------------------------- */
Order* get_next_order(OrderManager* om) {
    pthread_mutex_lock(&om->priority->lock);
    OrderListNode* prev = NULL;
    OrderListNode* node = om->priority->head;

    while(node) {
        Order* o = node->o;

        if(atomic_load(&o->expired)) {
            // Defensively remove expired Order from priority list
            OrderListNode* to_free = node;
            node = node->next;
            if (prev) prev->next = node;
            else      om->priority->head = node;
            om->priority->size--;
            free(to_free);

            // Move to discarded — unlock first to avoid lock ordering issues
            pthread_mutex_unlock(&om->priority->lock);
            printf("order discarded\n");
            list_insert_order(om->discarded_orders, o, 2);
            pthread_mutex_lock(&om->priority->lock);
            continue;
        }

        bool has_available = false;
        for(int i = 0; o->dishes[i] != NULL; i++) {
            if (!atomic_load(&o->dishes[i]->cooking) && !atomic_load(&o->dishes[i]->ready)) {
                has_available = true;
                break;
            }
        }
        if(has_available) {
            pthread_mutex_unlock(&om->priority->lock);
            return o;
        }

        prev = node;
        node = node->next;
    }

    /* All orders fully claimed or priority list empty */
    pthread_mutex_unlock(&om->priority->lock);
    return NULL;
}

void add_usage(Tool** used){
  for(int i = 0; used[i] != NULL; i++) atomic_fetch_add(&used[i]->dirty_usages, 1);
}

/* --------------------------------------------------------------------------
 * pick_dish — claim the unclaimed Dish with fewest required tools
 *             using CAS on d->cooking to avoid races between cooks.
 * -------------------------------------------------------------------------- */
Dish* pick_dish(Order* o) {
    Dish* best      = NULL;
    int   min_tools = INT_MAX;

    for(int i = 0; o->dishes[i] != NULL; i++) {
        Dish* d = o->dishes[i];
        //printf("\tselecting Dish...\n");
        if(atomic_load(&d->cooking) || atomic_load(&d->ready)){
            //printf("\tDish %s is being cooked or ready\n", d->name);
            continue;
        }

        int n = count_tools(d);
        if(n < min_tools) {
            //printf("\tDish %s has been selected\n", d->name);
            min_tools = n;
            best = d;
        }
    }

    if(!best) return NULL;

    // CAS claim — if another cook beat us return NULL
    bool expected = false;
    if(!atomic_compare_exchange_strong(&best->cooking, &expected, true)) return NULL;
    //printf("\tDish %s has been deemed best\n", best->name);
    return best;
}

/* --------------------------------------------------------------------------
 * cook_dish — acquire tools, wait clock ticks, mark ready,
 *             check if Order is complete and signal customer if so.
 * -------------------------------------------------------------------------- */

void print_ck(Cook* ck) {
    pthread_mutex_lock(ck->arg->print);
    printf(PURPLE " COOK %d" RESET ":\t", ck->arg->id);
    switch(ck->present) {
        case WAITING:
            printf(GRAY "waiting for an incoming order" RESET);
            break;

        case SELECT_DISH:
            printf( "selecting a dish from the order" RESET);
            break;

        case ACQUIRE_TOOL:
            printf(ORANGE "trying to acquire the tools to cook" RESET);
            break;

        case COOKING:
            printf(RED "i was cooking: %s" RESET, ck->target_dish->name);
            break;

        case DISH_COMPLETED:
            printf(GREEN "dish completed" RESET);
            break;

        case CLEANING:
            printf(YELLOW "cleaning the tools" RESET);
            break;
    }
    printf("\n");
    pthread_mutex_unlock(ck->arg->print);
}

void cook_waiting(Cook* ck) {
    pthread_mutex_lock(&ck->arg->om->priority->lock);
    if(ck->arg->om->priority->size > 0) {
        ck->future = SELECT_DISH;
    }
    else {
        refill_priority(ck->arg->om);
        ck->future = ck->present;
    }
    pthread_mutex_unlock(&ck->arg->om->priority->lock);
}


void cook_select_dish(Cook* ck) {
    ck->current_order = get_next_order(ck->arg->om);
    if(ck->current_order) {
        ck->target_dish = pick_dish(ck->current_order);
        if(ck->target_dish) {
            ck->future = ACQUIRE_TOOL;
        }
        else {
            ck->future = WAITING;   // or SELECT_DISH to retry immediately
        }
    }
    else {
        ck->future = WAITING;
    }
}

void cook_acquire_tool(Cook* ck) {
    ck->claimed_tools = acquire_tools(ck->target_dish, ck->arg->km);

    if(ck->claimed_tools) {
        ck->future = COOKING;
    }
    else {
        printf(RED_BOLD "Unable to acquire all tools\n");
        release_tools(ck->claimed_tools, ck->target_dish, ck->arg->km, ck->arg->sc);
        ck->future = WAITING;
    }
}

float get_penalty(Cook* ck){
    if(!ck || !ck->claimed_tools) return 0.0f;
    
    float res = 0.0f; 
    
    for(int i = 0; ck->claimed_tools[i] != NULL; i++){
        int du = atomic_load(&ck->claimed_tools[i]->dirty_usages);
        
        float penalty = (float)(1 << du); 
        penalty *= log2f(1.0f + ck->claimed_tools[i]->clean_time);
        
        res += penalty;
    }
    return res;
}

void cook_cooking(Cook* ck) {
    //printf("Cook %d selected dish: %s, time to wait: %d\n",ck->arg->id, ck->target_dish->name, ck->target_dish->time);
    // wait for cooking time
    pthread_mutex_lock(&ck->arg->sc->lock);
    for(int i = 0; i < ck->target_dish->time; i++){
        //printf("Time waited: %d\n", i);
        pthread_cond_wait(&ck->arg->sc->tick_cv, &ck->arg->sc->lock);
    }
    pthread_mutex_unlock(&ck->arg->sc->lock);

    atomic_store(&ck->target_dish->ready, true);
    //printf("Dish %s is completed", ck->target_dish->name);
    atomic_store(&ck->target_dish->cooking, false);
    float penalty = get_penalty(ck);
    if(penalty > 0) printf(RED "APPLYING PENALTY BECAUSE DISHES WERE DIRTY, PENALTY = %f" RESET , penalty);
    atomic_fetch_sub(&ck->arg->score, &penalty);
    // Decrement Order remaining time
    atomic_fetch_sub(&ck->current_order->remaining_time, ck->target_dish->time);
    add_usage(ck->claimed_tools);
    release_tools(ck->claimed_tools, ck->target_dish, ck->arg->km, ck->arg->sc);
    
    if(!ck->current_order->expired) {
        list_insert_dish(ck->arg->om->completed_dishes, ck->target_dish);
        ck->future = DISH_COMPLETED;
    }
    else {
        ck->future = WAITING;
    }
}

void cook_completed(Cook* ck) {
    if(atomic_load(&ck->current_order->remaining_time) == 0) {
                    
    bool expected = false;

        // if order is completed remove it from priority and go into order completed
        if(atomic_compare_exchange_strong(&ck->current_order->completed, &expected, true)) {

            // if order is not expired remove it from the priority list
            if(!atomic_load(&ck->current_order->expired)) {
                remove_order(ck->arg->om->priority, ck->current_order);
                ck->future = CLEANING;
                refill_priority(ck->arg->om);
                
            }
            else {
                ck->future = SELECT_DISH;
            }
        }
        else {
            ck->future = SELECT_DISH;
        }
    }
    else {
        ck->future = SELECT_DISH;
    }
}

void clean_tool(Tool* t, SimClock* sc) {
    pthread_mutex_lock(&sc->lock);
    for (int i = 0; i < t->clean_time; i++) {
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
    }
    pthread_mutex_unlock(&sc->lock);
    t->dirty_usages = 0;
}

void cook_cleaning(Cook* ck) {
    KitchenManager* km = ck->arg->km;
    SimClock* sc = ck->arg->sc;

    pthread_mutex_lock(&km->sink);
    for (int p = 0; p < km->num_pools; p++) {
        ToolPool* pool = km->pools[p];
        pthread_mutex_lock(&pool->lock);
        for (int i = 0; i < pool->quantity; i++) {
            Tool* tool = &pool->tools[i];
            pthread_mutex_lock(&tool->lock);
            if (tool->dirty_usages >= DIRTY_THRESHOLD) {
                printf("Cleaning tool %d from pool %s\n", i, pool->name);
                clean_tool(tool, sc);
            }
            pthread_mutex_unlock(&tool->lock);
        }
        pthread_mutex_unlock(&pool->lock);
    }
    pthread_mutex_unlock(&km->sink);
    ck->future = WAITING;
}


/* --------------------------------------------------------------------------
 * cook_loop — continuously looks for orders to cook.
 *             Spins on get_next_order when nothing is available.
 *             Yields on pick_dish failure to avoid hammering a fully
 *             claimed Order.
 * -------------------------------------------------------------------------- */
void cook_loop(Cook* ck) {
    while(atomic_load(ck->arg->running)) {
        pthread_mutex_lock(&ck->arg->sc->lock);
        pthread_cond_wait(&ck->arg->sc->tick_cv, &ck->arg->sc->lock);
        pthread_mutex_unlock(&ck->arg->sc->lock);

        switch(ck->present) {

            // check into the priority list for an order
            case WAITING:
                cook_waiting(ck);
                break;

            // pick a dish from the selected order and then try to acquire tools
            case SELECT_DISH:
                cook_select_dish(ck);
                break;

            // acquire tools in order to cook the dish
            case ACQUIRE_TOOL:
                cook_acquire_tool(ck);
                break;

            // starts cooking the dish, also handles queues
            case COOKING:
                cook_cooking(ck);
                break;

            case DISH_COMPLETED:
                cook_completed(ck);
                break;

            case CLEANING:
                cook_cleaning(ck);
                break;

            default:
                perror("Cook - Unknown State");
            
        }
        print_ck(ck);
        ck->present = ck->future;
    }
}

void* cook_thread(void* args) {
    if(!args) return NULL;
    Cook* ck = malloc(sizeof(Cook));
    ck->arg = (CookArgs*) args;

    cook_loop(ck);

    return NULL;
}
