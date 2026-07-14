#include "customer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Order creation
 * -------------------------------------------------------------------------- */

Order* make_order(Customer* c, Menu* menu, int num_dishes) {
    Order* o = malloc(sizeof(Order));
    if (!o) return NULL;

    o->dishes = calloc(num_dishes + 1, sizeof(Dish*));
    if (!o->dishes) {
        free(o);
        return NULL;
    }

    for (int i = 0; i < num_dishes; i++) {
        o->dishes[i] = copy_dish(menu->selection[safe_rand_range(menu->num_dishes)]);
    }
    o->dishes[num_dishes] = NULL;
    atomic_store(&o->completed, false);
    atomic_store(&o->remaining_time, get_prep_time(o));
    atomic_store(&o->expired, false);
    o->c = c;
    pthread_mutex_init(&o->lock, NULL);
    return o;
}

int get_prep_time(Order* o) {
    int tot = 0;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (atomic_load(&o->dishes[i]->ready)) continue;
        tot += o->dishes[i]->time;
    }
    return tot;
}

Dish* copy_dish(Dish* src) {
    if (!src) return NULL;

    Dish* d = malloc(sizeof(Dish));
    if (!d) return NULL;

    *d = *src;
    atomic_store(&d->cooking, false);
    atomic_store(&d->ready, false);
    pthread_mutex_init(&d->lock, NULL);
    return d;
}

void free_order(Order* o) {
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

bool is_empty(CustomerQueue* q) {
    return q->size == 0;
}

/* --------------------------------------------------------------------------
 * enqueue — allocate a new node and append to tail
 * -------------------------------------------------------------------------- */

void enqueue(Customer* c, CustomerQueue* q) {
    QueueNode* node = malloc(sizeof(QueueNode));
    if (!node) {
        perror("enqueue: malloc failed\n");
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
 * dequeue — remove head node, returning the customer
 * -------------------------------------------------------------------------- */

Customer* dequeue(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (is_empty(q)) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    QueueNode* old_head = q->head;
    q->head = old_head->next;
    if (!q->head) q->tail = NULL;
    q->size--;

    pthread_mutex_unlock(&q->lock);
    return old_head;
}

/* --------------------------------------------------------------------------
 * peek — return pointer to head customer without removing
 * -------------------------------------------------------------------------- */

Customer* peek(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);
    Customer* c = is_empty(q) ? NULL : q->head->c;
    pthread_mutex_unlock(&q->lock);
    return c;
}

/* --------------------------------------------------------------------------
 * pop — return the first customer whose patience >= Order remaining_time.
 *       Impatient or expired customers are signalled and removed immediately.
 *       Uses prev pointer to correctly unlink any node in the list.
 * -------------------------------------------------------------------------- */

Customer* pop(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    QueueNode* old_head = q->head;
    Customer* result = old_head->c;

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

void clean(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    QueueNode* node = q->head;
    while (node) {
        QueueNode* next = node->next;
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
 * 4. Update score automically
 * -------------------------------------------------------------------------- */

void customer_loop(Customer* cst) {
    
    while(cst->arg->running) {
        pthread_mutex_lock(&cst->arg->sc->lock);
        pthread_cond_wait(&cst->arg->sc->tick_cv, &cst->arg->sc->lock);
        pthread_mutex_unlock(&cst->arg->sc->lock);

        // waiting outside for a free seat
        switch(cst->present) {
            case STANDING:
                break;

            case SEATED:
                sem_wait(&cst->arg->rc);
                cst->future = ORDERING;
                break;

            case ORDERING:
                cst->o = make_order(cst, cst->arg->menu, safe_rand_range(5));
                break;

            case WAITING_ORDER:
                if(cst->served) {
                    cst->future = EATING;
                }
                else {
                    cst->future = cst->present;
                }
                break;

            case EATING:
                // eating time = 1 s (just to test the code)
                usleep(100000);
                break;

            case FINISHED:
                sem_post(&cst->arg->rc);
                break;

            default:
                perror("Customer - Unknown State");
            
        }
        cst->present = cst->future;
        cst->patience--;
    }

    // if space is available inside the restaurant, the customer can sit down and Order
    //atomic_store(&c->can_order, true);
/*
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
*/
    
}

void* customer_thread(void* args) {
  if(!args) return NULL;
  //creates customer then goes into customer loop
  Customer* cst = malloc(sizeof(Customer));
  cst->present = STANDING;
  cst->arg = (CustomerArgs*) args;

  customer_loop(cst);

  free(cst);
  cst = NULL;

  return NULL;
}