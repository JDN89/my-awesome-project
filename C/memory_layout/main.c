#include <stdbool.h>
#include <stddef.h>

typedef struct {
  bool true_or_false;
  char *yolo;
  int one;
} layout;

#include <stdio.h>
int main() {
  size_t size = sizeof(layout);
  printf("size is: %zu \n ", size);
  return 0;
}
