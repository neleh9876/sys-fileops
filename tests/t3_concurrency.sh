#!/bin/bash

echo "Testare tema 3"

mkdir -p data reports tmp/test_root/dir1 tmp/test_root/dir2

echo ""
echo "Testare mai multe instante fisiere"
INDEX_DB="data/test_index_concurent.db"
rm -f "$INDEX_DB"

echo "Fisier 1" > tmp/test_root/dir1/f1.txt
echo "Fisier 2" > tmp/test_root/dir2/f2.txt

./tools/fileops.sh run fileops_indexer --root tmp/test_root --db "$INDEX_DB" &
./tools/fileops.sh run fileops_indexer --root src --db "$INDEX_DB" &
./tools/fileops.sh run fileops_indexer --root include --db "$INDEX_DB" &
./tools/fileops.sh run fileops_indexer --root tools --db "$INDEX_DB" &

wait
echo "Baza de date fișiere generată"
ls -lh "$INDEX_DB"

echo ""
echo "Testare mai multe instante procese"
PROC_DB="data/test_proc_concurent.db"
rm -f "$PROC_DB"

./tools/fileops.sh run proc_snapshot --db "$PROC_DB" &
./tools/fileops.sh run proc_snapshot --db "$PROC_DB" &
./tools/fileops.sh run proc_snapshot --db "$PROC_DB" &
./tools/fileops.sh run proc_snapshot --db "$PROC_DB" &

wait
echo "Baza de date procese generată"
ls -lh "$PROC_DB"

echo ""
echo "Generare snapshot-uri distincte  pentru fișiere"
rm -f data/index_old.db data/index_new.db reports/T3_filediff.txt

./tools/fileops.sh run fileops_indexer --root tmp/test_root --db data/index_old.db

echo "Fisier nou" > tmp/test_root/dir1/fisier_nou.txt
echo "Update" >> tmp/test_root/dir2/f2.txt

./tools/fileops.sh run fileops_indexer --root tmp/test_root --db data/index_new.db

./tools/fileops.sh run db_diff --old data/index_old.db --new data/index_new.db --out reports/T3_filediff.txt
echo "Raport fișiere generat."
cat reports/T3_filediff.txt

echo ""
echo "Generare snapshot-uri distincte pentru procese"
rm -f data/proc_old.db data/proc_new.db reports/T3_procdiff.txt

./tools/fileops.sh run proc_snapshot --db data/proc_old.db

sleep 1 

./tools/fileops.sh run proc_snapshot --db data/proc_new.db

./tools/fileops.sh run db_diff --old data/proc_old.db --new data/proc_new.db --out reports/T3_procdiff.txt
echo "Raport procese generat."
cat reports/T3_procdiff.txt

echo ""
echo "Test finalizat cu succes."
if [ $? -eq 0 ]; then
    echo "t3_concurrency: PASS" >> reports/T2_tests.txt
else
    echo "t3_concurrency: FAIL" >> reports/T2_tests.txt
    exit 1
fi
