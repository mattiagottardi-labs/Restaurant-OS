#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "clock.h"

void tick_advance(sim_clock* sim) {
    pthread_mutex_lock(&sim->lock);
    sim->tick++;
    pthread_cond_broadcast(&sim->tick_cv);  // wake all waiting threads
    pthread_mutex_unlock(&sim->lock);
}