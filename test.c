#include "morloc.h"
#include <stdio.h>
#include <string.h>


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

    void* mem3 = shmalloc(8);
    strcpy((char*)mem3, "ruby");

    void* mem4 = shmalloc(512);
    strcpy((char*)mem4, "zzzbbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbzzzzbbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbb");

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
