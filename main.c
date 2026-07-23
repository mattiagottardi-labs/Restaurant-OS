#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <signal.h>

#include "cook.h"
#include "utils.h"

#define INVALID_PARAMETER -1
#define INVALID_FILE -2
#define PID_PATH "/tmp/program.pid"

struct sigaction act;
volatile sig_atomic_t sigusr1_received = 0;

typedef struct InfoArgs {
    SimClock*           sc;
    _Atomic bool*       running;
    CustomerQueue*      standing;
    CustomerQueue*      seated;
    CustomerQueue*      waiting_order;
    pthread_mutex_t*    print;
    KitchenManager*     km;
} InfoArgs;

void print_tool_status(KitchenManager* km);
void queue_init(CustomerQueue* q);
void print_customer(Customer* C);
void print_queue(CustomerQueue* q);
void print_list(OrderList* ol);

int NUM_COOKS;
int NUM_WAITERS;
int MAX_CUSTOMERS;
int TOTAL_CUSTOMERS;
int GAME_SPEED;
int RANDOM_SEED;
char* MENU_FILE;
char* RESOURCE_FILE;

_Atomic bool running;
_Atomic float score = 0.0f;
_Atomic int spawned_customers = 0;
_Atomic int customers_in_restaurant = 0;
_Atomic int left_unserved = 0;

int MAX_CUSTOMER_SPAWN_RATE; // caution, this time is in microseconds
int TICK_PERIOD;

// customer_thread_manager implements this function
void* thread_manager(void* args) {
  if(!args) return NULL;

  CustomerArgs* arguments = (CustomerArgs*) args;

  pthread_t customer_tid[TOTAL_CUSTOMERS];
  CustomerArgs customer_args[TOTAL_CUSTOMERS];

  MAX_CUSTOMER_SPAWN_RATE = 100000000 / GAME_SPEED;

  // customer threads has to be spawned at random time
  int random_delay;

  // cycle that keeps running to manage customer threads
  for(spawned_customers = 0; spawned_customers < TOTAL_CUSTOMERS; spawned_customers++) {
    random_delay = ((rand() % MAX_CUSTOMER_SPAWN_RATE) + 50000) / GAME_SPEED;
    usleep(random_delay);

    customer_args[spawned_customers].id = spawned_customers + 1;
    customer_args[spawned_customers].menu = arguments->menu;
    customer_args[spawned_customers].rc = arguments->rc;
    customer_args[spawned_customers].running = arguments->running;
    customer_args[spawned_customers].print = arguments->print;
    customer_args[spawned_customers].sc = arguments->sc;
    customer_args[spawned_customers].score = arguments->score;
    customer_args[spawned_customers].standing = arguments->standing;
    customer_args[spawned_customers].finished = arguments->finished;
    customer_args[spawned_customers].left_unserved = arguments->left_unserved;
    customer_args[spawned_customers].customers_in_restaurant = arguments->customers_in_restaurant;
    pthread_create(&customer_tid[spawned_customers], NULL, customer_thread, (void*) &customer_args[spawned_customers]);
  }

  for(int i = 0; i < TOTAL_CUSTOMERS; i++) {
    pthread_join(customer_tid[i], NULL);
  }

  atomic_store(arguments->running, false); 
  printf(BOLD_U "\nLAST CUSTOMER LEFT SO RUNNING SET TO FALSE!\n" RESET);

  return NULL;
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
  OrderListNode* current = ol->head;
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
  while(atomic_load(&running)) {
    usleep(TICK_PERIOD);
    pthread_mutex_lock(&sc->lock);
    sc->tick++;
    printf(BOLD_U "\n TICK: %d                                                   \n" RESET, sc->tick);
    pthread_cond_broadcast(&sc->tick_cv);
    pthread_mutex_unlock(&sc->lock);
  }

  pthread_mutex_lock(&sc->lock);
  pthread_cond_broadcast(&sc->tick_cv);
  pthread_mutex_unlock(&sc->lock);

  return NULL;
}

// ────────────────────────────────────────────────────────

void* info_thread(void* args) {
  InfoArgs* arg = (InfoArgs*) args;

  while(atomic_load(arg->running)) {
    pthread_mutex_lock(&arg->sc->lock);
    pthread_cond_wait(&arg->sc->tick_cv, &arg->sc->lock);
    pthread_mutex_unlock(&arg->sc->lock);

    if(sigusr1_received) {
      pthread_mutex_lock(arg->print);

      printf(YELLOW_BOLD_U "\n TICK: %d                                                   \n" RESET, arg->sc->tick);
      printf(YELLOW_BOLD " Current Score" RESET ": %.3f\n", score);
      printf(YELLOW_BOLD " Customers in the restaurant" RESET ": %d\n", customers_in_restaurant);
      printf(YELLOW_BOLD " Customers that left unserved" RESET ": %d\n", left_unserved);
      printf(YELLOW_BOLD " Spawned/Total" RESET ": %d/%d\n", spawned_customers, TOTAL_CUSTOMERS);
      printf(YELLOW_BOLD " Length fixed" RESET ": 10\n");
      print_tool_status(arg->km);
      printf("\n");

      pthread_mutex_unlock(arg->print);

      sigusr1_received = 0;
    }
  }
  return NULL;
}

void handler(int signum) {
    //printf(YELLOW_BOLD "Caught %d, printing infos on screen\n" RESET, signum);
    sigusr1_received = 1;
} 

bool write_pid_file(const char* path) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if(fd == -1) {
    perror("open pid file");
    return false;
  }

  char buffer[16];
  int len = snprintf(buffer, sizeof(buffer), "%d\n", getpid());

  if(write(fd, buffer, len) == -1) {
    perror("write pid file");
    return false;
  }

  close(fd);
  return true;
}

