#include "customer.h"
#include "waiter.h"
#include "cook.h"
#include "kitchen.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

void print_tool_status(kitchen_manager* km);
void queue_init(customer_queue* q);
void print_customer(customer* C);
void print_queue(customer_queue* q);

char* resources_path = "/home/mgottardi/OS/rewrite/2026-project-5/code/resources.csv";
char* menu_path = "/home/mgottardi/OS/rewrite/2026-project-5/code/menu.csv";

_Atomic float score = 0;
int seed = 100; 

int main(int argc, char* argv[]){
  //create structs
  kitchen_manager* km = (kitchen_manager*) malloc(sizeof(kitchen_manager));
  menu* Menu = (menu*) malloc(sizeof(menu));
  sim_clock* sc = (sim_clock*) malloc(sizeof(sim_clock));
  customer_queue* q = (customer_queue*) malloc(sizeof(customer_queue));
  order_manager* om = (order_manager*) malloc(sizeof(order_manager));
  order_list* waitlist = (order_list*) malloc(sizeof(order_list));
  order_list* priority = (order_list*) malloc(sizeof(order_list));
  order_list* discarded = (order_list*) malloc(sizeof(order_list));
  order_list* completed = (order_list*) malloc(sizeof(order_list));
  om->waitlist = waitlist;
  om->priority = priority;
  om->discarded = discarded;
  om->completed = completed;
  om->waitlist->head = NULL;
  om->waitlist->size = 0;
  pthread_mutex_init(&om->waitlist->lock);
  //init structs
  make_tools(resources_path, km,10);
  make_menu(menu_path, Menu, 20, 4);
  queue_init(q);
  clock_init(sc);
  srand(seed);

  for(int i = 0; i < 10; i++){
    customer* C = (customer*) malloc(sizeof(customer));
    C->o = make_order(C, Menu, safe_rand_range(5));
    enqueue(C, q);
  }

  list_insert()
  cook_dish()
  print_queue(q);
  return 0;
}

void print_tool_status(kitchen_manager* km){
   printf("TOOLS: \n");
  for(int i = 0; i < km->num_pools; i++){
    printf("%s, %d items, %d in use\n", km->pools[i]->name, km->pools[i]->quantity, km->pools[i]->in_use);
  }
}

void print_customer(customer* C){
  printf("\nCustomer's Order:\n");
  for(int i = 0; C->o->dishes[i] != NULL; i++){
    printf("%s\n", C->o->dishes[i]->name);
  }
}

void print_queue(customer_queue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        printf("queue is empty\n");
    } else {
        printf("head is there, printing QUEUE:\n");
        queue_node* current = q->head;
        int i = 0;
        while (current != NULL) {
            printf("%d. ", i);
            print_customer(current->c);
            current = current->next;
            i++;
        }
    }

    pthread_mutex_unlock(&q->lock);
}
void queue_init(customer_queue* q){
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  pthread_mutex_init(&q->lock, NULL);
}


