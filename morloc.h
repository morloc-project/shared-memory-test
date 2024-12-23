#ifndef __MORLOC_SHM_H__
#define __MORLOC_SHM_H__

// ===== morloc shared library pool handling =====

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>



#define SHM_MAGIC 0xFECA0DF0
#define BLK_MAGIC 0x0CB10DF0

#define MAX_FILENAME_SIZE 128
#define MAX_VOLUME_NUMBER 32

// An index into a multi-volume shared memory pool
typedef ssize_t relptr_t; 

// An index into a single volume. 0 is the start of the first block immediately
// following the shm object.
typedef ssize_t volptr_t; 

// An absolute pointer to system memory
typedef void* absptr_t; 

#define VOLNULL -1
#define RELNULL -1

typedef struct shm_s {
  // A constant identifying this as a morloc shared memory file
  unsigned int magic;

  // The name of this volume. Used for creating debugging messages.
  char volume_name[MAX_FILENAME_SIZE];

  // A memory pool will consist of one or more volumes. They all share the same
  // base name (volume_name) followed by an underscore and their index. E.g.,
  // `${TMPDIR}/morloc_shm_0`, `${TMPDIR}/morloc_shm_1`, etc.
  int volume_index;

  // The number of bytes that can be stored in this volume. This number will be
  // used to calculate relative offsets. Pools may pass relative pointers shared
  // objects. The pointer will first be checked against the first shared memory
  // volume. If this volume is smaller than the pointer, volume_size will be
  // subtracted from the pointer and it will be checked against the next volume.
  size_t volume_size;

  // There may be many shared memory volumes. If this is the first volume, its
  // relative offset will be 0. Otherwise, it will be the sum of prior volume
  // sizes. Clients do not know the size of the volume or the number of
  // volumes. From their point of view, there is one infinite memory pool and
  // the relative pointers they use point to positions in that pool.
  size_t relative_offset;

  // A global lock that allows many readers but only one writer at a time
  pthread_rwlock_t rwlock;

  // Pointer to the current free memory block header
  volptr_t cursor;
} shm_t;

typedef struct block_header_s {
    // a constant magic number identifying a block header
    unsigned int magic;
    // the number of references to this block
    unsigned int reference_count;
    // the amount of memory that is stored in the header
    size_t size;
} block_header_t;


// The index of the current volum
static size_t current_volume = 0;
static char common_basename[MAX_FILENAME_SIZE];

static shm_t* volumes[MAX_VOLUME_NUMBER] = {NULL};

shm_t* shinit(const char* shm_basename, size_t volume_index, size_t shm_size);
void shclose();
void* shmalloc(size_t size);
void* shmemcpy(void* dest, size_t size);
int shfree(void* ptr);
void* shcalloc(size_t nmemb, size_t size);
void* shrealloc(void* ptr, size_t size);

volptr_t rel2vol(relptr_t ptr);
absptr_t rel2abs(relptr_t ptr);
relptr_t vol2rel(volptr_t ptr, shm_t* shm);
absptr_t vol2abs(volptr_t ptr, shm_t* shm);
volptr_t abs2vol(absptr_t ptr, shm_t* shm);
relptr_t abs2rel(absptr_t ptr);
shm_t* abs2shm(absptr_t ptr);
block_header_t* abs2blk(void* ptr);


//             head of volume 1   inter-memory    head of volume 2
//              n=6  block1          n=20                block2
//         ---xxxxxx........--------------------xxxxxx............---->
//         |  |     |      |                    |     |          |
//         v  v     v      v                    v     v          v
// absptr  0  4    10     17                   38    44         55
// volptr           0      7                          0         10
// relptr           0      7                          8         19
//
//  * the volumes may not be ordered in memory
//    e.g., block2 may be less then block1
//  * absptr will vary between processes, it is the virtual pointer used in
//    process memory and mmap'd to the shared data structure.
//  * relptr will be shared between processes
//  * relptr abstracts away volumes, viewing the shared memory pool as a
//    single contiguous block

