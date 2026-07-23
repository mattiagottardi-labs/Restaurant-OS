#include <stdlib.h>
#include <stdio.h>

#include "waiter.h"
#include "customer.h"

EntertainmentActivity ea[5] = {
    {"chatting", 1},
    {"singing", 2},
    {"dancing", 3},
    {"performing magic tricks", 5},
    {"making puns", 4}
};
/* --------------------------------------------------------------------------
 * get_prio — slack time: lower value = less slack = higher urgency
 * -------------------------------------------------------------------------- */
int get_prio(Order* o, int algorithm) {
    switch (algorithm) {
        case 0:  /* Slack time — most urgent first */
            if(o) {
                return o->c->patience - atomic_load(&o->remaining_time);
            }
            else {
                return 100;
            }

        case 1:  /* SJF — shortest job first */
            return atomic_load(&o->remaining_time);
        case 2:  /* Unsorted — append to tail, prio irrelevant */
            return 0;
        default:
            return 0;
    }
}

void list_insert(OrderList* ol, Customer* cst, int algorithm) {
    Order* o = cst->o;
    if (!o) return;

    OrderListNode* new_node = malloc(sizeof(OrderListNode));
    if (!new_node) {
        perror("list_insert: malloc failed\n");
        return;
    }
    new_node->o    = o;
    new_node->prio = get_prio(o, algorithm);
    new_node->next = NULL;

    pthread_mutex_lock(&ol->lock);

    if (algorithm == 2 || !ol->head) {
        /* Append to tail */
        if (!ol->head) {
            ol->head = new_node;
        } else {
            OrderListNode* cur = ol->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < ol->head->prio) {
            new_node->next = ol->head;
            ol->head = new_node;
        } else {
            OrderListNode* cur = ol->head;
            while (cur->next && cur->next->prio <= new_node->prio) cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    ol->size++;
    pthread_mutex_unlock(&ol->lock);
}

/* --------------------------------------------------------------------------
 * list_remove_order — remove and return the Order at the head of the list
 * -------------------------------------------------------------------------- */
Order* list_remove_order(OrderList* ol) {
    pthread_mutex_lock(&ol->lock);

    if(!ol->head) {
        pthread_mutex_unlock(&ol->lock);
        return NULL;
    }

    OrderListNode* old_head  = ol->head;
    Order*    o         = old_head->o;
    ol->head = old_head->next;
    ol->size--;

    pthread_mutex_unlock(&ol->lock);

    return o;
}

/* --------------------------------------------------------------------------
 * list_pop_dish — remove and return the Dish at the head of the list
 * -------------------------------------------------------------------------- */
Dish* list_remove_dish(DishList* dl) {
    pthread_mutex_lock(&dl->lock);

    if(!dl->head) {
        pthread_mutex_unlock(&dl->lock);
        return NULL;
    }

    DishListNode* old_head  = dl->head;
    Dish* d = old_head->d;
    dl->head = old_head->next;
    dl->size--;

    pthread_mutex_unlock(&dl->lock);

    return d;
}

/* --------------------------------------------------------------------------
 * list_peek_order — return pointer to head order without removing
 * -------------------------------------------------------------------------- */
Order* list_peek_order(OrderList* ol) {
    pthread_mutex_lock(&ol->lock);
    Order* o  = ol->size == 0 ? NULL : ol->head->o;
    pthread_mutex_unlock(&ol->lock);
    return o;
}

/* --------------------------------------------------------------------------
 * refill_priority — top up the 10-slot cook buffer from waitlist head
 * -------------------------------------------------------------------------- */
void refill_priority(OrderManager* om) {
    while (om->priority->size < 10) {
        Order* o = list_remove_order(om->waitlist);
        if (!o) {
          break;
        }
        list_insert_order(om->priority, o, 1);
    }
}

void list_insert_order(OrderList* ol, Order* o, int algorithm) {
    if (!o) return;
    OrderListNode* new_node = malloc(sizeof(OrderListNode));
    if (!new_node) {
        perror("list_insert_order - malloc failed");
        return;
    }
    new_node->o    = o;
    new_node->prio = get_prio(o, algorithm);
    new_node->next = NULL;

    pthread_mutex_lock(&ol->lock);

    if (algorithm == 2 || !ol->head) {
        /* Append to tail */
        if(!ol->head) {
            ol->head = new_node;
        }
        else {
            OrderListNode* cur = ol->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    }
    else {
        /* Sorted insert by ascending prio */
        if(new_node->prio < ol->head->prio) {
            new_node->next = ol->head;
            ol->head = new_node;
        }
        else {
            OrderListNode* cur = ol->head;
            while (cur->next && cur->next->prio <= new_node->prio)
                cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }
    ol->size++;
    pthread_mutex_unlock(&ol->lock);
}

void list_insert_dish(DishList* dl, Dish* d) {
    pthread_mutex_lock(&dl->lock);
    if (!d) return;
    DishListNode* new_node = malloc(sizeof(DishListNode));
    if (!new_node) {
        perror("list_insert_order - malloc failed");
        return;
    }
    new_node->d    = d;
    new_node->next = NULL;

    DishListNode* tmp = dl->head;

    if (!dl->head) {
        dl->head = new_node;
    }
    else {
        while(tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = new_node;
    }
    dl->size++;

    pthread_mutex_unlock(&dl->lock);
}

void dishlist_init(DishList* dl){
  dl->head = NULL;
  dl->size = 0;
  pthread_mutex_init(&dl->lock, NULL);
}

void orderlist_init(OrderList* ol){
  ol->head = NULL;
  ol->size = 0;
  pthread_mutex_init(&ol->lock, NULL);
}

void om_init(OrderManager* om){
  pthread_mutex_init(&om->lock, NULL);
  DishList* completed_dish = malloc(sizeof(DishList));
  OrderList* waitlist = malloc(sizeof(OrderList));
  OrderList* priority = malloc(sizeof(OrderList));
  OrderList* discarded = malloc(sizeof(OrderList));

  om->completed_dishes = completed_dish;
  om->waitlist = waitlist;
  om->priority = priority;
  om->discarded_orders = discarded;

  dishlist_init(om->completed_dishes);
  orderlist_init(om->waitlist);
  orderlist_init(om->priority);
  orderlist_init(om->discarded_orders);
}

void print_wtr(Waiter* wtr, char* activity) {
    pthread_mutex_lock(wtr->arg->print);
    printf(MAGENTA " WAITER %d" RESET ":\t", wtr->arg->id);
    switch(wtr->present) {
        case IDLE:
           printf(GRAY "idle" RESET);
            break;

        case ACCOMODATING_CUSTOMER:
            printf("accomodating customer");
            break;

        case TAKING_ORDER:
            printf("taking the order");
            break;

        case DELIVERING_DISH:
            printf(BLUE "delivering dish to the customer" RESET);
            break;

        case ENTERTAINING:
            printf(MAGENTA "%s to entertain standing customers" RESET, activity);

            break;
    }
    printf("\n");
    pthread_mutex_unlock(wtr->arg->print);
}

void clean_queue(CustomerQueue* q, bool check_patience, bool check_served) {
    pthread_mutex_lock(&q->lock);

    QueueNode* tmp = q->head;
    while (tmp && tmp->c &&
           ((check_patience && tmp->c->patience <= 0) ||
            (tmp->c->o != NULL && tmp->c->o->expired) ||
            (check_served && tmp->c->o != NULL && tmp->c->served) ||
            (tmp->c->finish_eating))) {
        q->head = tmp->next;
        free(tmp);
        tmp = q->head;
    }
    if (!tmp) {
        q->tail = NULL;
        pthread_mutex_unlock(&q->lock);
        return;
    }

    QueueNode* prev = tmp;
    tmp = tmp->next;
    while (tmp) {
        if (tmp->c &&
            ((check_patience && tmp->c->patience <= 0) ||
             (tmp->c->o != NULL && tmp->c->o->expired) ||
             (check_served && tmp->c->o != NULL && tmp->c->served) ||
             (tmp->c->finish_eating))) {
            prev->next = tmp->next;
            free(tmp);
            tmp = prev->next;
        } else {
            prev = tmp;
            tmp = tmp->next;
        }
    }
    q->tail = prev;
    pthread_mutex_unlock(&q->lock);
}

void clean_queues(Waiter* wtr) {
    clean_queue(wtr->arg->standing,      true,  false); // patience matters
    clean_queue(wtr->arg->seated,        true,  false); // patience matters
    clean_queue(wtr->arg->waiting_order, false, true);  // expiry + served matter
}

/* --------------------------------------------------------------------------
 * FSM STATES FUNCTIONS
 * -------------------------------------------------------------------------- */

Customer* waiter_idle(Waiter* wtr, Dish** d) {
    Customer* cst = peek(wtr->arg->seated);

    if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE) && (sem_trywait(wtr->arg->rc) == 0)) {
        wtr->future = ACCOMODATING_CUSTOMER;
        return dequeue(wtr->arg->standing);
    }
    else if(cst != NULL) {
        if(cst->present == ORDER_CHOSEN) {
            wtr->future = TAKING_ORDER;
            return dequeue(wtr->arg->seated);
        }
        else {
            wtr->future = wtr->present;
            return NULL;
        }
    }
    else if(!is_empty(wtr->arg->om->completed_dishes, DISH_LIST)) {
        *d = list_remove_dish(wtr->arg->om->completed_dishes);
        wtr->future = DELIVERING_DISH;
        return NULL;
    }
    else if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE) && (sem_trywait(wtr->arg->ea_bin) == 0)) {
        wtr->future = ENTERTAINING;
        return NULL;
    }
    else {
        wtr->future = wtr->present;
        return NULL;
    }
}

void waiter_accomodating_customer(Waiter* wtr, Customer* cst) {
    if(cst != NULL) {
        atomic_store(&cst->order_made, (cst->arg->sc->tick + 1));
        atomic_store(&cst->future, SEATED);
        enqueue(cst, wtr->arg->seated);
    }
    else {
        sem_post(wtr->arg->rc);
    }

    wtr->future = IDLE;
}

void waiter_taking_order(Waiter* wtr, Customer* cst) {
    if(cst != NULL) {
        list_insert_order(wtr->arg->om->waitlist, cst->o, 2);
        refill_priority(wtr->arg->om);

        atomic_store(&cst->future, WAITING_DISH);
        enqueue(cst, wtr->arg->waiting_order);
    }

    wtr->future = IDLE;
}

Dish* waiter_delivering_dish(Waiter* wtr, Dish* d) {
    // take the dish and deliver it to the customer
    if(d) {
        d->delivered = true;
    }

    if(!is_empty(wtr->arg->om->completed_dishes, DISH_LIST)) {
        wtr->future = wtr->present;
        return list_remove_dish(wtr->arg->om->completed_dishes);
    }
    else {
        wtr->future = IDLE;
        return NULL;
    }
}

void waiter_entertaining(Waiter* wtr, char* name) {
    int activity = safe_rand_range(5) - 1;
    strcpy(name, ea[activity].name);

    pthread_mutex_lock(&wtr->arg->standing->lock);
    QueueNode* tmp = wtr->arg->standing->head;

    while(tmp != NULL) {
        tmp->c->patience += ea[activity].efficacy;
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&wtr->arg->standing->lock);
    sem_post(wtr->arg->ea_bin);

    wtr->future = IDLE;
}

/* --------------------------------------------------------------------------
 * waiter_loop — pops customers from the queue, unpacks their orders
 *               and inserts them into the waitlist sorted by prio.
 *               Defers refill_priority and cook interaction to later.
 * -------------------------------------------------------------------------- */
void waiter_loop(Waiter* wtr) {
    // buffer customer/order used to perform operation on queues
    Customer* cst = NULL;
    Dish* d = NULL;

    char name[32] = {0};

    while(atomic_load(wtr->arg->running)) {
        pthread_mutex_lock(&wtr->arg->sc->lock);
        pthread_cond_wait(&wtr->arg->sc->tick_cv, &wtr->arg->sc->lock);
        pthread_mutex_unlock(&wtr->arg->sc->lock);
        clean_queues(wtr);

        switch(wtr->present) {
            case IDLE:
                cst = waiter_idle(wtr, &d);
                break;

            case ACCOMODATING_CUSTOMER:
                waiter_accomodating_customer(wtr, cst);
                break;

            case TAKING_ORDER:
                waiter_taking_order(wtr, cst);
                break;

            case DELIVERING_DISH:
                d = waiter_delivering_dish(wtr, d);
                break;

            case ENTERTAINING:
                waiter_entertaining(wtr, name);
                break;

            default:
                perror("Waiter - Unknown state!");
        }
        print_wtr(wtr, name);
        // Update the state for next cycle
        wtr->present = wtr->future;
    }
}

void* waiter_thread(void* args){
    if(!args) return NULL;
    // creates waiter
    Waiter* wtr = malloc(sizeof(Waiter));
    wtr->present = IDLE;
    wtr->arg = (WaiterArgs*) args;

    // runs waiter loop
    waiter_loop(wtr);

    free(wtr);
    return NULL;
}