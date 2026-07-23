#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

// ─── Project Headers ─────────────────────────────────────────────────────────
// (Includes actual struct definitions needed for field access in cleanup code)
#include "kitchen.h"
#include "cook.h"
#include "customer.h"
#include "waiter.h"

// ─── Random & Atomic Utilities ───────────────────────────────────────────────

pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;

int safe_rand(void) {
    pthread_mutex_lock(&rand_mutex);
    int r = rand();
    pthread_mutex_unlock(&rand_mutex);
    return r;
}

void atomic_float_sub(_Atomic float* target, float value) {
    float old_val = atomic_load(target);
    float new_val;
    do {
        new_val = old_val - value;
    } while (!atomic_compare_exchange_weak(target, &old_val, new_val));
}

int safe_rand_range(int max) {
    pthread_mutex_lock(&rand_mutex);
    int r = rand() % (max - 1);
    r++;
    pthread_mutex_unlock(&rand_mutex);
    return r;
}

// ─── Destruction Functions ──────────────────────────────────────────────────

void customer_queue_destroy(CustomerQueue* queue) {
    if (!queue) return;
    QueueNode* current = queue->head;
    while (current != NULL) {
        QueueNode* next_node = current->next;
        free(current);
        current = next_node;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_destroy(&queue->lock);
    free(queue);
}

void kitchen_manager_destroy(KitchenManager* km) {
    if (!km) return;

    if (km->pools) {
        for (int i = 0; i < km->num_pools; i++) {
            ToolPool* pool = km->pools[i];
            if (pool != NULL) {
                if (pool->tools) {
                    for (int j = 0; j < pool->quantity; j++) {
                        if (pool->tools[j].name) {
                            free(pool->tools[j].name);
                        }
                        pthread_mutex_destroy(&pool->tools[j].lock);
                    }
                    free(pool->tools);
                }

                if (pool->name) {
                    free(pool->name);
                }

                pthread_mutex_destroy(&pool->lock);
                pthread_cond_destroy(&pool->cv);

                free(pool);
            }
        }
        free(km->pools);
    }

    pthread_mutex_destroy(&km->sink);
    free(km);
}

void destroy_customer(Customer* customer) {
    if (!customer) return;
    if (customer->o) {
        Order* order = customer->o;          
        if (order->dishes) {
            for (int j = 0; j < order->num_dishes; j++) {
                Dish* dish = order->dishes[j];
                if (dish != NULL) {
                    free(dish->name);
                    if (dish->tools) {
                        for (int k = 0; dish->tools[k] != NULL; k++) {
                            free(dish->tools[k]);
                        }
                        free(dish->tools);
                    }
                    pthread_mutex_destroy(&dish->lock);
                    free(dish);
                }
            }
            free(order->dishes);
        }
        pthread_mutex_destroy(&order->lock);
        free(order);
    }
    pthread_mutex_destroy(&customer->lock);
    free(customer);
}

void cook_destroy(Cook* cook) {
    if (!cook) return;

    if (cook->claimed_tools) {
        free(cook->claimed_tools);
    }

    free(cook);
}

void dish_list_destroy(DishList* list) {
    if (!list) return;

    DishListNode* current = list->head;
    while (current != NULL) {
        DishListNode* next_node = current->next;
        free(current);
        current = next_node;
    }

    pthread_mutex_destroy(&list->lock);
    free(list);
}

void order_list_destroy(OrderList* list) {
    if (!list) return;

    OrderListNode* current = list->head;
    while (current != NULL) {
        OrderListNode* next_node = current->next;
        free(current);
        current = next_node;
    }

    pthread_mutex_destroy(&list->lock);
    free(list);
}

void order_manager_destroy(OrderManager* om) {
    if (!om) return;

    dish_list_destroy(om->completed_dishes);
    order_list_destroy(om->waitlist);
    order_list_destroy(om->priority);
    order_list_destroy(om->discarded_orders);

    pthread_mutex_destroy(&om->lock);
    free(om);
}

void clean_memory(CleaningArgs* args) {
    if (!args) return;

    customer_queue_destroy(args->standing);
    customer_queue_destroy(args->seated);
    customer_queue_destroy(args->waiting_order);

    order_manager_destroy(args->om);
    kitchen_manager_destroy(args->km);

    free(args->cooka);
    free(args->waita);
    free(args->custa);

    if (args->print) {
        pthread_mutex_destroy(args->print);
    }
}
