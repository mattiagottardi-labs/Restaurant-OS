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

void print_tool_status(KitchenManager* km);
void queue_init(CustomerQueue* q);
void print_customer(Customer* C);
void print_queue(CustomerQueue* q);
void print_list(OrderList* ol);
char* resources_path = "/home/mgottardi/OS/rewrite/2026-project-5/code/resources.csv";
char* menu_path = "/home/mgottardi/OS/rewrite/2026-project-5/code/menu.csv";

_Atomic float score = 0;

int NUM_COOKS;
int NUM_WAITERS;
int MAX_CUSTOMERS;
int TOTAL_CUSTOMERS;
int GAME_SPEED;
int RANDOM_SEED;
char* MENU_FILE;
char* RESOURCE_FILE;

bool* running;
const int MAX_CUSTOMER_SPAWN_RATE = 5000000; // caution, this time is in microseconds
int CLK_PERIOD;

// customer_thread_manager implements this function
void* thread_manager(void* args) {
  if(!args) return NULL;

  pthread_t* customer_tid = NULL;

  CustomerArgs* arguments = (CustomerArgs*) args;
  int customer_counter = 0;

  // customer threads has to be spawned at random time
  int random_delay = ((rand() % MAX_CUSTOMER_SPAWN_RATE) + 1000) / GAME_SPEED;
  
  // cycle that keeps running to manage customer threads
  while(customer_counter < TOTAL_CUSTOMERS) {
    usleep(random_delay);

    void *tmp =  realloc((void*) customer_tid, (customer_counter + 1) * sizeof(pthread_t));
    if(tmp == NULL) {
      perror("Realloc failed!");
      free(customer_tid);
      return NULL;
    }

    customer_tid = (pthread_t*) tmp;
    pthread_create(&customer_tid[customer_counter], NULL, customer_thread, (void*) arguments);
    customer_counter++;
  }

  for(int i = 0; i < customer_counter; i++) {
    pthread_join(customer_tid[i], NULL);
  }

  free(customer_tid);
  pthread_exit(NULL);
}

// customer_thread_manager implements this function
void* thread_manager_mk2(void* args) {
  if(!args) return NULL;

  CustomerArgs* arguments = (CustomerArgs*) args;
  int customer_counter = 0;

  // customer threads has to be spawned at random time
  int random_delay = ((rand() % MAX_CUSTOMER_SPAWN_RATE) + 1000) / GAME_SPEED;

  Mapping* mappings = NULL;
  
  // cycle that keeps running to manage customer threads
  while(customer_counter < TOTAL_CUSTOMERS) {
    usleep(random_delay);

    Customer* c = malloc(sizeof(Customer));
    pthread_t* tid = malloc(sizeof(pthread_t));
    Mapping* mappings = realloc(mappings, (customer_counter + 1) * sizeof(Mapping));

    pthread_create(&tid[customer_counter], NULL, customer_thread, (void*) arguments);
    customer_counter++;

    enqueue(c, arguments->q);
    printf("New customer enqued in standing queue\n");
  }

  for(int i = 0; i < customer_counter; i++) {
    pthread_join(mappings->tid, NULL);
  }
  pthread_exit(NULL);
}

