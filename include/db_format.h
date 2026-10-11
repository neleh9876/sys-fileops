#ifndef DB_FORMAT_H
#define DB_FORMAT_H

#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#define SNAPSHOT_OPEN 0
#define SNAPSHOT_SEALED 1

#define MAGIC_INDEX "IDX_I"
#define MAGIC_PROC  "PRCI"

#define FORMAT_VERSION 1

typedef struct {
    char magic[8];           
    int format_version;     
    time_t snapshot_id;      
    int snapshot_state;      
    int active_writers;      
    int record_count;        
} DB_Header;

#define MAX_PATH_LEN 4096
#define MAX_COMM_LEN 256
#define MAX_CMDLINE_LEN 1024

typedef struct {
    char path[MAX_PATH_LEN]; 
    int type;                
    off_t size;              
    time_t mtime;          
    uint32_t checksum;    
    dev_t st_dev;            
    ino_t st_ino;        
    char symlink_target[MAX_PATH_LEN]; 
} FileRecord;

typedef struct {
    pid_t pid;              
    pid_t ppid;          
    char state;              
    char comm[MAX_COMM_LEN]; 
    char cmdline[MAX_CMDLINE_LEN]; 
    long rss;             
    long cpu_time;           
} ProcRecord;

#define DB_MAGIC "INV4"       
#define FORMAT_VERSION_T4 1    

typedef struct {
    char magic[4];
    uint32_t format_version;
    uint32_t complete;          
    uint32_t file_record_count; 
    uint32_t worker_count;     
} db_header_t;

typedef struct {
    char path[4096];         
    uint64_t size;              
    uint64_t mtime;          
    uint32_t mode;              
    uint32_t uid;              
    uint32_t gid;               
    unsigned char sha256[32];   
} db_file_record_t;

typedef struct {
    uint32_t worker_id;
    pid_t pid;
    int exit_status;
    uint32_t jobs_processed;
    uint32_t files_emitted;
    uint64_t bytes_emitted;
    uint64_t wall_time_ms;
    uint64_t user_cpu_us;
    uint64_t sys_cpu_us;
} db_worker_stats_t;

#endif
