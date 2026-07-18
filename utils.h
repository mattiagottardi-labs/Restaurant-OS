#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define RED     "\033[31m"
#define ORANGE  "\033[38;5;208m"
#define YELLOW  "\033[33m"
#define GREEN   "\033[32m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define MAGENTA "\033[35m"
#define PURPLE  "\033[38;5;141m"
#define WHITE   "\033[37m"
#define GRAY    "\033[38;5;244m"
#define RESET   "\033[0m"

typedef struct Order Order;

typedef enum Casting {
    ORDER_LIST,
    CUSTOMER_QUEUE,
} Casting;

typedef struct ListNode {
    Order*           o;
    int              prio;
    struct ListNode* next;
} ListNode;

typedef struct OrderList {
    ListNode*       head;
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

void clock_init(SimClock* sim);
void clock_destroy(SimClock* sim);
void* tick_advance(void* args);

// ─── random ──────────────────────────────────────────────────────────────────

extern pthread_mutex_t rand_mutex;

void     seed_init(int seed);
int      safe_rand(void);
int      safe_rand_range(int max);

void*   destructor(void* ptr, Casting cast);
#endif