volptr_t rel2vol(relptr_t ptr) {
    for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
        shm_t* shm = volumes[i];
        if (shm == NULL) {
            return VOLNULL;
        }
        if ((size_t)ptr < shm->volume_size) {
            return (volptr_t)ptr;
        }
        ptr -= (relptr_t)shm->volume_size;
    }
    return VOLNULL;
}

absptr_t rel2abs(relptr_t ptr) {
    for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
        shm_t* shm = volumes[i];
        if (shm == NULL) {
            return NULL;
        }
        if ((size_t)ptr < shm->volume_size) {
            char* shm_start = (char*)shm;
            return (absptr_t)(shm_start + sizeof(shm_t) + ptr);
        }
        ptr -= (relptr_t)shm->volume_size;
    }
    return NULL;
}

relptr_t vol2rel(volptr_t ptr, shm_t* shm) {
    return (relptr_t)(shm->relative_offset + ptr);
}

absptr_t vol2abs(volptr_t ptr, shm_t* shm) {
    char* shm_start = (char*)shm;
    return (absptr_t)(shm_start + sizeof(shm_t) + ptr);
}

volptr_t abs2vol(absptr_t ptr, shm_t* shm) {
    if (shm) {
        char* shm_start = (char*)shm;
        char* data_start = shm_start + sizeof(shm_t);
        char* data_end = data_start + shm->volume_size;
        
        if ((char*)ptr >= data_start && (char*)ptr < data_end) {
            return (volptr_t)((char*)ptr - data_start);
        } else {
            return VOLNULL;
        }
    } else {
        for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
            shm_t* current_shm = volumes[i];
            if (current_shm) {
                char* shm_start = (char*)current_shm;
                char* data_start = shm_start + sizeof(shm_t);
                char* data_end = data_start + current_shm->volume_size;
                
                if ((char*)ptr >= data_start && (char*)ptr < data_end) {
                    return (volptr_t)((char*)ptr - data_start);
                }
            }
        }
    }
    return VOLNULL;
}

relptr_t abs2rel(absptr_t ptr) {
    for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
        shm_t* shm = volumes[i];
        if (shm == NULL) {
            continue;
        }
        char* shm_start = (char*)shm;
        char* data_start = shm_start + sizeof(shm_t);
        char* data_end = data_start + shm->volume_size;
        
        if ((char*)ptr > shm_start && (char*)ptr < data_end) {
            return (relptr_t)((char*)ptr - data_start + shm->relative_offset);
        }
    }
    return RELNULL;
}

shm_t* abs2shm(absptr_t ptr) {
    for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
        shm_t* shm = volumes[i];
        if (shm == NULL) {
            continue;
        }
        char* shm_start = (char*)shm;
        char* data_start = shm_start + sizeof(shm_t);
        char* data_end = data_start + shm->volume_size;
        
        if ((char*)ptr >= data_start && (char*)ptr < data_end) {
            return shm;
        }
    }
    return NULL;
}



// Get and check a block header given a pointer to the beginning of a block's
// data section
block_header_t* abs2blk(void* ptr){
    block_header_t* blk = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    if(blk && blk->magic != BLK_MAGIC){
        return NULL;
    }
    return blk;
}

