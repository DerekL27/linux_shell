#include <stdio.h>
#include "tokens.h"

#include <assert.h>

int main(int argc, char **argv) {
  char input[256];

  fgets(input, 256, stdin);


  char **tokens = get_tokens(input);

  assert(tokens != NULL);

  char **current = tokens;
  int i = 0;
  while (current[i] != NULL) {
    printf("%s\n", current[i]);
    ++i;
  }

  free_tokens(tokens);
  return 0;
}
