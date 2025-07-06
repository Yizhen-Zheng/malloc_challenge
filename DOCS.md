### c syntax

- pointer arithmetic: type-aware, it implicitly converts what comes to the size of each type
  e.g., `my_ptr_t * my_ptr = (my_ptr_t *)(int *)ptr + size` where `size = 4` will make space of 4 \* 4 bytes(each int is 4 bytes), because we claimed `(int *)` to treat the size as int

[x]handle malloc request greater than 4096
[x]left merge (this 2 seem need pointer arithmetic to get to it's adjacent metadata((char\*)current_metadata + current_metadata->size))
[x]right merge
