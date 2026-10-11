#!/bin/bash
echo "Inceput test 4"
TEST_ROOT="data/test_root"
TEST_DB="data/inventory.db"
TEST_IPC="data/ipc.mmap"

rm -rf "$TEST_ROOT"
mkdir -p "$TEST_ROOT/folder_a"
mkdir -p "$TEST_ROOT/folder_b/subfolder"
mkdir -p reports

echo "continut 1" > "$TEST_ROOT/file1.txt"
echo "continut 2" > "$TEST_ROOT/folder_a/file2.txt"
echo "continut 3" > "$TEST_ROOT/folder_b/subfolder/file3.txt"

echo "DB test"

./tools/fileops.sh run -- fileops_manager --root "$TEST_ROOT" --workers 3 --db "$TEST_DB" --ipc "$TEST_IPC"

if [ $? -ne 0 ]; then
    echo "DB: Fail"
    exit 1
fi

echo "Verificare Verify"

./tools/fileops.sh run -- fileops_manager --db "$TEST_DB" --verify
if [ $? -eq 0 ]; then
    echo "VERIFY: PASS"
else
    echo "VERIFY: FAIL"
    exit 1
fi

echo "Verificare Dump"

./tools/fileops.sh run -- fileops_manager --db "$TEST_DB" --dump > reports/t4_dump.txt

M_OK=$(grep "magic=INV4" reports/t4_dump.txt)
C_OK=$(grep "complete=1" reports/t4_dump.txt)
F_OK=$(grep "file_record_count=3" reports/t4_dump.txt)
W_OK=$(grep "worker_count=3" reports/t4_dump.txt)

if [[ -n "$M_OK" && -n "$C_OK" && -n "$F_OK" && -n "$W_OK" ]]; then
    echo "DUMP: PASS"
    cat reports/t4_dump.txt

else
    echo "DUMP: FAIL"
    cat reports/t4_dump.txt
    exit 1
fi

echo "Test 4 finalizat"