int main(int argc, char* argv[]){
  // check if sufficient numer of argument is passed
  if(argc < 8) {
    perror("Too few arguemnts passed, while launching main binary!\n");
    return 1;
  }
  // variables sent by bootstrap.sh
  NUM_COOKS = atoi(argv[1]);
  NUM_WAITERS = atoi(argv[2]);
  MAX_CUSTOMERS = atoi(argv[3]);
  TOTAL_CUSTOMERS = atoi(argv[4]);
  GAME_SPEED = atoi(argv[5]);
  RANDOM_SEED = atoi(argv[6]);

  // strings don't need atoi()
  MENU_FILE = argv[7];
  RESOURCE_FILE = argv[8];

  CLK_PERIOD = 1000000 / GAME_SPEED;

  sem_t restaurant_capacity, ea_bin;
  sem_init(&restaurant_capacity, 0, MAX_CUSTOMERS);
  sem_init(&ea_bin, 0, 0);

  //create structs
  running = malloc(sizeof(bool));
  *running = true;
  KitchenManager* km = malloc(sizeof(KitchenManager));
  Menu* menu = malloc(sizeof(Menu));
  SimClock* sc = malloc(sizeof(SimClock));
  CustomerQueue* standing = malloc(sizeof(CustomerQueue));
  CustomerQueue* seated = malloc(sizeof(CustomerQueue));
  OrderManager* om = malloc(sizeof(OrderManager));
  //init structs
  make_tools(resources_path, km, 10);
  make_menu(menu_path, menu, 20, 4);
  om_init(om);
  queue_init(standing);
  queue_init(seated);
  clock_init(sc);
  srand(RANDOM_SEED);

  // if nothing (in the init steps) fails, running is true
  *running = true;

/*
  for(int i = 0; i < 15; i++){
    customer* C = malloc(sizeof(customer));
    C->o = make_order(C, Menu, safe_rand_range(5));
    C->patience = get_prep_time(C->o) + safe_rand_range(100);
    enqueue(C, standing);
  }
*/

  pthread_t cooks_tid[NUM_COOKS];
  pthread_t waiters_tid[NUM_WAITERS];
  pthread_t customer_thread_manager;

  CookArgs* cook_args = malloc(sizeof(CookArgs));
  cook_args->km = km;
  cook_args->m = om;
  cook_args->running = running;
  cook_args->sc = sc;

  for(int i = 0; i < NUM_COOKS; i++) {
    pthread_create(&cooks_tid[i], NULL, cook_thread, cook_args);
  }

  WaiterArgs* waiter_args = malloc(sizeof(WaiterArgs));
  waiter_args->ea_bin = &ea_bin;
  waiter_args->m = om;
  waiter_args->running = running;
  waiter_args->sc = sc;
  waiter_args->seated = seated;
  waiter_args->standing = standing;

  for(int i = 0; i < NUM_WAITERS; i++) {
    pthread_create(&waiters_tid[i], NULL, waiter_thread, waiter_args);
  }

  CustomerArgs* customer_args = malloc(sizeof(CustomerArgs));
  customer_args->menu = menu;
  customer_args->q = standing;
  customer_args->restaurant_capacity = &restaurant_capacity;
  customer_args->running = running;
  customer_args->sc = sc;
  customer_args->score = &score;

  // thread_manager manages all customer threads
  pthread_create(&customer_thread_manager, NULL, thread_manager, customer_args);

  // Missing pthread_join for cooks and waiters 

/*
  printf("QUEUE BEFORE POPPING:\n");
  print_queue(q);
  while(q->size != 0){
    list_insert(om->waitlist, pop(q), 0);
  }
  printf("EMPTY LIST:\n");
  print_queue(q);
  printf("PRIORITY LIST:\n");
  print_list(om->priority);
  printf("WAITLIST:\n");
  print_list(om->waitlist);
  refill_priority(om);
  printf("PRIORITY LIST:\n");
  print_list(om->priority);
  Order* target_order = get_next_order(om);
  Dish* target_dish = pick_dish(target_order);
  bool* running = malloc(sizeof(bool));
  *running = true;
  cook_dish(target_dish, target_order, om, sc, km, running );
  return 0;
*/

  // destroy the semaphore
  sem_destroy(&restaurant_capacity);
  sem_destroy(&ea_bin);
} 

void print_tool_status(KitchenManager* km){
   printf("TOOLS: \n");
  for(int i = 0; i < km->num_pools; i++){
    printf("%s, %d items, %d in use\n", km->pools[i]->name, km->pools[i]->quantity, km->pools[i]->in_use);
  }
}

void print_customer(Customer* C){
  printf("Customer's Patience: %d", C->patience);
  printf("\nCustomer's Slack: %d", get_prio(C->o, 0));
  printf("\nCustomer's Order:\n");
  for(int i = 0; C->o->dishes[i] != NULL; i++){
    printf("%s\n", C->o->dishes[i]->name);
  }
}

void print_queue(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        printf("queue is empty\n");
    } else {
        printf("head is there, printing QUEUE:\n");
        QueueNode* current = q->head;
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

void print_list(OrderList* ol){
  ListNode* current = ol->head;
  printf("list begin\n");
  for(int i = 0; i < ol->size; i++){
    printf("%d: ", i);
    print_customer(current->o->c);
    current = current->next;
  }
  printf("list end\n");
}

void queue_init(CustomerQueue* q){
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  pthread_mutex_init(&q->lock, NULL);
}

// ─── simulation clock ───────────────────────────────────

void clock_init(SimClock* sim) {
    sim->tick = 0;
    pthread_mutex_init(&sim->lock, NULL);
    pthread_cond_init(&sim->tick_cv, NULL);
}

void clock_destroy(SimClock* sim) {
    pthread_mutex_destroy(&sim->lock);
    pthread_cond_destroy(&sim->tick_cv);
}

// must be in main.c since GAME_SPEED adjusts the tick speed
void tick_advance(SimClock* sim) {
    usleep(CLK_PERIOD);
    pthread_mutex_lock(&sim->lock);
    printf("ticking %d", sim->tick);
    sim->tick++;
    pthread_cond_broadcast(&sim->tick_cv);
    pthread_mutex_unlock(&sim->lock);
}

// ────────────────────────────────────────────────────────