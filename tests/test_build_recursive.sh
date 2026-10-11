#!/bin/bash

rm -rf tmp/scenariu_test_src
mkdir -p tmp/scenariu_test_src/app tmp/scenariu_test_src/lib/include

cat <<EOF > tmp/scenariu_test_src/lib/include/util.h
int util_add(int a, int b);
EOF

cat <<EOF > tmp/scenariu_test_src/lib/util.c
#include "include/util.h"
int util_add(int a, int b) { return a + b; }
EOF

cat <<EOF > tmp/scenariu_test_src/app/main_demo.c
#include <stdio.h>
#include "../lib/include/util.h"
int main() { printf("%d", util_add(2,3)); return 0; }
EOF

export CFLAGS="-Itmp/scenariu_test_src/lib"
bash tools/fileops.sh build --src tmp/scenariu_test_src

if [[ -x "bin/demo" ]] && [[ $(./bin/demo) == "5" ]]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
