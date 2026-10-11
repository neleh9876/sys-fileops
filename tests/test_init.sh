#!/bin/bash

bash tools/fileops.sh init

dirs=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")

for dir in "${dirs[@]}"; do
    if [[ ! -d "$dir" ]]; then
        echo "FAIL: Directorul $dir lipsește."
        exit 1
    fi
done

echo "PASS"
exit 0
