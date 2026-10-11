#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <openssl/sha.h>
#include <signal.h>
#include "../include/ipc_protocol.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

volatile sig_atomic_t worker_stop_flag = 0;
void handle_worker_sigterm(int sig) { (void)sig; worker_stop_flag = 1; }

void compute_sha256(const char *path, unsigned char *output) {
    FILE *f = fopen(path, "rb");
    if(!f) return;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t bytes;
    while((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) SHA256_Update(&ctx, buffer, bytes);
    SHA256_Final(output, &ctx);
    fclose(f);
}

int main(int argc, char *argv[]) {
    int worker_id = -1; char *ipc_path = NULL;
    int control_fd = -1; 

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--worker-id") == 0) worker_id = atoi(argv[++i]);
        if(strcmp(argv[i], "--ipc") == 0) ipc_path = argv[++i];
        if(strcmp(argv[i], "--control-fd") == 0) control_fd = atoi(argv[++i]); 
    }
    
    if (worker_id == -1 || ipc_path == NULL) return 1;

    signal(SIGTERM, handle_worker_sigterm);
    signal(SIGPIPE, SIG_IGN); 

    int fd = open(ipc_path, O_RDWR);
    if (fd < 0) return 1;
    shared_memory_t *shm = mmap(NULL, sizeof(shared_memory_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    while(1) {
        if (worker_stop_flag) break; 
        inventory_job_t job;
        sem_wait(&shm->header.sem_jobs_count);
        
        if (worker_stop_flag) {
            sem_post(&shm->header.sem_jobs_count);
            break;
        }

        sem_wait(&shm->header.sem_jobs_mutex);
        
        if(shm->header.termination_flag && shm->header.pending_jobs == 0) { 
            sem_post(&shm->header.sem_jobs_mutex); break; 
        }
        
        job = shm->job_queue[shm->job_head];
        shm->job_head = (shm->job_head + 1) % MAX_JOBS;
        shm->header.pending_jobs--;
        sem_post(&shm->header.sem_jobs_mutex);
        sem_post(&shm->header.sem_jobs_free);

        DIR *dir = opendir(job.path);
        if(!dir) continue;
        struct dirent *entry;
        while((entry = readdir(dir))) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            if (shm->header.simulate_work_ms > 0) usleep(shm->header.simulate_work_ms * 1000);

            char full_path[4096];
            size_t p_len = strlen(job.path);
            size_t n_len = strlen(entry->d_name);
            
            if (p_len + n_len + 2 > 4096) continue;
    
            memcpy(full_path, job.path, p_len);
            full_path[p_len] = '/';
            memcpy(full_path + p_len + 1, entry->d_name, n_len);
            full_path[p_len + 1 + n_len] = '\0';

            struct stat st;
            if(lstat(full_path, &st) != 0) continue;
            if(S_ISLNK(st.st_mode)) continue;

            if(S_ISDIR(st.st_mode)) {
                if (shm->header.max_depth == -1 || job.depth < shm->header.max_depth) {
                    sem_wait(&shm->header.sem_jobs_free);
                    sem_wait(&shm->header.sem_jobs_mutex);
                    size_t final_len = strlen(full_path);
                    if (final_len > 4095) final_len = 4095;
                    memset(shm->job_queue[shm->job_tail].path, 0, 4096);
                    memcpy(shm->job_queue[shm->job_tail].path, full_path, final_len);
                    
                    shm->job_queue[shm->job_tail].depth = job.depth + 1;
                    shm->job_tail = (shm->job_tail + 1) % MAX_JOBS;
                    shm->header.pending_jobs++;
                    sem_post(&shm->header.sem_jobs_mutex);
                    sem_post(&shm->header.sem_jobs_count);
                }
            } else if(S_ISREG(st.st_mode)) {
                sem_wait(&shm->channels[worker_id].sem_res_free);
                sem_wait(&shm->channels[worker_id].sem_res_mutex);
                file_result_t *res = &shm->channels[worker_id].buffer[shm->channels[worker_id].tail];
                size_t final_len = strlen(full_path);
                if (final_len > 4095) final_len = 4095;
                memset(res->path, 0, 4096);
                memcpy(res->path, full_path, final_len);
                
                res->size = st.st_size; 
                res->mtime = st.st_mtime; 
                res->uid = st.st_uid; 
                res->gid = st.st_gid; 
                res->mode = st.st_mode;
                
                compute_sha256(full_path, res->sha256);
                
                shm->channels[worker_id].tail = (shm->channels[worker_id].tail + 1) % MAX_RESULTS_PER_CHANNEL;
                shm->stats[worker_id].files_emitted++;
                shm->stats[worker_id].bytes_emitted += (uint64_t)st.st_size;
                sem_post(&shm->channels[worker_id].sem_res_mutex);
                sem_post(&shm->channels[worker_id].sem_res_count);
            }
        }
        closedir(dir);
        shm->stats[worker_id].jobs_processed++;

        if (control_fd != -1) {
            char msg[256];
            int len = snprintf(msg, sizeof(msg), "T5MSG type=JOB_DONE worker_id=%d jobs=%llu files=%llu bytes=%llu\n", 
                               worker_id, 
                               (unsigned long long)shm->stats[worker_id].jobs_processed,
                               (unsigned long long)shm->stats[worker_id].files_emitted,
                               (unsigned long long)shm->stats[worker_id].bytes_emitted);
            write(control_fd, msg, len); 
        }
    }

    if (control_fd != -1) {
        char exit_msg[256];
        int len = snprintf(exit_msg, sizeof(exit_msg), "T5MSG type=WORKER_EXITING reason=shutdown worker_id=%d\n", worker_id);
        write(control_fd, exit_msg, len);
        close(control_fd);
    }

    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        shm->stats[worker_id].user_cpu_us = usage.ru_utime.tv_sec * 1000000ULL + usage.ru_utime.tv_usec;
        shm->stats[worker_id].sys_cpu_us = usage.ru_stime.tv_sec * 1000000ULL + usage.ru_stime.tv_usec;
    }

    shm->stats[worker_id].pid = getpid();
    return 0;
}
