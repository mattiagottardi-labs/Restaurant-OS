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

    if(!ol->head) {
        pthread_mutex_unlock(&ol->lock);
        return NULL;
    }

    ListNode* old_head  = ol->head;
    Order*    o         = old_head->o;
    ol->head = old_head->next;
    ol->size--;

    pthread_mutex_unlock(&ol->lock);

    return o;
}

/* --------------------------------------------------------------------------
 * list_peek — return pointer to head order without removing
 * -------------------------------------------------------------------------- */
Order* list_peek(OrderList* ol) {
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
        Order* o = list_pop(om->waitlist);
        if (!o) break;
        list_insert_order(om->priority, o, 1);
    }
}

void list_insert_order(OrderList* ol, Order* o, int algorithm) {
    if (!o) return;
    ListNode* new_node = malloc(sizeof(ListNode));
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
            ListNode* cur = ol->head;
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
void customer_entertainment(Waiter* wtr, EntertainmentActivity *ea) {
    sem_wait(wtr->arg->ea_bin);
    int activity = safe_rand_range(5) - 1;
    printf("Waiter is %s, to entertain standing customers.\n", ea[activity].name);
    usleep(ea[activity].duration);

    QueueNode* tmp = wtr->arg->standing->head;

    for(int i = 0; i < wtr->arg->standing->size; i++) {
        tmp->c->patience += ea[activity].efficacy;
        tmp = tmp->next;
    }

    free(tmp);
    tmp = NULL;

    sem_post(wtr->arg->ea_bin);
}

void print_wtr(Waiter* wtr) {
    pthread_mutex_lock(wtr->arg->print);
    printf("\tWAITER %d: ", wtr->arg->id);
    switch(wtr->present) {
        case IDLE:
            printf("idle");
            break;

        case ACCOMODATING_CUSTOMER:
            printf("accomodating customer");
            break;

        case TAKING_ORDER:
            printf("taking the order");
            break;

        case DELIVERING_FOOD:
            printf("delivering the food");
            break;

        case ENTERTAINING:
            printf("entertaining the standing customer/s");
            break;
    }
    printf("\n");
    pthread_mutex_unlock(wtr->arg->print);
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
    Customer* cst = malloc(sizeof(Customer));
    Order* o = malloc(sizeof(Order));

    while (wtr->arg->running) {
        pthread_mutex_lock(&wtr->arg->sc->lock);
        pthread_cond_wait(&wtr->arg->sc->tick_cv, &wtr->arg->sc->lock);
        pthread_mutex_unlock(&wtr->arg->sc->lock);

        print_wtr(wtr);

        switch(wtr->present) {
            case IDLE:
                // if there are seated customer, try to serve them
                if(!is_empty(wtr->arg->seated, CUSTOMER_QUEUE)) {
                    wtr->future = TAKING_ORDER;
                }
                // otherwise check for standing customer and try to accomodate them
                else if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE)) {
                    if(sem_trywait(wtr->arg->ea_bin) == 0) {
                        wtr->future = ENTERTAINING;
                    }
                    else if(sem_trywait(wtr->arg->rc) == 0) {
                        wtr->future = ACCOMODATING_CUSTOMER;
                    }
                    else {
                        wtr->future = wtr->present;
                    }
                }
                else {
                    wtr->future = wtr->present;
                }
                break;

            // when customer is waiting outside but there is free space -> accomodate the customer
            case ACCOMODATING_CUSTOMER:
                cst = pop(wtr->arg->standing);
                if(cst) {
                    enqueue(cst, wtr->arg->seated);
                    cst->future = SEATED;
                }

                if(wtr->arg->om->completed_orders->head != NULL) {
                    printf("Order ready to be delivered");
                    wtr->future = DELIVERING_FOOD;
                }
                else {
                    wtr->future = IDLE;
                }

                //wtr->future = is_empty(wtr->arg->om->completed_orders, ORDER_LIST) ? IDLE : DELIVERING_FOOD;
                break;
            
            case TAKING_ORDER:
                // obtain customer without removing from the queue
                cst = peek(wtr->arg->seated);
                if(cst->o) {
                    list_insert_order(wtr->arg->om->waitlist, cst->o, 0);
                    refill_priority(wtr->arg->om);
                    
                    cst = pop(wtr->arg->seated);
                    cst->future = WAITING_ORDER;
                    enqueue(cst, wtr->arg->waiting_order);
                } 

                if(!is_empty(wtr->arg->standing, CUSTOMER_QUEUE)) {
                    if(sem_trywait(wtr->arg->ea_bin) == 0) {
                        wtr->future = ENTERTAINING;
                    }
                    else {
                        wtr->future = IDLE;
                    }
                }
                else {
                    //wtr->future = is_empty(wtr->arg->om->completed_orders, ORDER_LIST) ? DELIVERING_FOOD : IDLE;
                    wtr->future = IDLE;
                }
                break;

            case DELIVERING_FOOD:
                // take the order and deliver to the customer
                o = list_pop(wtr->arg->om->completed_orders);
                if(o) {
                    atomic_store(&o->c->served, true);
                }
                
                if(list_peek(wtr->arg->om->completed_orders)) {
                    wtr->future = wtr->present;
                }
                else {
                    wtr->future = IDLE;
                }
                break;

            case ENTERTAINING:
                customer_entertainment(wtr, ea);
                wtr->future = IDLE;
                break;

            default:
                perror("Waiter - Unknown state!");          
        }
        // Update the state for next cycle
        wtr->present = wtr->future;
    }
}

void* waiter_thread(void* args){
    if(!args) return NULL;
    // creates waiter
    Waiter* wtr = malloc(sizeof(Waiter));
    wtr->present = IDLE;
    wtr->arg = (WaiterArgs*) args;;

    // runs waiter loop
    waiter_loop(wtr);

    return NULL;
}