#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

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

void release_pool(ToolPool* pool, Tool* t, SimClock* sc, KitchenManager* km) {
    //printf("\tRealeasing pool %s\n", pool->name);
    t->dirty_usages++;
    bool clean_condition = t->dirty_usages >= DIRTY_THRESHOLD;
    if (clean_condition) {
        /* Wash at the shared sink — blocks other cooks from washing */
        pthread_mutex_lock(&km->sink);
        pthread_mutex_lock(&sc->lock);
        int ticks = t->clean_time;
        while (ticks > 0) {
            pthread_cond_wait(&sc->tick_cv, &sc->lock);
            ticks--;
        }
        t->dirty_usages = 0;
        pthread_mutex_unlock(&sc->lock);
        pthread_mutex_unlock(&km->sink);
    }

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
/*
void cook_dish(Dish* d, Order* o, OrderManager* om, SimClock* sc, KitchenManager* km, bool* running) {
    if(!running) return;
    Tool** used = acquire_tools(d, km);
    if (!used) {
        atomic_store(&d->cooking, false);
        return;
    }

    // Wait for cooking time
    pthread_mutex_lock(&sc->lock);
    int ticks = d->time;
    while (ticks > 0) {
        if(!running) break;
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
        ticks--;
    }
    pthread_mutex_unlock(&sc->lock);

    if(!running) {
        release_tools(used, d, km, sc);
        return;
    }

    atomic_store(&d->ready, true);
    printf("Dish %s is completed", d->name);
    atomic_store(&d->cooking, false);

    // Decrement Order remaining time
    atomic_fetch_sub(&o->remaining_time, d->time);

    // Check if all dishes are ready
    bool all_ready = true;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (!atomic_load(&o->dishes[i]->ready)) {
            all_ready = false;
            break;
        }
    }

    if (all_ready) {
        printf("Order completed\n");
        // CAS on completed — only one cook proceeds even if two finish simultaneously
        bool expected = false;
        if (atomic_compare_exchange_strong(&o->completed, &expected, true)) {

            // Check expiry — customer may have timed out while we were cooking
            if (atomic_load(&o->expired)) {
                list_insert_order(om->discarded_orders, o, 2);
            } else {
                // Remove from priority list
                pthread_mutex_lock(&om->priority->lock);
                OrderListNode* prev = NULL;
                OrderListNode* node = om->priority->head;
                while (node) {
                    if (node->o == o) {
                        if (prev) prev->next = node->next;
                        else      om->priority->head = node->next;
                        om->priority->size--;
                        free(node);
                        break;
                    }
                    prev = node;
                    node = node->next;
                }
                pthread_mutex_unlock(&om->priority->lock);

                // Move to completed and signal customer
                list_insert_order(om->completed_orders, o, 2);
                atomic_store(&o->c->served, true);
            }
        }
    }
    release_tools(used, d, km, sc);
}
*/

void print_ck(Cook* ck) {
    pthread_mutex_lock(ck->arg->print);
    printf(PURPLE " COOK %d" RESET ":\t", ck->arg->id);
    switch(ck->present) {
        case WAITING:
            printf(GRAY "waiting for an incoming order" RESET);
            break;

        case SELECT_DISH:
            printf(YELLOW "selecting a dish from the order" RESET);
            break;

        case ACQUIRE_TOOL:
            printf(ORANGE "trying to acquire the tools to cook" RESET);
            break;

        case COOKING:
            printf(RED "cooking the dish" RESET);
            break;

        case DISH_COMPLETED:
            printf(GREEN "dish completed" RESET);
            break;

        case ORDER_COMPLETED:
            printf(GREEN_BOLD "ORDER COMPLETED" RESET);
            break;

        case CLEANING:
            printf(BLUE "cleaning the tools" RESET);
            break;
    }
    printf("\n");
    pthread_mutex_unlock(ck->arg->print);
}

/* --------------------------------------------------------------------------
 * cook_loop — continuously looks for orders to cook.
 *             Spins on get_next_order when nothing is available.
 *             Yields on pick_dish failure to avoid hammering a fully
 *             claimed Order.
 * -------------------------------------------------------------------------- */
void cook_loop(Cook* ck) {
    Order* o;

    while(ck->arg->running) {
        int ticks;

        pthread_mutex_lock(&ck->arg->sc->lock);
        pthread_cond_wait(&ck->arg->sc->tick_cv, &ck->arg->sc->lock);
        pthread_mutex_unlock(&ck->arg->sc->lock);

        switch(ck->present) {

            // check into the priority list for an order
            case WAITING:
                pthread_mutex_lock(&ck->arg->om->priority->lock);
                if(ck->arg->om->priority->size > 0) {
                //if(!is_empty(ck->arg->om->priority, ORDER_LIST)) {
                    ck->future = SELECT_DISH;
                }
                else {
                    refill_priority(ck->arg->om);
                    ck->future = ck->present;
                }
                pthread_mutex_unlock(&ck->arg->om->priority->lock);
                break;

            // pick a dish from the selected order and then try to acquire tools
            case SELECT_DISH:
                o = get_next_order(ck->arg->om);
                if(o) {
                    ck->target_dish = pick_dish(o);
                    ck->future = ACQUIRE_TOOL;
                }
                else {
                    ck->future = WAITING;
                }
                break;

            // acquire tools in order to cook the dish
            case ACQUIRE_TOOL:
                ck->claimed_tools = acquire_tools(ck->target_dish, ck->arg->km);

                if(ck->claimed_tools) {
                    ticks = ck->target_dish->time;
                    ck->future = COOKING;
                }
                else {
                    release_tools(ck->claimed_tools, ck->target_dish, ck->arg->km, ck->arg->sc);
                    ck->future = WAITING;
                }
                break;

            // starts cooking the dish, also handles queues
            case COOKING:
                // Wait for cooking time
                if(ticks > 0) {
                    ticks--;
                    ck->future = ck->present;
                    break;
                }

                atomic_store(&ck->target_dish->ready, true);
                //printf("Dish %s is completed", ck->target_dish->name);
                atomic_store(&ck->target_dish->cooking, false);

                // Decrement Order remaining time
                atomic_fetch_sub(&o->remaining_time, ck->target_dish->time);

                release_tools(ck->claimed_tools, ck->target_dish, ck->arg->km, ck->arg->sc);

                // insert ready dish in dish list so then the waiter can pick it up
                list_insert_dish(ck->arg->om->completed_dishes, ck->target_dish);

                ck->future = DISH_COMPLETED;
                break;

            case DISH_COMPLETED:
                // Check if all dishes are ready
                if(atomic_load(&o->remaining_time) == 0) {
                    
                    bool expected = false;

                    if(atomic_compare_exchange_strong(&o->completed, &expected, true)) {
                        // Check expiry — customer may have timed out while we were cooking
                        
                        if(!atomic_load(&o->expired)) {
                            // Remove from priority list
                            pthread_mutex_lock(&ck->arg->om->priority->lock);
                            OrderListNode* prev = NULL;
                            OrderListNode* node = ck->arg->om->priority->head;
                            while(node) {
                                if (node->o == o) {
                                    if (prev) prev->next = node->next;
                                    else      ck->arg->om->priority->head = node->next;
                                    ck->arg->om->priority->size--;
                                    free(node);
                                    break;
                                }
                                prev = node;
                                node = node->next;
                            }
                            pthread_mutex_unlock(&ck->arg->om->priority->lock);

                            // Move to completed and signal customer
                            ck->future = ORDER_COMPLETED;
                            list_insert_order(ck->arg->om->completed_orders, o, 2);
                        }
                    }
                    refill_priority(ck->arg->om);
                }
                else {
                    ck->future = SELECT_DISH;
                }
                break;

            case ORDER_COMPLETED:
                list_remove_order(ck->arg->om->priority);
                ck->future = WAITING;
                break;

            case CLEANING:
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
