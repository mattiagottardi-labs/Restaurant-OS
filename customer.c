#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "customer.h"

const int DEFAULT_PATIENCE = 60;

/* --------------------------------------------------------------------------
 * Order creation
 * -------------------------------------------------------------------------- */
Order* make_order(Menu* menu, int num_dishes) {
    Order* o = malloc(sizeof(Order));
    if(!o) {
        perror("order creation - malloc");
        return NULL;
    }

    o->dishes = calloc(num_dishes + 1, sizeof(Dish*));
    if(!o->dishes) {
        perror("dish creation - calloc");
        free(o);
        return NULL;
    }

    for(int i = 0; i < num_dishes; i++) {
        o->dishes[i] = copy_dish(menu->selection[safe_rand_range(menu->num_dishes)]);
    }
    o->dishes[num_dishes] = NULL;
    atomic_store(&o->completed, false);
    atomic_store(&o->remaining_time, get_prep_time(o));
    atomic_store(&o->total_price, get_price(o));
    atomic_store(&o->expired, false);
    pthread_mutex_init(&o->lock, NULL);
    return o;
}

float atomic_float_add(_Atomic float *target, float amount) {
    float old_val = atomic_load(target);
    float new_val;
    do {
        new_val = old_val + amount;
    } while (!atomic_compare_exchange_weak(target, &old_val, new_val));
    return new_val;
}

int get_prep_time(Order* o) {
    int tot = 0;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (atomic_load(&o->dishes[i]->ready)) continue;
        tot += o->dishes[i]->time;
    }
    return tot;
}

int get_price(Order* o) {
    int tot = 0;
    for (int i = 0; o->dishes[i] != NULL; i++) {
        if (atomic_load(&o->dishes[i]->ready)) continue;
        tot += o->dishes[i]->price;
    }
    return tot;
}

