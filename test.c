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

    size_t NEPOCHS = 64;
    size_t NLOOPS[7] = { 1000000, 100000, 10000,   1000,     100, 10,      1       };
    size_t MSIZE[7]  = {      10,    100,  1000,  10000,  100000, 1000000, 10000000 };

    shm_t* shm = shinit("memtest", 0, 128);

    size_t total_allocated_memory = 0;
    size_t current_allocated_memory = 0;

    printf("epoch nloop size efficiency talloc talloc_bytewise tfree tfree_byte\n");
    for(size_t epoch = 0; epoch < NEPOCHS; epoch++){

        for(size_t test = 0; test < 7; test++){ 

            size_t N = NLOOPS[test];
            size_t M = MSIZE[test];

            char* memories[N];

            clock_t start_time = clock();

            // allocate lots of memory sequentially
            for (int i = 0; i < N; i++) {
                int random_size = rand() % M + 1; // Random size between 1 and M bytes
                memories[i] = (char*)shmalloc(random_size);
                size_t new_memory = random_size + sizeof(block_header_t);
                total_allocated_memory += new_memory;
                current_allocated_memory += new_memory;
                memset(memories[i], 'a', random_size);
            }

            clock_t end_time = clock();
            double allocation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

            start_time = clock();

            // randomly free memory
            for (int i = 0; i < N; i++) {
                bool free_it = (rand() % 10) > 0; // free at probability 0.1
                if(free_it){
                    block_header_t* blk = (block_header_t*)((char*)memories[i] - sizeof(block_header_t));
                    current_allocated_memory -= sizeof(block_header_t) + blk->size;
                    shfree(memories[i]);
                }
            }

            end_time = clock();
            double freeing_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

            size_t shm_total_size = total_shm_size();

            double efficiency = (double)current_allocated_memory / (double)shm_total_size;

            printf(
                "%zu %zu %zu %.6f %.6f %.6f %.6f %.6f\n",
                epoch, N, M,
                efficiency,
                1000000 * allocation_time / N,
                1000000000 * allocation_time / N / M / 2,
                1000000 * freeing_time / N,
                1000000000 * freeing_time / N / M / 2);
        }

    }

    //// Uncomment this to stop before cleaning up the shm files
    //// This is useful if you want to read the files in /dev/shm
    /* char input[10];                            */
    /* while(1) {                                 */
    /*     printf("Enter 'q' to quit: ");         */
    /*     fgets(input, sizeof(input), stdin);    */
    /*     // Remove newline character if present */
    /*     input[strcspn(input, "\n")] = 0;       */
    /*     if (strcmp(input, "q") == 0) {         */
    /*         printf("Quitting...\n");           */
    /*         shclose();                         */
    /*         break;                             */
    /*     }                                      */
    /* }                                          */

    return 0;
}
