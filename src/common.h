#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define SEM_FREE_NAME "/sem_free"
#define SHM_QUERY_NAME "/shm_query"

#define SHM_NAME_SIZE 30
#define COMMAND_BUFFER_SIZE 10

// Query types
enum find_type {
    max = 0,
    min = 1
};

// Query struct for client-server communication
struct query_t {
    enum find_type type;
    char shm_data_name[SHM_NAME_SIZE];
    int data_size;
    int32_t return_value;
    sem_t sem_start;    // Semaphore to signal start of processing
    sem_t sem_stop;     // Semaphore to signal end of processing
};

#endif // COMMON_H