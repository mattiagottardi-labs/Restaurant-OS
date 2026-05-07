#include<stdlib.h>
#include<pthread.h>
#include<stdbool.h>
#include "customer.h"
#include "kitchen.h"
#include "clock.h"
#include<math.h>

#define GAME_SPEED 1

static pthread_mutex_t sink_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for sink access





/*dish* cook_loop(dish* d, order* o){
    if(d == NULL){
        printf("No dish to cook\n");
        return 0;
    }

    for(int i = 0; i < d->num_tools_required; i++){
        if(is_tool_available(d->tools) && !is_tool_dirty(d->tools)){ //tool available and clean
            //use tool
            printf("Tool %s used for %s, available\n", d->tools, d->name);
        }
    }else if(is_tool_available(d->tools) && is_tool_dirty(d->tools)){ //tool available but dirty
     //I need to understand if I should clean/use the tool dirty or if I should skip the client
        if(o->patience > (t->clean_time + d->time)){ //clean the tool and the cook the dish
            printf("Cleaning %s before cooking %s\n", d->tools, d->name);
            sink_cleaning(d->tools);
            printf("Tool %s used for  %s, available\n", d->tools, d->name);
        }else{ //I need to understand if it's worth for the restaurant cooking with dirty tools or skipping the client
            if(){
                //DA ANDARE AVANTI!!!
            }
        }

    }else{ //tool not available --> I need to search for another dish to make until I wait for the tool to be available
        printf("Tool not available, searching for another dish to cook\n");
        return NULL;
    }
    return d;  
}*/

bool is_tool_available(char* tool_name, kitchen_manager* kitchen){ //cook reads tool name as string from dish, not directly as tool.
    int i = 0;
    while(kitchen->pools[i] != NULL){
        if(strcmp(kitchen->pools[i]->name, tool_name) == 0){ 
            if(kitchen->pools[i]->available){
                return true;
            }else{
                return false;
            }
        }
        i++;
    }
    printf("ERROR: tool not found in kitchen\n");
    return false;
}

bool is_tool_dirty(tool* t){
    if(t->dirty_usages > 0) return true;
    else return false;
}

void* sink_cleaning(tool* t){
    pthread_mutex_lock(&sink_mutex);
    usleep(t->clean_time / GAME_SPEED);
    t->dirty_usages = 0; //set this parameter to 0 to indicate tool is clean
    printf("Cleaned %s\n", t->name);
    pthread_mutex_unlock(&sink_mutex);
    printf("Sink available\n");
}


/*bool is_worth_cooking(dish* d, order* o){
    float customer_served = (get_order_price(o)*(1 - (current_time - o->order_time)/o->patience)); //we need the time to serve in the ()
    float customer_skipped = (get_order_price(o)*log2(1+(o->patience)/(1 + number_of_dishes_served)));
    float cooked_dirty = 2^()
}*/
//NOT A FUNCTION FOR COOK, THE CUSTOMER UPDATES SCOORE AS HE GETS THE ORDER DELIVERED TO HIM

