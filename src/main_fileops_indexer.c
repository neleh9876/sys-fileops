#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include "../include/db_format.h"

int lock_region(int fd, int type, off_t offset, int whence, off_t len) {
    struct flock fl;
    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = offset;
    fl.l_len = len;
    return fcntl(fd, F_SETLKW, &fl);
}

unsigned int calculate_checksum(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return 0;
    unsigned int sum = 0;
    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) sum += buffer[i];
    }
    fclose(f);
    return sum;
}

void process_directory(const char *current_path, int db_fd) {
    DIR *dir = opendir(current_path);
    if (!dir) return;

    struct dirent *entry;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (current_path[strlen(current_path) - 1] == '/')
            snprintf(full_path, sizeof(full_path), "%s%s", current_path, entry->d_name);
        else
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) continue;

        FileRecord record;
        memset(&record, 0, sizeof(FileRecord));
        strncpy(record.path, full_path, sizeof(record.path) - 1);
        record.mtime = st.st_mtime;
        record.st_dev = st.st_dev;
        record.st_ino = st.st_ino;

        if (S_ISREG(st.st_mode)) {
            record.type = 0;
            record.size = st.st_size;
            record.checksum = calculate_checksum(full_path);
        } else if (S_ISDIR(st.st_mode)) {
            record.type = 1;
        } else if (S_ISLNK(st.st_mode)) {
            record.type = 2;
            ssize_t rl = readlink(full_path, record.symlink_target, sizeof(record.symlink_target) - 1);
            if (rl != -1) record.symlink_target[rl] = '\0';
        } else {
            record.type = 3;
        }

        lock_region(db_fd, F_WRLCK, 0, SEEK_SET, 0); 
        
        DB_Header header;
        lseek(db_fd, 0, SEEK_SET);
        if (read(db_fd, &header, sizeof(DB_Header)) != sizeof(DB_Header)) {
            lock_region(db_fd, F_UNLCK, 0, SEEK_SET, 0);
            continue;
        }
        
        int found = 0;
        off_t write_offset = sizeof(DB_Header) + (header.record_count * sizeof(FileRecord));
        
        lseek(db_fd, sizeof(DB_Header), SEEK_SET);
        for (int i = 0; i < header.record_count; i++) {
            FileRecord temp;
            if (read(db_fd, &temp, sizeof(FileRecord)) == sizeof(FileRecord)) {
                if (strcmp(temp.path, record.path) == 0) { 
                    found = 1;
                    write_offset = sizeof(DB_Header) + (i * sizeof(FileRecord));
                    break;
                }
            }
        }
        
        lseek(db_fd, write_offset, SEEK_SET);
        if (write(db_fd, &record, sizeof(FileRecord)) != sizeof(FileRecord)) {
             perror("Error writing record");
        }
        
        if (!found) {
            header.record_count++;
            lseek(db_fd, 0, SEEK_SET);
            (void)!write(db_fd, &header, sizeof(DB_Header));
        }
        lock_region(db_fd, F_UNLCK, 0, SEEK_SET, 0);
        if (S_ISDIR(st.st_mode)) {
            process_directory(full_path, db_fd);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    char *root_dir = ".";
    char *db_path = "data/index.db";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) root_dir = argv[++i];
        else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) db_path = argv[++i];
    }
    int db_fd = open(db_path, O_RDWR | O_CREAT, 0666);
    if (db_fd < 0) return 1;
    
    lock_region(db_fd, F_WRLCK, 0, SEEK_SET, sizeof(DB_Header));
    DB_Header header;
    ssize_t bytes_read = read(db_fd, &header, sizeof(DB_Header));

    if (bytes_read <= 0) {
        memset(&header, 0, sizeof(DB_Header));
        strncpy(header.magic, MAGIC_INDEX, sizeof(header.magic));
        header.format_version = FORMAT_VERSION;
        header.snapshot_id = time(NULL);
        header.snapshot_state = SNAPSHOT_OPEN;
        header.active_writers = 1;
        header.record_count = 0;
        (void)!write(db_fd, &header, sizeof(DB_Header));
    } else {
        if (header.snapshot_state == SNAPSHOT_SEALED) {
            fprintf(stderr, "Eroare: Snapshot-ul este sealed.\n");
            lock_region(db_fd, F_UNLCK, 0, SEEK_SET, sizeof(DB_Header));
            close(db_fd);
            return 1;
        }
        header.active_writers++;
        lseek(db_fd, 0, SEEK_SET);
        (void)!write(db_fd, &header, sizeof(DB_Header));
    }
    lock_region(db_fd, F_UNLCK, 0, SEEK_SET, sizeof(DB_Header));

    process_directory(root_dir, db_fd);

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
