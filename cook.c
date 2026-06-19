#include "cook.h"
#include "customer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* --------------------------------------------------------------------------
 * Tool helpers
 * -------------------------------------------------------------------------- */

int count_tools(dish* d) {
    int i = 0;
    while (d->tools[i] != NULL) i++;
    return i;
}

float get_pressure(order_list* l){
  int tot_prio;
  int tot_orders;
  pthread_mutex_lock(&l->lock);
  list_node* current = l->head;
  for(int i = 0; i < l->size; i++){
    if(atomic_load(&current->o->completed) || atomic_load(&current->o->expired)){
      current = current->next;
      continue;
    }
    tot_prio += get_prio(current->o, 0);
    tot_orders++;
  }
  return (float) tot_prio/tot_orders;
}

tool_pool* find_pool(const char* tool_name, kitchen_manager* km) {
    for (int i = 0; i < km->num_pools; i++) {
        if (strcmp(km->pools[i]->name, tool_name) == 0)
            return km->pools[i];
    }
    fprintf(stderr, "find_pool: tool '%s' not found\n", tool_name);
    return NULL;
}

tool* acquire_pool(tool_pool* pool) {
    pthread_mutex_lock(&pool->lock);
    while (pool->in_use >= pool->quantity)
        pthread_cond_wait(&pool->cv, &pool->lock);

    tool* picked = NULL;
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

void release_pool(tool_pool* pool, tool* t, sim_clock* sc, kitchen_manager* km) {
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

tool** acquire_tools(dish* d, kitchen_manager* km) {
    int n = count_tools(d);
    tool** used = calloc(n + 1, sizeof(tool*));
    if (!used) return NULL;
    for (int i = 0; d->tools[i] != NULL; i++) {
        tool_pool* pool = find_pool(d->tools[i], km);
        if (!pool) continue;
        used[i] = acquire_pool(pool);
    }
    used[n] = NULL;
    return used;
}

void release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* sc) {
    if (!used) return;
    for (int i = 0; d->tools[i] != NULL; i++) {
        if (!used[i]) continue;
        tool_pool* pool = find_pool(d->tools[i], km);
        if (pool) release_pool(pool, used[i], sc, km);
    }
    free(used);
}

/* --------------------------------------------------------------------------
 * get_next_order — walk priority list from head, return first order
 *                  that has at least one dish available to claim.
 *                  Skips fully claimed and expired orders.
 *                  Returns NULL if nothing is available.
 * -------------------------------------------------------------------------- */

order* get_next_order(order_manager* m) {
    pthread_mutex_lock(&m->priority->lock);

    list_node* prev = NULL;
    list_node* node = m->priority->head;

    while (node) {
        order* o = node->o;

        if (atomic_load(&o->expired)) {
            /* Defensively remove expired order from priority list */
            list_node* to_free = node;
            node = node->next;
            if (prev) prev->next = node;
            else      m->priority->head = node;
            m->priority->size--;
            free(to_free);

            /* Move to discarded — unlock first to avoid lock ordering issues */
            pthread_mutex_unlock(&m->priority->lock);
            list_insert_order(m->discarded_orders, o, 2);
            pthread_mutex_lock(&m->priority->lock);
            continue;
        }

        bool has_available = false;
        for (int i = 0; o->dishes[i] != NULL; i++) {
            if (!atomic_load(&o->dishes[i]->cooking) &&
                !atomic_load(&o->dishes[i]->ready)) {
                has_available = true;
                break;
            }
        }
        if (has_available) {
            pthread_mutex_unlock(&m->priority->lock);
            return o;
        }

        prev = node;
        node = node->next;
    }

    /* All orders fully claimed or priority list empty */
    pthread_mutex_unlock(&m->priority->lock);
    return NULL;
}

/* --------------------------------------------------------------------------
 * pick_dish — claim the unclaimed dish with fewest required tools
 *             using CAS on d->cooking to avoid races between cooks.
 * -------------------------------------------------------------------------- */

dish* pick_dish(order* o) {
    dish* best      = NULL;
    int   min_tools = INT_MAX;

    for (int i = 0; o->dishes[i] != NULL; i++) {
        dish* d = o->dishes[i];

        if (atomic_load(&d->cooking) || atomic_load(&d->ready))
            continue;

        int n = count_tools(d);
        if (n < min_tools) {
            min_tools = n;
            best = d;
        }
    }

    if (!best) return NULL;

    /* CAS claim — if another cook beat us return NULL */
    bool expected = false;
    if (!atomic_compare_exchange_strong(&best->cooking, &expected, true))
        return NULL;

    return best;
}

/* --------------------------------------------------------------------------
 * cook_dish — acquire tools, wait clock ticks, mark ready,
 *             check if order is complete and signal customer if so.
 * -------------------------------------------------------------------------- */

void cook_dish(dish* d, order* o, order_manager* m, sim_clock* sc, kitchen_manager* km, bool* running) {
    if(!running) return;
    tool** used = acquire_tools(d, km);
    if (!used) {
        atomic_store(&d->cooking, false);
        return;
    }

    /* Wait for cooking time */
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
    atomic_store(&d->cooking, false);

    /* Decrement order remaining time */
    atomic_fetch_sub(&o->remaining_time, d->time);

    /* Check if all dishes are ready */
    bool all_ready = true;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (!atomic_load(&o->dishes[i]->ready)) {
            all_ready = false;
            break;
        }
    }

    if (all_ready) {
        /* CAS on completed — only one cook proceeds even if two finish simultaneously */
        bool expected = false;
        if (atomic_compare_exchange_strong(&o->completed, &expected, true)) {

            /* Check expiry — customer may have timed out while we were cooking */
            if (atomic_load(&o->expired)) {
                list_insert_order(m->discarded_orders, o, 2);
            } else {
                /* Remove from priority list */
                pthread_mutex_lock(&m->priority->lock);
                list_node* prev = NULL;
                list_node* node = m->priority->head;
                while (node) {
                    if (node->o == o) {
                        if (prev) prev->next = node->next;
                        else      m->priority->head = node->next;
                        m->priority->size--;
                        free(node);
                        break;
                    }
                    prev = node;
                    node = node->next;
                }
                pthread_mutex_unlock(&m->priority->lock);

                /* Move to completed and signal customer */
                list_insert_order(m->completed_orders, o, 2);
                atomic_store(&o->c->served, true);
            }
        }
    }

    release_tools(used, d, km, sc);
}

/* --------------------------------------------------------------------------
 * cook_loop — continuously looks for orders to cook.
 *             Spins on get_next_order when nothing is available.
 *             Yields on pick_dish failure to avoid hammering a fully
 *             claimed order.
 * -------------------------------------------------------------------------- */

void cook_loop(order_manager* m, sim_clock* sc, kitchen_manager* km, bool* running) {
    while (atomic_load(&m->running)) {
        order* o = get_next_order(m);
        if (!o) continue;

        dish* d = pick_dish(o);
        if (!d) {
            sched_yield();
            continue;
        }

        cook_dish(d, o, m, sc, km, running);
    }
}

void* cook_thread(void* arg) {
    if(!arg) return NULL;
    cook_args* arguments = (cook_args*) arg;

    cook_loop(arguments->m, arguments->sc, arguments->km);
}