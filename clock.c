#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "clock.h"

void clock_run(int* time, int stop){
    for(int i = 0; i < stop; i++){ //runs for a max of 1000s
        sleep(1);
        time++;
    }
    return;
}