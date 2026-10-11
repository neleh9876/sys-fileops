#!/bin/bash

export CFLAGS="-Wall -Wextra -O2"
export LDFLAGS="-lpthread -lssl -lcrypto"

timestamp_start=$(date +%s)
datetime_string=$(date +%Y%m%d_%H%M%S)
logName="logs/fileops_${datetime_string}.log"

finish_log() {
    local exit_code=$1
    local subcommand=$2
    local timestamp_end=$(date +%s)
    local duration=$((timestamp_end - timestamp_start))
    local start_readable=$(date -d "@$timestamp_start" "+%Y-%m-%d %H:%M:%S")
    local end_readable=$(date -d "@$timestamp_end" "+%Y-%m-%d %H:%M:%S")
    
    mkdir -p logs/
    echo "Subcomanda: $subcommand" > "$logName"
    echo "Start: $start_readable" >> "$logName"
    echo "End: $end_readable" >> "$logName"
    echo "Durata: ${duration}s" >> "$logName"
    echo "Exit code: $exit_code" >> "$logName"
    exit $exit_code
}

init() {
    mkdir -p bin/ src/ include/ data/ logs/ reports/ tmp/obj/ tests/ doc/ tools/ 

    if [[ ! -x "/usr/bin/gcc" ]] && ! command -v gcc &> /dev/null; then
        echo "Nu ai instalat gcc" 
        finish_log 1 "init"
    fi
}

cFiles=""
getCfilesRecursively() {
    if [[ ! -d "$1" ]]; then return; fi

    for file in "$1"*; do
        if [[ -d "$file" ]]; then
            local next_dir="$file"
            [[ "$next_dir" != */ ]] && next_dir="$next_dir/"
            getCfilesRecursively "$next_dir"
        elif [[ "$file" == *.c ]]; then
            cFiles+="$file "
        fi
    done
}

build() {
    local srcDir="src/" 
    if [[ "$1" == "--src" ]]; then
        srcDir="$2"
        [[ "$srcDir" != */ ]] && srcDir="$srcDir/"
        shift 2
    fi

    mkdir -p tmp/obj bin 

    cFiles=""
    getCfilesRecursively "$srcDir" 

    if [[ -z "$cFiles" ]]; then
        echo "Nu s-au găsit fișiere .c în $srcDir"
        return
    fi

    local main_objs=""
    local aux_objs=""

    for file in $cFiles; do 
        local filename=$(basename "$file")
        local objFile="tmp/obj/${filename%.c}.o"

        if [[ ! -e "$objFile" ]] || [[ "$file" -nt "$objFile" ]]; then 
            gcc $CFLAGS -Iinclude/ -c -o "$objFile" "$file" || finish_log 1 "build"
        fi    
        
        if [[ "$filename" == main_* ]]; then
            main_objs+="$objFile "
        else 
            aux_objs+="$objFile "
        fi
    done


    for m_obj in $main_objs; do
        local exe_name=$(basename "$m_obj")
        exe_name=${exe_name#main_} 
        exe_name=${exe_name%.o}
        gcc -o "bin/$exe_name" "$m_obj" $aux_objs $LDFLAGS || finish_log 1 "build"
    done
}

run() {
    if [[ "$1" == "--" ]]; then
        shift
    fi

    local exe=$1 
    shift
    
    if [[ -f "bin/$exe" ]]; then
        "./bin/$exe" "$@"
    else
        echo "Executabil negasit: bin/$exe"
        exit 1
    fi
}

clean() {
    rm -f tmp/obj/*.o bin/* 
}

shFiles=""

search_local() {
    if [[ -d "$1" ]]; then
        search_branch "$1/"
    elif [[ "$1" == *.sh ]]; then
        shFiles+="$1 "
    fi
}

search_branch() {
    for file in "$1"*; do
        search_local "$file"
    done
}

test_cmd() {
    local allPass=1
    echo "" > reports/T2_tests.txt 

    shFiles=""
    search_branch "tests/" 
    
    for file in $shFiles; do
        bash "$file"
        if [[ $? != 0 ]]; then
            allPass=0
            echo "FAIL" >> reports/T2_tests.txt 
        else
            echo "PASS" >> reports/T2_tests.txt 
        fi
    done
    
    if [[ $allPass == 0 ]]; then 
        finish_log 1 "test"
    fi
}

rulat="$1"

case "$rulat" in
    init) init ;;
    build) shift; build "$@" ;;
    run) shift; run "$@" ;;
    clean) clean ;;
    test) test_cmd ;;
    *) echo "Comanda gresita"; finish_log 1 "error" ;;
esac

finish_log 0 "$rulat"




