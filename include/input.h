#ifndef INPUT_H
#define INPUT_H

int get_token(FILE *in, char *buf);
void unget_token(FILE *in, char *token);
int read_until(FILE *in, char *buf, size_t buf_size, char until);

#endif
