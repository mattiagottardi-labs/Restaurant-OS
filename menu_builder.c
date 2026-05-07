#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

menu Menu;//contains all dishes
tool_pool** kitchen;

typedef struct tool_pool{
    bool available;
    int in_use;
    int quantity;
    char* name;
    tool* tools;
}tool_pool;

typedef struct tool{
    char* name;
    int quantity;
    int clean_time;
    int dirty_usages;
}tool;

typedef struct dish{
    char* name;
    int price;
    int time;
    char* tools; //useless to save the entire tool or to point to tools directly in the dish
}dish;

typedef struct menu{
    dish** selection;
    int num_dishes;
}menu;



int get_quantity(char* tool_name, FILE* tools_csv){
    char line[1024];
    fgets(line, sizeof(line), tools_csv); //skip first line
    while(fgets(line, sizeof(line), tools_csv)){
        line[strcspn(line, "\n")] = 0;
        if(strcmp(strtok(line, ","), tool_name) == 0){
            return atoi(strtok(NULL, ","));
        }
    }
    fclose(tools_csv);
}

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
    fclose(tools_csv);
}

tool*ctot(char* arg, FILE* tools_csv){ //char to tool
    tool temp;
    temp.name = NULL;
    if (!arg) return &temp;
    temp.name = arg;
    temp.quantity = get_quantity(arg, tools_csv);
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
    while(fgets(line, sizeof(line), tools_csv)){
        line[strcspn(line, "\n")] = 0;
        char* name = strtok(line, ',');
        kitchen[j]->name = name;
        kitchen[j]->quantity = atoi(strtok(line, ','));
        for(int i = 0; i < kitchen[j]->quantity; i++){
            kitchen[j]->tools = ctot(name, tools_csv);
        }
        kitchen[j]->available = true;
        kitchen[j]->in_use = 0;
    }
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