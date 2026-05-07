#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "clock.h"

void run(){
    for(int i = 0; i < 1000; i++){ //runs for a max of 1000s
        sleep(1);
        current_time++;
    }
    return;
}