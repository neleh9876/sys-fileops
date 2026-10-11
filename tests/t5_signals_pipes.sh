#!/bin/bash

set -e

echo "Inceput test 5"

TEST_DIR="t5_test_tree"
mkdir -p "$TEST_DIR"
for i in {1..50}; do
    mkdir -p "$TEST_DIR/subdir_$i"
    dd if=/dev/urandom of="$TEST_DIR/subdir_$i/file_$i.bin" bs=1024 count=1 2>/dev/null
done

DB_FILE="data/inventory_t5.db"
IPC_FILE="data/ipc_t5.mmap"
PID_FILE="manager_t5.pid"

rm -f "$DB_FILE" "$IPC_FILE" "$PID_FILE"


./bin/fileops_manager --root "$TEST_DIR" --workers 2 --ipc "$IPC_FILE" --db "$DB_FILE" --simulate-work-ms 50 --pid-file "$PID_FILE" > manager_output.log 2>&1 &
BASH_JOB_PID=$!

while [ ! -f "$PID_FILE" ]; do
    sleep 0.1
done
REAL_PID=$(cat "$PID_FILE")
echo "PID-ul real: $REAL_PID"

sleep 1 

echo "SIGUSR1"
kill -USR1 "$REAL_PID"

sleep 1 

echo "SIGTERM"
kill -TERM "$REAL_PID"

wait $BASH_JOB_PID || true

if pgrep -f "fileops_worker" > /dev/null; then
    echo "EROARE: au ramas workeri"
    pkill -f "fileops_worker"
    exit 1
else
    echo "Succes: nu exista workeri ramasi"
fi

echo "Verif baza de date"
./bin/fileops_manager --db "$DB_FILE" --verify

echo "Verif complete=0"
DUMP_OUT=$(./bin/fileops_manager --db "$DB_FILE" --dump)
if echo "$DUMP_OUT" | grep -q "complete=0"; then
    echo "Succes: Baza de date are complete=0."
else
    echo "EROARE: Flag-ul complete nu este 0! Dump output:"
    echo "$DUMP_OUT"
    exit 1
fi

if grep -q "STATUS" manager_output.log; then
    echo "Succes: STATUS la SIGUSR1."
else
    echo "EROARE: fara STATUS in output"
    cat manager_output.log
    exit 1
fi

echo "Testul 5 finalizat"

rm -rf "$TEST_DIR" "$DB_FILE" "$IPC_FILE" "$PID_FILE" manager_output.log
