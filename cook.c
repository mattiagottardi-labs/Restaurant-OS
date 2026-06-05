#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include "customer.h"
#include "kitchen.h"
#include "cook.h"
#include "waiter.h"
#include "utils.h"
#include <math.h>

#define DIRTY_THRESHOLD 3
#define GAME_SPEED 1

static pthread_mutex_t sink_mutex = PTHREAD_MUTEX_INITIALIZER;

// ─── forward declarations ────────────────────────────────────────────────────

order_queue* pick_queue(queue_manager* qm);
order*       pick_order(order_queue* oq);
dish*        pick_dish(order* o);
void         cook(queue_manager* qm, sim_clock* sim, kitchen_manager* km, order_queue* finished);

// ─── tool helpers ────────────────────────────────────────────────────────────

int count_tools(dish* d) {
    int i = 0;
    while (d->tools[i] != NULL) i++;
    return i;
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
        if (!pool->tools[i].in_use) {
            picked = &pool->tools[i];
            picked->in_use = true;
            pool->in_use++;
            break;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return picked;
}

void release_pool(tool_pool* pool, tool* t, sim_clock* clock, pthread_mutex_t* sink) {
    t->dirty_usages++;

    if (t->dirty_usages >= DIRTY_THRESHOLD) {
        pthread_mutex_lock(sink);
        pthread_mutex_lock(&clock->lock);
        int ticks = t->clean_time;
        while (ticks > 0) {
            pthread_cond_wait(&clock->tick_cv, &clock->lock);
            ticks--;
        }
        t->dirty_usages = 0;
        pthread_mutex_unlock(&clock->lock);
        pthread_mutex_unlock(sink);
    }

    // release tool back to pool after washing (if needed)
    pthread_mutex_lock(&pool->lock);
    t->in_use = false;
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
    return used;
}

void release_tools(tool** used, dish* d, kitchen_manager* km, sim_clock* clock, pthread_mutex_t* sink) {
    if (!used) return;
    for (int i = 0; d->tools[i] != NULL; i++) {
        if (!used[i]) continue;
        tool_pool* pool = find_pool(d->tools[i], km);
        if (pool) release_pool(pool, used[i], clock, sink);
    }
}

// ─── queue management ────────────────────────────────────────────────────────

void push_finished(order* o, order_queue* finished, order_queue* current) {
    
    // remove from current queue
    pthread_mutex_lock(&current->lock);
    for (int i = 0; i < current->num_orders; i++) {
        if (current->queue[i] == o) {
            for (int j = i; j < current->num_orders - 1; j++)
                current->queue[j] = current->queue[j + 1];
            current->queue[current->num_orders - 1] = NULL;
            current->num_orders--;
            break;
        }
    }
    pthread_mutex_unlock(&current->lock);

    // push to finished queue, growing if needed
    pthread_mutex_lock(&finished->lock);
    if (finished->num_orders == finished->max_capacity) {
        int new_cap = finished->max_capacity * 2;
        order** tmp = realloc(finished->queue, sizeof(order*) * new_cap);
        if (!tmp) {
            fprintf(stderr, "push_finished: out of memory\n");
            pthread_mutex_unlock(&finished->lock);
            return;
        }
        finished->queue = tmp;
        finished->max_capacity = new_cap;
    }
    finished->queue[finished->num_orders] = o;
    finished->num_orders++;
    pthread_mutex_unlock(&finished->lock);
}

order_queue* pick_queue(queue_manager* qm) {
    pthread_mutex_lock(&qm->lock);

    order_queue* current = qm->queue_start;
    order_queue* prev = NULL;

    while (current != NULL) {
        if (current->num_orders == 0) {
            // unlink
            if (prev == NULL)
                qm->queue_start = current->next_queue;
            else
                prev->next_queue = current->next_queue;

            order_queue* empty = current;
            current = current->next_queue;
            qm->num_queue--;

            // free the queue
            pthread_mutex_destroy(&empty->lock);
            free(empty->queue);
            free(empty);
            continue;
        }

        // first non-empty queue
        pthread_mutex_unlock(&qm->lock);
        return current;
    }

    pthread_mutex_unlock(&qm->lock);
    return NULL;
}

order* pick_order(order_queue* oq) {
    pthread_mutex_lock(&oq->lock);

    order* best = NULL;
    int min_time = INT_MAX;

    for (int i = 0; i < oq->num_orders; i++) {
        order* o = oq->queue[i];
        if (o == NULL) continue;

        // calculate remaining cook time for this order
        int remaining = 0;
        bool has_unclaimed = false;
        for (int j = 0; o->dishes[j] != NULL; j++) {
            dish* d = o->dishes[j];
            if (!d->cooking && !d->ready) {
                remaining += d->time;
                has_unclaimed = true;
            }
        }

        if (has_unclaimed && remaining < min_time) {
            min_time = remaining;
            best = o;
        }
    }

    pthread_mutex_unlock(&oq->lock);
    return best;
}

dish* pick_dish(order* o) {
    // count total dishes and how many are ready
    int total = 0, ready = 0;
    for (int j = 0; o->dishes[j] != NULL; j++) {
        total++;
        if (o->dishes[j]->ready) ready++;
    }

    // try to claim the first unclaimed dish
    for (int j = 0; o->dishes[j] != NULL; j++) {
        dish* d = o->dishes[j];
        pthread_mutex_lock(&d->lock);
        if (!d->cooking && !d->ready) {
            d->cooking = true;
            // last dish = this one plus all already ready = total
            d->last = (ready + 1 == total);
            pthread_mutex_unlock(&d->lock);
            return d;
        }
        pthread_mutex_unlock(&d->lock);
    }
    return NULL;
}

// ─── cook_dish ───────────────────────────────────────────────────────────────

void cook_dish(dish* d, sim_clock* clock, kitchen_manager* km, pthread_mutex_t* sink, order_queue* finished, order_queue* oq) {
    tool** used = acquire_tools(d, km);
    if (!used) {
        pthread_mutex_lock(&d->lock);
        d->cooking = false;
        pthread_mutex_unlock(&d->lock);
        return;
    }

    // simulate cooking time
    pthread_mutex_lock(&clock->lock);
    int ticks = d->time;
    while (ticks > 0) {
        pthread_cond_wait(&clock->tick_cv, &clock->lock);
        ticks--;
    }
    pthread_mutex_unlock(&clock->lock);

    pthread_mutex_lock(&d->lock);
    d->ready = true;
    d->cooking = false;
    bool last = d->last;
    pthread_mutex_unlock(&d->lock);

    if (last) push_finished(d->o, finished, oq);
  release_tools(used, d, km, clock, sink);
  free(used);
}

// ─── cook entry point ────────────────────────────────────────────────────────

void cook(queue_manager* qm, sim_clock* sim, kitchen_manager* km, order_queue* finished) {
    order_queue* oq = pick_queue(qm);
    if (!oq) return;

    order* o = pick_order(oq);
    if (!o) return;

    dish* d = pick_dish(o);
    if (!d) return;

    cook_dish(d, sim, km, &sink_mutex, finished, oq);
}
