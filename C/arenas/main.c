
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// practic with AST nodes
typedef enum {
  AST_NODE_TYPE_OPERATOR,
  AST_NODE_TYPE_OPERAND,
  // Add other node types as needed
} AstNodeType;

typedef struct AstNode {
  AstNodeType type;
  union {
    struct { // For an operator node
      struct AstNode *left;
      struct AstNode *right;
      char operator;
    } op;
    int value; // For operand (literal) nodes
  };
} AstNode;

bool is_power_of_two(uintptr_t x) { return (x & (x - 1)) == 0; }

// uintptr_t is an unsigned integer type defined in the <stdint.h> header of the
// C standard library. It is designed to be large enough to hold any pointer
// value. This makes it an essential type for scenarios where pointers need to
// be manipulated as integers.
// A common use of uintptr_t is to perform pointer alignment, as in the
// align_forward function in your original code. By converting a pointer to
// uintptr_t, you can safely apply arithmetic operations to align it to a
// specific boundary, as pointer types themselves don’t support direct
// arithmetic in C.

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

typedef struct Arena arena;
struct Arena {
  unsigned char *buf;
  size_t buf_len;
  size_t prev_offset;
  size_t curr_offset;
};

void *arena_alloc_align(struct Arena *a, size_t size, size_t align) {
  // Align 'curr_offset' forward to the specified alignment
  // This is the pointer (in the form of uintptr_t) that you are aligning. p
  // points to the current position in the buffer (a->buf) plus the offset
  // (a->curr_offset). This pointer is adjusted if needed by the alignment
  // logic.
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

int main() {
  unsigned char backing_buffer[256];
  struct Arena a = {0};
  arena_init(&a, backing_buffer, 256);

  // Example 1: Allocate some memory for an integer
  int *num = (int *)arena_alloc(&a, sizeof(int));
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
  } *point = (typeof(point))arena_alloc(&a, sizeof(*point));
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
      (unsigned char *)arena_alloc(&a, 5 * sizeof(unsigned char));
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
  arena_free_all(&a);

  // Final message
  printf("Arena reset. All memory freed.\n");

  return 0;
}
