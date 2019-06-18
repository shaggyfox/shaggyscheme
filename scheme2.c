#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tokenizer */

struct tokenizer_ctx_s {
  char *token_buf;
  int token_buf_size;
  int token_buf_pos;
  char look_ahead;
  char (*get_char)(void *data);
  void *get_char_data;
  int in_quotes;
};

typedef struct tokenizer_ctx_s tokenizer_ctx_t;

char default_get_char(void *data)
{
  int v = getchar();
  if (v == EOF) {
    return 0;
  }
  return (char)v;
}

void tokenizer_init(tokenizer_ctx_t *ctx, char (*get_char)(void*), void *data)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->get_char = get_char ? get_char : default_get_char;
  ctx->get_char_data = data;
  ctx->token_buf_size = 32;
  ctx->token_buf = calloc(1, ctx->token_buf_size);
  /* initialize look-ahead with whitespace */
  ctx->look_ahead = ' ';
}

#define is_whitespace(x) ((x) == ' ' || (x) == '\n')
#define is_special(x) ((x) == '(' || (x) == ')' || (x) == '\'')
#define is_qoute(x)

char *tokenizer_get_token(tokenizer_ctx_t *ctx)
{
  /* skip whitespace */
  while(is_whitespace(ctx->look_ahead)) {
    ctx->look_ahead = ctx->get_char(ctx->get_char_data);
  }

  do {
    if (ctx->look_ahead == 0) {
      if (ctx->token_buf_pos) {
        break;
      }
      return NULL;
    } else if (is_special(ctx->look_ahead)) {
      ctx->token_buf[ctx->token_buf_pos ++] = ctx->look_ahead;
      ctx->look_ahead = ctx->get_char(ctx->get_char_data);
      break;
    } else {
      do {
        if (ctx->token_buf_pos >= ctx->token_buf_size - 1) {
          ctx->token_buf_size *= 2;
          char *resized_buf = realloc(ctx->token_buf, ctx->token_buf_size);
          if (!resized_buf) {
            /* out of memory */
            ctx->look_ahead = 0;
            return NULL;
          }
          ctx->token_buf = resized_buf;
        }
        if (ctx->look_ahead == '"') {
          ctx->in_quotes = !ctx->in_quotes;
        } else if (ctx->look_ahead == 0) {
          /* this is an error */
          break;
        }
        ctx->token_buf[ctx->token_buf_pos ++] = ctx->look_ahead;
        ctx->look_ahead = ctx->get_char(ctx->get_char_data);
      } while (ctx->in_quotes);
    }
  } while(!is_special(ctx->look_ahead) && !is_whitespace(ctx->look_ahead));
  ctx->token_buf[ctx->token_buf_pos] = '\0';
  ctx->token_buf_pos = 0;
  return ctx->token_buf;
}


/* -------------------- end of tokenizer ------------------------------- */
/* -----------------------memory management ---------------------------- */


typedef struct cell_s cell_t;

enum cell_type_e {
  CELL_T_EMPTY, CELL_T_PAIR, CELL_T_STRING, CELL_T_SYMBOL, CELL_T_INTEGER};

struct cell_s {
  enum cell_type_e type;
  int flags;
#define CELL_F_MARK 1
#define CELL_F_USED 2
  union {
    struct {
      cell_t *car;
      cell_t *cdr;
    } pair;
    char *string;
    char *symbol;
    int integer;
  } u;
};

typedef struct scheme_ctx_s scheme_ctx_t;

#define MAX_MEMORY 100
struct scheme_ctx_s {
  cell_t NIL_VALUE;
  cell_t *NIL;
  cell_t *syms;
  cell_t *sink;
  cell_t *env;
  cell_t *result;
  cell_t memory[MAX_MEMORY];
  tokenizer_ctx_t tokenizer_ctx;
};

cell_t *add_to_sink(scheme_ctx_t *ctx, cell_t *);

cell_t *raw_get_cell(scheme_ctx_t *ctx)
{
  for (int i = 0; i < MAX_MEMORY; ++i) {
    if (!(ctx->memory[i].flags & CELL_F_USED)) {
      ctx->memory[i].flags |= CELL_F_USED;
      return &ctx->memory[i];
    }
  }
  return NULL;
}

cell_t *raw_cons(scheme_ctx_t *ctx, cell_t *car, cell_t *cdr)
{
  cell_t *ret = raw_get_cell(ctx);
  ret->type = CELL_T_PAIR;
  ret->u.pair.car = car;
  ret->u.pair.cdr = cdr;
  return ret;
}

cell_t *get_cell(scheme_ctx_t *ctx)
{
  return add_to_sink(ctx, raw_get_cell(ctx));
}

cell_t *add_to_sink(scheme_ctx_t *ctx, cell_t *cell)
{
  ctx->sink = raw_cons(ctx, cell, ctx->sink);
  return cell;
}

