#include "customer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Order creation
 * -------------------------------------------------------------------------- */

order* make_order(customer* c, menu* Menu, int num_dishes) {
    order* o = malloc(sizeof(order));
    if (!o) return NULL;

    o->dishes = calloc(num_dishes + 1, sizeof(dish*));
    if (!o->dishes) {
        free(o);
        return NULL;
    }

    for (int i = 0; i < num_dishes; i++) {
        o->dishes[i] = copy_dish(Menu->selection[safe_rand_range(Menu->num_dishes)]);
    }
    o->dishes[num_dishes] = NULL;
    atomic_store(&o->completed, false);
    atomic_store(&o->remaining_time, get_prep_time(o));
    atomic_store(&o->expired, false);
    o->c = c;
    pthread_mutex_init(&o->lock, NULL);
    return o;
}

int get_prep_time(order* o) {
    int tot = 0;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (atomic_load(&o->dishes[i]->ready)) continue;
        tot += o->dishes[i]->time;
    }
    return tot;
}

dish* copy_dish(dish* src) {
    if (!src) return NULL;

    dish* d = malloc(sizeof(dish));
    if (!d) return NULL;

    *d = *src;
    atomic_store(&d->cooking, false);
    atomic_store(&d->ready, false);
    pthread_mutex_init(&d->lock, NULL);
    return d;
}

void free_order(order* o) {
    if (!o) return;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        pthread_mutex_destroy(&o->dishes[i]->lock);
        free(o->dishes[i]);
    }
    free(o->dishes);
    pthread_mutex_destroy(&o->lock);
    free(o);
}

/* --------------------------------------------------------------------------
 * Queue helpers
 * -------------------------------------------------------------------------- */

bool is_empty(customer_queue* q) {
    return q->size == 0;
}

/* --------------------------------------------------------------------------
 * enqueue — allocate a new node and append to tail
 * -------------------------------------------------------------------------- */

void enqueue(customer* c, customer_queue* q) {
    queue_node* node = malloc(sizeof(queue_node));
    if (!node) {
        fprintf(stderr, "enqueue: malloc failed\n");
        return;
    }
    node->c    = c;
    node->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->tail) {
        q->tail->next = node;
    } else {
        q->head = node;
    }
    q->tail = node;
    q->size++;

    pthread_mutex_unlock(&q->lock);
}

/* --------------------------------------------------------------------------
 * dequeue — remove head node without returning the customer
 * -------------------------------------------------------------------------- */

void dequeue(customer_queue* q) {
    pthread_mutex_lock(&q->lock);

    if (is_empty(q)) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    queue_node* old_head = q->head;
    q->head = old_head->next;
    if (!q->head) q->tail = NULL;
    q->size--;

    pthread_mutex_unlock(&q->lock);
    free(old_head);
}

/* --------------------------------------------------------------------------
 * peek — return pointer to head customer without removing
 * -------------------------------------------------------------------------- */

customer* peek(customer_queue* q) {
    pthread_mutex_lock(&q->lock);
    customer* c = is_empty(q) ? NULL : q->head->c;
    pthread_mutex_unlock(&q->lock);
    return c;
}

/* --------------------------------------------------------------------------
 * pop — return the first customer whose patience >= order remaining_time.
 *       Impatient or expired customers are signalled and removed immediately.
 *       Uses prev pointer to correctly unlink any node in the list.
 * -------------------------------------------------------------------------- */

customer* pop(customer_queue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    queue_node* old_head = q->head;
    customer*   result   = old_head->c;

    q->head = old_head->next;
    if (!q->head) q->tail = NULL;
    q->size--;

    free(old_head);
    pthread_mutex_unlock(&q->lock);
    return result;
}
/* --------------------------------------------------------------------------
 * clean — free all nodes and reset queue to empty state
 *         Does NOT free the customers themselves.
 * -------------------------------------------------------------------------- */

void clean(customer_queue* q) {
    pthread_mutex_lock(&q->lock);

    queue_node* node = q->head;
    while (node) {
        queue_node* next = node->next;
        free(node);
        node = next;
    }

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    pthread_mutex_unlock(&q->lock);
}

/* --------------------------------------------------------------------------
 * customer_loop — lifecycle of a single customer thread
 *
 * 1. Spin until arrival_time
 * 2. Enqueue self
 * 3. Wait to be served, discarded, or patience expires
 * 4. Update score atomically
 * -------------------------------------------------------------------------- */

void customer_loop(customer* c, customer_queue* q, sim_clock* sc, _Atomic float* score) {

    /* Wait until arrival time */
    pthread_mutex_lock(&sc->lock);
    while (sc->tick < c->arrival_time)
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
    pthread_mutex_unlock(&sc->lock);

    enqueue(c, q);
    printf("enqueued\n");
    int wait_start = sc->tick;

    while (!atomic_load(&c->served)) {
        printf("hello from customer\n");
        pthread_mutex_lock(&sc->lock);
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
        int current_tick = sc->tick;
        pthread_mutex_unlock(&sc->lock);

        if (atomic_load(&c->discarded)) {
            free_order(c->o);
            c->o = NULL;
            float current = *score;
            do {
            // Keep trying until we successfully update the value safely
            } while (!atomic_compare_exchange_weak(score, &current, current + 1.0f));
            return;
        }

        if ((current_tick - wait_start) >= c->patience) {
            if (c->o) atomic_store(&c->o->expired, true);
            float current = *score;
            do {
            // Keep trying until we successfully update the value safely
            }while (!atomic_compare_exchange_weak(score, &current, current - 1.0f));
            return;
        }
    }

  float current = *score;
  do {
  printf("stuck");
  // Keep trying until we successfully update the value safely
  } while (!atomic_compare_exchange_weak(score, &current, current + 1.0f));
  return;
}

<<<<<<< HEAD
void* customer_thread(void* arg){
  if(!arg) return NULL;
  customer_args* arguments = (customer_args*) arg;
  //creates customer and order, thus goes into customer loop
  customer* C = (customer*) malloc(sizeof(customer));
  C->o = make_order(C, arguments->Menu, safe_rand_range(4)+1);
}
