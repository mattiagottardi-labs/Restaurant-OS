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

#define INVALID_PARAMETER -1
#define INVALID_FILE -2

typedef struct InfoArgs {
    SimClock*           sc;
    CustomerQueue*      standing;
    CustomerQueue*      seated;
    CustomerQueue*      waiting_order;
    pthread_mutex_t*    print;
} InfoArgs;

void print_tool_status(KitchenManager* km);
void queue_init(CustomerQueue* q);
void print_customer(Customer* C);
void print_queue(CustomerQueue* q);
void print_list(OrderList* ol);

_Atomic float score = 0.0f;

int NUM_COOKS;
int NUM_WAITERS;
int MAX_CUSTOMERS;
int TOTAL_CUSTOMERS;
int GAME_SPEED;
int RANDOM_SEED;
char* MENU_FILE;
char* RESOURCE_FILE;

bool* running;
int MAX_CUSTOMER_SPAWN_RATE; // caution, this time is in microseconds
int CLK_PERIOD;

// customer_thread_manager implements this function
void* thread_manager(void* args) {
  if(!args) return NULL;

  pthread_t* customer_tid = malloc(TOTAL_CUSTOMERS * sizeof(pthread_t));

  CustomerArgs* arguments = (CustomerArgs*) args;
  CustomerArgs* customer_args = malloc(TOTAL_CUSTOMERS * sizeof(CustomerArgs));

  MAX_CUSTOMER_SPAWN_RATE = 100000000 / GAME_SPEED;

  // customer threads has to be spawned at random time
  int random_delay = ((rand() % MAX_CUSTOMER_SPAWN_RATE) + 10000) / GAME_SPEED;
  
  // customer counter index
  int cc;

  // cycle that keeps running to manage customer threads
  for(cc = 0; cc < TOTAL_CUSTOMERS; cc++) {
    usleep(random_delay);

    customer_args[cc].id = cc + 1;
    customer_args[cc].menu = arguments->menu;
    customer_args[cc].rc = arguments->rc;
    customer_args[cc].running = arguments->running;
    customer_args[cc].print = arguments->print;
    customer_args[cc].sc = arguments->sc;
    customer_args[cc].score = arguments->score;
    customer_args[cc].standing = arguments->standing;
    pthread_create(&customer_tid[cc], NULL, customer_thread, (void*) &customer_args[cc]);
  }

  for(int i = 0; i < cc; i++) {
    pthread_join(customer_tid[i], NULL);
  }

  running = false;
  printf(BOLD_U "\nRUNNING SET TO FALSE!\n" RESET);

  return NULL;
}

void print_tool_status(KitchenManager* km){
   printf("TOOLS: \n");
  for(int i = 0; i < km->num_pools; i++){
    printf("%s, %d items, %d in use\n", km->pools[i]->name, km->pools[i]->quantity, km->pools[i]->in_use);
  }
}

void print_customer(Customer* C){
  printf("Customer's stats:");
  printf("\n\tPatience: %d\n", C->patience);
  if(C->o) {
   printf("\n\tSlack: %d", get_prio(C->o, 0));
    printf("\n\tOrder:\n");
    for(int i = 0; C->o->dishes[i] != NULL; i++){
      printf("%s\n", C->o->dishes[i]->name);
    }
  }
}

