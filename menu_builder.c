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
    for(int i = 4; i < 7; i ++){ 
        if(strcmp(tool_name, tool_names[i])) return 2;
    }
    for(int i = 8; i > 7; i --){
         if(strcmp(tool_name, tool_names[i])) return 3;
    } //minimize comparisons by checking against the least populated classes
    return 1;
}

tool ctot(char* arg){ //char to tool
    tool temp;
    for(int i = 0; i < 9; i++){
        if(strcmp(arg, tool_names[i])){
            temp.name = tool_names[i];
            temp.clean_time = get_clean_time(tool_names[i]);
            break;
        }
    }
    return temp;
}

void make_menu(char* file_location, menu Menu, int max_dishes){
    FILE *fp = fopen(file_location, "r");
    if(!fp){ 
        perror("file cannot be opened");
        return;
    }
    char line[1024];
    fgets(line, sizeof(line), fp); // skip first line
    int j = 0;
    Menu.selection = malloc(max_dishes * sizeof(dish));
    while(fgets(line, sizeof(line), fp)){
        dish* d = malloc(sizeof(dish));
        char* field = strtok(line, ",");
        d->name = field;
        field = strtok(line, ",");
        d->price = atof(field);
        field = strtok(line, ","); 
        int i = 0;
        while(field){
            d->tools[i] = ctot(field); //char to tool
            strtok(line, ",");
            i++;
        }
        if(strchr(d->tools[i].name, ':') != NULL){ //accounts for burner:2 by doubling burner
                                              //resulting in pot, pan, burner, burner.
                d->tools[i+1] = d->tools[i];
        }
        Menu.selection[j] = d;
        j++;
    }

}
