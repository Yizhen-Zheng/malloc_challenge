
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Interfaces to get memory pages from OS

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//10 bins for 2's power of size
#define BIN_NUMBER 10

// Struct definitions

typedef struct metadata_t {
  size_t size;
  struct metadata_t *next;
  struct metadata_t *prev;
} metadata_t;

typedef struct bin_t {
  metadata_t dummy_head;
  metadata_t dummy_tail; 
} bin_t;

typedef struct heap_t {
  // an array of bins
  bin_t bins[BIN_NUMBER];
} heap_t;

// Static variables (DO NOT ADD ANOTHER STATIC VARIABLES!)
heap_t my_heap;


// Helper functions (feel free to add/remove/edit!)

int get_bin_index(size_t size) {
  if (size <= 8) return 0;   // bin 0: 0-8 bytes
  if (size <= 16) return 1;  // bin 1: 8-16 bytes
  if (size <= 32) return 2;  // bin 2: 16-32 bytes
  if (size <= 64) return 3;  // bin 3: 32-64 bytes
  if (size <= 128) return 4; // bin 4: 64-128 bytes
  if (size <= 256) return 5; // bin 5: 128-256 bytes
  if (size <= 512) return 6; // bin 6: 256-512 bytes
  if (size <= 1024) return 7; // bin 7: 512-1024 bytes
  if (size <= 2048) return 8; // bin 8: 1024-2048 bytes
  return 9; // bin 9: 2048-4096 bytes (and above)
}

void my_add_to_free_list(metadata_t *metadata) {
  assert(!metadata->next);
  int bin_idx = get_bin_index(metadata->size);
  bin_t *bin = &my_heap.bins[bin_idx];
  metadata->next = bin->dummy_head.next; //the next is a pointer
  metadata->prev = &bin->dummy_head; // the dummy_head is an actuall value, so need &
  bin->dummy_head.next->prev = metadata;
  bin->dummy_head.next = metadata;
}

void my_remove_from_free_list(metadata_t *metadata) {
  // reconnect
  metadata->prev->next = metadata->next;
  metadata->next->prev = metadata->prev;
  //ensure we don't creat a circle when latter put it back
  metadata->next = NULL;
  metadata->prev = NULL;
}


// Interfaces of malloc (DO NOT RENAME FOLLOWING FUNCTIONS!)

// This is called at the beginning of each challenge.
void my_initialize() {
  for (int i = 0; i < BIN_NUMBER; i++){
    my_heap.bins[i].dummy_head.size = 0;
    my_heap.bins[i].dummy_tail.size = 0;
    my_heap.bins[i].dummy_head.next = &my_heap.bins[i].dummy_tail;
    my_heap.bins[i].dummy_tail.prev = &my_heap.bins[i].dummy_head;
    my_heap.bins[i].dummy_tail.next = NULL;
    my_heap.bins[i].dummy_head.prev = NULL;
  }
}

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <=
// 4000. You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().
void *my_malloc(size_t size) {
  int bin_idx=get_bin_index(size);
  metadata_t *metadata = my_heap.bins[bin_idx].dummy_head.next;
  metadata_t *best_slot=NULL; // a pointer variable to keep watch the current best fit

  while (metadata != &my_heap.bins[bin_idx].dummy_tail ) {
    if (metadata->size >= size){
      if (!best_slot || best_slot->size > metadata->size){ 
        // update best_slot if found a fitter metadata
        best_slot=metadata;
      }
    }
    metadata = metadata->next;
  }

  if (!best_slot) {
    // cannot find free slot available. request a new memory region from the system by calling mmap_from_system().
    // buffer_size: metadata + free_slot
    size_t buffer_size = 4096;
    metadata_t *new_metadata = (metadata_t *)mmap_from_system(buffer_size); //request a new block of 4096 
    new_metadata->size = size;
    new_metadata->next = NULL;
    new_metadata->prev = NULL;

    size_t new_remain_size = buffer_size - size;
    if(new_remain_size > sizeof(metadata_t)){
      void *end_of_new_metadata = new_metadata +1;
      metadata_t *new_remain_of_new_metadata = (metadata_t *)((char *)end_of_new_metadata + size);
      // add the remain of new requested space to corresponding free list
      my_add_to_free_list(new_remain_of_new_metadata);
    }
    // Add the memory region to the correct size of free list.
    my_add_to_free_list(new_metadata);
    // try my_malloc() again. 
    return my_malloc(size);
  }
  // found the new available slot
  //  ptr: point to next after the metadata itself
  void *ptr = best_slot + 1;
  size_t remaining_size = best_slot->size - size;
  // Remove the free slot from the free list.
  my_remove_from_free_list(best_slot);

  if (remaining_size > sizeof(metadata_t)) {
    // Shrink the metadata for the allocated object
    // to separate the rest of the region corresponding to remaining_size.
    // If the remaining_size is not large enough to make a new metadata,
    // this code path will not be taken and the region will be managed
    // as a part of the allocated object.
    best_slot->size = size; // currently the best_slot represents an allocated space, so it's size is used size
    // Create a new metadata for the remaining free slot that comes after allocated object
    metadata_t *new_metadata = (metadata_t *)((char *)ptr + size);//pointer arithmetic, 
    // cast to metadata_t cuz we put new free slot here
    new_metadata->size = remaining_size - sizeof(metadata_t);
    new_metadata->next = NULL;
    // Add the remaining free slot to the free list.
    my_add_to_free_list(new_metadata);
  }
  return ptr;
}


// furthur: check if the whole 4096 is empty and call munmap_to_system.
void my_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //the size remains unchanged as the size it gives the obj
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  metadata_t *metadata = (metadata_t *)ptr - 1;
  // Add the free slot to the free list.
  my_add_to_free_list(metadata);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Nothing is here for now.
  // feel free to add something if you want!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}
