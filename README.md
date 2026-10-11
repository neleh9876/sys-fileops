FileOps & Process Monitoring Toolkit
Toolkit modular de nivel de sistem pentru mediul Linux (POSIX), destinat auditului de sistem, monitorizării proceselor și inventarierii concurente a fișierelor.
Arhitectura proiectului:
1.Audit & CLI Tools: Colectare de metadate de sistem și generare de rapoarte structurate. 
2.Orchestrator Bash (tools/fileops.sh): Management unificat al ciclului de viață (compilare incrementală, execuție, testare). 
3.Persistență & Sincronizare la nivel de fișier: Indexare recursivă și snapshot-uri din /proc în baze de date binare versionate, cu acces concurent protejat prin fcntl(2). 
4.Data Plane Multiproces: Arhitectură Manager-Worker cu coadă dinamică de directoare (ring buffer) și canale de rezultate mapate în memorie (mmap, MAP_SHARED). 
5.Control Plane & Semnale: Canale de control non-blocante prin Pipe/FIFO, raportare asincronă de status (SIGUSR1) și terminare controlată cu persistență atomică (SIGINT/SIGTERM).
