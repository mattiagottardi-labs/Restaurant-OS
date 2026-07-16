#!/bin/bash

# command is the argument passed when launching the script
command=$1

flags="-Wall -Wextra -lpthread -c"
name="program"

usage() {
    echo -e "How to build the code:\n./build.sh build num_cooks num_waiters max_customers game_speed random_seed\n"
    echo -e "How to clean the system:\n/build.sh clean\n"
    echo -e "How to run the code:\n/build.sh run\n"
}

# function to build the code
build() {
    echo "Building..."

    # command substitution that redirects output to variable
    output=$(gcc $flags *.c)
    output=$(gcc *.o -o $name)

    # check if the gcc exit code is 0: ok, 1: failed
    if [ $? -ne 0 ]; then
        echo "Build failed!"
        echo $output
    else
        echo "Build successful!"
        # command substitution $(). the output of the command is assigned to var executable
        echo "The executable is: $name"
    fi
}

# function to clean and restore the system
clean() {
    echo "Cleaning..."
    output=$(rm *.out)

    if [ $? -ne 0 ]; then
        echo "Cleaning failed!"
    else
        echo "Cleaning done!"
        echo $output
    fi
}

# function to execute scenarios
run() {
    echo "Running default scenario"
    ./bootstrap.sh
}

# case that choses the right function depending on the argument
case $command in
    usage)
        usage;;

    build)
        build;;

    clean)
        clean;;

    run)
        run;;

    *)
        echo "Command: $command not found!"
esac