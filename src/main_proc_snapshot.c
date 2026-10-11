#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include "../include/db_format.h"

int lock_region(int fd, int type, off_t offset, int whence, off_t len) {
    struct flock fl;
    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = offset;
    fl.l_len = len;
    return fcntl(fd, F_SETLKW, &fl);
}

int is_numeric(const char *str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

void process_proc_directory(int db_fd) {
    DIR *dir = opendir("/proc");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && is_numeric(entry->d_name)) {
            char stat_path[512];
            char cmdline_path[512];
            
            snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);
            snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);

            FILE *f_stat = fopen(stat_path, "r");
            if (!f_stat) continue; 

            ProcRecord record;
            memset(&record, 0, sizeof(ProcRecord));
            
            long unsigned int utime, stime;
            long rss;

            int items = fscanf(f_stat, "%d %s %c %d %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %*s %*s %*s %*s %*s %*s %ld",
                               &record.pid, record.comm, &record.state, &record.ppid, &utime, &stime, &rss);
            fclose(f_stat);

            if (items != 7) continue; 

            record.cpu_time = utime + stime;
            record.rss = rss;

            FILE *f_cmd = fopen(cmdline_path, "r");
            if (f_cmd) {
                size_t bytes_read = fread(record.cmdline, 1, MAX_CMDLINE_LEN - 1, f_cmd);
                for (size_t i = 0; i < bytes_read; i++) {
                    if (record.cmdline[i] == '\0') record.cmdline[i] = ' '; 
                }
                record.cmdline[bytes_read] = '\0';
                fclose(f_cmd);
            }

            lock_region(db_fd, F_WRLCK, 0, SEEK_SET, 0); 
            
            DB_Header header;
            lseek(db_fd, 0, SEEK_SET);
            if (read(db_fd, &header, sizeof(DB_Header)) != sizeof(DB_Header)) {
                lock_region(db_fd, F_UNLCK, 0, SEEK_SET, 0);
                continue;
            }
            
            int found = 0;
            off_t write_offset = sizeof(DB_Header) + (header.record_count * sizeof(ProcRecord));
            
            lseek(db_fd, sizeof(DB_Header), SEEK_SET);
            for (int i = 0; i < header.record_count; i++) {
                ProcRecord temp;
                if (read(db_fd, &temp, sizeof(ProcRecord)) == sizeof(ProcRecord)) {
                    if (temp.pid == record.pid) {
                        found = 1;
                        write_offset = sizeof(DB_Header) + (i * sizeof(ProcRecord));
                        break;
                    }
                }
            }
            
            lseek(db_fd, write_offset, SEEK_SET);
            if (write(db_fd, &record, sizeof(ProcRecord)) != sizeof(ProcRecord)) {
                perror("Error writing process record");
            }
            
            if (!found) {
                header.record_count++;
                lseek(db_fd, 0, SEEK_SET);
                (void)!write(db_fd, &header, sizeof(DB_Header));
            }
            
            lock_region(db_fd, F_UNLCK, 0, SEEK_SET, 0);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    char *db_path = "data/proc.db"; 

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        }
    }

    int db_fd = open(db_path, O_RDWR | O_CREAT, 0666);
    if (db_fd < 0) return 1;

    lock_region(db_fd, F_WRLCK, 0, SEEK_SET, sizeof(DB_Header));
    DB_Header header;
    ssize_t bytes_read = read(db_fd, &header, sizeof(DB_Header));

    if (bytes_read <= 0) {
        memset(&header, 0, sizeof(DB_Header));
        strncpy(header.magic, MAGIC_PROC, sizeof(header.magic));
        header.format_version = FORMAT_VERSION;
        header.snapshot_id = time(NULL);
        header.snapshot_state = SNAPSHOT_OPEN;
        header.active_writers = 1;
        header.record_count = 0;
        (void)!write(db_fd, &header, sizeof(DB_Header));
    } else {
        if (header.snapshot_state == SNAPSHOT_SEALED) {
            fprintf(stderr, "Eroare: Snapshot-ul este sealed\n");
            lock_region(db_fd, F_UNLCK, 0, SEEK_SET, sizeof(DB_Header));
            close(db_fd);
            return 1;
        }
        header.active_writers++;
        lseek(db_fd, 0, SEEK_SET);
        (void)!write(db_fd, &header, sizeof(DB_Header));
    }
    lock_region(db_fd, F_UNLCK, 0, SEEK_SET, sizeof(DB_Header));

    process_proc_directory(db_fd);

    lock_region(db_fd, F_WRLCK, 0, SEEK_SET, sizeof(DB_Header));
    lseek(db_fd, 0, SEEK_SET);
    if (read(db_fd, &header, sizeof(DB_Header)) == sizeof(DB_Header)) {
        header.active_writers--;
        if (header.active_writers == 0) header.snapshot_state = SNAPSHOT_SEALED;
        lseek(db_fd, 0, SEEK_SET);
        (void)!write(db_fd, &header, sizeof(DB_Header));
    }
    lock_region(db_fd, F_UNLCK, 0, SEEK_SET, sizeof(DB_Header));

    close(db_fd);
    return 0;
}
