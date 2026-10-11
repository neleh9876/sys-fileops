#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <getopt.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include "../include/ipc_protocol.h"
#include "../include/db_format.h"

volatile sig_atomic_t flag_status = 0;
volatile sig_atomic_t flag_shutdown = 0;
volatile sig_atomic_t flag_sigchld = 0;

void handle_sigusr1(int sig) { (void)sig; flag_status = 1; }
void handle_sigterm_int(int sig) { (void)sig; flag_shutdown = 1; }
void handle_sigchld(int sig) { (void)sig; flag_sigchld = 1; }

void run_verify(const char* db_path) {
    int fd = open(db_path, O_RDONLY);
    if (fd < 0) { perror("Eroare deschidere DB"); exit(1); }
    db_header_t header;
    if (read(fd, &header, sizeof(db_header_t)) != sizeof(db_header_t)) {
        fprintf(stderr, "Eroare: Header incomplet\n"); close(fd); exit(1);
    }
    if (memcmp(header.magic, DB_MAGIC, 4) != 0) {
        fprintf(stderr, "Eroare: Magic invalid\n"); close(fd); exit(1);
    }
    struct stat st;
    fstat(fd, &st);
    size_t expected = sizeof(db_header_t) + (header.file_record_count * sizeof(db_file_record_t));
    if ((size_t)st.st_size < expected) {
        fprintf(stderr, "Eroare: Dimensiune invalida\n"); close(fd); exit(1);
    }
    close(fd);
    printf("Baza de date este valida structural.\n");
    exit(0);
}

void run_dump(const char* db_path) {
    int fd = open(db_path, O_RDONLY);
    if (fd < 0) { perror("open"); exit(1); }
    db_header_t h;
    if (read(fd, &h, sizeof(db_header_t)) == sizeof(db_header_t)) {
        printf("magic=%.4s\n", h.magic);
        printf("version=%u\n", h.format_version);
        printf("complete=%u\n", h.complete);
        printf("file_record_count=%u\n", h.file_record_count);
        printf("worker_count=%u\n", h.worker_count);
    }
    close(fd);
    exit(0);
}

