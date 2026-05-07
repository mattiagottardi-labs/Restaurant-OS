#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "customer.h"
#include "menu_builder.h"

static pthread_mutex_t sink_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sink_cond = PTHREAD_COND_INITIALIZER;

dish* cook_loop(dish* d){
    if(d == NULL){
        printf("No dish to cook\n");
        return 0;
    }

}

void* sink_cleaning(tool* t){
    pthread_mutex_lock(&sink_mutex);
    usleep(t->clean_time / GAME_SPEED);
    t->dirty_usages = 0; //set this parameter to 0 to indicate tool is clean
    printf("Cleaned %s\n", t->name);
    pthread_mutex_unlock(&sink_mutex);
    printf("Sink available\n");
}