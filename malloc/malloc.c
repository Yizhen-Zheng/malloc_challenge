
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
#define BUFFER_SIZE 4096

// Struct definitions

typedef struct metadata_t {
  size_t size;
  struct metadata_t *next;
  struct metadata_t *prev;
  // a footer points to the metadata?
} metadata_t;

typedef struct footer_t{
  size_t size;
}footer_t;

typedef struct page_info_t{
  void *start_addr;
  struct page_info_t *next;
  struct page_info_t *prev;
}page_info_t;

typedef struct bin_t {
  metadata_t dummy_head;
  metadata_t dummy_tail; 
} bin_t;

typedef struct heap_t {
  // an array of bins
  bin_t bins[BIN_NUMBER];
  // the start of pages
  page_info_t *page_head;
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

// given an address, traverse all page DLL and find which page it belongs to
page_info_t *find_page(void *addr){
  page_info_t *page = my_heap.page_head;
  while (page){
    if(addr >= page->start_addr && addr < (void *)((char *)page->start_addr + BUFFER_SIZE)){
      //if addr is in the middle of page region(start, start + 4096)
      return page;
    }
    page = page->next;
  }
  return NULL;//pointer out of mmaped area
}


// get current metadata's left neighbor from footer
// return it's left neighbor if it's free, else NULL
// what type should i return?
metadata_t *get_left_neighbor(metadata_t *metadata){
  //check page and metadatas' range
  page_info_t *page = find_page(metadata);
  if(!page){return NULL;}//out of mmaped range

  // if it's the first metadata in page, there's no left
  if (((char *)page->start_addr + sizeof(page_info_t)) == (void *)metadata){
    return NULL;
  }

  footer_t *left_neighbor_footer = (footer_t *)((char *)metadata - sizeof(footer_t));//cast to footer type

  // since we assume there's no space between page start and first metadata in page 
  // so if we can find the footer, there must be a metadata. so no need check left metadata, 
  // get left metadata address
  metadata_t *left_neighbor = (metadata_t *)((char *)left_neighbor_footer - left_neighbor_footer->size - sizeof(metadata_t));
  if (left_neighbor->next && left_neighbor->prev){//check if it's in free bins DLL
    return left_neighbor;
  }
  return NULL;
}

metadata_t *get_right_neighbor(metadata_t *metadata){
  // move pointer (current)metadata|size|footer|(go_to_here)right_metadata|
  page_info_t *page = find_page(metadata);
  if(!page){return NULL;}//out of mmaped range
  void *page_end = (char *)page->start_addr + BUFFER_SIZE;
  void *footer_end = (char *)metadata + sizeof(metadata_t) + metadata->size + sizeof(footer_t);
  // if it's the last metadata in page, there's no right
  if (footer_end >= page_end ){return NULL;}

  // find right metadata
  metadata_t *right_neighbor = (metadata_t *)((char *)metadata + sizeof(metadata_t) + metadata->size + sizeof(footer_t));
  if ( right_neighbor->next && right_neighbor->prev){
    //check if metadata exists(not NULL) and it's in free bins DLL
    //if metadata exists, we can say footer also in same page because we check at adding time
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


bool is_empty_page(page_info_t *page){
  // find first metadata
  void *first_metafata_addr = (char *)page->start_addr + sizeof(page_info_t);
  metadata_t *metadata = (metadata_t *)first_metafata_addr;

  void *page_end = (char *)page->start_addr + BUFFER_SIZE;
  size_t empty_size = BUFFER_SIZE - sizeof(page_info_t) - sizeof(metadata_t) - sizeof(footer_t);
  // check metadata size matches and it's free
  if (metadata->size == empty_size && metadata->next!=NULL && metadata->prev!=NULL){
    // double check footer
    // footer_t *footer = (footer_t *)((char *)metadata + sizeof(metadata) + metadata->size);
    // if((char *)footer + sizeof(footer_t) == (char *)page_end){
    //   return true;
    // }
    return true;
  }
  return false;
}

void remove_page_from_list(page_info_t *page){
  if (page->prev){
    //if current page is not head, reconnect prev to next
    page->prev->next = page->next;
  }else{
    //if current page is head, change head
    my_heap.page_head = page->next;
  }
  if (page->next){
    //if current page is not tail, reconnect next to prev
    page->next->prev = page->prev;
  }
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

//pages helper, add a new page to pages' head by it's start address
void add_to_page_list(page_info_t *page_start){
  //claim the page start address to contain page_info
  page_info_t *page_info = (page_info_t *)page_start;
  page_info->start_addr = page_start;
  //add new page to head of connect pages DLL, 
  page_info->next = my_heap.page_head;
  page_info->prev = NULL;
  if(my_heap.page_head!=NULL){
    my_heap.page_head->prev=page_info;
  }
  my_heap.page_head = page_info;
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
  my_heap.page_head = NULL;
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
    // request a new page region from the system by calling mmap_from_system().
    void *page_start = mmap_from_system(BUFFER_SIZE);
    if (!page_start){// if no more memory in mmap, return null(failed to mmap)
      return NULL;
    }
    add_to_page_list(page_start);

    best_slot = (metadata_t *)((char *)page_start + sizeof(page_info_t));
    best_slot->size = BUFFER_SIZE - sizeof(page_info_t) - sizeof(metadata_t) - sizeof(footer_t);
    best_slot->next = NULL;
    best_slot->prev = NULL;
    // continue to use the new got metadata
  }
  //set footer to the newly allocated memory
  //(may be greater than required size if remain is smaller than metadata+footer )
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
    // currently the best_slot represents an allocated space, so it's size is required size
    best_slot->size = size; 
    //reset footer if we shrink the best_slot's size
    set_footer(best_slot);
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


void my_free(void *ptr) {
  //the size remains unchanged as the size it gives the obj
  // Look up the metadata. The metadata is placed just prior to the object.
  //since the ptr points to the start of object, move it back by one metadata size
  metadata_t *metadata = (metadata_t *)ptr - 1;
  // Add the free slot to the free list.
  my_add_to_free_list(metadata);

  // check munmap merged data
  // the ptr will remain on the same page whether or not it was merged
  //(merge only happend within the same page)
  page_info_t *page = find_page(ptr);
  if(page && is_empty_page(page)){
    //remove first metadata from free list
    void *first_metadata_addr = (char *)page->start_addr + sizeof(page_info_t);
    //find the first_metadata and remove from free list (not available)
    metadata_t * first_metadata = (metadata_t *)first_metadata_addr;
    my_remove_from_free_list(first_metadata);
    remove_page_from_list(page);
    // munmap the page
    munmap_to_system(page->start_addr, BUFFER_SIZE);
  }
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
