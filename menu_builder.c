#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct tool{
    char* name;
    int clean_time;
}tool;

char* tool_names[9] = {"burner", "cutting_board", "knife","bowl", // 1 clean time
                        "pot", "pan","mixer", // 2 clean time
                        "oven", "grill"}; // 3 clean time 

typedef struct dish{
    char* name;
    double price;
    tool tools[4];
}dish;

typedef struct menu{
    dish** selection;
    int num_dishes;
}menu;

menu Menu;//contains all dishes

int get_clean_time(char* tool_name){
    if(tool_name == NULL) {
        perror("no name\n");
    }
    for(int i = 4; i < 7; i ++){ 
        if(strcmp(tool_name, tool_names[i]) == 0) return 2;
    }
    for(int i = 8; i > 6; i --){
        if(strcmp(tool_name, tool_names[i]) == 0) return 3;
    } //minimize comparisons by checking against the least populated classes
    return 1;
}

tool ctot(char* arg){ //char to tool
    tool temp;
    temp.name = NULL;
    for(int i = 0; i < 9; i++){
        if(strcmp(arg, tool_names[i]) == 0){
            temp.name = tool_names[i];
            temp.clean_time = get_clean_time(tool_names[i]);
            break;
        }
    }
    return temp;
}

void make_menu(char* file_location, menu* Menu, int max_dishes){
    FILE *fp = fopen(file_location, "r");
    if(!fp){ 
        perror("file cannot be opened");
        return;
    }
    char line[1024];
    fgets(line, sizeof(line), fp); // skip first line
    int j = 0;
    Menu->selection = malloc(max_dishes * sizeof(dish*));
    while(fgets(line, sizeof(line), fp) && j < max_dishes){
        line[strcspn(line, "\n")] = 0; //insert null terminator at end of line
        dish* d = malloc(sizeof(dish));
        d->name = strdup(strtok(line, ","));
        d->price = atof(strtok(NULL, ","));
        strtok(NULL, ","); // skip time column
        char* tools_field = strtok(NULL, ","); // get whole tools field e.g. "pan;burner"
        int i = 0;
        char* tool_field = strtok(tools_field, ";"); // split tools on ;
        while(tool_field && i < 4){
            d->tools[i] = ctot(tool_field); //char* to tool
            i++;
            tool_field = strtok(NULL, ";");
        }
        if(i > 0 && d->tools[i-1].name != NULL &&
           strchr(d->tools[i-1].name, ':') != NULL){ //accounts for burner:2 by doubling burner
                                                      //resulting in pot, pan, burner, burner.
            d->tools[i] = d->tools[i-1];
        }
        Menu->selection[j] = d;
        j++;
    }
    Menu->num_dishes = j;
    fclose(fp);
}