void print_queue(CustomerQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        printf("queue is empty\n");
    } else {
        printf("printing QUEUE:\n");
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

void clock_init(SimClock* sc) {
    sc->tick = 0;
    pthread_mutex_init(&sc->lock, NULL);
    pthread_cond_init(&sc->tick_cv, NULL);
}

void clock_destroy(SimClock* sc) {
    pthread_mutex_destroy(&sc->lock);
    pthread_cond_destroy(&sc->tick_cv);
}

void* tick_advance(void* args) {
  SimClock* sc = (SimClock*) args;
  while(running) {
    usleep(CLK_PERIOD);
    pthread_mutex_lock(&sc->lock);
    sc->tick++;
    
    pthread_cond_broadcast(&sc->tick_cv);
    pthread_mutex_unlock(&sc->lock);
  }
  return NULL;
}

// ────────────────────────────────────────────────────────


void* info_thread(void* args) {
  InfoArgs* arg = (InfoArgs*) args;
  while(running) {
    pthread_mutex_lock(&arg->sc->lock);
    pthread_cond_wait(&arg->sc->tick_cv, &arg->sc->lock);
    pthread_mutex_unlock(&arg->sc->lock);

    pthread_mutex_lock(arg->print);/*
    printf(BOLD_U "\nTICK: %d\n" RESET, arg->sc->tick);

    printf("Standing customer/s:\n");
    print_queue(arg->standing);

    printf("Seated customer/s:\n");
    print_queue(arg->seated);

    printf("Waiting Order customer/s:\n");
    print_queue(arg->waiting_order);
*/
    printf(BOLD_U "\nTICK: %d" RESET, arg->sc->tick);
    printf(BOLD_U "\tSCORE: %f\n" RESET, score);

    pthread_mutex_unlock(arg->print);
  }

  return NULL;
}

int main(int argc, char* argv[]){
  printf(BOLD_U "\nC MAIN BINARY STARTING!\n" RESET);

  // check if sufficient numer of argument is passed
  if(argc < 8) {
    perror("Too few arguments passed while launching main binary!\n");
    return 1;
  }

  // handles the error in the arguments
  for(int i = 0; i < argc; i++) {
    switch(atoi(argv[i])) {
      case INVALID_PARAMETER:
        perror("Invalid parameter passed!\n");
        return 1;
        break;

      case INVALID_FILE:
        perror("Invalid file passed!\n");
        return 1;
        break;
    }
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

  pthread_mutex_t print = PTHREAD_MUTEX_INITIALIZER;

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
  CustomerQueue* waiting_order = malloc(sizeof(CustomerQueue));
  OrderManager* om = malloc(sizeof(OrderManager));
  //init structs
  make_tools(RESOURCE_FILE, km, 10);
  make_menu(MENU_FILE, menu, 20, 4);
  om_init(om);
  queue_init(standing);
  queue_init(seated);
  clock_init(sc);
  srand(RANDOM_SEED);
  //printf("Init Successful!\n");

  // if nothing (in the init steps) fails, running is true
  *running = true;

  pthread_t clock, info;
  pthread_t cooks_tid[NUM_COOKS];
  pthread_t waiters_tid[NUM_WAITERS];
  pthread_t customer_thread_manager;

  pthread_create(&clock, NULL, tick_advance, sc);

  InfoArgs* info_args = malloc(sizeof(InfoArgs));

  info_args->print = &print;
  info_args->sc = sc;
  info_args->seated = seated;
  info_args->standing = standing;
  info_args->waiting_order = waiting_order;
  pthread_create(&info, NULL, info_thread, info_args);

  CookArgs* cook_args = malloc(NUM_COOKS * sizeof(CookArgs));

  for(int i = 0; i < NUM_COOKS; i++) {
    cook_args[i].id = i + 1;
    cook_args[i].km = km;
    cook_args[i].om = om;
    cook_args[i].running = running;
    cook_args[i].sc = sc;
    cook_args[i].print = &print;
    pthread_create(&cooks_tid[i], NULL, cook_thread, &cook_args[i]);
  }

  WaiterArgs* waiter_args = malloc(NUM_WAITERS * sizeof(WaiterArgs));

  for(int i = 0; i < NUM_WAITERS; i++) {
    waiter_args[i].id = i + 1;
    waiter_args[i].ea_bin = &ea_bin;
    waiter_args[i].om = om;
    waiter_args[i].running = running;
    waiter_args[i].sc = sc;
    waiter_args[i].seated = seated;
    waiter_args[i].standing = standing;
    waiter_args[i].waiting_order = waiting_order;
    waiter_args[i].rc = &restaurant_capacity;
    waiter_args[i].print = &print;
    pthread_create(&waiters_tid[i], NULL, waiter_thread, &waiter_args[i]);
  }

  CustomerArgs* customer_args = malloc(sizeof(CustomerArgs));
  customer_args->id = 0;
  customer_args->menu = menu;
  customer_args->print = &print;
  customer_args->rc = &restaurant_capacity;
  customer_args->running = running;
  customer_args->sc = sc;
  customer_args->score = &score;
  customer_args->standing = standing;

  // thread_manager manages all customer threads
  pthread_create(&customer_thread_manager, NULL, thread_manager, customer_args);

  // Missing pthread_join for cooks and waiters
  for(int i = 0; i < NUM_COOKS; i++) {
    pthread_join(cooks_tid[i], NULL);
  }

  for(int i = 0; i < NUM_WAITERS; i++) {
    pthread_join(waiters_tid[i], NULL);
  }

  // destroy the simulation clock
  clock_destroy(sc);

  // destroy the semaphore
  sem_destroy(&restaurant_capacity);
  sem_destroy(&ea_bin);

  printf("Semaphores destroyed");
  return 0;
}
