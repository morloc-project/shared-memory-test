#include "morloc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


 // [x] allocattion
 // [x] volume expansion
 // [ ] freeing
 // [x] closing
 // [ ] copying
 // [ ] handle broken states and re-entrancy
 // [ ] message passing

int main(int argc, char * argv[]){

    shm_t* shm = shinit("memtest", 0, 128);

    void* mem1 = shmalloc(8);
    strcpy((char*)mem1, "gattaca");

    void* mem2 = shmalloc(8);
    strcpy((char*)mem2, "gandalf");

    shfree(mem2);

    void* mem3 = shmalloc(8);
    strcpy((char*)mem3, "ruby");

    shfree(mem3);

    void* mem4 = shmalloc(64);
    strcpy((char*)mem4, "zzzzzzzbbbbbbbbaaaaaaabbbbbbbbaaaaaaabbbbbbbbaaaaaaabbbbbbbbxx");
    shfree(mem1);
    void* mem5 = shmalloc(8);
    strcpy((char*)mem5, "uuuuuuu");


    clock_t start_time = clock();

    size_t N = 0x203;
    char* memories[N];

    // allocate lots of memory sequentially
    for (int i = 0; i < N; i++) {
        int random_size = rand() % 32 + 1; // Random size between 1 and 20 bytes
        memories[i] = (char*)shmalloc(random_size);
        memset(memories[i], 'a', random_size);
    }

    // randomly free memory
    for (int i = 0; i < N; i++) {
        bool free_it = (rand() % 10) > 0; // free at probability 0.1
        if(free_it){
            shfree(memories[i]);
        }
    }

    // add new memory (fill the gaps)
    char* new_memories[N];
    for (int i = 0; i < N; i++) {
        int random_size = rand() % 32 + 1; // Random size between 1 and 20 bytes
        new_memories[i] = (char*)shmalloc(random_size);
        memset(new_memories[i], 'a', random_size);
    }

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Random memory writing loop took %.6f seconds\n", elapsed_time);

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
