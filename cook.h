#ifndef COOK_H
#define COOK_H
#include <pthread.h>
#include <stdatomic.h>
#include "kitchen.h"
#include "waiter.h"
#include "utils.h"

#define DIRTY_THRESHOLD 3

typedef enum CookState {
    WAITING,
    SELECT_DISH,
    ACQUIRE_TOOL,
    COOKING,
    CLEANING
} CookState;

// cook arguments structure passed to the thread
typedef struct CookArgs {
    OrderManager*   om;
    SimClock*       sc;
    KitchenManager* km;
    bool*           running;
} CookArgs;

typedef struct Cook {
    CookArgs* arg;
    Dish*     target_dish;
    CookState present;
    CookState future;
} Cook;


// Tool helpers
int        count_tools(Dish* d);
ToolPool*  find_pool(const char* name, KitchenManager* km);
Tool*      acquire_pool(ToolPool* pool);
void       release_pool(ToolPool* pool, Tool* t, SimClock* sc, KitchenManager* km);
Tool**     acquire_tools(Dish* d, KitchenManager* km);
void       release_tools(Tool** used, Dish* d, KitchenManager* km, SimClock* sc);

// Order/Dish selection
Order* get_next_order(OrderManager* m);
Dish*  pick_dish(Order* o);

// cook lifecycle
void    cook_dish(Dish* d, Order* o, OrderManager* om, SimClock* sc, KitchenManager* km, bool* running);
void    cook_loop(Cook* ck);
float   get_pressure(OrderList* l); //will estimate how hard the kitchen must work
void*   cook_thread(void* arg);
#endif