#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "input.h"
#include "util_types.h"

// assume that all tokens are separated by spaces
int get_token(FILE *in, char *buf)
{
  char in_c;
  int buf_idx = 0;

  while (isspace(in_c = fgetc(in))); // skip all whitespace characters

  if (in_c == EOF)
    return EOF;
  else
    buf[buf_idx++] = in_c;

  do {
    if (!isspace(in_c = fgetc(in)))
      buf[buf_idx++] = in_c;
    else
      break;
    if (in_c == EOF)
      fprintf(stderr, "EOF\n");
  } while (in_c != EOF);

  buf[buf_idx] = '\0';
  return (in_c == EOF) ? EOF : buf_idx;
}

// returns number of characters read
int read_until(FILE *in, char *buf, size_t buf_size, char until)
{
  char in_c;
  int buf_idx = 0;
  while ((in_c = fgetc(in)) != until && in_c != EOF && buf_idx < buf_size) {
    buf[buf_idx++] = in_c;
  }

  buf[buf_idx] = '\0';

  return (in_c == EOF) ? EOF : buf_idx;
}

void unget_token(FILE *in, char *token)
{
  if (ungetc(' ', in) == EOF) {
    error("couldn't unget character ' '");
  }
  for (int i = strlen(token) - 1; i >= 0; i--) {
    if (ungetc(token[i], in) == EOF) {
      error("couldn't unget character '%c'", token[i]);
    }
  }
}
