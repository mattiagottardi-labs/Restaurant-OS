#ifndef CLOCK_H
#define CLOCK_H
#include <pthread.h>
typedef struct sim_clock {
    int             tick;
    pthread_mutex_t lock;
    pthread_cond_t  tick_cv;
} sim_clock;

void tick_advance(sim_clock* clock);

#endif