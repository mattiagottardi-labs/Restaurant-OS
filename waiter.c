#include "waiter.h"
#include "customer.h"
#include <stdlib.h>
#include <stdio.h>

EntertainmentActivity ea[5] = {
    {"chatting", 1, 1000000},
    {"singing", 2, 2000000},
    {"dancing", 3, 3000000},
    {"performing magic tricks", 5, 2500000},
    {"making puns", 2, 500000}
};

/* --------------------------------------------------------------------------
 * get_prio — slack time: lower value = less slack = higher urgency
 * -------------------------------------------------------------------------- */
int get_prio(Order* o, int algorithm) {
    switch (algorithm) {
        case 0:  /* Slack time — most urgent first */
            return o->c->patience - atomic_load(&o->remaining_time);
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

    ListNode* new_node = malloc(sizeof(ListNode));
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
            ListNode* cur = ol->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < ol->head->prio) {
            new_node->next = ol->head;
            ol->head = new_node;
        } else {
            ListNode* cur = ol->head;
            while (cur->next && cur->next->prio <= new_node->prio) cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    ol->size++;
    pthread_mutex_unlock(&ol->lock);
}

/* --------------------------------------------------------------------------
 * list_pop — remove and return the Order at the head of the list
 * -------------------------------------------------------------------------- */
Order* list_pop(OrderList* ol) {
    pthread_mutex_lock(&ol->lock);

    if (!ol->head) {
        pthread_mutex_unlock(&ol->lock);
        return NULL;
    }

    ListNode* old_head  = ol->head;
    Order*    o         = old_head->o;
    ol->head = old_head->next;
    ol->size--;

    pthread_mutex_unlock(&ol->lock);

    free(old_head);
    return o;
}

/* --------------------------------------------------------------------------
 * order_ready — return true if list is empty and false if not
 * -------------------------------------------------------------------------- */
bool order_ready(OrderList* ol) {
    return ol->size == 0;
}

/* --------------------------------------------------------------------------
 * is_empty — return true if list is empty and false if not
 * -------------------------------------------------------------------------- */
bool is_empty(CustomerQueue* q) {
    return q->size == 0;
}

/* --------------------------------------------------------------------------
 * refill_priority — top up the 10-slot cook buffer from waitlist head
 * -------------------------------------------------------------------------- */
void refill_priority(OrderManager* om) {
    while (om->priority->size < 10) {
        Order* o = list_pop(om->waitlist);
        if (!o) break;
        list_insert_order(om->priority, o, 1);
    }
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
    while (wtr->arg->running) {
        pthread_mutex_lock(&wtr->arg->sc->lock);
        pthread_cond_wait(&wtr->arg->sc->tick_cv, &wtr->arg->sc->lock);
        pthread_mutex_unlock(&wtr->arg->sc->lock);

        switch(wtr->present) {
            case IDLE:
                // if there are standing customer, try to accomodate one of them
                if(!is_empty(wtr->arg->standing)) {
                    // check if semaphore has available spaces
                    int sval;
                    sem_getvalue(&wtr->arg->rc, &sval);
                    if(sval > 0) {
                        wtr->future = ACCOMODATING_CUSTOMER;
                    }
                }
                else if(!is_empty(wtr->arg->seated)) {
                    wtr->future = TAKING_ORDER;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            // when customer is waiting outisde but there is free space, accomodate the customer
            case ACCOMODATING_CUSTOMER:
                Customer* cst = dequeue(wtr->arg->standing);

                enqueue(cst, wtr->arg->seated);
                cst->present = SEATED;

                free(cst);
                cst = NULL;

                wtr->future = IDLE;
                break;
            
            case TAKING_ORDER:
                // obtain customer without removing from the queue
                Customer* cst = pop(wtr->arg->seated);
                if(cst->o != NULL) {
                    // insert the order, if not NULL
                    list_insert_order(wtr->arg->om->waitlist, cst, 0);

                    cst = dequeue(wtr->arg->seated);
                    enqueue(cst, wtr->arg->waiting_order);
                    cst->present = WAITING_ORDER;
                }              

                // free the pointer
                free(cst);
                cst = NULL;

                if(is_empty(wtr->arg->standing) && is_empty(wtr->arg->seated)) {
                    wtr->future = IDLE;
                }
                else if(!is_empty(wtr->arg->standing)) {
                    wtr->future = ENTERTAINING;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            case CHECKING_FOOD:
                if(!order_ready(wtr->arg->om->completed_orders)) {
                    wtr->future = DELIVERING_FOOD;
                }
                else {
                    wtr->future = IDLE;
                }
                break;

            case DELIVERING_FOOD:
                // take the order and search for the customer in the waiting_order queue
                Order* o = list_pop(wtr->arg->om->completed_orders);

                // loop through all orders and search the one pointing to the right customer
                QueueNode* find_order = wtr->arg->waiting_order->head;
                while(o->c != find_order->c) {
                    find_order = find_order->next;
                }
                o->c->served = true;

                free(find_order);
                find_order = NULL;
                break;

            case ENTERTAINING:
                customer_entertainment(ea);
                wtr->future = IDLE;
                break;

            default:
                perror("Waiter - Unknown state!");          
        }
        // Update the state for next cycle
        wtr->present = wtr->future;

        /*
        sem_wait(&wtr->arg->ea_bin);
        // waiter checks if a customer has to Order
        if(!is_empty(seated)) {

            // waiter checks if a Dish is ready to deliver
            if(!order_ready(wtr->arg->m->completed_orders)) {

                // waiter checks if there are standing customers waiting
                if(!is_empty(standing)) {
                    customer_entertainment(ea);
                    sem_post(&ea_bin);
                }
            }
        }
        
        else {

            // moving customer from standing to seated queue
            customer* c = NULL;
            while ((c = pop(standing)) != NULL && seated->size < seated->max_size) {
                if (!c->o || atomic_load(&c->o->expired)) {
                    list_insert(m->discarded_orders, c, 2);
                    continue;
                }
                list_insert(m->waitlist, c, 0);  // or 1 for SJF
            }

            refill_priority(m);
        }
*/
    }
}

void list_insert_order(OrderList* ol, Order* o, int algorithm) {
    if (!o) return;
    ListNode* new_node = malloc(sizeof(ListNode));
    if (!new_node) {
        perror("list_insert_order: malloc failed\n");
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
            ListNode* cur = ol->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < ol->head->prio) {
            new_node->next = ol->head;
            ol->head = new_node;
        } else {
            ListNode* cur = ol->head;
            while (cur->next && cur->next->prio <= new_node->prio)
                cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    ol->size++;
    pthread_mutex_unlock(&ol->lock);
}

void* waiter_thread(void* args){
    if(!args) return NULL;
    // creates waiter
    Waiter* wtr = malloc(sizeof(Waiter));
    wtr->present = IDLE;
    wtr->arg = (WaiterArgs*) args;;

    // runs waiter loop
    waiter_loop(wtr);

    free(wtr);
    wtr = NULL;

    return NULL;
}

void list_init(OrderList* ol){
  ol->head = NULL;
  ol->size = 0;
  pthread_mutex_init(&ol->lock, NULL);
}

void om_init(OrderManager* om){
  pthread_mutex_init(&om->lock, NULL);
  OrderList* waitlist = malloc(sizeof(OrderList));
  OrderList* priority = malloc(sizeof(OrderList));
  OrderList* discarded = malloc(sizeof(OrderList));
  OrderList* completed = malloc(sizeof(OrderList));
  om->waitlist = waitlist;
  om->priority = priority;
  om->discarded_orders = discarded;
  om->completed_orders = completed;
  list_init(om->waitlist);
  list_init(om->priority);
  list_init(om->completed_orders);
  list_init(om->discarded_orders);
}

// if waiting customers and no waiting orders -> call this function
int customer_entertainment(EntertainmentActivity *ea) {
    int activity = safe_rand_range(5) - 1;
    printf("Waiter is %s, to entertain waiting customers.", ea[activity].name);
    usleep(ea[activity].duration);
    return ea[activity].efficacy;
}
/*
void take_order(CustomerQueue* seated, CustomerQueue* waiting_food, OrderList* waiting){
  pthread_mutex_lock(&seated->lock);
  if(seated->size == 0){
    pthread_mutex_unlock(&seated->lock);
    return;
  }
  customer* temp = pop(seated);
  enqueue(temp, waiting_food);
  list_insert(temp);
}
*/