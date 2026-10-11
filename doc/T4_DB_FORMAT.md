Formatul bazei de date (T4)

Header-ul fisierului
Fiecare baza de date incepe cu o structura de dimensiune fixa numita DB_Header:

    Magic: INV4 (4 octeti) pentru identificarea corecta a fisierului.
    Versiune: 1.
    Status (Complete): Flag setat pe 1 la finalul procesului pentru a marca o baza de date valida.
    File Record Count: Numarul total de fisiere gasite si salvate.
    Worker Count: Numarul de procese care au lucrat la baza de date.

Inregistrarile de Fisiere

Dupa header urmeaza vectorul de structuri FileRecord. Fiecare element contine:

    Path: Calea completa a fisierului (limitata la 4096 caractere).
    Metadate: Dimensiunea in octeti, timpul ultimei modificari, UID, GID si permisiunile.
    Checksum (SHA256): Un hash de 32 de octeti calculat pentru continutul fiecarui fisier.

Sectiunea de Statistici
La finalul fisierului, Managerul scrie statisticile per worker:

    PID: Identificatorul procesului.
    Performanta: Timpul CPU (User si System) in microsecunde.
    Activitate: Cate job-uri a rezolvat si cati octeti a procesat in total.

Reguli de Siguranta

    Toate sirurile de caractere au terminatorul null la final.
    Dimensiunea fisierului trebuie sa fie egala cu Header + Inregistrari + Statistici.

