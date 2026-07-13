#include "waiter.h"
#include "customer.h"
#include <stdlib.h>
#include <stdio.h>

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

void list_insert(OrderList* l, Customer* c, int algorithm) {
    Order* o = c->o;
    if (!o) return;

    ListNode* new_node = malloc(sizeof(ListNode));
    if (!new_node) {
        perror("list_insert: malloc failed\n");
        return;
    }
    new_node->o    = o;
    new_node->prio = get_prio(o, algorithm);
    new_node->next = NULL;

    pthread_mutex_lock(&l->lock);

    if (algorithm == 2 || !l->head) {
        /* Append to tail */
        if (!l->head) {
            l->head = new_node;
        } else {
            ListNode* cur = l->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < l->head->prio) {
            new_node->next = l->head;
            l->head = new_node;
        } else {
            ListNode* cur = l->head;
            while (cur->next && cur->next->prio <= new_node->prio) cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    l->size++;
    pthread_mutex_unlock(&l->lock);
}

/* --------------------------------------------------------------------------
 * list_pop — remove and return the Order at the head of the list
 * -------------------------------------------------------------------------- */

Order* list_pop(OrderList* l) {
    pthread_mutex_lock(&l->lock);

    if (!l->head) {
        pthread_mutex_unlock(&l->lock);
        return NULL;
    }

    ListNode* old_head  = l->head;
    Order*    o         = old_head->o;
    l->head = old_head->next;
    l->size--;

    pthread_mutex_unlock(&l->lock);

    free(old_head);
    return o;
}

/* --------------------------------------------------------------------------
 * order_ready — return true if list is empty and false if not
 * -------------------------------------------------------------------------- */

bool order_ready(OrderList* l) {
    return l->size == 0;
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

void refill_priority(OrderManager* m) {
    while (m->priority->size < 10) {
        Order* o = list_pop(m->waitlist);
        if (!o) break;
        list_insert_order(m->priority, o, 1);
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
    while (wtr->wtr_arg->running) {
        pthread_mutex_lock(&wtr->wtr_arg->sc->lock);
        pthread_cond_wait(&wtr->wtr_arg->sc->tick_cv, &wtr->wtr_arg->sc->lock);
        pthread_mutex_unlock(&wtr->wtr_arg->sc->lock);

        switch(wtr->present) {
            case IDLE:
                if(!is_empty(wtr->wtr_arg->seated)) {
                    wtr->future = ACCOMODATING_CUSTOMER;
                }
                else if(!is_empty(wtr->wtr_arg->seated)) {
                    wtr->future = TAKING_ORDER;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            // when customer is waiting outisde but there is free space, accomodate the customer
            case ACCOMODATING_CUSTOMER:
                pthread_mutex_lock(&wtr->wtr_arg->standing->lock);
                Customer* cst = dequeue(wtr->wtr_arg->standing);
                pthread_mutex_unlock(&wtr->wtr_arg->standing->lock);

                pthread_mutex_lock(&wtr->wtr_arg->seated->lock);
                enqueue(cst, wtr->wtr_arg->seated);
                cst->present = SEATED;
                pthread_mutex_unlock(&wtr->wtr_arg->seated->lock);

                free(cst);
                cst = NULL;

                wtr->future = IDLE;
                break;
            
            case TAKING_ORDER:
                pthread_mutex_lock(&wtr->wtr_arg->seated->lock);
                Customer* cst = dequeue(wtr->wtr_arg->seated);
                pthread_mutex_unlock(&wtr->wtr_arg->seated->lock);

                list_insert(wtr->wtr_arg->m->waitlist, cst, 0);

                pthread_mutex_lock(&wtr->wtr_arg->waiting_order->lock);
                enqueue(cst, wtr->wtr_arg->waiting_order);
                cst->present = WAITING_ORDER;
                pthread_mutex_unlock(&wtr->wtr_arg->waiting_order->lock);

                free(cst);
                cst = NULL;

                if(is_empty(wtr->wtr_arg->standing) && is_empty(wtr->wtr_arg->seated)) {
                    wtr->future = IDLE;
                }
                else if(!is_empty(wtr->wtr_arg->standing)) {
                    wtr->future = ENTERTAINING;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            case CHECKING_FOOD:
                
                if(!order_ready(wtr->wtr_arg->m->completed_orders)) {
                    wtr->future = DELIVERING_FOOD;
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            case DELIVERING_FOOD:

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
        sem_wait(&wtr->wtr_arg->ea_bin);
        // waiter checks if a customer has to Order
        if(!is_empty(seated)) {

            // waiter checks if a Dish is ready to deliver
            if(!order_ready(wtr->wtr_arg->m->completed_orders)) {

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

void list_insert_order(OrderList* l, Order* o, int algorithm) {
    if (!o) return;
    ListNode* new_node = malloc(sizeof(ListNode));
    if (!new_node) {
        perror("list_insert_order: malloc failed\n");
        return;
    }
    new_node->o    = o;
    new_node->prio = get_prio(o, algorithm);
    new_node->next = NULL;

    pthread_mutex_lock(&l->lock);

    if (algorithm == 2 || !l->head) {
        /* Append to tail */
        if (!l->head) {
            l->head = new_node;
        } else {
            ListNode* cur = l->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < l->head->prio) {
            new_node->next = l->head;
            l->head = new_node;
        } else {
            ListNode* cur = l->head;
            while (cur->next && cur->next->prio <= new_node->prio)
                cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    l->size++;
    pthread_mutex_unlock(&l->lock);
}

void* waiter_thread(void* args){
    if(!args) return NULL;
    // creates waiter
    Waiter* wtr = malloc(sizeof(Waiter));
    wtr->present = IDLE;
    wtr->wtr_arg = (WaiterArgs*) args;;

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