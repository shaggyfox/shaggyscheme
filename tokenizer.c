#include "tokenizer.h"

char default_get_char(void *data)
{
  int v;
  if (data) {
    v = getc((FILE*) data);
  } else {
    v = getchar();
  }
  if (v == EOF) {
    return 0;
  }
  return (char)v;
}

struct memory_get_char_data_s {
  char *memory;
  int len;
  int pos;
};

char memory_get_char(void *in_data)
{
  struct memory_get_char_data_s *data = in_data;
  if (data->pos >= data->len) {
    return '\0';
  }
  return data->memory[data->pos ++];
}
static void tokenizer_init(tokenizer_ctx_t *ctx, char (*get_char)(void*), void *data);

void tokenizer_init_stdio(tokenizer_ctx_t *ctx, FILE *fd) {
  tokenizer_init(ctx, default_get_char, (void*)fd);
}

#if 0
void tokenizer_init_memory(tokenizer_ctx_t *ctx, char *memory, size_t len) {
  tokenizer_init(
}
#endif

static void tokenizer_init(tokenizer_ctx_t *ctx, char (*get_char)(void*), void *data)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->get_char = get_char ? get_char : default_get_char;
  ctx->get_char_data = data;
  ctx->token_buf_size = 32;
  ctx->token_buf = calloc(1, ctx->token_buf_size);
  /* initialize look-ahead with whitespace */
  ctx->next_char = 0;
}

#define is_whitespace(x) ((x) == ' ' || (x) == '\n' || (x) == ';')

int match_special(char *buf, int pos, int next)
{
  static char *tokens[] = {
    "(", ")", ",", "'", "`", "\"", " ", "\n", ",@", NULL
  };
  int ret = 0;
  if (pos == 2) {
    for (int i = 0; tokens[i]; ++i) {
      if (!strncmp(tokens[i], buf, 2)) {
        /* full match */
        ret = 1;
        break;
      }
    }
  } else if (pos == 1) {
    for (int i = 0; tokens[i]; ++i) {
      if (strlen(tokens[i]) == 1 && buf[0] == *tokens[i]) {
        /* semi match */
        ret = 1;
      } else if (strlen(tokens[i]) == 2 && buf[0] == *tokens[i] && ret) {
        if (tokens[i][1] == next) {
          /* will completed in next call */
          ret = 0;
          break;
        }
      }
    }
  }
  if (pos && !ret) {
    for (int i = 0; tokens[i]; ++i) {
      if (next == *tokens[i]) {
        ret = 1;
        break;
      }
    }
  }

  return ret;
}

char *tokenizer_get_token(tokenizer_ctx_t *ctx)
{
  for (;;) {

    if (ctx->next_char != 0) {
      if (ctx->token_buf_pos >= ctx->token_buf_size) {
        ctx->token_buf_size *= 2;
        ctx->token_buf = realloc(ctx->token_buf, ctx->token_buf_size);
      }
      ctx->token_buf[ctx->token_buf_pos ++] = ctx->next_char;
    }
    if (0 == (ctx->next_char = ctx->get_char(ctx->get_char_data))) {
      if (ctx->token_buf_pos) {
        ctx->token_buf[ctx->token_buf_pos] = '\0';
        ctx->token_buf_pos = 0;
        break;
      } else {
        return NULL;
      }
    } else if (!ctx->str && ctx->token_buf_pos == 1 && *ctx->token_buf == '"') {
      ctx->str = 1;
      if (ctx->next_char == '\\') {
        ctx->esc = 1;
        ctx->next_char = 0;
      }
    } else if (ctx->str) {
      /* handle esc */
      if (ctx->esc == 1) {
        ctx->esc = 0;
        switch (ctx->next_char) {
          case 'n':
            ctx->next_char = '\n';
            break;
          case 'r':
            ctx->next_char = '\r';
            break;
          case '"':
            /* special */
            ctx->esc = 2;
            break;
          default:
            break;
        }
      } else {
        if (ctx->token_buf[ctx->token_buf_pos-1] == '"') {
          if (ctx->esc == 2) {
            ctx->esc = 0;
          } else {
            ctx->str = 0;
            ctx->token_buf[ctx->token_buf_pos] = '\0';
            ctx->token_buf_pos = 0;
          }
          break;
        } else if (ctx->next_char == '\\') {
          ctx->esc = 1;
          ctx->next_char = 0;
        }
      }
    } else if (ctx->token_buf_pos == 1 && (ctx->com || is_whitespace(*ctx->token_buf))) {
      ctx->token_buf_pos = 0;
      if (*ctx->token_buf == ';') {
        ctx->com = 1;
      } else if (*ctx->token_buf == '\n') {
        ctx->com = 0;
      }
    } else if (is_whitespace(ctx->next_char)) {
      if (ctx->token_buf_pos) {
        ctx->token_buf[ctx->token_buf_pos] = '\0';
        ctx->token_buf_pos = 0;
        break;
      }
      continue;
    } else if (match_special(ctx->token_buf, ctx->token_buf_pos, ctx->next_char)) {
      ctx->token_buf[ctx->token_buf_pos] = '\0';
      ctx->token_buf_pos = 0;
      break;
    }
  }
  return ctx->token_buf;
}
