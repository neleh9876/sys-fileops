#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <semaphore.h>
#include <stdint.h>

#define MAX_JOBS 1024
#define MAX_RESULTS_PER_CHANNEL 256
#define IPC_MAGIC 0x49504334
#define IPC_VERSION 1

typedef struct {
    char path[4096];
    int depth; 
} inventory_job_t;

typedef struct {
    char path[4096];
    uint64_t size;
    time_t mtime;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    unsigned char sha256[32];
} file_result_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int num_workers;
    int max_depth;        
    int simulate_work_ms;   
    int termination_flag;
    int pending_jobs;
    sem_t sem_jobs_mutex;
    sem_t sem_jobs_count;
    sem_t sem_jobs_free;
    sem_t sem_stats_mutex;
} shared_memory_header_t;

typedef struct {
    sem_t sem_res_mutex;
    sem_t sem_res_count;
    sem_t sem_res_free;
    file_result_t buffer[MAX_RESULTS_PER_CHANNEL];
    int head;
    int tail;
} worker_channel_t;

typedef struct {
    uint32_t pid;
    uint32_t exit_status;
    uint64_t jobs_processed;
    uint64_t files_emitted;
    uint64_t bytes_emitted;
    uint64_t user_cpu_us;
    uint64_t sys_cpu_us;
} worker_stats_t;

typedef struct {
    shared_memory_header_t header;
    inventory_job_t job_queue[MAX_JOBS];
    int job_head;
    int job_tail;
    worker_channel_t channels[32]; 
    worker_stats_t stats[32];
} shared_memory_t;

#endif
