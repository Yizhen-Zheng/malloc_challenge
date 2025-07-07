### c syntax

- pointer arithmetic: type-aware, it implicitly converts what comes to the size of each type
  e.g., `my_ptr_t * my_ptr = (my_ptr_t *)(int *)ptr + size` where `size = 4` will make space of 4 \* 4 bytes(each int is 4 bytes), because we claimed `(int *)` to treat the size as int

#### key learning of free bins implementation:

- why we'd want to check every bin above required size instead of only the bin for that size:
  find no free in target bin -> require a free 4096 -> the remaining of new required space is added to whatever other bins -> the remaining will rarely be reused unless we need that size of memory(we can say only 1/10 probability to use existing memory in bins, and 9/10 of time it just keep requiring new 4096 and generate new remaining into bins)

- pay attention to memory layout consistency when adding footer(caused a bug)
- make sure the way we add free slot is not adding cross-page slot

##### page edge detect strategy:

- when trying to get left, only check if current is at page start
- when trying to get right, only check if current is at page end
- when we require a new page, we create metadata right after the page start,
  this ensures we won't create small pieces in left(the start after page_start)
- when we require a new page and return the requested space, we immediately create a free metadata for the remainning spaces(if any),
  this ensures every metadata(if not fill the page) will have at least one right neighbor
- why: the mechanism that 'requested metadata will manage small spaces that not enough to split into another free slot',
  this ensures we won't create small pieces in right(the end of page)

so we currently don't need these additional check:

```c
  // check left footer range by moving to left by footer size
  void *left_neighbor_footer_addr = (char *)metadata - sizeof(footer_t);
  if (left_neighbor_footer_addr < page->start_addr){
    return NULL;//footer belongs to another page
  }

  // check right metadata range
  void *right_neighbor_addr = (char *)metadata + sizeof(metadata_t) + metadata->size + sizeof(footer_t);
  if (right_neighbor_addr + sizeof(metadata_t) > page_end){
    return NULL;//right metadata out of range
  }
```

[x]detect and return unused pages, munmap them
[x]handle malloc request greater than 4096
