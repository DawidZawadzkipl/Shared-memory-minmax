#include "common.h"

static int stats_min = 0;
static int stats_max = 0;
static pthread_mutex_t mtx_stats = PTHREAD_MUTEX_INITIALIZER;

// Termination flag
static int terminate = 0;
static pthread_mutex_t mtx_terminate = PTHREAD_MUTEX_INITIALIZER;


// User interface thread
void* ui_thread_func(void* arg) {
    struct query_t* query = (struct query_t*)arg;
    char command[10];

    printf("Server started. Commands: stat, reset, quit\n");

    while(1) {
        if(scanf("%9s", command) != 1) {
            continue;
        }
        if(strcmp(command, "quit") == 0) {
            pthread_mutex_lock(&mtx_terminate);
            terminate = 1;
            pthread_mutex_unlock(&mtx_terminate);
            sem_post(&query->sem_start);  // Wake up processing thread
            break;
        }
        else if(strcmp(command, "stat") == 0) {
            pthread_mutex_lock(&mtx_stats);
            printf("Min operations: %d, Max operations: %d\n",
                   stats_min, stats_max);
            pthread_mutex_unlock(&mtx_stats);
        }
        else if(strcmp(command, "reset") == 0) {
            pthread_mutex_lock(&mtx_stats);
            stats_min = 0;
            stats_max = 0;
            pthread_mutex_unlock(&mtx_stats);
            printf("Statistics reset.\n");
        }
        else {
            printf("Unknown command: %s\n", command);
        }
    }

    return NULL;
}

// Processing thread
void* processing_thread_func(void* arg) {
    struct query_t* query = (struct query_t*)arg;

    while(1) {
        // Wait for client request
        sem_wait(&query->sem_start);

        // Check for termination
        pthread_mutex_lock(&mtx_terminate);
        if(terminate) {
            pthread_mutex_unlock(&mtx_terminate);
            break;
        }
        pthread_mutex_unlock(&mtx_terminate);

        // Open client's shared memory
        int shm_data_id = shm_open(query->shm_data_name, O_RDONLY, 0444);
        if(shm_data_id == -1) {
            perror("shm_open client data");
            query->return_value = 0;
            sem_post(&query->sem_stop);
            continue;
        }

        // Map client's data
        int32_t* data = mmap(NULL, query->data_size * sizeof(int32_t), PROT_READ, MAP_SHARED, shm_data_id, 0);
        if(data == MAP_FAILED) {
            perror("mmap client data");
            close(shm_data_id);
            query->return_value = 0;
            sem_post(&query->sem_stop);
            continue;
        }

        // Find min/max
        if(query->data_size > 0) {
            int32_t result = data[0];

            for(int i = 1; i < query->data_size; i++) {
                if((query->type == min?data[i] < result:data[i]>result)){
                    result = data[i];
                }
            }

            query->return_value = result;

            // Update statistics
            pthread_mutex_lock(&mtx_stats);
            if(query->type == min) {
                stats_min++;
            } else {
                stats_max++;
            }
            pthread_mutex_unlock(&mtx_stats);

            printf("Processed %s request: result = %" PRId32 "\n", query->type == min ? "MIN" : "MAX", result);
        }

        // Cleanup
        munmap(data, query->data_size * sizeof(int32_t));
        close(shm_data_id);

        // Signal completion
        sem_post(&query->sem_stop);
    }

    return NULL;
}

int main() {
    // Cleaning up pre-existing resources
    sem_unlink(SEM_FREE_NAME);
    shm_unlink(SHM_QUERY_NAME);

    // Create server semaphore
    sem_t* sem_server = sem_open(SEM_FREE_NAME, O_CREAT | O_EXCL, 0666, 0);
    if(sem_server == SEM_FAILED) {
        perror("sem_open server");
        return 1;
    }

    // Create shared memory for queries
    int shm_query_id = shm_open(SHM_QUERY_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if(shm_query_id == -1) {
        perror("shm_open query");
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    if(ftruncate(shm_query_id, sizeof(struct query_t)) == -1) {
        perror("ftruncate");
        close(shm_query_id);
        shm_unlink(SHM_QUERY_NAME);
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    struct query_t* query = mmap(NULL, sizeof(struct query_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_query_id, 0);
    if(query == MAP_FAILED) {
        perror("mmap query");
        close(shm_query_id);
        shm_unlink(SHM_QUERY_NAME);
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    // Initialize semaphores in shared memory
    if(sem_init(&query->sem_start, 1, 0) == -1 ||
       sem_init(&query->sem_stop, 1, 0) == -1) {
        perror("sem_init");
        munmap(query, sizeof(struct query_t));
        close(shm_query_id);
        shm_unlink(SHM_QUERY_NAME);
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    // Create threads
    pthread_t ui_thread, processing_thread;

    if(pthread_create(&ui_thread, NULL, ui_thread_func, query) != 0) {
        perror("pthread_create ui");
        sem_destroy(&query->sem_start);
        sem_destroy(&query->sem_stop);
        munmap(query, sizeof(struct query_t));
        close(shm_query_id);
        shm_unlink(SHM_QUERY_NAME);
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    if(pthread_create(&processing_thread, NULL, processing_thread_func, query) != 0) {
        perror("pthread_create processing");
        pthread_mutex_lock(&mtx_terminate);
        terminate = 1;
        pthread_mutex_unlock(&mtx_terminate);
        pthread_join(ui_thread, NULL);
        sem_destroy(&query->sem_start);
        sem_destroy(&query->sem_stop);
        munmap(query, sizeof(struct query_t));
        close(shm_query_id);
        shm_unlink(SHM_QUERY_NAME);
        sem_close(sem_server);
        sem_unlink(SEM_FREE_NAME);
        return 1;
    }

    // Server is ready
    sem_post(sem_server);
    printf("Server is up\n");

    // Wait for threads to finish
    pthread_join(ui_thread, NULL);
    pthread_join(processing_thread, NULL);

    printf("\nShutting down server\n");

    // Cleanup
    sem_destroy(&query->sem_start);
    sem_destroy(&query->sem_stop);
    munmap(query, sizeof(struct query_t));
    close(shm_query_id);
    shm_unlink(SHM_QUERY_NAME);
    sem_close(sem_server);
    sem_unlink(SEM_FREE_NAME);

    printf("Server shutdown complete.\n");

    return 0;
}