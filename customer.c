#include "menu_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
int main(){
    char* file_location = "/home/gottardimattia/Restaurant-OS/resources/2026-project-5/code/menu.csv";
    printf("opening file...\n");
    make_menu(file_location, &Menu, 20);
    printf("menu made, num_dishes: %d\n", Menu.num_dishes);
    for(int i = 0; i < Menu.num_dishes; i++){
        printf("dish %d: %s\n", i, Menu.selection[i]->name);
    }
    return 0;
}
