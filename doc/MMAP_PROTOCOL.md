Protocolul de Comunicare prin Memorie Partajata (IPC)

Structura Memoriei Partajate
Comunicarea se realizeaza printr-un segment de memorie mapat cu mmap, structurat astfel:

    Header Global: Contine metadate ca magic si versiune, plus semafoarele pentru coada de job-uri.
    Coada de Job-uri (Ring Buffer): Gestionata de Manager (Producator) si consumata de Workeri (Consumatori).
    Canale de Rezultate: Fiecare worker are propriul buffer dedicat pentru a trimite datele inapoi la Manager fara a se bloca reciproc.
    Statistici: Regiune unde workerii scriu datele de performanta (CPU, PID, status) la final.

Mecanisme de Sincronizare (Semafoare)
Pentru a preveni accesul simultan corupt, am implementat urmatoarele semafoare:

    1.Semafoare Coada Job-uri:
    
    sem_jobs_mutex: Garanteaza accesul exclusiv la indecsii de head si tail.
    sem_jobs_count: Contorizeaza job-urile disponibile pentru workeri.
    sem_jobs_free: Limiteaza managerul sa nu puna mai multe job-uri decat capacitatea cozii.
    
    2.Semafoare Canale Rezultate:

    sem_res_mutex: Protejeaza buffer-ul de rezultate al fiecarui worker.
    sem_res_count: Anunta Managerul ca un nou record este gata de citire.
    sem_res_free: Previne workerul din a suprascrie rezultate care nu au fost inca salvate.

Fluxul de Terminare
Managerul seteaza termination_flag pe 1 cand nu mai sunt directoare de scanat. Workerii isi termina executia doar dupa ce termina toate job-urile ramase in coada.
