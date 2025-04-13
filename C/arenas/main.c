
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Arena arena;
struct Arena {
  unsigned char *buf;
  size_t buf_len;
  size_t prev_offset;
  size_t curr_offset;
};

// ____________________linear ARENA _______________________

void arena_init(struct Arena *a, void *backing_buffer,
                size_t backing_buffer_length) {
  a->buf = (unsigned char *)backing_buffer;
  a->buf_len = backing_buffer_length;
  a->curr_offset = 0;
  a->prev_offset = 0;
}

void arena_free_all(struct Arena *a) {
  a->curr_offset = 0;
  a->prev_offset = 0;
}

bool is_power_of_two(uintptr_t x) { return (x & (x - 1)) == 0; }

// Kijk of het adres deelbaar is door 16 zoniet, verleg pointer zodat het mooi
// op de grens ligt
uintptr_t align_forward(uintptr_t ptr, size_t align) {
  uintptr_t p, a, modulo;
  assert(is_power_of_two(align));

  p = ptr;
  a = (uintptr_t)align;

  // same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);
  // if 'p' is not aligned, push the address to the next value which is aligned
  if (modulo != 0) {
    p += a - modulo;
  }
  return p;
}

// align forward indien nodig en gebruik de berekend offset als index in onze
// buffer, en return het pointer zodat we hier onze data kunnen storen
void *arena_alloc_align(struct Arena *a, size_t size, size_t align) {

  uintptr_t curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
  // align curr position + offset
  uintptr_t offset = align_forward(curr_ptr, align);
  offset -= (uintptr_t)a->buf; // Change to relative offset

  // Check to see if the backing memory has space left
  if (offset + size <= a->buf_len) {
    void *ptr = &a->buf[offset];
    a->prev_offset = offset;
    a->curr_offset = offset + size;

    // Zero new memory by default
    memset(ptr, 0, size);
    return ptr;
  }
  // Return NULL if the arena is out of memory (or handle differently)
  return NULL;
}
#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

// Because C doesn't have default parameters
void *arena_alloc(struct Arena *a, size_t size) {
  return arena_alloc_align(a, size, DEFAULT_ALIGNMENT);
}

// ----- RESIZE
void *arena_resize_align(struct Arena *a, void *old_memory, size_t old_size,
                         size_t new_size, size_t align) {
  unsigned char *old_mem = (unsigned char *)old_memory;

  assert(is_power_of_two(align));

  if (old_mem == NULL || old_size == 0) {
    return arena_alloc_align(a, new_size, align);
  } else if (a->buf <= old_mem && old_mem < a->buf + a->buf_len) {
    if (a->buf + a->prev_offset == old_mem) {
      a->curr_offset = a->prev_offset + new_size;
      if (new_size > old_size) {
        // Zero the new memory by default
        memset(&a->buf[a->curr_offset], 0, new_size - old_size);
      }
      return old_memory;
    } else {
      void *new_memory = arena_alloc_align(a, new_size, align);
      size_t copy_size = old_size < new_size ? old_size : new_size;
      // Copy across old memory to the new memory
      memmove(new_memory, old_memory, copy_size);
      return new_memory;
    }

  } else {
    assert(0 && "Memory is out of bounds of the buffer in this arena");
    return NULL;
  }
}

// Because C doesn't have default parameters
void *arena_resize(struct Arena *a, void *old_memory, size_t old_size,
                   size_t new_size) {
  return arena_resize_align(a, old_memory, old_size, new_size,
                            DEFAULT_ALIGNMENT);
}
// ___________________________STACK arena ____________________

typedef struct Stack Stack;
struct Stack {
  unsigned char *buf;
  size_t buf_len;
  size_t offset;
};

typedef struct Stack_Allocation_Header Stack_Allocation_Header;
struct Stack_Allocation_Header {
  uint8_t padding;
};

void stack_init(Stack *s, void *backing_buffer, size_t backing_buffer_length) {
  s->buf = (unsigned char *)backing_buffer;
  s->buf_len = backing_buffer_length;
  s->offset = 0;
}

