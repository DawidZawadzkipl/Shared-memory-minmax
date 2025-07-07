#include "common.h"

int main(int argc, char **argv) {
    //Args validation
    if(argc != 3){
        fprintf(stderr, "Invalid number of arguments\n");
        return 1;
    }

    if(strcmp(argv[2], "min") != 0 && strcmp(argv[2], "max") != 0){
        fprintf(stderr, "Error: Operation must be 'min' or 'max'\n");
        return 1;
    }

    // Try to connect to server
    sem_t* sem_server = sem_open(SEM_FREE_NAME, 0);
    if(sem_server == SEM_FAILED){
        fprintf(stderr, "Error: Server not running!\n");
        return 1;
    }

    // Open data file
    FILE* f = fopen(argv[1], "r");
    if(f == NULL){
        fprintf(stderr, "Error: Cannot open file '%s'\n", argv[1]);
        sem_close(sem_server);
        return 1;
    }

    // Count numbers in file
    int32_t temp = 0;
    int num_of_data = 0;
    while(fscanf(f, "%" SCNd32, &temp) == 1){
        num_of_data++;
    }

    if(num_of_data == 0){
        fprintf(stderr, "Error: No valid numbers found in file\n");
        fclose(f);
        sem_close(sem_server);
        return 1;
    }

    rewind(f);

    // Create shared memory for data
    char shm_client_name[30];
    snprintf(shm_client_name, sizeof(shm_client_name), "/shm_data_%d", getpid());

    int shm_data_id = shm_open(shm_client_name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if(shm_data_id == -1){
        perror("shm_open data");
        fclose(f);
        sem_close(sem_server);
        return 1;
    }

    if(ftruncate(shm_data_id, num_of_data * sizeof(int32_t)) == -1){
        perror("ftruncate");
        close(shm_data_id);
        shm_unlink(shm_client_name);
        fclose(f);
        sem_close(sem_server);
        return 1;
    }

    int32_t* data = mmap(NULL, num_of_data * sizeof(int32_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_data_id, 0);
    if(data == MAP_FAILED){
        perror("mmap data");
        close(shm_data_id);
        shm_unlink(shm_client_name);
        fclose(f);
        sem_close(sem_server);
        return 1;
    }

    // Read data into shared memory
    for (int i = 0; i < num_of_data; ++i) {
        if(fscanf(f, "%" SCNd32, &data[i]) != 1){
            fprintf(stderr, "Error reading data at position %d\n", i);
            break;
        }
    }
    fclose(f);

    // Wait for server to be free
    sem_wait(sem_server);

    // Connect to query shared memory
    int shm_query_id = shm_open(SHM_QUERY_NAME, O_RDWR, 0666);
    if(shm_query_id == -1){
        perror("shm_open query");
        munmap(data, num_of_data * sizeof(int32_t));
        close(shm_data_id);
        shm_unlink(shm_client_name);
        sem_post(sem_server);
        sem_close(sem_server);
        return 1;
    }

    struct query_t* query = mmap(NULL, sizeof(struct query_t), PROT_WRITE | PROT_READ, MAP_SHARED, shm_query_id, 0);
    if(query == MAP_FAILED){
        perror("mmap query");
        close(shm_query_id);
        munmap(data, num_of_data * sizeof(int32_t));
        close(shm_data_id);
        shm_unlink(shm_client_name);
        sem_post(sem_server);
        sem_close(sem_server);
        return 1;
    }

    // Prepare query
    strncpy(query->shm_data_name, shm_client_name, sizeof(query->shm_data_name) - 1);
    query->shm_data_name[sizeof(query->shm_data_name) - 1] = '\0';
    query->data_size = num_of_data;
    query->type = (strcmp(argv[2], "min") == 0) ? min : max;

    // Signal server to start processing
    sem_post(&query->sem_start);

    // Wait for result
    sem_wait(&query->sem_stop);

    // Print result
    printf("Value %s: %" PRId32 "\n", argv[2], query->return_value);

    // Release server
    sem_post(sem_server);

    // Cleanup
    munmap(query, sizeof(struct query_t));
    close(shm_query_id);
    munmap(data, num_of_data * sizeof(int32_t));
    close(shm_data_id);
    shm_unlink(shm_client_name);
    sem_close(sem_server);

    return 0;
}