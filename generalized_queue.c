#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>


static node* make_customer_node(customer* c) {
    node* n = malloc(sizeof(node));
    if (!n) {
        fprintf(stderr, "make_customer_node: out of memory\n");
        return NULL;
    }
    n->customer = c;
    n->extra_patience = c->patience - get_prep_time(c->o->dishes);
    n->next = NULL;
    return n;
}

static node* make_order_node(order* o) {
    node* n = malloc(sizeof(node));
    if (!n) {
        fprintf(stderr, "make_order_node: out of memory\n");
        return NULL;
    }
    n->customer = o->customer; // This node is for an order, not a customer
    n->extra_patience = 0; // Not relevant for orders
    n->next = NULL;
    return n;
}


void add_customer(customer* c, customer_queue* cq) {
    if (!c || !cq) return;
    node* new_node = make_node(c);
    if (!new_node) return;
    pthread_mutex_lock(&cq->lock);
    // Insert in sorted (ascending extra_patience) position
    if (!cq->start || new_node->extra_patience <= cq->start->extra_patience) {
        /* goes at the front */
        new_node->next = cq->start;
        cq->start = new_node;
    } else {
        node* cur = cq->start;
        while (cur->next && cur->next->extra_patience < new_node->extra_patience) {
            cur = cur->next;
        }
        new_node->next = cur->next;
        cur->next = new_node;
    }

    cq->num_customers++;
    pthread_mutex_unlock(&cq->lock);
}

void remove_customer(customer* c, customer_queue* cq) {
    if (!c || !cq || !cq->start) return;

    node* cur  = cq->start;
    node* prev = NULL;
    pthread_mutex_lock(&cq->lock);
    while (cur) {
        if (cur->customer == c) {
            if (prev) {
                prev->next = cur->next;
            } else {
                cq->start = cur->next;
            }
            free(cur);
            cq->num_customers--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&cq->lock);
    fprintf(stderr, "remove_customer: customer not found in queue\n");
}

order_queue * create_new_queue(){
    order_queue* q = malloc(sizeof(*q));
    if(!q) return NULL;

    q->num_orders = 0;
    q->max_capacity = MAX_ORDERS_PER_QUEUE;
    q->avg_patience = 0;
    q->queue = malloc(sizeof(*q->queue) * q->max_capacity);
    if(!q->queue){
        free(q);
        return NULL;
    }
    return q;
}


void add_order(order* o, order_queue* oq, queue_manager* qm) {
    if (!o || !oq) return;
    node* new_node = make_order_node(o);
    if (!new_node) return;
    pthread_mutex_lock(&oq->lock);
 
    if (!oq->start) {
        oq->start = new_node;
    } else {
        node* cur = oq->start;
        while (cur->next && cur->next->extra_patience < new_node->extra_patience) {
            cur = cur->next;
        }
        new_node->next = cur->next;
        cur->next = new_node;
    }

    oq->num_orders++;
    if(oq->num_orders > oq->max_capacity) {
        //split and create a new queue if we exceed capacity
        order_queue* new_queue = create_new_queue();
        if (!new_queue) {
            fprintf(stderr, "add_order: failed to create new queue\n");
            pthread_mutex_unlock(&oq->lock);
            return;
        }
        // Move half of the orders to the new queue
        int mid = oq->num_orders / 2;
        node* cur = oq->start;
        for (int i = 0; i < mid - 1; i++) {
            cur = cur->next;
        }
        new_queue->start = cur->next;
        cur->next = NULL;
        new_queue->num_orders = oq->num_orders - mid;
        oq->num_orders = mid;
        // Here you would need to add the new queue to your queue manager's list of queues
        oq->next_queue = new_queue;
    }
    pthread_mutex_unlock(&oq->lock);
}
