
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
  // a footer points to the metadata?
} metadata_t;

typedef struct  footer_t{
  size_t size;
}footer_t;


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


void set_footer(metadata_t *metadata){
  // note: when calling set_footer, the metadata's size should NOT contain the footer's size
  // in other words, we need to subtract footer size from metadata's size before calling set_footer
  // find where to insert footer (metadata|object or free memory|footer|another metadata)
  footer_t *footer = (footer_t *)((char *)metadata + sizeof(metadata_t) + metadata->size);
  footer->size = metadata->size;
}

// get current metadata's left neighbor from footer
// return it's left neighbor if it's free, else NULL
// what type should i return?
metadata_t *get_left_neighbor(metadata_t *metadata){
// move to left by footer size -> find where it's metadata is ->check if it is in free bins
  footer_t *left_neighbor_foolter=(footer_t *)((char *)metadata - sizeof(footer_t));
  metadata_t *left_neighbor = (metadata_t *)((char *)metadata - sizeof(footer_t) - left_neighbor_foolter->size - sizeof(metadata_t));
  if (left_neighbor->next && left_neighbor->prev){//check if it's in free bins DLL
    return left_neighbor;
  }
  return NULL;
}

metadata_t *get_right_neighbor(metadata_t *metadata){
  // move pointer (current)metadata|size|footer|(go_to_here)right_metadata|
  metadata_t *right_neighbor = (metadata_t *)((char *)metadata + sizeof(metadata_t) + metadata->size + sizeof(footer_t));
  if (right_neighbor->next && right_neighbor->prev){//check if it's in free bins DLL
    return right_neighbor;
  }
  return NULL;
}
metadata_t *merge_left(metadata_t *metadata, metadata_t *left){
  // new size: original left's free size + metadata's free size + original left footer size + metadata header's size
  // not contain origin metadata's footer size 
  size_t new_size = left->size + sizeof(footer_t) + sizeof(metadata_t) + metadata->size;
  left->size = new_size;
  return left;
}

metadata_t *merge_right(metadata_t *metadata, metadata_t *right){
  size_t new_size = metadata->size + sizeof(footer_t) + sizeof(metadata_t) + right->size;
  metadata->size = new_size;
  return metadata;
}


void my_remove_from_free_list(metadata_t *metadata) {
  // reconnect DLL
  metadata->prev->next = metadata->next;
  metadata->next->prev = metadata->prev;
  //ensure we don't creat a circle when latter put it back
  metadata->next = NULL;
  metadata->prev = NULL;
}


metadata_t *check_and_merge(metadata_t *metadata){
  // no need to remove new income metadata from free list because we do this before adding
  //the wrapper function to perform left/right/both side merge before adding to free list
  metadata_t *left = get_left_neighbor(metadata);
  metadata_t *right = get_right_neighbor(metadata);
  if (left){
    my_remove_from_free_list(left);//remove origin left
    metadata = merge_left(metadata, left);//current metadata is pointing to original left
  }
  if (right){
    my_remove_from_free_list(right);//remove origin right
    metadata = merge_right(metadata, right);
  }
  return metadata;
}


void my_add_to_free_list(metadata_t *metadata) {
  assert(!metadata->next && !metadata->prev);
  // check if anything to merge, update metadata points to the merged address
  metadata_t *merged_metadata = check_and_merge(metadata);
  set_footer(merged_metadata);//add a new footer at the end of merged free memory

  //put into corresponding bin:
  int bin_idx = get_bin_index(merged_metadata->size);
  bin_t *bin = &my_heap.bins[bin_idx];

  // reconnect DLL
  merged_metadata->next = bin->dummy_head.next; //the next is merged_metadata pointer
  merged_metadata->prev = &bin->dummy_head; // the dummy_head is an actuall value, so need &
  bin->dummy_head.next->prev = merged_metadata;
  bin->dummy_head.next = merged_metadata;
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
  metadata_t *best_slot=NULL; // a pointer variable to keep watch the current best fit
  // a for loop check all bins above required size 
  for (int i = bin_idx; i < BIN_NUMBER; i++){
    metadata_t *metadata = my_heap.bins[i].dummy_head.next;
    while (metadata != &my_heap.bins[i].dummy_tail ) {
      if (metadata->size >= size){
        if (!best_slot || best_slot->size > metadata->size){ 
          // update best_slot if found a fitter metadata
          best_slot=metadata;
        }
        if (best_slot->size==size){
          break;//if it's a exactly fit slot, break immediately
        }
      }
      metadata = metadata->next;
    }
    if (best_slot){
      break;// if find best_slot in current bin size, stop explore larger bins
    }
  }


  if (!best_slot) {
    // cannot find free slot available in all bins, means we're going to use the new memory immediatly
    // request a new memory region from the system by calling mmap_from_system().
    // buffer_size: metadata + free_slot
    size_t buffer_size = 4096;
    best_slot = (metadata_t *)mmap_from_system(buffer_size); //request a new block of 4096 
    if (!best_slot){// if no more memory in mmap, return null
      return NULL;
    }
    best_slot->size = buffer_size - sizeof(metadata_t) - sizeof(footer_t);
    best_slot->next = NULL;
    best_slot->prev = NULL;
    // continue to use the new got metadata
  }

  //set footer to the newly allocated memory(which may contains greater than required size
  //if remaining is smaller than metadata size )
  set_footer(best_slot);
  // Remove the best_slot from the free list if it's original in bins
  if (best_slot->next && best_slot->prev){
    my_remove_from_free_list(best_slot);
  }

  //  ptr: point to right after the metadata itself
  void *ptr = best_slot + 1;
  size_t remaining_size = best_slot->size - size ;
  if (remaining_size > sizeof(metadata_t) + sizeof(footer_t)) { //add remaining back to free list conditionally
    // If the remaining is smaller than sizeof metadata, the remaining will be taken as a part of the allocated object.
    best_slot->size = size; // currently the best_slot represents an allocated space, so it's size is required size
    set_footer(best_slot);//reset footer if we shrink the best_slot's size
    // Create a new metadata for the remaining free slot that comes after allocated object
    metadata_t *new_metadata = (metadata_t *)((char *)best_slot + sizeof(metadata_t) + best_slot->size + sizeof(footer_t));//add start of required by the required object size
    // cast to metadata_t since we put new free slot metadata here
    new_metadata->size = remaining_size - sizeof(metadata_t) - sizeof(footer_t);
    new_metadata->next = NULL;
    new_metadata->prev = NULL;
    // Add the remaining free slot to the free list.
    set_footer(new_metadata);
    my_add_to_free_list(new_metadata);
  } 
  return ptr;//return start address of required
}


// furthur: check if the whole 4096 is empty and call munmap_to_system.
void my_free(void *ptr) {
  //the size remains unchanged as the size it gives the obj
  // Look up the metadata. The metadata is placed just prior to the object.
  //since the ptr points to the start of object, move it back by one metadata size
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
