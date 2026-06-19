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

int NUM_COOKS;
int NUM_WAITERS;
int MAX_CUSTOMERS;
int TOTAL_CUSTOMERS;
int GAME_SPEED;
char* MENU_FILE;
char* RESOURCE_FILE;

int main(int argc, char* argv[]){
  // check if sufficient numer of argument is passed
 /* if(argc < 8) {
    perror("Too few arguemnts passed, while launching main binary!\n");
    return 1;
  }
  // variables sent by bootstrap.sh
  NUM_COOKS = atoi(argv[1]);
  NUM_WAITERS = atoi(argv[2]);
  MAX_CUSTOMERS = atoi(argv[3]);
  TOTAL_CUSTOMERS = atoi(argv[4]);
  GAME_SPEED = atoi(argv[5]);

  // strings don't need atoi()
  MENU_FILE = argv[6];
  RESOURCE_FILE = argv[7];
  */
  //create structs
  kitchen_manager* km = (kitchen_manager*) malloc(sizeof(kitchen_manager));
  menu* Menu = (menu*) malloc(sizeof(menu));
  sim_clock* sc = (sim_clock*) malloc(sizeof(sim_clock));
  customer_queue* q = (customer_queue*) malloc(sizeof(customer_queue));
  order_manager* om = (order_manager*) malloc(sizeof(order_manager));
  //init structs
  make_tools(resources_path, km, 10);
  make_menu(menu_path, Menu, 20, 4);
  om_init(om);
  queue_init(q);
  clock_init(sc);
  srand(seed);

  for(int i = 0; i < 10; i++){
    customer* C = (customer*) malloc(sizeof(customer));
    C->o = make_order(C, Menu, safe_rand_range(5));
    enqueue(C, q);
  }

  pthread_t cooks_tid[NUM_COOKS];
  pthread_t waiters_tid[NUM_WAITERS];
  pthread_t customers_tid[TOTAL_CUSTOMERS];

 /*  void* cook_arg = (cook_args) {} 
  cook_args* ptr_cook_args = {om, sc, km};
  for(int i = 0; i < NUM_COOKS; i++) {
    pthread_create(&cooks_tid[i], NULL, cook_thread(), (void*) ptr_cook_args);
  }

  waiter_args* ptr_waiter_args = {om, q, sc};
  for(int i = 0; i < NUM_WAITERS; i++) {
    pthread_create(&waiters_tid[i], NULL, waiter_thread(), (void*) ptr_waiter_args);
  }

  customer_args* ptr_customer_args = {om, sc, km, score, };
  for(int i = 0; i < TOTAL_CUSTOMERS; i++) {
    pthread_create(&customers_tid[i], NULL, customer_thread(), (void*) ptr_customer_args);
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
    printf("%d\n", i);
    print_customer(current->c);
    }

    pthread_mutex_unlock(&q->lock);
}
void queue_init(customer_queue* q){
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  pthread_mutex_init(&q->lock, NULL);
}


