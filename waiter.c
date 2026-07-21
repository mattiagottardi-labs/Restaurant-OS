#include <stdlib.h>
#include <stdio.h>

#include "waiter.h"
#include "customer.h"

EntertainmentActivity ea[5] = {
    {"chatting", 2},
    {"singing", 6},
    {"dancing", 5},
    {"performing magic tricks", 10},
    {"making puns", 8}
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
        if (!o) break;
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
  OrderList* completed = malloc(sizeof(OrderList));

  om->completed_dishes = completed_dish;
  om->waitlist = waitlist;
  om->priority = priority;
  om->discarded_orders = discarded;
  om->completed_orders = completed;

  dishlist_init(om->completed_dishes);
  orderlist_init(om->waitlist);
  orderlist_init(om->priority);
  orderlist_init(om->completed_orders);
  orderlist_init(om->discarded_orders);
}

void print_wtr(Waiter* wtr, char* activity) {
    pthread_mutex_lock(wtr->arg->print);
    printf(MAGENTA " WAITER %d" RESET ":\t", wtr->arg->id);
    switch(wtr->present) {
        case IDLE:
            printf(GRAY "idle, standing empty? %b, seated? %b, order_made? %b" RESET, is_empty(wtr->arg->standing, CUSTOMER_QUEUE), is_empty(wtr->arg->seated, CUSTOMER_QUEUE), is_empty(wtr->arg->waiting_order, CUSTOMER_QUEUE));
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
            printf(MAGENTA "is %s, to entertain standing customers, standing empty? %b " RESET, activity, is_empty(wtr->arg->standing, CUSTOMER_QUEUE));

            break;
    }
    printf("\n");
    pthread_mutex_unlock(wtr->arg->print);
}

void clean_queue(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);
    QueueNode* tmp = q->head;
    QueueNode* prev = NULL;

    // Strip leading nodes with patience == 0
    while (tmp && tmp->c->patience == 0) {
        q->head = tmp->next;
        free(tmp);              // + free tmp->c if the queue owns it
        tmp = q->head;
    }

    if (!tmp) {
        pthread_mutex_unlock(&q->lock);
        return;           // whole queue emptied out
    }

    prev = tmp;
    tmp = tmp->next;

    while (tmp) {
        if (tmp->c->patience == 0) {
            prev->next = tmp->next;
            free(tmp);
            tmp = prev->next;   // prev stays put, only tmp advances
        } else {
            prev = tmp;
            tmp = tmp->next;
        }
    }
    pthread_mutex_unlock(&q->lock);
}

void clean_queues(Waiter* wtr) {
    clean_queue(wtr->arg->seated);
    clean_queue(wtr->arg->standing);
    clean_queue(wtr->arg->waiting_order);
}

/* --------------------------------------------------------------------------
 * waiter_loop — pops customers from the queue, unpacks their orders
 *               and inserts them into the waitlist sorted by prio.
 *               Defers refill_priority and cook interaction to later.
 * 
 * Davide:  additionaly the waiter has to check 2 different lists (or queues)
 *          for the standing customers outside the restaurant (they have to
 *          be entertained by the waiter ![only one so mutex needed]), 
 * -------------------------------------------------------------------------- */
void waiter_loop(Waiter* wtr) {
    // buffer customer/order used to perform operation on queues
    Customer* cst = NULL;
    Dish* d = NULL;

    int activity;
    char name[16] = {0};

    while(wtr->arg->running) {
        pthread_mutex_lock(&wtr->arg->sc->lock);
        pthread_cond_wait(&wtr->arg->sc->tick_cv, &wtr->arg->sc->lock);
        pthread_mutex_unlock(&wtr->arg->sc->lock);
        clean_queues(wtr);

        switch(wtr->present) {
            case IDLE:
                cst = peek(wtr->arg->seated);
                
                if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE) && (sem_trywait(wtr->arg->rc) == 0)) {
                    cst = dequeue(wtr->arg->standing);
                    wtr->future = ACCOMODATING_CUSTOMER;
                }
                else if(cst != NULL) {
                    if(cst->present == ORDER_CHOSEN) {
                        cst = dequeue(wtr->arg->seated);
                        wtr->future = TAKING_ORDER;
                    }
                    else {
                        wtr->future = wtr->present;
                    }
                }
                else if(!is_empty(wtr->arg->om->completed_dishes, DISH_LIST)) {
                    d = list_remove_dish(wtr->arg->om->completed_dishes);
                    wtr->future = DELIVERING_DISH;
                }
                else if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE) && (sem_trywait(wtr->arg->ea_bin) == 0)) {
                    wtr->future = !is_empty(wtr->arg->standing, CUSTOMER_QUEUE) ? IDLE : ENTERTAINING;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            case ACCOMODATING_CUSTOMER:
                if(cst != NULL) {
                    atomic_store(&cst->order_made, (cst->arg->sc->tick + 1));
                    atomic_store(&cst->future, SEATED);
                    enqueue(cst, wtr->arg->seated);
                }
                else {
                    sem_post(wtr->arg->rc);
                }

                wtr->future = IDLE;
                break;
            
            case TAKING_ORDER:
                if(cst != NULL) {
                    list_insert_order(wtr->arg->om->waitlist, cst->o, 2);
                    refill_priority(wtr->arg->om);
                        
                    atomic_store(&cst->future, WAITING_DISH);
                    enqueue(cst, wtr->arg->waiting_order);
                }

                wtr->future = IDLE;
                break;

            case DELIVERING_DISH:
                // take the dish and deliver it to the customer
                if(d) {
                    d->delivered = true;                  
                }
                
                if(!is_empty(wtr->arg->om->completed_dishes, DISH_LIST)) {
                    wtr->future = wtr->present;
                }
                else {
                    wtr->future = IDLE;
                }
                break;

            case ENTERTAINING:
                activity = safe_rand_range(5) - 1;
                strcpy(name, ea[activity].name);

                pthread_mutex_lock(&wtr->arg->standing->lock);
                QueueNode* tmp = wtr->arg->standing->head;

                while(tmp != NULL) {
                    tmp->c->patience += ea[activity].efficacy;
                    tmp = tmp->next;
                }
                pthread_mutex_unlock(&wtr->arg->standing->lock);

                free(tmp);
                tmp = NULL;
                
                sem_post(wtr->arg->ea_bin);

                wtr->future = IDLE;
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

    return NULL;
}
