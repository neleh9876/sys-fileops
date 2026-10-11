A.
  1. Listează conținutul directorului curent în format “long” și include fișierele ascunse.
   Comanda: ls -la 
   Fișierul de output: reports/fs/A1_ls_long.txt
   
  2. Găsește în proiect toate fișierele cu extensia .sh și afișează calea lor relativă
   Comanda: find . -name "*.sh"
   Fisierul de output: reports/fs/A2_find_sh.txt
   
  3. Afișează dimensiunea (în mod human-readable) pentru directoarele de nivel 1 din
proiect (ex: doc/, reports/, etc.). 
     Comanda: du -sh */
     Fisierul de output: reports/fs/A3_du_level1.txt
     
B.
   1. Afișează primele 10 procese ordonate descrescător după consumul de memorie (RSS sau %MEM).
    Comanda: ps aux --sort=-%mem | head -n 11
    Fișierul de output: reports/process/B1_top_mem.txt
    
   2.  Afișează arborele de procese (process tree) pentru sistem, cu PID-uri vizibile.
    Comanda: pstree -p
    Fișierul de output: reports/process/B2_pstree.txt
    
   3.   Pornește un proces de test (ex: sleep 60 în background) și demonstrează că îl poți identifica după nume și PID.
       Comanda: 1)Pornire: sleep 60 &
       Comanda: 2)Identificare: pgrep -a sleep  (afiseaza procesele denumite sleep)
       Fișierul de output: reports/process/B3_pgrep_sleep.txt
       Observație: Pentru a opri procesul, dupa ce identificăm PID-ul lui folosim comanda "kill PID"

C.
  1. Extrage modelul CPU (sau “model name”) din /proc/cpuinfo.
     Comanda: grep -m 1 "model name" /proc/cpuinfo
     Fișierul de output: reports/proc/C1_cpu_model.txt
     
  2. Extrage “MemTotal” și “MemAvailable” din /proc/meminfo.
     Comanda: grep -e "MemTotal" -e "MemAvailable" /proc/meminfo
     Fișierul de output: reports/proc/C2_mem_total_avail.txt
     
   3. Afișează “uptime” din /proc/uptime (valoarea brută, în secunde).
     Comanda:  cut -d' ' -f1 /proc/uptime
     Fișierul de output: reports/proc/C3_uptime.txt
 D.
  1.  Construiește un top 5 al celor mai “mari” fișiere (după dimensiune) din proiect, afișând doar dimensiunea și calea.
      Comanda: find . -type f  | xargs  du -h | sort -rh | head -n 5
      Fișierul de output: reports/pipeline/D1_top5_large_files.txt
    
   2.  Construiește un “top 5” procese după memorie, extrăgând doar câmpurile relevante (PID și nume), eliminând duplicatele (dacă apar) și limitând la 5 linii de output.
      Comanda: ps -eo pid,comm --sort=-%mem | tail -n +2 | uniq | head -n 5
      Fișierul de output: reports/pipeline/D2_top5_proc_mem_pid_name.txt
      
   3. Din fișierul doc/T1_comenzi.md, extrage liniile care conțin comenzi (de ex. cele din blocuri marcate sau cele care încep cu un prompt), sortează-le și numără câte sunt.
      Comanda: grep "Comanda:" doc/T1_comenzi.md | cut -d':' -f2 | sort | wc -l
      Fișierul de output: reports/pipeline/D3_count_commands.txt
