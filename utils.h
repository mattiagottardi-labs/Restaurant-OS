#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <semaphore.h>

// ─── ANSI Color Codes ────────────────────────────────────────────────────────

#define RED             "\033[31m"
#define RED_BOLD        "\033[1;31m"
#define ORANGE          "\033[38;5;208m"
#define YELLOW          "\033[33m"
#define YELLOW_BOLD     "\033[1;33m"
#define YELLOW_BOLD_U   "\033[1;4;33m"
#define GREEN           "\033[32m"
#define GREEN_BOLD      "\033[1;32m"
#define BLUE            "\033[34m"
#define CYAN            "\033[36m"
#define MAGENTA         "\033[35m"
#define PURPLE          "\033[38;5;141m"
#define WHITE           "\033[37m"
#define GRAY            "\033[38;5;244m"
#define RESET           "\033[0m"
#define BOLD            "\033[1;37m"
#define UND             "\033[4;37m"
#define BOLD_U          "\033[1;4m"

// ─── Forward Declarations & Types ───────────────────────────────────────────

typedef struct Order Order;
typedef struct Dish Dish;
typedef struct Customer Customer;
typedef struct Cook Cook;
typedef struct CustomerQueue CustomerQueue;
typedef struct KitchenManager KitchenManager;
typedef struct OrderManager OrderManager;
typedef struct WaiterArgs WaiterArgs;
typedef struct CustomerArgs CustomerArgs;
typedef struct CookArgs CookArgs;

typedef enum Casting {
    DISH_LIST,
    ORDER_LIST,
    CUSTOMER_QUEUE
} Casting;

// ─── Data Structures ─────────────────────────────────────────────────────────

typedef struct DishListNode {
    Dish*                 d;
    int                   prio;
    struct DishListNode*  next;
} DishListNode;

typedef struct DishList {
    DishListNode*         head;
    int                   size;
    pthread_mutex_t       lock;
} DishList;

typedef struct OrderListNode {
    Order*                o;
    int                   prio;
    struct OrderListNode* next;
} OrderListNode;

typedef struct OrderList {
    OrderListNode*        head;
    int                   size;
    pthread_mutex_t       lock;
} OrderList;

// ─── Simulation Clock ────────────────────────────────────────────────────────

typedef struct SimClock {
    int             tick;
    pthread_mutex_t lock;
    pthread_cond_t  tick_cv;
} SimClock;

typedef struct ClockThreadArgs {
    SimClock* clock;
    int       running;
    unsigned  tick_ms;
} ClockThreadArgs;

// ─── Cleaning Arguments Structure ────────────────────────────────────────────

typedef struct CleaningArgs {
    _Atomic bool     running;
    CustomerQueue*   standing;
    CustomerQueue*   seated;
    CustomerQueue*   waiting_order;
    pthread_mutex_t* print;
    KitchenManager*  km;
    OrderManager*    om;
    WaiterArgs*      waita;
    CustomerArgs*    custa;
    CookArgs*        cooka;
    CustomerQueue*   finished;
} CleaningArgs;

// ─── Simulation Clock Prototypes ─────────────────────────────────────────────

void  clock_init(SimClock* sc);
void  clock_destroy(SimClock* sc);
void* tick_advance(void* args);

// ─── Random & Atomic Utility Prototypes ───────────────────────────────────────

extern pthread_mutex_t rand_mutex;

void atomic_float_sub(_Atomic float* target, float value);
void seed_init(int seed);
int  safe_rand(void);
int  safe_rand_range(int max);

// ─── Destruction & Memory Cleanup Prototypes ──────────────────────────────────

void* destructor(void* ptr, Casting cast);
void  customer_queue_destroy(CustomerQueue* queue);
void  kitchen_manager_destroy(KitchenManager* km);
void  destroy_customer(Customer* customer);
void  cook_destroy(Cook* cook);
void  dish_list_destroy(DishList* list);
void  order_list_destroy(OrderList* list);
void  order_manager_destroy(OrderManager* om);
void  clean_memory(CleaningArgs* args);
void customer_queue_destroy_extended(CustomerQueue* queue);

#endif // UTILS_H
