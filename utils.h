#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

// ─── simulation clock ────────────────────────────────────────────────────────

typedef struct sim_clock {
    int             tick;
    pthread_mutex_t lock;
    pthread_cond_t  tick_cv;
} sim_clock;

typedef struct {
    sim_clock* clock;
    int running;
    unsigned tick_ms;
} clock_thread_args;

void clock_init(sim_clock* sim);
void clock_destroy(sim_clock* sim);
void tick_advance(sim_clock* sim);

// ─── random ──────────────────────────────────────────────────────────────────

extern pthread_mutex_t rand_mutex;

void     seed_init(int seed);
int      safe_rand(void);
int      safe_rand_range(int max);

#endif
