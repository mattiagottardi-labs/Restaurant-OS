#include "waiter.h"
#include "customer.h"
#include <stdlib.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * get_prio — slack time: lower value = less slack = higher urgency
 * -------------------------------------------------------------------------- */
int get_prio(order* o, int algorithm) {
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


void list_insert(order_list* l, customer* c, int algorithm) {
    order* o = c->o;
    if (!o) return;

    list_node* new_node = malloc(sizeof(list_node));
    if (!new_node) {
        fprintf(stderr, "list_insert: malloc failed\n");
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
            list_node* cur = l->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < l->head->prio) {
            new_node->next = l->head;
            l->head = new_node;
        } else {
            list_node* cur = l->head;
            while (cur->next && cur->next->prio <= new_node->prio)
                cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    l->size++;
    pthread_mutex_unlock(&l->lock);
}

/* --------------------------------------------------------------------------
 * list_pop — remove and return the order at the head of the list
 * -------------------------------------------------------------------------- */

order* list_pop(order_list* l) {
    pthread_mutex_lock(&l->lock);

    if (!l->head) {
        pthread_mutex_unlock(&l->lock);
        return NULL;
    }

    list_node* old_head = l->head;
    order*     o        = old_head->o;
    l->head = old_head->next;
    l->size--;

    pthread_mutex_unlock(&l->lock);

    free(old_head);
    return o;
}

/* --------------------------------------------------------------------------
 * refill_priority — top up the 10-slot cook buffer from waitlist head
 * -------------------------------------------------------------------------- */

void refill_priority(order_manager* m) {
    while (m->priority->size < 10) {
        order* o = list_pop(m->waitlist);
        if (!o) break;
        list_insert_order(m->priority, o, 1);
    }
}

/* --------------------------------------------------------------------------
 * waiter_loop — pops customers from the queue, unpacks their orders
 *               and inserts them into the waitlist sorted by prio.
 *               Defers refill_priority and cook interaction to later.
 * -------------------------------------------------------------------------- */

void waiter_loop(order_manager* m, customer_queue* q, sim_clock* sc, bool* running) {
    while (running) {
        pthread_mutex_lock(&sc->lock);
        pthread_cond_wait(&sc->tick_cv, &sc->lock);
        pthread_mutex_unlock(&sc->lock);

        customer* c = NULL;
        while ((c = pop(q)) != NULL) {
            if (!c->o || atomic_load(&c->o->expired)) {
                list_insert(m->discarded_orders, c, 2);
                continue;
            }
            list_insert(m->waitlist, c, 0);  // or 1 for SJF
        }

        refill_priority(m);
    }
}

void list_insert_order(order_list* l, order* o, int algorithm) {
    if (!o) return;
    list_node* new_node = malloc(sizeof(list_node));
    if (!new_node) {
        fprintf(stderr, "list_insert_order: malloc failed\n");
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
            list_node* cur = l->head;
            while (cur->next) cur = cur->next;
            cur->next = new_node;
        }
    } else {
        /* Sorted insert by ascending prio */
        if (new_node->prio < l->head->prio) {
            new_node->next = l->head;
            l->head = new_node;
        } else {
            list_node* cur = l->head;
            while (cur->next && cur->next->prio <= new_node->prio)
                cur = cur->next;
            new_node->next = cur->next;
            cur->next = new_node;
        }
    }

    l->size++;
    pthread_mutex_unlock(&l->lock);
}


void* waiter_thread(void* arg){
    if(!arg) return NULL;
    waiter_args* arguments = (waiter_args*) arg;
    //creates waiter and runs waiter loop
    waiter_loop(arguments->m, arguments->q, arguments->sc);
    return NULL;
}

void list_init(order_list* ol){
  ol->head = NULL;
  ol->size = NULL;
  pthread_mutex_init(&ol->lock);
}

void om_init(order_manager* om){
  om->running = true;
  pthread_mutex_init(&om->lock);
  order_list* waitlist = (order_list*) malloc(sizeof(order_list));
  order_list* priority = (order_list*) malloc(sizeof(order_list));
  order_list* discarded = (order_list*) malloc(sizeof(order_list));
  order_list* completed = (order_list*) malloc(sizeof(order_list));
  om->waitlist = waitlist;
  om->priority = priority;
  om->discarded = discarded;
  om->completed = completed;
  list_init(om->waitlist);
  list_init(om->priority);
  list_init(om->completed_orders);
  list_init(om->discarded_orders);
}
