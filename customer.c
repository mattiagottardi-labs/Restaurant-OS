#include "customer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const int DEFAULT_PATIENCE = 40;

/* --------------------------------------------------------------------------
 * Order creation
 * -------------------------------------------------------------------------- */
Order* make_order(Customer* c, Menu* menu, int num_dishes) {
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
        case ORDER_LIST: {
            OrderList* ol = (OrderList*) q;
            pthread_mutex_lock(&ol->lock);
            empty = (ol->size == 0);
            pthread_mutex_unlock(&ol->lock);
            return empty;
        }

        case CUSTOMER_QUEUE: {
            CustomerQueue* cq = (CustomerQueue*) q;
            pthread_mutex_lock(&cq->lock);
            empty = (cq->size == 0);
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
void dequeue(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (is_empty(q, CUSTOMER_QUEUE)) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    QueueNode* old_head = q->head;
    q->head = old_head->next;
    if (!q->head) q->tail = NULL;
    q->size--;

    pthread_mutex_unlock(&q->lock);

    free(old_head);
    old_head = NULL;
}

/* --------------------------------------------------------------------------
 * peek — return pointer to head customer without removing
 * -------------------------------------------------------------------------- */
Customer* peek(CustomerQueue* q) {
    //pthread_mutex_lock(&q->lock);
    Customer* c = is_empty(q, CUSTOMER_QUEUE) ? NULL : q->head->c;
    //pthread_mutex_unlock(&q->lock);
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

        case WAITING_ORDER:
            printf(YELLOW "waiting for my food" RESET);
            break;

        case EATING:
            printf(ORANGE "eating" RESET);
            break;

        case FINISHED:
            break;
            
        case TIRED:
            break;
    }
    printf(", patience = %d \n", cst->patience);
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

    while(cst->arg->running) {
        pthread_mutex_lock(&cst->arg->sc->lock);
        pthread_cond_wait(&cst->arg->sc->tick_cv, &cst->arg->sc->lock);
        pthread_mutex_unlock(&cst->arg->sc->lock);   

        switch(cst->present) {
            case STANDING:
                break;

            case SEATED:
                cst->o = make_order(cst, cst->arg->menu, safe_rand_range(5));
                cst->patience += get_prep_time(cst->o) + safe_rand_range(10);
                cst->order_made = cst->arg->sc->tick;
                break;

            case WAITING_ORDER:
                if(cst->served) {
                    cst->order_received = cst->arg->sc->tick;
                    atomic_store(&cst->future, EATING);
                }
                else {
                    atomic_store(&cst->future, cst->present);
                }
                break;

            case EATING:
                atomic_store(&cst->future, FINISHED);
                break;

            case FINISHED:
                atomic_store(cst->arg->score, cst->o->total_price * (1.0f - ( tts / cst->patience)));
                sem_post(cst->arg->rc);
                printf(CYAN " CUSTOMER %d" RESET ":\t", cst->arg->id);
                printf(GREEN "done, bye\n" RESET);
                return;
                break;

            case TIRED:
                if(cst->o) {
                    atomic_store(&cst->o->expired, true);
                    printf("\t\t\tOrder discarded (1 true, 0 false): %d\n", cst->o->expired);
                }
                // cst->arg->score = ;
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
            atomic_store(&cst->future, TIRED);
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