int count_available_tools(ToolPool* pool) {
    pthread_mutex_lock(&pool->lock);
    int available = 0;
    for(int i = 0; i < pool->quantity; i++){
        if(!pool->tools[i].in_use) {
            available++;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return available;
}

void print_tool_status(KitchenManager* km) {
    printf(YELLOW_BOLD " Tools: \n" RESET);
    for(int i = 0; i < km->num_pools; i++) {
        int available = count_available_tools(km->pools[i]);
        printf(YELLOW "  %s" RESET ": %d items, %d in use, %d available\n" RESET, km->pools[i]->name, km->pools[i]->quantity, km->pools[i]->in_use, available);
    }
}

int main(int argc, char* argv[]){
  printf(BOLD_U "\nC MAIN BINARY STARTING!\n" RESET);

  if(!write_pid_file(PID_PATH)) {
    perror("Error while performing operations on the file!\n");
    return 1;
  }
  printf(GREEN "PID File wrote successfully!\n" RESET);

  act.sa_handler = handler;
  sigaction(SIGUSR1, &act, NULL);

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

  // period of time that a tick waits before ticking again
  TICK_PERIOD = 2500000 / GAME_SPEED;

  // mutex for printing on the screen
  pthread_mutex_t print = PTHREAD_MUTEX_INITIALIZER;

  sem_t restaurant_capacity, ea_bin;
  sem_init(&restaurant_capacity, 0, MAX_CUSTOMERS);
  sem_init(&ea_bin, 0, 1);

  //create structs
  KitchenManager* km = malloc(sizeof(KitchenManager));
  Menu* menu = malloc(sizeof(Menu));
  SimClock* sc = malloc(sizeof(SimClock));
  CustomerQueue* standing = malloc(sizeof(CustomerQueue));
  CustomerQueue* seated = malloc(sizeof(CustomerQueue));
  CustomerQueue* waiting_order = malloc(sizeof(CustomerQueue));
  OrderManager* om = malloc(sizeof(OrderManager));
  CustomerQueue* finished = malloc(sizeof(CustomerQueue));

  atomic_store(&running, true);

  //init structs
  make_tools(RESOURCE_FILE, km, 10);
  make_menu(MENU_FILE, menu, 20, 4);
  om_init(om);
  queue_init(standing);
  queue_init(seated);
  queue_init(waiting_order);
  queue_init(finished);
  clock_init(sc);
  srand(RANDOM_SEED);

  pthread_t clock, info;
  pthread_t cooks_tid[NUM_COOKS];
  pthread_t waiters_tid[NUM_WAITERS];
  pthread_t customer_thread_manager;

  pthread_create(&clock, NULL, tick_advance, sc);

  InfoArgs* info_args = malloc(sizeof(InfoArgs));
  info_args->print = &print;
  info_args->sc = sc;
  info_args->running = &running;
  info_args->seated = seated;
  info_args->standing = standing;
  info_args->waiting_order = waiting_order;
  info_args->km = km;
  pthread_create(&info, NULL, info_thread, info_args);

  CookArgs* cook_args = malloc(NUM_COOKS * sizeof(CookArgs));
  for(int i = 0; i < NUM_COOKS; i++) {
    cook_args[i].id = i + 1;
    cook_args[i].km = km;
    cook_args[i].om = om;
    cook_args[i].running = &running;
    cook_args[i].sc = sc;
    cook_args[i].print = &print;
    pthread_create(&cooks_tid[i], NULL, cook_thread, &cook_args[i]);
  }

  WaiterArgs* waiter_args = malloc(NUM_WAITERS * sizeof(WaiterArgs));
  for(int i = 0; i < NUM_WAITERS; i++) {
    waiter_args[i].id = i + 1;
    waiter_args[i].ea_bin = &ea_bin;
    waiter_args[i].om = om;
    waiter_args[i].running = &running;
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
  customer_args->running = &running;
  customer_args->sc = sc;
  customer_args->score = &score;
  customer_args->standing = standing;
  customer_args->finished = finished;
  customer_args->left_unserved = &left_unserved;
  customer_args->customers_in_restaurant = &customers_in_restaurant;
  // thread_manager manages all customer threads
  pthread_create(&customer_thread_manager, NULL, thread_manager, customer_args);

  printf("\nCustomer Thread Manager joined\n");
  pthread_join(customer_thread_manager, NULL);
  
  pthread_join(clock, NULL);
  printf("Clock joined\n");

  pthread_join(info, NULL);
  printf("Info joined\n");

  for(int i = 0; i < NUM_COOKS; i++) {
    pthread_join(cooks_tid[i], NULL);
  }
  printf(PURPLE "Cooks joined\n" RESET);

  for(int i = 0; i < NUM_WAITERS; i++) {
    pthread_join(waiters_tid[i], NULL);
  }
  printf(MAGENTA "Waiter joined\n" RESET);

  CleaningArgs cargs = {
        .standing = standing,
        .seated = seated,
        .finished = finished,
        .waiting_order = waiting_order,
        .om = om,
        .km = km,
        .cooka = cook_args,    
        .waita = waiter_args,  
        .custa = customer_args, 
        .print = &print
  };
  printf("All thread finished\n");

  // destroy the simulation clock
  clock_destroy(sc);

  // destroy the semaphore
  sem_destroy(&restaurant_capacity);
  sem_destroy(&ea_bin);
  clean_memory(&cargs);

  printf("Semaphores destroyed\n");
  return 0;
}
