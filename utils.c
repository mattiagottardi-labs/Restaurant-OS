#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

// ─── simulation clock ────────────────────────────────────────────────────────

void clock_init(sim_clock* sim) {
    sim->tick = 0;
    pthread_mutex_init(&sim->lock, NULL);
    pthread_cond_init(&sim->tick_cv, NULL);
}

void clock_destroy(sim_clock* sim) {
    pthread_mutex_destroy(&sim->lock);
    pthread_cond_destroy(&sim->tick_cv);
}

void tick_advance(sim_clock* sim) {
    pthread_mutex_lock(&sim->lock);
    printf("ticking %d", sim->tick);
    sim->tick++;
    pthread_cond_broadcast(&sim->tick_cv);
    pthread_mutex_unlock(&sim->lock);
}



// ─── random ──────────────────────────────────────────────────────────────────

pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;

int safe_rand(void) {
    pthread_mutex_lock(&rand_mutex);
    int r = rand();
    pthread_mutex_unlock(&rand_mutex);
    return r;
}

int safe_rand_range(int max) {
    pthread_mutex_lock(&rand_mutex);
    int r = rand() % (max-1);
    r++;
    pthread_mutex_unlock(&rand_mutex);
    return r;
}
