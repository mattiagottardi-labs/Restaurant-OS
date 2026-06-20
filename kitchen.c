#include "kitchen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* my_strdup(const char* s) {
    if (!s) return NULL;
    char* d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

void make_tools(const char* tools_location, kitchen_manager* my_kitchen,const int max_tools) {
    FILE* f = fopen(tools_location, "r");
    if (!f) {
        perror("Failed to open tools file");
        exit(1);
    }

    my_kitchen->pools = malloc(max_tools * sizeof(tool_pool*));
    char line[1024];
    int count = 0;

    fgets(line, sizeof(line), f); // Skip header
    while (fgets(line, sizeof(line), f) && count < max_tools) {
        line[strcspn(line, "\n")] = 0;

        char* name = strtok(line, ",");
        char* qty_str = strtok(NULL, ",");
        char* time_str = strtok(NULL, ",");

        if (name && qty_str && time_str) {
            tool_pool* pool = malloc(sizeof(tool_pool));
            pool->name = my_strdup(name);
            pool->quantity = atoi(qty_str);
            pool->in_use = 0;

            // Initialize individual tools within the pool
            pool->tools = malloc(pool->quantity * sizeof(tool));
            int clean_time = atoi(time_str);
            pthread_mutex_init(&pool->lock, NULL);
            pthread_cond_init(&pool->cv, NULL);
            for (int i = 0; i < pool->quantity; i++) {
                pool->tools[i].name = my_strdup(name);
                pool->tools[i].clean_time = clean_time;
                pool->tools[i].dirty_usages = 0;
                atomic_store(&pool->tools[i].in_use, false);
            }
            my_kitchen->pools[count++] = pool;
        }
    }
    my_kitchen->num_pools = count;
    fclose(f);
}

void make_menu(const char* menu_location, menu* Menu, const int max_dishes, const int max_tools_per_dish) {
    FILE *menu_csv = fopen(menu_location, "r");
    if(!menu_csv){ 
        perror("file cannot be opened");
        return;
    }

    char line[1024];
    fgets(line, sizeof(line), menu_csv); // skip header
    
    int j = 0;
    Menu->selection = malloc(max_dishes * sizeof(dish*));

    while(fgets(line, sizeof(line), menu_csv) && j < max_dishes){
        line[strcspn(line, "\n")] = 0; 
        
        dish* d = malloc(sizeof(dish));
        d->name = strdup(strtok(line, ","));
        d->price = atoi(strtok(NULL, ","));
        d->time = atoi(strtok(NULL, ","));
        atomic_store(&d->ready, false);
        atomic_store(&d->cooking, false);
        char* tools_field = strtok(NULL, ","); 
        d->tools = malloc((max_tools_per_dish+1)* sizeof(char*)); // Space for tool names (max 4)
        int i = 0;

        char* tool_field = strtok(tools_field, ";"); 
        while(tool_field && i < max_tools_per_dish){
            char* p = strchr(tool_field, ':');
            if(p != NULL){
                //:n case
                int n = atoi(p + 1);
                size_t len = p - tool_field; 
                char *before = malloc(len + 1);
                memcpy(before, tool_field, len);
                before[len] = '\0';

                for(int k = 0; k < n ; k++){
                    d->tools[i] = strdup(before);
                    i++;
                }
                free(before);
            } else {
                // Standard single tool
                d->tools[i] = strdup(tool_field);
                i++;
            }
            tool_field = strtok(NULL, ";");
        }
        d->tools[max_tools_per_dish] = NULL;
        Menu->selection[j] = d;
        j++;
    }
    Menu->num_dishes = j;
    fclose(menu_csv);
}