#include "morloc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


 // [x] allocattion
 // [x] volume expansion
 // [x] freeing
 // [x] closing
 // [x] handle broken states and re-entrancy
 // [ ] message passing - make a separate exectutable that that listens and responds with random messages
 // [ ] locking

int main(int argc, char * argv[]){

    size_t NLOOPS = 10000;
    size_t MSIZE = 0x10;

    shm_t* shm = shinit("memtest", 0, 128);

    char* memories[NLOOPS];

    clock_t start_time = clock();

    // allocate lots of memory sequentially
    for (int i = 0; i < NLOOPS; i++) {
        int random_size = rand() % MSIZE + 1; // Random size between 1 and MSIZE bytes
        memories[i] = (char*)shmalloc(random_size);
        memset(memories[i], 'a', random_size);
    }

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Allocation loop took %.6f us per iteration (%.6f ns per byte)\n", NLOOPS, 1000000 * elapsed_time / NLOOPS, 1000000000 * elapsed_time / NLOOPS / MSIZE / 2);

    start_time = clock();

    // randomly free memory
    for (int i = 0; i < NLOOPS; i++) {
        bool free_it = (rand() % 10) > 0; // free at probability 0.1
        if(free_it){
            shfree(memories[i]);
        }
    }

    end_time = clock();
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Freeing loop took %.6f us per iteration (%.6f ns per byte)\n", NLOOPS, 1000000 * elapsed_time / NLOOPS, 1000000000 * elapsed_time / NLOOPS / MSIZE / 2);

    start_time = clock();

    // add new memory (fill the gaps)
    char* new_memories[NLOOPS];
    for (int i = 0; i < NLOOPS; i++) {
        int random_size = rand() % MSIZE + 1; // Random size between 1 and 20 bytes
        new_memories[i] = (char*)shmalloc(random_size);
        memset(new_memories[i], 'a', random_size);
    }

    end_time = clock();
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Second allocation loop took %.6f us per iteration (%.6f ns per byte)\n", NLOOPS, 1000000 * elapsed_time / NLOOPS, 1000000000 * elapsed_time / NLOOPS / MSIZE / 2);

    char input[10];
    while(1) {
        printf("Enter 'q' to quit: ");
        fgets(input, sizeof(input), stdin);

        // Remove newline character if present
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0) {
            printf("Quitting...\n");
            shclose();
            break;
        }
    }

    return 0;
}
