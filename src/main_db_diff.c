#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "../include/db_format.h"

void compare_files(int fd_old, DB_Header *h_old, int fd_new, DB_Header *h_new, FILE *out) {
    FileRecord *old_recs = malloc(h_old->record_count * sizeof(FileRecord));
    FileRecord *new_recs = malloc(h_new->record_count * sizeof(FileRecord));
    int *old_matched = calloc(h_old->record_count, sizeof(int));

    if (!old_recs || !new_recs || !old_matched) {
        fprintf(stderr, "Eroare alocare memorie compare_files\n");
        free(old_recs); free(new_recs); free(old_matched);
        return;
    }

    if (lseek(fd_old, sizeof(DB_Header), SEEK_SET) == -1) { perror("lseek old"); }
    if (read(fd_old, old_recs, h_old->record_count * sizeof(FileRecord)) < 0) { perror("read old"); }
    
    if (lseek(fd_new, sizeof(DB_Header), SEEK_SET) == -1) { perror("lseek new"); }
    if (read(fd_new, new_recs, h_new->record_count * sizeof(FileRecord)) < 0) { perror("read new"); }

    fprintf(out, "Raport fisiere\n\n");
    fprintf(out, "Fisiere noi \n");
    
    int count_new = 0, count_mod = 0, count_del = 0;
    char mod_buffer[65536] = ""; 

    for (int i = 0; i < h_new->record_count; i++) {
        int found = 0;
        for (int j = 0; j < h_old->record_count; j++) {
            if (strcmp(new_recs[i].path, old_recs[j].path) == 0) {
                found = 1;
                old_matched[j] = 1; 
                if (new_recs[i].size != old_recs[j].size || 
                    new_recs[i].mtime != old_recs[j].mtime ||
                    new_recs[i].checksum != old_recs[j].checksum ||
                    new_recs[i].type != old_recs[j].type ||
                    strcmp(new_recs[i].symlink_target, old_recs[j].symlink_target) != 0) {
                    
                    char temp[1024];
                    snprintf(temp, sizeof(temp), "[Modificat] %s (Size: %ld -> %ld)\n", 
                             new_recs[i].path, (long)old_recs[j].size, (long)new_recs[i].size);
                    strncat(mod_buffer, temp, sizeof(mod_buffer) - strlen(mod_buffer) - 1);
                    count_mod++;
                }
                break;
            }
        }
        if (!found) {
            fprintf(out, "[Nou] %s\n", new_recs[i].path);
            count_new++;
        }
    }
    if (count_new == 0) fprintf(out, "Niciun fisier nou.\n");

    fprintf(out, "\n Fisiere modificate \n");
    if (count_mod > 0) fprintf(out, "%s", mod_buffer);
    else fprintf(out, "Niciun fisier modificat.\n");

    fprintf(out, "\n Fisiere disparute\n");
    for (int j = 0; j < h_old->record_count; j++) {
        if (!old_matched[j]) {
            fprintf(out, "[Disparut] %s\n", old_recs[j].path);
            count_del++;
        }
    }
    if (count_del == 0) fprintf(out, "Niciun fisier disparut.\n");

    free(old_recs); free(new_recs); free(old_matched);
}

