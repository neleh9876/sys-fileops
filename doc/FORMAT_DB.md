Documentatie baza de date tema 3

Acest document descrie structura interna a fisierelor binare generate de utilitarele fileops_indexer si proc_snapshot, limitele alese, strategia de actualizare si regulile de validare si comparare.

    Antetul Bazei de Date (DB_Header)

Campurile antetului:

    magic (char[8]): Identificator unic ("IDX_I" pentru fisiere, "PRCI" pentru procese).

    format_version (int): Versiunea formatului (actual: 1).

    snapshot_id (long): Timestamp-ul UNIX de la crearea bazei de date.

    snapshot_state (int): Starea de scriere (0 = OPEN, 1 = SEALED).

    active_writers (int): Contor atomic al proceselor concurente active.

    record_count (int): Numarul total de inregistrari salvate in fisier.

    Formatul Inregistrarilor (Records)

A. Inregistrare Fisier (FileRecord) - Pentru magic IDX_I

    path (char[4096]): Calea absoluta a resursei scanate.

    type (int): Tipul resursei (0=Regulat, 1=Director, 2=Symlink, 3=Altul).

    size (long): Dimensiunea in octeti.

    mtime (long): Timpul ultimei modificari.

    checksum (unsigned int): Suma octetilor fisierului regulat.

    st_dev (long): ID-ul dispozitivului pe care se afla fisierul.

    st_ino (long): Numarul i-node alocat de sistem.

    symlink_target (char[4096]): Calea target pentru symlink-uri.

B. Inregistrare Proces (ProcRecord) - Pentru magic PRCI

    pid (int): Identificatorul procesului.

    comm (char[256]): Numele comenzii.

    state (char): Starea procesului (R, S, Z etc.).

    ppid (int): Identificatorul parintelui.

    cpu_time (long): Suma dintre utime si stime.

    rss (long): Memoria fizica utilizata.

    cmdline (char[512]): Linia de comanda completa.

    Limitele Alese (Hardcoded Limits)

Pentru a garanta o dimensiune fixa a structurilor (vitala in scrierea concurenta cu offset-uri), s-au ales urmatoarele limite maxime:

    MAX_PATH_LEN = 4096 octeti (limita maxima standard in Linux pentru o cale absoluta).

    MAX_COMM_LEN = 256 octeti (acopera cu marja numele executabilelor din stat).

    MAX_CMDLINE_LEN = 512 octeti (suficient pentru a extrage flag-urile principale de rulare, trunchiat controlat daca depaseste).

    Strategia de Actualizare (Model SPMD si Evitare Dubluri)

Sincronizarea intre instantele concurente se face prin fcntl(2) cu F_SETLKW direct pe fisierul bazei de date.
Strategia per inregistrare:

    Blocare exclusiva a fisierului (F_WRLCK).

    Citirea antetului pentru a afla numarul curent de recorduri.

    Cautarea liniara a cheii (calea pentru fisiere, PID-ul pentru procese).

    Daca cheia exista: se calculeaza offset-ul ei specific si se face suprascriere (update in-place).

    Daca cheia nu exista: inregistrarea se adauga la sfarsitul fisierului (append), iar record_count din antet este incrementat.

    Deblocarea fisierului (F_UNLCK).

    Conditiile Minime de Validitate

Pentru ca un fisier sa fie considerat o baza de date valida in cadrul acestui proiect, trebuie sa indeplineasca urmatoarele:

    Dimensiunea fisierului sa fie consistenta: trebuie sa fie exact egala cu sizeof(DB_Header) + (record_count * sizeof(RecordSpecific)).

    Campul "magic" sa fie obligatoriu "IDX_I" sau "PRCI".

    Sa nu existe inregistrari duplicate (doua inregistrari cu aceeasi cale absoluta sau acelasi PID in acelasi fisier).

    Campul snapshot_state sa ajunga in starea 1 (SEALED) la finalizarea tuturor proceselor de scriere.

    Regulile de Comparare (db_diff)

Utilitarul db_diff deschide doua baze de date, verifica compatibilitatea (acelasi magic si format_version) si aplica urmatoarele reguli:

Pentru Fisiere (IDX_I):

    Fisiere Noi: Calea exista in db_new dar nu in db_old.

    Fisiere Disparute: Calea exista in db_old dar nu in db_new.

    Fisiere Modificate: Calea exista in ambele, dar difera cel putin unul din campurile: size, mtime, checksum, type sau symlink_target.

Pentru Procese (PRCI):

    Procese Noi: PID-ul exista in db_new dar nu in db_old.

    Procese Disparute: PID-ul exista in db_old dar nu in db_new.

    Procese Modificate Semnificativ: PID-ul exista in ambele, dar diferenta absoluta de memorie (RSS) este mai mare de 5% din valoarea RSS initiala.
