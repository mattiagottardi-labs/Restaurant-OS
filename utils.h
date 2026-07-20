#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define RED     "\033[31m"
#define ORANGE  "\033[38;5;208m"
#define YELLOW  "\033[33m"
#define GREEN   "\033[32m"
#define GREEN_BOLD "\033[1;32m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define MAGENTA "\033[35m"
#define PURPLE  "\033[38;5;141m"
#define WHITE   "\033[37m"
#define GRAY    "\033[38;5;244m"
#define RESET   "\033[0m"
#define BOLD_U  "\033[1;4m"

typedef struct Order Order;
typedef struct Dish Dish;

typedef enum Casting {
    DISH_LIST,
    ORDER_LIST,
    CUSTOMER_QUEUE,
} Casting;

typedef struct DishListNode {
    Dish*            d;
    int              prio;
    struct DishListNode* next;
} DishListNode;

typedef struct DishList {
    DishListNode*   head;
    int             size;
    pthread_mutex_t lock;
} DishList;

typedef struct OrderListNode {
    Order*           o;
    int              prio;
    struct OrderListNode* next;
} OrderListNode;

typedef struct OrderList {
    OrderListNode*       head;
    int             size;
    pthread_mutex_t lock;
} OrderList;

// ─── simulation clock ────────────────────────────────────────────────────────

typedef struct SimClock {
    int             tick;
    pthread_mutex_t lock;
    pthread_cond_t  tick_cv;
} SimClock;

typedef struct ClockThreadArgs {
    SimClock*       clock;
    int             running;
    unsigned        tick_ms;
} ClockThreadArgs;

void clock_init(SimClock* sc);
void clock_destroy(SimClock* sc);
void* tick_advance(void* args);

// ─── random ──────────────────────────────────────────────────────────────────

extern pthread_mutex_t rand_mutex;

void     seed_init(int seed);
int      safe_rand(void);
int      safe_rand_range(int max);

void*   destructor(void* ptr, Casting cast);
#endif