#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tokenizer_ctx_s {
  char *token_buf;
  size_t token_buf_size;
  size_t token_buf_pos;
  char next_char;
  char (*get_char)(void *data);
  void *get_char_data;
  int str;
  int esc;
  int com;
};

typedef struct tokenizer_ctx_s tokenizer_ctx_t;


void tokenizer_init_stdio(tokenizer_ctx_t *ctx, FILE *fd);
char *tokenizer_get_token(tokenizer_ctx_t *ctx);