cell_t *cons(scheme_ctx_t *ctx, cell_t *car, cell_t *cdr)
{
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_PAIR;
  ret->u.pair.car = car;
  ret->u.pair.cdr = cdr;
  return ret;
}

#define mark_cell(cell) (cell)->flags |= CELL_F_MARK;
#define unmark_cell(cell) (cell)->flags &= ~CELL_F_MARK;

void mark_cells(cell_t *cell)
{
  switch(cell->type) {
    case CELL_T_PAIR:
      mark_cells(cell->u.pair.car);
      mark_cells(cell->u.pair.cdr);
    default:
      mark_cell(cell);
      break;
  }
}

void unmark_cells(cell_t *cell)
{
  switch(cell->type) {
    case CELL_T_PAIR:
      unmark_cells(cell->u.pair.car);
      unmark_cells(cell->u.pair.cdr);
    default:
      unmark_cell(cell);
      break;
  }
}

void gc_collect(scheme_ctx_t *ctx)
{
  mark_cells(ctx->sink);
  for (int i = 0; i < MAX_MEMORY; ++i) {
    if (ctx->memory[i].flags & CELL_F_USED) {
      if (!(ctx->memory[i].flags & CELL_F_MARK)) {
        printf("collect cell ...\n");
        ctx->memory[i].flags &= ~CELL_F_USED;
        ctx->memory[i].type = CELL_T_EMPTY;
      }
    }
  }
  unmark_cells(ctx->sink);
}

#define _car(obj) ((obj)->u.pair.car)
#define _cdr(obj) ((obj)->u.pair.cdr)
#define is_null(ctx, obj) ((ctx)->NIL == obj)
#define is_sym(obj) ((obj)->type == CELL_T_SYMBOL)
#define is_pair(obj) ((obj)->type == CELL_T_PAIR)

/* symbols */

cell_t *mk_symbol(scheme_ctx_t *ctx, char *str)
{
  cell_t *ret = ctx->NIL;
  for (cell_t *tmp = ctx->syms; !is_null(ctx, tmp); tmp = _cdr(tmp)) {
    if (!strcmp(str, _car(tmp)->u.symbol)) {
      ret = _car(tmp);
      break;
    }
  }
  if (is_null(ctx, ret)) {
    /* add new entry */
    ret = get_cell(ctx);
    ret->type = CELL_T_SYMBOL;
    ret->u.symbol = strdup(str);
    ctx->syms = cons(ctx, ret, ctx->syms);
  }
  return ret;
}

/* FIXME */
cell_t *mk_primop(scheme_ctx_t *ctx, cell_t (*fn)(cell_t *))
{
  return ctx->NIL;
}

/* --- obj --- */

cell_t *mk_object_from_token(scheme_ctx_t *ctx, char *token)
{
  return  mk_symbol(ctx, token);
}

cell_t *get_object(scheme_ctx_t *ctx);
cell_t *get_obj_list(scheme_ctx_t *ctx)
{
  cell_t *obj = get_object(ctx);
  if (obj == mk_symbol(ctx, ")")) {
    return ctx->NIL;
  }
  return cons(ctx, obj, get_obj_list(ctx));
}

cell_t *get_object(scheme_ctx_t *ctx)
{
  char *value = tokenizer_get_token(&ctx->tokenizer_ctx);
  if (value) {
    if (*value == '(') {
      return get_obj_list(ctx);
    }
    return mk_object_from_token(ctx, value);
  }
  return NULL; /* err or end of file XXX */
}

/*------------------------ print -------------------- */

void print_obj(scheme_ctx_t *ctx, cell_t *obj);
void print_pair(scheme_ctx_t *ctx, cell_t *pair)
{
  printf("( ");
  while (is_pair(pair)) {
    print_obj(ctx, _car(pair));
    pair = _cdr(pair);
  }
  if (!is_null(ctx, pair)) {
    printf(". ");
    print_obj(ctx, pair);
  }
  printf(") ");
}

void print_obj(scheme_ctx_t *ctx, cell_t *obj) {
  switch ((obj)->type) {
    case CELL_T_SYMBOL:
      printf("<sym:%s> ", obj->u.symbol);
      break;
    case CELL_T_PAIR:
      print_pair(ctx, obj);
      break;
    default:
      if (is_null(ctx, obj)) {
        printf("()");
      } else {
        printf("<unknown> ");
      }
      break;
  }
}


/* ---------------t main .. */
void scheme_init(scheme_ctx_t *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->NIL = &ctx->NIL_VALUE;
  ctx->result = ctx->NIL;
  ctx->sink = ctx->NIL;
  ctx->syms = ctx->NIL;
  ctx->env = ctx->NIL;
  tokenizer_init(&ctx->tokenizer_ctx, NULL, NULL);
}

int main()
{
  scheme_ctx_t ctx;
  scheme_init(&ctx);
  cell_t *obj = get_object(&ctx);
  print_obj(&ctx, obj);
  printf("\n");
  return 0;
}