Dish* copy_dish(Dish* src) {
    if(!src) return NULL;

    Dish* d = malloc(sizeof(Dish));
    if(!d) return NULL;

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
 * is_empty - void* and a casting type defined in utils.h enum, true if q is empty (size = 0)
 * -------------------------------------------------------------------------- */
bool is_empty(void* q, Casting cast) {
    bool empty;
    switch(cast) {
        case DISH_LIST: {
            DishList* dl = (DishList*) q;
            pthread_mutex_lock(&dl->lock);
            empty = (dl->head == NULL);
            pthread_mutex_unlock(&dl->lock);
            return empty;
        }

        case ORDER_LIST: {
            OrderList* ol = (OrderList*) q;
            pthread_mutex_lock(&ol->lock);
            empty = (ol->head == NULL);
            pthread_mutex_unlock(&ol->lock);
            return empty;
        }

        case CUSTOMER_QUEUE: {
            CustomerQueue* cq = (CustomerQueue*) q;
            pthread_mutex_lock(&cq->lock);
            empty = (cq->head == NULL);
            pthread_mutex_unlock(&cq->lock);
            return empty;
        }
        
        default:
            perror("is_empty - Unknown Cast");
            return false; // see note below
    }
}

/* --------------------------------------------------------------------------
 * enqueue — allocate a new node and append to tail
 * -------------------------------------------------------------------------- */
void enqueue(Customer* c, CustomerQueue* cq) {
    if(cq == NULL || c == NULL) {
        perror("null pointers passed!");
        return;
    }

    QueueNode* node = malloc(sizeof(QueueNode));
    if(node == NULL) {
        return;
    }
    node->c = c;
    node->next = NULL;

    pthread_mutex_lock(&cq->lock);

    if(cq->tail == NULL) {
        cq->head = node;
        cq->tail = node;
    }
    else {
        cq->tail->next = node;
        cq->tail = node;
    }
    cq->size++;

    pthread_mutex_unlock(&cq->lock);

    return;
}

/* --------------------------------------------------------------------------
 * dequeue — remove head node, returning the customer
 * -------------------------------------------------------------------------- */
Customer* dequeue(CustomerQueue* cq) {
    if(is_empty(cq, CUSTOMER_QUEUE)) {
        pthread_mutex_unlock(&cq->lock);
        return NULL;
    }
    pthread_mutex_lock(&cq->lock);

    QueueNode* old_head = cq->head;
    Customer* c = old_head->c;

    cq->head = old_head->next;
    if (!cq->head) cq->tail = NULL;
    cq->size--;

    pthread_mutex_unlock(&cq->lock);
    free(old_head);

    return c;
}

/* --------------------------------------------------------------------------
 * peek — return pointer to head customer without removing
 * -------------------------------------------------------------------------- */
Customer* peek(CustomerQueue* cq) {
    pthread_mutex_lock(&cq->lock);
    Customer* c = (!cq->head) ? NULL : cq->head->c;
    pthread_mutex_unlock(&cq->lock);
    return c;
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

void print_cst(Customer* cst) {
    pthread_mutex_lock(cst->arg->print);
    printf(CYAN " CUSTOMER %d" RESET ":\t", cst->arg->id);
    switch(cst->present) {
        case STANDING:
            printf(GRAY "standing" RESET);
            break;

        case SEATED:
            printf("seated");
            break;

        case ORDER_CHOSEN:
            printf("order chosen");
            break;

        case WAITING_DISH:
            printf(YELLOW "waiting for a dish" RESET);
            break;

        case EATING:
            printf(ORANGE "eating" RESET);
            break;

        case FINISHED:
            break;
            
        case LEFT_TIRED:
            break;
    }
    printf(", p = %d\n", cst->patience);
    pthread_mutex_unlock(cst->arg->print);
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
    // time to serve
    float tts = cst->order_made - cst->order_received;

    int num_dishes;

    while(cst->arg->running) {
        pthread_mutex_lock(&cst->arg->sc->lock);
        pthread_cond_wait(&cst->arg->sc->tick_cv, &cst->arg->sc->lock);
        pthread_mutex_unlock(&cst->arg->sc->lock);   

        switch(cst->present) {
            case STANDING:
                break;

            case SEATED:
                num_dishes = safe_rand_range(5);
                cst->o = make_order(cst->arg->menu, num_dishes);
                cst->o->num_dishes = num_dishes;
                cst->patience += get_prep_time(cst->o) + safe_rand_range(10);
                cst->order_made = cst->arg->sc->tick;
                atomic_store(&cst->future, ORDER_CHOSEN);
                break;

            case ORDER_CHOSEN:
                break;

            case WAITING_DISH: {
                for (int i = 0; i < cst->o->num_dishes; i++) {
                    if (cst->o->dishes[i]->delivered) {
                        pthread_mutex_lock(&cst->o->lock);

                        for (int j = i; j < cst->o->num_dishes - 1; j++) {
                            cst->o->dishes[j] = cst->o->dishes[j + 1];
                        }
                        cst->o->num_dishes--;

                        pthread_mutex_unlock(&cst->o->lock);

                        cst->future = EATING;
                        break;
                    }
                }
                break;
            }

            case EATING:
                if(atomic_load(&cst->o->completed)) {
                    atomic_store(&cst->future, FINISHED);
                }
                else {
                    atomic_store(&cst->future, WAITING_DISH);
                }
                break;

            case FINISHED:
                cst->order_received = cst->arg->sc->tick;
                tts = cst->order_received - cst->order_made;
                atomic_float_add(cst->arg->score, cst->o->total_price * (1.0f - (tts / cst->patience)));

                sem_post(cst->arg->rc);
                printf(CYAN " CUSTOMER %d" RESET ":\t", cst->arg->id);
                printf(GREEN "done, bye\n" RESET);
                return;
                break;

            case LEFT_TIRED:
                if(cst->o) {
                    atomic_store(&cst->o->expired, true);
                }
                //atomic_float_sub(cst->arg->score, cst->o->total_price * log2f(1));
                sem_post(cst->arg->rc);
                printf(CYAN " CUSTOMER %d" RESET ":\t", cst->arg->id);
                printf(RED "tired of waiting\n" RESET);
                return;
                break;

            default:
                perror("Customer - Unknown State");
            
        }
        print_cst(cst);

        if(cst->patience > 0) {
            cst->patience--;
        }
        else {
            atomic_store(&cst->future, LEFT_TIRED);
        }
        atomic_store(&cst->present, cst->future);
    }

    // clean memory    
}

void* customer_thread(void* args) {
  if(!args) return NULL;
  //creates customer then goes into customer loop
  Customer* cst = malloc(sizeof(Customer));
  atomic_store(&cst->present, STANDING);
  cst->arg = (CustomerArgs*) args;
  cst->patience = DEFAULT_PATIENCE;
  enqueue(cst, cst->arg->standing);

  customer_loop(cst);

  return NULL;
}
