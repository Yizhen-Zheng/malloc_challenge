### c syntax

- pointer arithmetic: type-aware, it implicitly converts what comes to the size of each type
  e.g., `my_ptr_t * my_ptr = (my_ptr_t *)(int *)ptr + size` where `size = 4` will make space of 4 \* 4 bytes(each int is 4 bytes), because we claimed `(int *)` to treat the size as int

#### key learning of free bins implementation:

- why we'd want to check every bin above required size instead of only the bin for that size:
  find no free in target bin -> require a free 4096 -> the remaining of new required space is added to whatever other bins -> the remaining will rarely be reused unless we need that size of memory(we can say only 1/10 probability to use existing memory in bins, and 9/10 of time it just keep requiring new 4096 and generate new remaining into bins)

- pay attention to memory layout consistency when adding footer(caused a bug)
- make sure the way we add free slot is not adding cross-page slot

[x]detect page edges
[x]detect and return unused pages, munmap them
[x]handle malloc request greater than 4096