size_t calc_padding_with_header(uintptr_t ptr, uintptr_t alignment,
                                size_t header_size) {
  uintptr_t p, a, modulo, padding, needed_space;

  assert(is_power_of_two(alignment));

  p = ptr;
  a = alignment;
  modulo = p & (a - 1); // (p % a) as it assumes alignment is a power of two

  padding = 0;
  needed_space = 0;

  if (modulo != 0) { // Same logic as 'align_forward'
    padding = a - modulo;
  }

  needed_space = (uintptr_t)header_size;

  // We hit this when we are allready alligned, then we add padding of 16
  if (padding < needed_space) {
    needed_space -= padding;

    if ((needed_space & (a - 1)) != 0) {
      padding += a * (1 + (needed_space / a));
    } else {
      // We hit this else block only in case we have headers that are bigger
      // then 1 byte. Not possible in my current code, but in case we want
      // bigger headers this is handy
      padding += a * (needed_space / a);
    }
  }

  return (size_t)padding;
}

void *stack_alloc_align(Stack *s, size_t size, size_t alignment) {
  uintptr_t curr_addr, next_addr;
  size_t padding;
  Stack_Allocation_Header *header;

  assert(is_power_of_two(alignment));

  if (alignment > 128) {
    // As the padding is 8 bits (1 byte), the largest alignment that can
    // be used is 128 bytes
    // because header type is uint8_t
    alignment = 128;
  }

  curr_addr = (uintptr_t)s->buf + (uintptr_t)s->offset;
  padding = calc_padding_with_header(curr_addr, (uintptr_t)alignment,
                                     sizeof(Stack_Allocation_Header));
  if (s->offset + padding + size > s->buf_len) {
    // Stack allocator is out of memory
    return NULL;
  }
  s->offset += padding;

  next_addr = curr_addr + (uintptr_t)padding;
  header =
      (Stack_Allocation_Header *)(next_addr - sizeof(Stack_Allocation_Header));
  header->padding = (uint8_t)padding;

  s->offset += size;

  return memset((void *)next_addr, 0, size);
}

// ----- FREE
void stack_free(Stack *s, void *ptr) {
  if (ptr != NULL) {
    uintptr_t start, end, curr_addr;
    Stack_Allocation_Header *header;
    size_t prev_offset;

    start = (uintptr_t)s->buf;
    end = start + (uintptr_t)s->buf_len;
    curr_addr = (uintptr_t)ptr;

    if (!(start <= curr_addr && curr_addr < end)) {
      assert(0 &&
             "Out of bounds memory address passed to stack allocator (free)");
      return;
    }

    if (curr_addr >= start + (uintptr_t)s->offset) {
      // Allow double frees
      return;
    }

    header = (Stack_Allocation_Header *)(curr_addr -
                                         sizeof(Stack_Allocation_Header));
    prev_offset = (size_t)(curr_addr - (uintptr_t)header->padding - start);

    s->offset = prev_offset;
  }
}

// Because C does not have default parameters
void *stack_alloc(Stack *s, size_t size) {
  return stack_alloc_align(s, size, DEFAULT_ALIGNMENT);
}
void stack_free_all(Stack *s) { s->offset = 0; }

// ----------------------------------------------------

void print_arena_state(struct Arena *a) {}

// -------------------------------------------MAIN -------------------
int main() {
  unsigned char backing_buffer[256];
  struct Arena a = {0};
  arena_init(&a, backing_buffer, 256);
  struct Stack s = {0};
  stack_init(&s, backing_buffer, 256);

  // Example 1: Allocate some memory for an integer
  int *num = (int *)stack_alloc(&s, sizeof(int));
  if (num) {
    *num = 42;
    printf("Allocated integer with value: %d\n", *num);
  } else {
    printf("Memory allocation failed for integer\n");
  }

  // Example 2: Allocate memory for a larger object, a struct
  struct {
    float x;
    float y;
  } *point = (typeof(point))stack_alloc(&s, sizeof(*point));
  if (point) {
    point->x = 10.0f;
    point->y = 20.0f;
    printf("Allocated struct with values: x = %.2f, y = %.2f\n", point->x,
           point->y);
  } else {
    printf("Memory allocation failed for struct\n");
  }

  // Example 3: Allocate memory for a small array of bytes
  unsigned char *arr =
      (unsigned char *)stack_alloc(&s, 5 * sizeof(unsigned char));
  if (arr) {
    for (int i = 0; i < 5; ++i) {
      arr[i] = (unsigned char)(i + 1);
    }
    printf("Allocated byte array: ");
    for (int i = 0; i < 5; ++i) {
      printf("%d ", arr[i]);
    }
    printf("\n");
  } else {
    printf("Memory allocation failed for byte array\n");
  }

  // Free all memory (reset the arena)
  stack_free_all(&s);
  // Final message
  printf("Arena reset. All memory freed.\n");

  return 0;
}