void compare_processes(int fd_old, DB_Header *h_old, int fd_new, DB_Header *h_new, FILE *out) {
    ProcRecord *old_recs = malloc(h_old->record_count * sizeof(ProcRecord));
    ProcRecord *new_recs = malloc(h_new->record_count * sizeof(ProcRecord));
    int *old_matched = calloc(h_old->record_count, sizeof(int));

    if (!old_recs || !new_recs || !old_matched) {
        fprintf(stderr, "Eroare alocare memorie compare_processes\n");
        free(old_recs); free(new_recs); free(old_matched);
        return;
    }

    if (lseek(fd_old, sizeof(DB_Header), SEEK_SET) == -1) { perror("lseek old"); }
    if (read(fd_old, old_recs, h_old->record_count * sizeof(ProcRecord)) < 0) { perror("read old"); }
    
    if (lseek(fd_new, sizeof(DB_Header), SEEK_SET) == -1) { perror("lseek new"); }
    if (read(fd_new, new_recs, h_new->record_count * sizeof(ProcRecord)) < 0) { perror("read new"); }

    fprintf(out, "Raport procese\n\n");
    fprintf(out, "Procese noi\n");
    
    int count_new = 0, count_mod = 0, count_del = 0;
    char mod_buffer[65536] = "";

    for (int i = 0; i < h_new->record_count; i++) {
        int found = 0;
        for (int j = 0; j < h_old->record_count; j++) {
            if (new_recs[i].pid == old_recs[j].pid) {
                found = 1;
                old_matched[j] = 1;
                double rss_diff = fabs((double)new_recs[i].rss - (double)old_recs[j].rss);
                double threshold = 0.05 * (double)old_recs[j].rss;
                
                if (old_recs[j].rss > 0 && rss_diff > threshold) {
                    char temp[1024];
                    snprintf(temp, sizeof(temp), "[Modificat semnificativ] PID %d (%s) - RSS: %ld -> %ld\n", 
                             new_recs[i].pid, new_recs[i].comm, old_recs[j].rss, new_recs[i].rss);
                    strncat(mod_buffer, temp, sizeof(mod_buffer) - strlen(mod_buffer) - 1);
                    count_mod++;
                }
                break;
            }
        }
        if (!found) {
            fprintf(out, "[Nou] PID %d (%s)\n", new_recs[i].pid, new_recs[i].comm);
            count_new++;
        }
    }
    if (count_new == 0) fprintf(out, "Niciun proces nou.\n");

    fprintf(out, "\nProcese modificate semnificativ\n");
    if (count_mod > 0) fprintf(out, "%s", mod_buffer);
    else fprintf(out, "Niciun proces modificat semnificativ (> 5%% diferenta memorie).\n");

    fprintf(out, "\nProcese disparute\n");
    for (int j = 0; j < h_old->record_count; j++) {
        if (!old_matched[j]) {
            fprintf(out, "[Disparut] PID %d (%s)\n", old_recs[j].pid, old_recs[j].comm);
            count_del++;
        }
    }
    if (count_del == 0) fprintf(out, "Niciun proces dispărut.\n");

    free(old_recs); free(new_recs); free(old_matched);
}

int main(int argc, char *argv[]) {
    char *db_old_path = NULL;
    char *db_new_path = NULL;
    char *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--old") == 0 && i + 1 < argc) db_old_path = argv[++i];
        else if (strcmp(argv[i], "--new") == 0 && i + 1 < argc) db_new_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    if (!db_old_path || !db_new_path || !out_path) {
        fprintf(stderr, "Utilizare: %s --old <db1> --new <db2> --out <path>\n", argv[0]);
        return 1;
    }

    int fd_old = open(db_old_path, O_RDONLY);
    int fd_new = open(db_new_path, O_RDONLY);
    if (fd_old < 0 || fd_new < 0) {
        fprintf(stderr, "Eroare la deschiderea bazelor de date pentru comparare.\n");
        if (fd_old >= 0) close(fd_old);
        if (fd_new >= 0) close(fd_new);
        return 1;
    }

    DB_Header h_old, h_new;
    if (read(fd_old, &h_old, sizeof(DB_Header)) != sizeof(DB_Header)) {
        perror("Eroare citire header old");
        close(fd_old); close(fd_new); return 1;
    }
    if (read(fd_new, &h_new, sizeof(DB_Header)) != sizeof(DB_Header)) {
        perror("Eroare citire header new");
        close(fd_old); close(fd_new); return 1;
    }

    if (strcmp(h_old.magic, h_new.magic) != 0) {
        fprintf(stderr, "Eroare: Bazele de date nu sunt de același tip (%s vs %s)!\n", h_old.magic, h_new.magic);
        close(fd_old); close(fd_new); return 1;
    }
    if (h_old.format_version != h_new.format_version) {
        fprintf(stderr, "Eroare: Versiuni de format diferite!\n");
        close(fd_old); close(fd_new); return 1;
    }

    FILE *out_file = fopen(out_path, "w");
    if (!out_file) {
        perror("Eroare la crearea fisierului de raport");
        close(fd_old); close(fd_new);
        return 1;
    }

    if (strcmp(h_old.magic, MAGIC_INDEX) == 0) {
        compare_files(fd_old, &h_old, fd_new, &h_new, out_file);
    } else if (strcmp(h_old.magic, MAGIC_PROC) == 0) {
        compare_processes(fd_old, &h_old, fd_new, &h_new, out_file);
    }

    fclose(out_file);
    close(fd_old);
    close(fd_new);
    printf("Raport generat cu succes: %s\n", out_path);
    return 0;
}