shared_memory_t* init_ipc(const char* ipc_path, int num_workers, int max_depth, int simulate_ms) {
    int fd = open(ipc_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { perror("open ipc"); exit(1); }
    if (ftruncate(fd, sizeof(shared_memory_t)) == -1) { perror("ftruncate"); exit(1); }
    shared_memory_t* shm = mmap(NULL, sizeof(shared_memory_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    
    memset(shm, 0, sizeof(shared_memory_t));
    shm->header.magic = IPC_MAGIC;
    shm->header.num_workers = num_workers;
    shm->header.max_depth = max_depth;
    shm->header.simulate_work_ms = simulate_ms;

    sem_init(&shm->header.sem_jobs_mutex, 1, 1);
    sem_init(&shm->header.sem_jobs_count, 1, 0);
    sem_init(&shm->header.sem_jobs_free, 1, MAX_JOBS);
    sem_init(&shm->header.sem_stats_mutex, 1, 1);
    for (int i = 0; i < num_workers; i++) {
        sem_init(&shm->channels[i].sem_res_mutex, 1, 1);
        sem_init(&shm->channels[i].sem_res_count, 1, 0);
        sem_init(&shm->channels[i].sem_res_free, 1, MAX_RESULTS_PER_CHANNEL);
    }
    return shm;
}

int main(int argc, char *argv[]) {
    char *root_dir = NULL, *db_path = "data/inventory.db", *ipc_path = "data/ipc.mmap";
    int num_workers = 0, max_depth = -1, simulate_ms = 0, mode = 0;
    
    // --- T5: argumente noi CLI ---
    int graceful_timeout = 5; 
    char *pid_file = NULL;

    static struct option long_options[] = {
        {"root",             required_argument, 0, 'r'},
        {"workers",          required_argument, 0, 'w'},
        {"db",               required_argument, 0, 'd'},
        {"ipc",              required_argument, 0, 'i'},
        {"max-depth",        required_argument, 0, 'm'},
        {"simulate-work-ms", required_argument, 0, 's'},
        {"graceful-timeout", required_argument, 0, 'g'}, 
        {"pid-file",         required_argument, 0, 'p'}, 
        {"verify",           no_argument,       0, 'v'},
        {"dump",             no_argument,       0, 'u'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "r:w:d:i:m:s:g:p:vu", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r': root_dir = optarg; break;
            case 'w': num_workers = atoi(optarg); break;
            case 'd': db_path = optarg; break;
            case 'i': ipc_path = optarg; break;
            case 'm': max_depth = atoi(optarg); break;
            case 's': simulate_ms = atoi(optarg); break;
            case 'g': graceful_timeout = atoi(optarg); break; 
            case 'p': pid_file = optarg; break;               
            case 'v': mode = 1; break;
            case 'u': mode = 2; break;
        }
    }

    if (mode == 1) run_verify(db_path);
    if (mode == 2) run_dump(db_path);

    if (!root_dir || num_workers < 1) {
        fprintf(stderr, "Eroare: --root si --workers >= 1 sunt obligatorii.\n");
        return 1;
    }

    signal(SIGUSR1, handle_sigusr1);
    signal(SIGINT, handle_sigterm_int);
    signal(SIGTERM, handle_sigterm_int);
    signal(SIGCHLD, handle_sigchld);

    if (pid_file) {
        FILE *pf = fopen(pid_file, "w");
        if (pf) {
            fprintf(pf, "%d\n", getpid());
            fclose(pf);
        }
    }

    int control_pipe[2];
    if (pipe(control_pipe) < 0) { perror("pipe"); exit(1); }
    fcntl(control_pipe[0], F_SETFL, O_NONBLOCK); 

    shared_memory_t *shm = init_ipc(ipc_path, num_workers, max_depth, simulate_ms);

    sem_wait(&shm->header.sem_jobs_free);
    strncpy(shm->job_queue[0].path, root_dir, 4095);
    shm->job_queue[0].depth = 0;
    shm->job_tail = 1;
    shm->header.pending_jobs = 1;
    sem_post(&shm->header.sem_jobs_count);

    pid_t worker_pids[32]; memset(worker_pids, 0, sizeof(worker_pids));

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(control_pipe[0]); // Workerul nu citeste
            char wid[12], fd_str[12]; 
            snprintf(wid, sizeof(wid), "%d", i);
            snprintf(fd_str, sizeof(fd_str), "%d", control_pipe[1]);

            execl("./bin/fileops_worker", "fileops_worker", 
                  "--worker-id", wid, 
                  "--ipc", ipc_path, 
                  "--control-fd", fd_str, 
                  NULL);
            perror("execl"); exit(1);
        }
        worker_pids[i] = pid;
    }
    close(control_pipe[1]); 

    char tmp_db[4100]; snprintf(tmp_db, sizeof(tmp_db), "%s.tmp", db_path);
    FILE *fdb = fopen(tmp_db, "wb");
    if (!fdb) { perror("fopen tmp db"); exit(1); }

    db_header_t db_hdr = { .format_version = 1, .file_record_count = 0, .worker_count = num_workers, .complete = 0 };
    memcpy(db_hdr.magic, DB_MAGIC, 4);
    fwrite(&db_hdr, sizeof(db_header_t), 1, fdb);

    int active = num_workers;
    int state_running = 1;
    time_t shutdown_start_time = 0;

    unsigned long long w_jobs[32] = {0};
    unsigned long long w_files[32] = {0};
    unsigned long long w_bytes[32] = {0};

    while (active > 0) {
        if (flag_status) {
            flag_status = 0;
            unsigned long long total_jobs = 0, total_files = 0, total_bytes = 0;
            for (int i = 0; i < num_workers; i++) {
                total_jobs += w_jobs[i];
                total_files += w_files[i];
                total_bytes += w_bytes[i];
            }
            int active_j = num_workers < shm->header.pending_jobs ? num_workers : shm->header.pending_jobs;
            printf("STATUS queued_jobs=%d active_jobs=%d files=%llu bytes=%llu workers_alive=%d complete=0\n", 
                   shm->header.pending_jobs, 
                   active_j,
                   total_files, 
                   total_bytes, 
                   active);
            fflush(stdout);
        }

        if (flag_shutdown && state_running) {
            state_running = 0;
            shutdown_start_time = time(NULL);
            
            sem_wait(&shm->header.sem_jobs_mutex);
            shm->header.termination_flag = 1; 
            for (int k = 0; k < num_workers; k++) sem_post(&shm->header.sem_jobs_count);
            sem_post(&shm->header.sem_jobs_mutex);

            for (int i = 0; i < num_workers; i++) {
                if (worker_pids[i] > 0) kill(worker_pids[i], SIGTERM);
            }
        }

        if (!state_running && (time(NULL) - shutdown_start_time > graceful_timeout)) {
            for (int i = 0; i < num_workers; i++) {
                if (worker_pids[i] > 0) kill(worker_pids[i], SIGKILL);
            }
        }

        if (flag_sigchld || !state_running) {
            flag_sigchld = 0;
            int status; pid_t dead;
            while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
                active--;
                for (int i = 0; i < num_workers; i++) {
                    if (worker_pids[i] == dead) worker_pids[i] = 0;
                }
            }
        }
        char pipe_buf[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(control_pipe[0], pipe_buf, sizeof(pipe_buf) - 1)) > 0) {
            pipe_buf[bytes_read] = '\0'; 
            char *line = strtok(pipe_buf, "\n");
            
            while (line != NULL) {
                if (strncmp(line, "T5MSG", 5) == 0) {
                    int w_id; 
                    unsigned long long j, f, b;
                    
                    if (sscanf(line, "T5MSG type=JOB_DONE worker_id=%d jobs=%llu files=%llu bytes=%llu", 
                               &w_id, &j, &f, &b) == 4) {
                        if (w_id >= 0 && w_id < 32) {
                            w_jobs[w_id] = j; w_files[w_id] = f; w_bytes[w_id] = b;
                        }
                    } else if (strstr(line, "type=WORKER_EXITING")) {
                    }
                }
                line = strtok(NULL, "\n");
            }
        }

        int processed_any = 0;
        for (int i = 0; i < num_workers; i++) {
            while (sem_trywait(&shm->channels[i].sem_res_count) == 0) {
                sem_wait(&shm->channels[i].sem_res_mutex);
                file_result_t *res = &shm->channels[i].buffer[shm->channels[i].head];
                
                db_file_record_t rec; memset(&rec, 0, sizeof(rec));
                strncpy(rec.path, res->path, 4095);
                rec.size = res->size; rec.mtime = res->mtime;
                memcpy(rec.sha256, res->sha256, 32);
                
                if (fwrite(&rec, sizeof(db_file_record_t), 1, fdb) == 1) db_hdr.file_record_count++;
                
                shm->channels[i].head = (shm->channels[i].head + 1) % MAX_RESULTS_PER_CHANNEL;
                sem_post(&shm->channels[i].sem_res_mutex);
                sem_post(&shm->channels[i].sem_res_free);
                processed_any = 1;
            }
        }

        if (state_running) {
            sem_wait(&shm->header.sem_jobs_mutex);
            if (shm->header.pending_jobs == 0) {
                shm->header.termination_flag = 1;
                for (int k = 0; k < num_workers; k++) sem_post(&shm->header.sem_jobs_count);
            }
            sem_post(&shm->header.sem_jobs_mutex);
        }

        if (!processed_any) usleep(5000); 
    }

    for (int i = 0; i < num_workers; i++) {
        db_worker_stats_t ds; memset(&ds, 0, sizeof(ds));
        ds.worker_id = i; ds.pid = shm->stats[i].pid;
        ds.jobs_processed = shm->stats[i].jobs_processed;
        ds.files_emitted = shm->stats[i].files_emitted;
        ds.bytes_emitted = shm->stats[i].bytes_emitted;
        ds.user_cpu_us = shm->stats[i].user_cpu_us;
        ds.sys_cpu_us = shm->stats[i].sys_cpu_us;
        fwrite(&ds, sizeof(db_worker_stats_t), 1, fdb);
    }

    db_hdr.complete = state_running ? 1 : 0; 

    fseek(fdb, 0, SEEK_SET);
    fwrite(&db_hdr, sizeof(db_header_t), 1, fdb);
    fclose(fdb);
    rename(tmp_db, db_path);

    printf("Inventariere terminata. Complete: %d, Inregistrari: %u\n", db_hdr.complete, db_hdr.file_record_count);
    return 0;
}
