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


typedef struct scheme_ctx_s scheme_ctx_t;
typedef struct cell_s cell_t;

enum cell_type_e {
  CELL_T_EMPTY, CELL_T_PAIR, CELL_T_STRING, CELL_T_SYMBOL,
  CELL_T_INTEGER, CELL_T_PRIMOP};

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
    cell_t *(*primop)(scheme_ctx_t *, cell_t *);
  } u;
};

#define MAX_MEMORY 100
struct scheme_ctx_s {
  cell_t NIL_VALUE;
  cell_t TRUE_VALUE;
  cell_t FALSE_VALUE;
  cell_t *NIL;
  cell_t *FALSE;
  cell_t *TRUE;
  cell_t *syms;
  cell_t *sink;
  cell_t *env;
  cell_t *code;
  cell_t *result;
  cell_t memory[MAX_MEMORY];
  tokenizer_ctx_t tokenizer_ctx;
  cell_t *PARENTHESIS_OPEN;
  cell_t *PARENTHESIS_CLOSE;
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

void gc_collect(scheme_ctx_t *ctx);
cell_t *get_cell(scheme_ctx_t *ctx)
{
  gc_collect(ctx);
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
  mark_cells(ctx->syms);
  mark_cells(ctx->env);
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
  unmark_cells(ctx->syms);
  unmark_cells(ctx->env);
}

void gc_info(scheme_ctx_t *ctx)
{
  int ret = MAX_MEMORY;
  for (int i = 0; i < MAX_MEMORY; ++i) {
    if (ctx->memory[i].flags & CELL_F_USED) {
      -- ret;
    }
  }
  printf("%d cells are free\n", ret);
}

#define _car(obj) ((obj)->u.pair.car)
#define _cdr(obj) ((obj)->u.pair.cdr)
#define is_null(ctx, obj) ((ctx)->NIL == obj)
#define is_sym(obj) ((obj)->type == CELL_T_SYMBOL)
#define is_pair(obj) ((obj)->type == CELL_T_PAIR)
#define is_true(ctx, obj) ((obj) != (ctx)->FALSE)
#define is_false(ctx, obj) ((obj) == (ctx)->FALSE)
#define is_primop(obj) ((obj)->type == CELL_T_PRIMOP)
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
cell_t *mk_primop(scheme_ctx_t *ctx, cell_t *(*fn)(scheme_ctx_t *, cell_t *))
{
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_PRIMOP;
  ret->u.primop = fn;
  return ret;
}

cell_t *mk_string(scheme_ctx_t *ctx, char* str)
{
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_STRING;
  ret->u.string = strdup(str);
  return ret;
}

/* --- obj --- */

cell_t *mk_object_from_token(scheme_ctx_t *ctx, char *token)
{
  cell_t *ret = ctx->NIL;
  char *c;
  switch(*token) {
    case '"':
      if ((c = strchr(token+1, '"'))) {
        *c = '\0';
      }
      ret = mk_string(ctx, token+1);
      break;
    default:
      ret = mk_symbol(ctx, token);
      break;
  }
  return ret;
}

cell_t *get_object(scheme_ctx_t *ctx);
cell_t *get_obj_list(scheme_ctx_t *ctx)
{
  cell_t *obj = get_object(ctx);
  if (obj == ctx->PARENTHESIS_CLOSE) {
    return ctx->NIL;
  }
  return cons(ctx, obj, get_obj_list(ctx));
}

cell_t *get_object(scheme_ctx_t *ctx)
{
  char *value = tokenizer_get_token(&ctx->tokenizer_ctx);
  cell_t *obj = mk_object_from_token(ctx, value);
  if (obj == mk_symbol(ctx, "'")) {
    return cons(ctx, mk_symbol(ctx, "quote"), cons(ctx, get_object(ctx), ctx->NIL));
  } else if (obj == ctx->PARENTHESIS_OPEN) {
    return get_obj_list(ctx);
  }
  return obj;
}

/*------------------------ print -------------------- */

void print_obj(scheme_ctx_t *ctx, cell_t *obj);
void print_pair(scheme_ctx_t *ctx, cell_t *pair)
{
  printf("(");
  while (is_pair(pair)) {
    print_obj(ctx, _car(pair));
    pair = _cdr(pair);
  }
  if (!is_null(ctx, pair)) {
    printf(". ");
    print_obj(ctx, pair);
  }
  printf(")");
}

void print_obj(scheme_ctx_t *ctx, cell_t *obj) {
  switch ((obj)->type) {
    case CELL_T_PRIMOP:
      printf("<primop> ");
      break;
    case CELL_T_SYMBOL:
      printf("%s ", obj->u.symbol);
      break;
    case CELL_T_STRING:
      printf("\"%s\" ", obj->u.string);
      break;
    case CELL_T_PAIR:
      print_pair(ctx, obj);
      break;
    default:
      if (is_null(ctx, obj)) {
        printf("()");
      } else if (is_true(ctx, obj)) {
        printf("#t ");
      } else if (is_false(ctx, obj)) {
        printf("#f ");
      } else {
        printf("<unknown> ");
      }
      break;
  }
}

/* primops */

cell_t *eq(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *ret = ctx->FALSE;
  cell_t *arg1 = _car(args);
  cell_t *arg2 = _car(_cdr(args));
  return (arg1 == arg2) ? ctx->TRUE : ctx->FALSE;
}

cell_t *eqv(scheme_ctx_t *ctx, cell_t *args)
{
  /* XXX */
  return ctx->FALSE;
}

cell_t *apply_primop(scheme_ctx_t *ctx, cell_t *primop, cell_t *args)
{
  return primop->u.primop(ctx, args);
}

/* environment hanlding */

cell_t *env_define(scheme_ctx_t *ctx, cell_t *symbol, cell_t *value)
{
  ctx->env = cons(ctx, cons(ctx, symbol, cons(ctx, value, ctx->NIL)), ctx->env);
  return value;
}

cell_t *env_resolve(scheme_ctx_t *ctx, cell_t *symbol)
{
  for (cell_t *e = ctx->env; !is_null(ctx, e); e = _cdr(e)) {
    if (symbol == _car(_car(e))) {
      return _car(_cdr(_car(e)));
    }
  }
  printf("ERROR: symbol '%s' is not defined\n", symbol->u.symbol);
  return ctx->NIL; /* symbol not found in environment */
}

/* eval */

cell_t *eval(scheme_ctx_t *ctx, cell_t *obj);
cell_t *eval_list(scheme_ctx_t *ctx, cell_t *list)
{
  if (ctx->NIL == list) {
    return ctx->NIL;
  }
  return cons(ctx, eval(ctx, _car(list)), eval_list(ctx, _cdr(list)));
}

cell_t *eval(scheme_ctx_t *ctx, cell_t *obj)
{
  if (!is_pair(obj)) {
    if (is_sym(obj)) {
      return env_resolve(ctx, obj);
    } else {
      return obj;
    }
  } else if (is_sym(_car(obj))) {
    cell_t *cmd = _car(obj);
    cell_t *args = _cdr(obj);
    if (cmd == mk_symbol(ctx, "if")) {
      /* (if a b c) */
      cell_t *a = _car(args);
      cell_t *b = _car(_cdr(args));
      cell_t *c = _car(_cdr(_cdr(args)));
      if (is_true(ctx, eval(ctx, a))) {
        return eval(ctx, b);
      } else {
        return eval(ctx, c);
      }
    } else if (cmd == mk_symbol(ctx, "define")) {
      cell_t *name = _car(args);
      cell_t *value = _car(_cdr(args));
      if (!is_sym(name)) {
        printf("define: name is not a symbol\n");
        return ctx->NIL;
      } else {
        return env_define(ctx, name, eval(ctx, value));
      }
    } else if (cmd == mk_symbol(ctx, "lambda")) {
      return obj;
    } else if (cmd == mk_symbol(ctx, "quote")) {
      return _car(args);
    } else {
      /* try to resolve symbol */
      cell_t *resolved_cmd = env_resolve(ctx, cmd);
      if (is_null(ctx, resolved_cmd)) {
        /* ERROR */
        return ctx->NIL;
      } else if (is_primop(resolved_cmd)) {
        return apply_primop(ctx, resolved_cmd, eval_list(ctx, args));
      } else if (is_pair(resolved_cmd)) {
        return eval(ctx, cons(ctx, resolved_cmd, args));
      }
    }
  } else if(is_pair(_car(obj))){
    /* lambda? */
    cell_t *lambda = _car(obj);
    cell_t *args   = _cdr(obj);
    if (mk_symbol(ctx, "lambda") == _car(lambda)) {
      cell_t *old_env = ctx->env; /* XXX */
      cell_t *old_sink = ctx->sink; /* XXX */

      cell_t *names = _car(_cdr(lambda));
      cell_t *vars  = args;
      cell_t *body  = _car(_cdr(_cdr(lambda)));

      for( ;
          !is_null(ctx, names) && !is_null(ctx, vars);
          names = _cdr(names), vars = _cdr(vars)) {
        env_define(ctx, _car(names), eval(ctx, _car(vars)));
      }
      cell_t *ret = eval(ctx, body);

      ctx->env = old_env;  /* XXX */
      ctx->sink = old_sink; /* XXX */
      return ret;
    } else {
      printf("cannot apply");
    }
  } else {
    printf("cannot apply");
  }
  return ctx->NIL;
}

/* ---------------t main .. */
void scheme_init(scheme_ctx_t *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->NIL = &ctx->NIL_VALUE;
  ctx->TRUE = &ctx->TRUE_VALUE;
  ctx->FALSE = &ctx->FALSE_VALUE;
  ctx->sink = ctx->NIL;
  ctx->syms = ctx->NIL;
  ctx->env = ctx->NIL;
  ctx->code = ctx->NIL;
  ctx->result = ctx->NIL;
  tokenizer_init(&ctx->tokenizer_ctx, NULL, NULL);
  ctx->PARENTHESIS_OPEN = mk_symbol(ctx, "(");
  ctx->PARENTHESIS_CLOSE = mk_symbol(ctx, ")");
}

int main()
{
  scheme_ctx_t ctx;
  scheme_init(&ctx);
  env_define(&ctx, mk_symbol(&ctx, "#t"), ctx.TRUE);
  env_define(&ctx, mk_symbol(&ctx, "#f"), ctx.FALSE);
  env_define(&ctx, mk_symbol(&ctx, "eq"), mk_primop(&ctx, &eq));
  ctx.sink = ctx.NIL;
  gc_collect(&ctx);
  gc_info(&ctx);
  for(;;) {
    cell_t *obj = get_object(&ctx);
    print_obj(&ctx, eval(&ctx, obj));
    printf("\n");
    gc_info(&ctx);
    ctx.sink = ctx.NIL;
  }
  return 0;
}
