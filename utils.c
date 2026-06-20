#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

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