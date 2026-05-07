#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <kitchen.h>

int get_clean_time(char* tool_name, FILE* tools_csv){
    char line[1024];
    fgets(line, sizeof(line), tools_csv); //skip first line
    while(fgets(line, sizeof(line), tools_csv)){
        line[strcspn(line, "\n")] = 0;
        if(strcmp(strtok(line, ","), tool_name) == 0){
            strtok(NULL, ",");
            return atoi(strtok(NULL, ","));
        }
    }
}

tool*ctot(char* arg, FILE* tools_csv){ //char to tool
    tool temp;
    temp.name = NULL;
    if (!arg) return &temp;
    temp.name = arg;
    temp.clean_time = get_clean_time(arg, tools_csv);
    return &temp;
}

void make_tools( char* tools_location, tool_pool** kitchen, int max_tools){
    FILE* tools_csv = fopen(tools_location, "r");
    if(!tools_csv){
        printf("file not found\n");
        exit(1);
    }
    kitchen = malloc(max_tools* sizeof(tool_pool));
    char line[1024];
    int j = 0;
    fgets(line, sizeof(line), tools_csv); // skip first line
    while (fgets(line, sizeof(line), tools_csv) && j < max_tools) {
        line[strcspn(line, "\n")] = 0;
        
        kitchen[j] = malloc(sizeof(tool_pool));
        char* name = strtok(line, ",");
        int qty = atoi(strtok(NULL, ","));
        int c_time = atoi(strtok(NULL, ","));

        kitchen[j]->name = strdup(name);
        kitchen[j]->quantity = qty;
        kitchen[j]->in_use = 0;
        kitchen[j]->available = true;
        
        // Allocate the individual tools in the pool
        kitchen[j]->tools = malloc(qty * sizeof(tool));
        for (int i = 0; i < qty; i++) {
            kitchen[j]->tools[i].name = strdup(name);
            kitchen[j]->tools[i].clean_time = c_time;
            kitchen[j]->tools[i].dirty_usages = 0;
        }
        j++;
    }
    fclose(tools_csv);
}

void make_menu(char* menu_location, menu* Menu, int max_dishes){
    FILE *menu_csv = fopen(menu_location, "r");

    if(!menu_csv){ 
        perror("file cannot be opened");
        return;
    }
    char line[1024];
    fgets(line, sizeof(line), menu_csv); // skip first line
    int j = 0;
    Menu->selection = malloc(max_dishes * sizeof(dish*));
    while(fgets(line, sizeof(line), menu_csv) && j < max_dishes){
        line[strcspn(line, "\n")] = 0; //insert null terminator at end of line
        dish* d = malloc(sizeof(dish));
        d->name = strdup(strtok(line, ","));
        d->price = atoi(strtok(NULL, ","));
        d->time = atoi(strtok(NULL, ","));
        char* tools_field = strtok(NULL, ","); // get whole tools field e.g. "pan;burner"
        int i = 0;
        char* tool_field = strtok(tools_field, ";"); // split tools on ;
        while(tool_field && i < 4){
            if(strchr(tool_field, ':') != NULL){
                char* p = strchr(tool_field, ':');
                int n = atoi(p+1);
                size_t len = p - tool_field;     // number of chars before '/'
                char *before = malloc(len + 1);
                memcpy(before, tool_field, len);
                before[len] = '\0';
                for(int k = 0; k < n ; k++){
                    d->tools[i] = before;
                    i++;
                }
                free(before);
            }else{
                d->tools[i] = tool_field;
                i++;
                tool_field = strtok(NULL, ";");
            }
        }
        Menu->selection[j] = d;
        j++;
    }
    Menu->num_dishes = j;
    fclose(menu_csv);
}