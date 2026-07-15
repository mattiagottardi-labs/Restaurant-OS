#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
//#include "customer.h"

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
void tick_advance(SimClock* sim);

// ─── random ──────────────────────────────────────────────────────────────────

extern pthread_mutex_t rand_mutex;

void     seed_init(int seed);
int      safe_rand(void);
int      safe_rand_range(int max);
#endif