shm_t* shinit(const char* shm_basename, size_t volume_index, size_t shm_size) {
    // Calculate the total size needed for the shared memory segment
    size_t full_size = shm_size + sizeof(shm_t);
    
    // Prepare the shared memory name
    char shm_name[MAX_FILENAME_SIZE];
    snprintf(shm_name, sizeof(shm_name), "%s_%zu", shm_basename, volume_index);
    shm_name[MAX_FILENAME_SIZE - 1] = '\0'; // ensure the name is NULL terminated

    // Set the global basename, this will be used to name future volumes
    strncpy(common_basename, shm_basename, MAX_FILENAME_SIZE - 1);
    
    // Create or open a shared memory object
    // O_RDWR: Open for reading and writing
    // O_CREAT: Create if it doesn't exist
    // 0666: Set permissions (rw-rw-rw-)
    int fd = shm_open(shm_name, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("shm_open");
        return NULL;
    }

    // Map the shared memory object into the process's address space
    volumes[volume_index] = (shm_t*)mmap(NULL, full_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (volumes[volume_index] == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    // Get information about the shared memory object
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return NULL;
    }

    // Check if we've just created the shared memory object
    bool created = (sb.st_size == 0);
    if (created && ftruncate(fd, full_size) == -1) {
        // Set the size of the shared memory object
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    // Adjust sizes based on whether we created a new object or opened an existing one
    full_size = created ? full_size : sb.st_size;
    shm_size = full_size - sizeof(shm_t);

    // Cast the mapped memory to our shared memory structure
    shm_t* shm = (shm_t*)volumes[volume_index];
    if (created) {
        // Initialize the shared memory structure
        shm->magic = SHM_MAGIC;
        strncpy(shm->volume_name, shm_name, sizeof(shm->volume_name) - 1);
        shm->volume_name[sizeof(shm->volume_name) - 1] = '\0';
        shm->volume_index = volume_index;
        shm->relative_offset = 0;
        
        // Calculate the relative offset based on previous volumes
        // POTENTIAL ISSUE: This assumes volumes[] is initialized and accessible
        for (size_t i = 0; i < volume_index; i++) {
            shm->relative_offset += volumes[i]->volume_size;
        }
        
        // volume size does not count the shm header
        shm->volume_size = shm_size;
        
        // Initialize the read-write lock
        if (pthread_rwlock_init(&shm->rwlock, NULL) != 0) {
            perror("pthread_rwlock_init");
            munmap(volumes[volume_index], full_size);
            close(fd);
            return NULL;
        }
        shm->cursor = 0;

        // Initialize the first block header
        block_header_t* first_block = (block_header_t*)(shm + 1);
        first_block->magic = BLK_MAGIC;
        first_block->reference_count = 0;
        // block size does not count the block headers
        first_block->size = shm_size - sizeof(block_header_t);
    }

    close(fd);
    return shm;
}



void shclose() {
    for (int i = 0; i < MAX_VOLUME_NUMBER; i++) {
        if (volumes[i] != NULL) {
            // Get the name of the shared memory object
            char shm_name[MAX_FILENAME_SIZE];
            strncpy(shm_name, volumes[i]->volume_name, MAX_FILENAME_SIZE);

            // Unmap the shared memory
            size_t full_size = volumes[i]->volume_size + sizeof(shm_t);
            if (munmap(volumes[i], full_size) == -1) {
                perror("munmap");
                // Continue with other volumes even if this one fails
            }

            // Mark the shared memory object for deletion
            if (shm_unlink(shm_name) == -1) {
                perror("shm_unlink");
                // Continue with other volumes even if this one fails
            }

            // Set the pointer to NULL to indicate it's no longer valid
            volumes[i] = NULL;
        }
    }
}


size_t get_available_memory() {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo == NULL) return -1;

    char line[256];
    size_t total_memory = 0;

    while (fgets(line, sizeof(line), meminfo)) {
        if (sscanf(line, "MemAvailable: %zu kB", &total_memory) == 1) {
            break;
        }
    }

    fclose(meminfo);
    return total_memory * 1024; // convert from kB to bytes
}


static size_t choose_next_volume_size(size_t new_data_size) {
    size_t total_shm_size = 0;
    size_t last_shm_size = 0;
    size_t new_volume_size;
    size_t minimum_required_size = sizeof(shm_t) + sizeof(block_header_t) + new_data_size;

    // Iterate through volumes to calculate total and last shared memory sizes
    for (size_t i = 0; i < MAX_VOLUME_NUMBER; i++) {
        shm_t* shm = volumes[i];
        if (!shm) break;
        total_shm_size += shm->volume_size;
        last_shm_size = shm->volume_size;
    }

    size_t available_memory = get_available_memory();

    // Check if there's enough memory for the new data
    if (minimum_required_size > available_memory) {
        fprintf(stderr, "Insufficient memory for new data size\n");
        return 0;
    }

    // Determine the new volume size based on available memory and existing volumes
    if (total_shm_size < available_memory && minimum_required_size < total_shm_size) {
        new_volume_size = total_shm_size;
    } else if (last_shm_size < available_memory && minimum_required_size < last_shm_size) {
        new_volume_size = last_shm_size;
    } else {
        new_volume_size = minimum_required_size;
    }

    return new_volume_size;
}




block_header_t* get_block(shm_t* shm, ssize_t cursor){
    if (shm == NULL) {
        perror("Shared memory pool is not defined");
        return NULL;
    }

    // This will occur when a volume is filled, it does not necessarily mean
    // there is no space in the volume, but new space will need to be sought.
    if (cursor == -1){
        return NULL;
    }

    block_header_t* blk = (block_header_t*) vol2abs(cursor, shm);

    if(blk->magic != BLK_MAGIC){
        perror("Missing BLK_MAGIC - corrupted memory");
        return NULL;
    }

    return blk;
}


block_header_t* find_free_block_in_volume(shm_t* shm, size_t size) {
    if (shm == NULL || size == 0) {
        return NULL;
    }

    block_header_t* blk = get_block(shm, shm->cursor);

    if (blk != NULL && blk->size >= size + sizeof(block_header_t) && blk->reference_count == 0) {
        return blk;
    }

    blk = get_block(shm, 0);
    if (blk == NULL) {
        return NULL;
    }

    char* shm_end = (char*)shm + sizeof(shm_t) + shm->volume_size;

    // Lock this pool while searching for a non-cursor block. This is necessary
    // since adjacent free blocks will be merged, which could potentially lead to
    // conflicts.
    if (pthread_rwlock_wrlock(&shm->rwlock) != 0) {
        perror("Failed to acquire write lock");
        return NULL;
    }

    while ((char*)blk + sizeof(block_header_t) + size <= shm_end) {
        if (blk->magic != BLK_MAGIC) {
            pthread_rwlock_unlock(&shm->rwlock);
            fprintf(stderr, "Corrupted memory: invalid block magic\n");
            return NULL;
        }

        // Merge all following free blocks
        while (blk->reference_count == 0) {
            block_header_t* next_blk = (block_header_t*)((char*)blk + sizeof(block_header_t) + blk->size);
            
            if ((char*)next_blk >= shm_end || next_blk->reference_count != 0) {
                break;
            }

            // Merge the blocks
            blk->size += sizeof(block_header_t) + next_blk->size;
            memset(next_blk, 0, sizeof(block_header_t) + next_blk->size);
        }

        if (blk->reference_count == 0 && blk->size >= size) {
            pthread_rwlock_unlock(&shm->rwlock);
            return blk;
        }

        blk = (block_header_t*)((char*)blk + sizeof(block_header_t) + blk->size);
    }

    pthread_rwlock_unlock(&shm->rwlock);
    return NULL;  // No suitable space was found in this volume
}


// Find a free block that can allocate a given size of memory. If no lbock is
// found, create a new volume.
static block_header_t* find_free_block(size_t size, shm_t** shm_ptr) {
    block_header_t* blk;
    shm_t* shm = volumes[current_volume];
    if (shm != NULL) {
        blk = get_block(shm, shm->cursor);

        if(blk->size >= size + sizeof(block_header_t)){
            goto success;
        }
    }

    // If no suitable block is found at the cursor, search through all volumes
    // for a suitable block, merging free blocks as they are observed
    for(size_t i = 0; i < MAX_VOLUME_NUMBER; i++){
      shm = volumes[i]; 

      // If no block is found in any volume, create a new volume
      if(!shm){
        size_t new_volume_size = choose_next_volume_size(size);
        shm = shinit(common_basename, i, new_volume_size);
        blk = (block_header_t*)(shm + 1);
      }

      blk = find_free_block_in_volume(shm, size);
      if(blk) goto success;
    }

    perror("Could not find suitable block");
    return NULL;

success:
    *shm_ptr = shm;
    return blk;
}

static block_header_t* split_block(shm_t* shm, block_header_t* old_block, size_t size) {
    if (old_block->reference_count > 0){
        perror("Cannot split block since reference_count > 0");
        return NULL;
    }
    if (old_block->size == size){
        // hello goldilocks, this block is just the right size
        return old_block;
    }
    if (old_block->size < size){
        perror("This block is too small");
        return NULL;
    }

    // lock memory in this pool
    pthread_rwlock_wrlock(&shm->rwlock);

    size_t remaining_free_space = old_block->size - size;
    old_block->size = size;

    block_header_t* new_free_block = (block_header_t*)((char*)old_block + sizeof(block_header_t) + size);
    ssize_t new_cursor = abs2vol(new_free_block, shm);

    // if there is enough free space remaining to create a new block, do so
    if (remaining_free_space > sizeof(block_header_t)){
        // start of the new free block
        shm->cursor = new_cursor;
        new_free_block->magic = BLK_MAGIC;
        new_free_block->reference_count = 0;
        new_free_block->size = remaining_free_space - sizeof(block_header_t);
    } else {
        old_block->size += remaining_free_space;
        memset((void*)new_free_block, 0, remaining_free_space);
        shm->cursor = -1; 
    }

    pthread_rwlock_unlock(&shm->rwlock);

    return old_block;
}


void* shmalloc(size_t size) {
    // Can't allocate nothing ... though technically I could make a 0-sized
    // block, but why?
    if (size == 0)
        return NULL;

    shm_t* shm;
    // find a block with sufficient free space
    block_header_t* blk = find_free_block(size, &shm);

    // If a suitable block is found
    if (blk) {
        // trim the block down to size and reset the cursor to the next free block
        block_header_t* final_blk = split_block(shm, blk, size);
        final_blk->reference_count++;
        return (void*)(final_blk + 1);
    } else {
        // No suitable block found
        return NULL;
    }
}

void* shmemcpy(void* dest, size_t size){
    void* src = shmalloc(size);
    memmove(dest, src, size);
    return src;
}

void* shcalloc(size_t nmemb, size_t size) {
    size_t total_size;

    total_size = nmemb * size;
    if (nmemb != 0 && total_size / nmemb != size)
        return NULL; // Check for overflow

    shm_t* shm;
    block_header_t* block = find_free_block(size, &shm);

    if(!block){
        perror("No free memory");
        return NULL;
    }

    void* ptr = split_block(shm, block, size);

    if (ptr){
        pthread_rwlock_wrlock(&shm->rwlock);
        memset(ptr, 0, total_size);
        pthread_rwlock_unlock(&shm->rwlock);
    } else {
        perror("Failed to split memory block");
    }

    return ptr;
}


void* shrealloc(void* ptr, size_t size) {
    block_header_t* blk = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    shm_t* shm = abs2shm(ptr);
    void* new_ptr;

    if (!ptr) return shmalloc(size);

    if (size == 0) {
        shfree(ptr);
        return NULL;
    }

    if (blk->size >= size) {
        // The current block is large enough
        return split_block(shm, blk, size);
    } else {
        // Need to allocate a new block
        new_ptr = shmalloc(size);
        if (new_ptr) {
            pthread_rwlock_wrlock(&shm->rwlock);
            memcpy(new_ptr, ptr, blk->size);
            pthread_rwlock_unlock(&shm->rwlock);
            shfree(ptr);
        }
        return new_ptr;
    }

    return new_ptr;
}


// Free a chunk of memory. The pointer points to the start of the memory that
// the user is given relative to the user's process. The block header is just
// upstream of this position.
//
// return 0 for success
int shfree(absptr_t ptr) {
    block_header_t* blk = (block_header_t*)(ptr - sizeof(block_header_t));

    if(!blk){
      perror("Out-of-bounds relative pointer");
      return 1;
    }

    if(blk->magic != BLK_MAGIC){
      perror("Corrupted memory");
      return 1;
    }

    if(blk->reference_count == 0){
      perror("Cannot free memory, reference count is already 0");
      return 1;
    }

    // This is an atomic operation, so no need to lock
    blk->reference_count--;

    // Set memory to 0, this may be perhaps be removed in the future for
    // performance sake, but for now it helps with diagnostics. Note that the
    // head remains, it may be used to merge free blocks in the future.
    memset(blk + 1, 0, blk->size);

    return 0;
}

#endif
