Documentatie Control Plane - Tema 5

  1.  Arhitectura

    Data Plane: Memoria partajata (mmap). Aici stau coada de joburi si rezultatele. Fisierele inventariate raman aici, nu trec prin pipe.
    Control Plane: Un pipe anonim creat de manager inainte de a porni workerii. Workerii folosesc acest pipe doar ca sa trimita mesaje scurte despre progres, nu pentru date mari.
    Signal Plane: Managerul prinde semnale din exterior (SIGUSR1, SIGINT, SIGTERM, SIGCHLD) si ia decizii in functie de ele.

  2. Comunicarea prin Pipe
   
    Workerii scriu mesaje in pipe dintr-o singura miscare. Mesajele sunt scurte, asa ca nu se amesteca intre ele daca doi workeri scriu in acelasi timp.
 
    JOB_DONE: Workerul anunta managerul cand a terminat de scanat un director. 
    WORKER_EXITING: Workerul anunta ca se inchide normal. 
    ERROR: Daca un worker da de o eroare locala, o raporteaza asa. 

  3. Semnale si oprirea controlata 

    SIGUSR1: In handler setam doar un flag, pentru siguranta. Managerul vede flag-ul in bucla principala si afiseaza pe ecran o linie de STATUS cu cate joburi si fisiere au fost procesate.
    SIGINT / SIGTERM: Cand utilizatorul vrea sa opreasca programul, managerul nu mai accepta joburi noi. Trimite semnalul SIGTERM catre workeri si asteapta un timp setat. Daca workerii nu se opresc la timp, ii inchide fortat cu SIGKILL. Workerul, cand primeste SIGTERM, se opreste curat si trimite mesajul de exit. 
    SIGCHLD: Folosim waitpid() ca managerul sa curete workerii opriti, sa nu ramana procese zombie in sistem. 

  4.Baza de Date Incompleta (complete=0)
  
    Daca oprim programul fortat inainte sa termine de scanat, nu pierdem datele. Managerul scrie baza de date in siguranta (atomic, cu fisier temporar).
    Diferenta este ca seteaza valoarea complete=0, care inseamna ca inventarierea a fost intrerupta. Baza de date nu este stricata. Comenzile de verificare si de afisare functioneaza perfect si recunosc ca fisierul e valid, chiar daca e incomplet. 

