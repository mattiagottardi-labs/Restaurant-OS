#!/bin/bash

mode=$1

file=.env
required_index=0
lower_bound=1
upper_bound=10
max_customers_upper_bound=20
total_customers_upper_bound=99
missing_arg_flag=0

# save all the names in order to check if there are really the needed parameters (there could be repetitions of arguments too fool the code)
required_args=("NUM_COOKS" "NUM_WAITERS" "MAX_CUSTOMERS" "TOTAL_CUSTOMERS" "GAME_SPEED" "RANDOM_SEED" "MENU_FILE" "RESOURCE_FILE")

# creates associative array (dictionary)
declare -A arg_dict

parse_file() {
    # esnure all line are read even the last one
    while read -r line || [[ -n $line ]]; do

        # get the argument name
        arg_name="${line%%=*}"

        # loop through all elements in the required_args array
        while [[ $arg_name != ${required_args[$required_index]} && $required_index -le 7 ]]; do
            # increment the value if no match is found
            ((required_index++))
        done

        # if all the array was traversed and no match found, signal invalid argument
        if [[ $required_index -gt 7 ]]; then
            echo "Arguemnt: $arg_name is invalid"
            missing_arg_flag=1
        else
            # drop element from array
            unset "dropped_required_args[$required_index]"

            # reindex the array, since unset leaves a hole in the array
            dropped_required_args=("${dropped_required_args[@]}")

            if [[ $line =~ [0-9]+ ]]; then
                # use parameter expansion: everything dropped up to = sign
                arg_number="${line#*=}"

                # check if the number read is positive and in a reasonable range
                if [[ $arg_number -ge $lower_bound ]]; then
                    if [[ $arg_name = "MAX_CUSTOMERS" && $arg_number -le max_customers_upper_bound ]]; then
                        arg_dict[$arg_name]=$arg_number
                    elif [[ $arg_name = "TOTAL_CUSTOMERS" && $arg_number -le total_customers_upper_bound ]]; then
                        arg_dict[$arg_name]=$arg_number
                    elif [[ $arg_number -le upper_bound ]]; then
                        arg_dict[$arg_name]=$arg_number
                    else
                        echo "Number: $arg_number too large!"
                    fi
                else
                    echo "Number: $arg_number out of range!"
                    arg_dict[$arg_name]=-1
                fi
            else
                arg_string="${line#*=}"
                #echo "NaN argument: $arg_string"
                #echo "Name: $arg_name"
                arg_dict[$arg_name]=$arg_string
            fi
            echo -e "$arg_name\t=\t${arg_dict[$arg_name]}"
        fi

        dropped_required_args=("${required_args[@]}")

        required_index=0
    done < $file

    if [[ $missing_arg_flag -eq 1 ]]; then
        echo "Argument/s missing in the $file file"
    fi
}

check_file() {
    if [[ -e $file && -r $file ]]; then
        echo "File $file exists and is readable"

        #-----------------------------------------------------------------------------------------------------------------------#

        # 2) read all the lines in a loop from $file
        parse_file

    else
        echo "File $file is not existing/readable. Skipping al "
    fi

    echo ""
}

#-----------------------------------------------------------------------------------------------------------------------#

# 1) check if file exists and is readable

check_file

#-----------------------------------------------------------------------------------------------------------------------#

# 3) check for arguemnts and if needed override them
for arg in "$@"; do
    case $arg in
        --env-file=*)
            file="${arg#*=}"
            check_file
            echo "$file overrode .env arguments"
            ;;

        --num-cooks=*)
            arg_dict["NUM_COOKS"]="${arg#*=}"
            ;;

        --num-waiters=*)
            arg_dict["NUM_WAITERS"]="${arg#*=}"
            ;;

        --max-customers=*)
            arg_dict["MAX_CUSTOMERS"]="${arg#*=}"
            ;;

        --total-customers=*)
            arg_dict["TOTAL_CUSTOMERS"]="${arg#*=}"
            ;;

        --game-speed=*)
            arg_dict["GAME_SPEED"]="${arg#*=}"
            ;;

        --random-seed=*)
            arg_dict["RANDOM_SEED"]="${arg#*=}"
            ;;

        --menu-file=*)
            arg_dict["MENU_FILE"]="${arg#*=}"
            ;;

        --resource-file=*)
            arg_dict["RESOURCE_FILE"]="${arg#*=}"
            ;;

        *)
            echo "Invalid argument: $arg"
            ;;
    esac
done

#-----------------------------------------------------------------------------------------------------------------------#

# 4) print the final arguments that will be passed to the main binary
final_args=()

for j in {0..7}; do
    key="${required_args[$j]}"
    value="${arg_dict[$key]}"
    final_args+=("$value")
    echo -e "$key\t= $value"
done

if [[ $mode == "gdb" ]]; then
    gdb --args ./program "${final_args[@]}"
elif [[ $mode == "gdbt" ]]; then
    gdb -tui --args ./program "${final_args[@]}"
else
    ./program "${final_args[@]}"
fi