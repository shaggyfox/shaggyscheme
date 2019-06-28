#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenizer.h"

/* -------------------- end of tokenizer ------------------------------- */
/* -----------------------memory management ---------------------------- */


typedef struct scheme_ctx_s scheme_ctx_t;
typedef struct cell_s cell_t;

enum cell_type_e {
  CELL_T_EMPTY, CELL_T_PAIR, CELL_T_STRING, CELL_T_SYMBOL,
  CELL_T_INTEGER, CELL_T_PRIMOP, CELL_T_LAMBDA, CELL_T_MACRO};

static char *cell_type_names[] = {
  "empty", "pair", "string", "symbol", "integer", "primop", "lambda", "macro", NULL
};

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
    struct {
      cell_t *names;
      cell_t *body;
    } lambda;
    struct {
      cell_t *arg_name;
      cell_t *body;
    } macro;
  } u;
};

#define MAX_MEMORY (1024 * 16)
#define MAX_SINK_SIZE 1024

struct scheme_ctx_s {
  cell_t *sink[MAX_SINK_SIZE];
  int  sink_pos;
  cell_t NIL_VALUE;
  cell_t TRUE_VALUE;
  cell_t FALSE_VALUE;
  cell_t *NIL;
  cell_t *FALSE;
  cell_t *TRUE;
  cell_t *syms;
  cell_t *env;
  cell_t *code;
  cell_t *result;
  cell_t *args;
  cell_t *memory;
  int memory_in_use;
  int memory_pos;
  tokenizer_ctx_t tokenizer_ctx;
  cell_t *PARENTHESIS_OPEN;
  cell_t *PARENTHESIS_CLOSE;
  cell_t *SYMBOL_IF;
  cell_t *SYMBOL_BEGIN;
  cell_t *SYMBOL_QUOTE;
  cell_t *SYMBOL_LAMBDA;
  cell_t *SYMBOL_DEFINE;
  cell_t *SYMBOL_DOT;
  cell_t *SYMBOL_QUOTE_ALIAS;
  cell_t *SYMBOL_QUASIQUOTE;
  cell_t *SYMBOL_QUASIQUOTE_ALIAS;
  cell_t *SYMBOL_MACRO;
  cell_t *SYMBOL_UNQUOTE;
  cell_t *SYMBOL_UNQUOTE_ALIAS;
  cell_t *SYMBOL_UNQUOTE_SPLICE;
  cell_t *SYMBOL_UNQUOTE_SPLICE_ALIAS;
};

cell_t *add_to_sink(scheme_ctx_t *ctx, cell_t *);
void gc_collect(scheme_ctx_t *ctx, cell_t *tmp_a, cell_t *tmp_b);
void print_obj(scheme_ctx_t *ctx, cell_t *obj);

cell_t *raw_get_cell(scheme_ctx_t *ctx, cell_t *tmp_a, cell_t *tmp_b)
{
  if (MAX_MEMORY - ctx->memory_in_use < 1) {
    gc_collect(ctx, tmp_a, tmp_b);
  }
  int i = ctx->memory_pos;
  for (int cnt = 0; cnt < MAX_MEMORY; ++cnt) {
    cell_t *current_cell = &ctx->memory[i];
    if (!(current_cell->flags & CELL_F_USED)) {
      current_cell->flags |= CELL_F_USED;
      ctx->memory_in_use += 1;
      ctx->memory_pos = (i + 1) % MAX_MEMORY;
      return current_cell;
    }
    i = (i + 1) % MAX_MEMORY;
  }
  printf("out of memory\n");
  exit(1);
  return NULL;
}

cell_t *raw_cons(scheme_ctx_t *ctx, cell_t *car, cell_t *cdr)
{
  cell_t *ret = raw_get_cell(ctx, car, cdr);
  ret->type = CELL_T_PAIR;
  ret->u.pair.car = car;
  ret->u.pair.cdr = cdr;
  return ret;
}

cell_t *get_cell(scheme_ctx_t *ctx)
{
  cell_t *ret = raw_get_cell(ctx, ctx->NIL, ctx->NIL);
  return add_to_sink(ctx, ret);
}

cell_t *add_to_sink(scheme_ctx_t *ctx, cell_t *cell)
{
  if (ctx->sink_pos >= MAX_SINK_SIZE) {
    printf("out of sink space\n");
    return cell;
  }
  ctx->sink[ctx->sink_pos ++] = cell;
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
  if (cell->flags & CELL_F_MARK) {
    /* cell ist already marked */
    return;
  }
  /* mark this cell */
  mark_cell(cell);
  switch(cell->type) {
    case CELL_T_PAIR:
      mark_cells(cell->u.pair.car);
      mark_cells(cell->u.pair.cdr);
      break;
    case CELL_T_LAMBDA:
      mark_cells(cell->u.lambda.names);
      mark_cells(cell->u.lambda.body);
      break;
    case CELL_T_MACRO:
      mark_cells(cell->u.macro.arg_name);
      mark_cells(cell->u.macro.body);
      break;
    default:
      break;
  }
}

void unmark_cells(cell_t *cell)
{
  if (!(cell->flags & CELL_F_MARK)) {
     return;
  }
  unmark_cell(cell);
  switch(cell->type) {
    case CELL_T_PAIR:
      unmark_cells(cell->u.pair.car);
      unmark_cells(cell->u.pair.cdr);
      break;
    case CELL_T_LAMBDA:
      unmark_cells(cell->u.lambda.names);
      unmark_cells(cell->u.lambda.body);
      break;
    case CELL_T_MACRO:
      unmark_cells(cell->u.macro.arg_name);
      unmark_cells(cell->u.macro.body);
      break;
    default:
      break;
  }
}

void print_obj(scheme_ctx_t *ctx, cell_t *obj);
void gc_collect(scheme_ctx_t *ctx, cell_t *tmp_a, cell_t *tmp_b)
{
  for(int i = 0; i < ctx->sink_pos; ++i) {
    mark_cells(ctx->sink[i]);
  }
  mark_cells(ctx->syms);
  mark_cells(ctx->env);
  mark_cells(ctx->args);
  mark_cells(ctx->result);
  mark_cells(tmp_a);
  mark_cells(tmp_b);


  for (int i = 0; i < MAX_MEMORY; ++i) {
    cell_t *current_cell = &ctx->memory[i];
    if ((current_cell->flags & (CELL_F_USED | CELL_F_MARK)) == CELL_F_USED) {
      current_cell->flags = 0;
#if 0
      /* this is useful for debugging garbage collector */
      memset(current_cell, 0, sizeof(*current_cell));
#endif
      ctx->memory_in_use -= 1;
    }
  }

  for (int i = 0; i < ctx->sink_pos; ++i) {
    unmark_cells(ctx->sink[i]);
  }
  unmark_cells(ctx->syms);
  unmark_cells(ctx->env);
  unmark_cells(ctx->args);
  unmark_cells(ctx->result);
  unmark_cells(tmp_a);
  unmark_cells(tmp_b);
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
#define is_integer(obj) ((obj)->type == CELL_T_INTEGER)
#define is_null(ctx, obj) ((ctx)->NIL == obj)
#define is_sym(obj) ((obj)->type == CELL_T_SYMBOL)
#define is_pair(obj) ((obj)->type == CELL_T_PAIR)
#define is_true(ctx, obj) ((obj) != (ctx)->FALSE)
#define is_false(ctx, obj) ((obj) == (ctx)->FALSE)
#define is_primop(obj) ((obj)->type == CELL_T_PRIMOP)
#define is_lambda(obj) ((obj)->type == CELL_T_LAMBDA)
#define is_macro(obj) ((obj)->type == CELL_T_MACRO)
/* symbols */

static int get_args(cell_t *args, int nr, int types[], cell_t *ret[]);

cell_t *mk_symbol(scheme_ctx_t *ctx, char *str)
{
  for (cell_t *tmp = ctx->syms; !is_null(ctx, tmp); tmp = _cdr(tmp)) {
    if (!strcmp(str, _car(tmp)->u.symbol)) {
      return _car(tmp);
    }
  }
  /* add new entry */
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_SYMBOL;
  ret->u.symbol = strdup(str);
  ctx->syms = cons(ctx, ret, ctx->syms);
  return ret;
}

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

cell_t *mk_integer(scheme_ctx_t *ctx, int integer)
{
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_INTEGER;
  ret->u.integer = integer;
  return ret;
}

cell_t *mk_lambda(scheme_ctx_t *ctx, cell_t *lambda)
{
  cell_t *arg[2];
  int types[2] = {CELL_T_EMPTY, CELL_T_EMPTY};
  if (get_args(lambda, 2, types, arg)) {
    return ctx->NIL;
  }
  if (!is_pair(arg[0]) && !is_sym(arg[0]) && !is_null(ctx, arg[0])) {
    printf("lambda: parameter 1 must be a pair or sym\n");
    return ctx->NIL;
  }
  cell_t *ret = get_cell(ctx);
  ret->type = CELL_T_LAMBDA;
  ret->u.lambda.names = _car(lambda); /* XXX arg 1 */
  ret->u.lambda.body = _car(_cdr(lambda)); /* XXX arg 2 */
  return ret;
}

cell_t *mk_macro(scheme_ctx_t *ctx, cell_t *arg, cell_t *body)
{

#if 0
      printf("DEBUG:");
      printf("\nname = ");
      print_obj(ctx, macro_name);
      printf("\narg = ");
      print_obj(ctx, macro_arg);
      printf("\nbody= ");
      print_obj(ctx, body);
      printf("\n");
#endif
      cell_t *ret = get_cell(ctx);
      ret->type = CELL_T_MACRO;
      ret->u.macro.arg_name = arg;
      ret->u.macro.body = body;
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
      if (strlen(token) == strspn(token, "-0123456789")) {
        char *rest = NULL;
        int v = strtol(token, &rest, 10);
        if (*rest == '\0') {
          ret = mk_integer(ctx, v);
        } else {
          ret = mk_symbol(ctx, token);
        }
      } else {
        ret = mk_symbol(ctx, token);
      }
      break;
  }
  return ret;
}

cell_t *get_object(scheme_ctx_t *ctx);
cell_t *get_obj_list(scheme_ctx_t *ctx)
{
  cell_t *obj = get_object(ctx);
  if (!obj) {
    /* EOF reached */
    printf("missing ')' at end of input\n");
    exit(1);
  }
  if (obj == ctx->PARENTHESIS_CLOSE) {
    return ctx->NIL;
  }
  if (obj == ctx->SYMBOL_DOT) {
    cell_t *ret = get_object(ctx);
    if (ctx->PARENTHESIS_CLOSE == ret) {
      /* error */
      printf("unexpected ')'\n");
      return ctx->NIL;
    } else if (ctx->PARENTHESIS_CLOSE != get_object(ctx)) {
      printf("expect ')'!\n");
      return ctx->NIL;
    }
    return ret;
  }
  return cons(ctx, obj, get_obj_list(ctx));
}

cell_t *get_object(scheme_ctx_t *ctx)
{
  char *value = tokenizer_get_token(&ctx->tokenizer_ctx);
  if (!value) {
    return NULL; /* eof */
  }
  cell_t *obj = mk_object_from_token(ctx, value);
  if (obj == ctx->SYMBOL_QUOTE_ALIAS) {
    return cons(ctx, ctx->SYMBOL_QUOTE, cons(ctx, get_object(ctx), ctx->NIL));
  } else if (obj == ctx->SYMBOL_QUASIQUOTE_ALIAS) {
    return cons(ctx, ctx->SYMBOL_QUASIQUOTE, get_object(ctx));
  } else if (obj == ctx->SYMBOL_UNQUOTE_ALIAS) {
    return cons(ctx, ctx->SYMBOL_UNQUOTE, cons(ctx, get_object(ctx), ctx->NIL));
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
    case CELL_T_INTEGER:
      printf("%i ", obj->u.integer);
      break;
    case CELL_T_LAMBDA:
      printf("<lambda>");
      break;
    case CELL_T_MACRO:
      printf("<macro>");
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

static char *get_type_name(int type)
{
  return cell_type_names[type];
}

static int get_args(cell_t *args, int nr, int types[], cell_t *ret[])
{
  int i = 0;
  while (i < nr) {
    if (!is_pair(args)) {
      printf("ERROR: missing argument, expected %d given %d\n", nr, i);
      return -1;
    }
    cell_t *obj = _car(args);
    if (obj->type != types[i]) {
      if (types[i] != CELL_T_EMPTY) {
        printf("ERROR: %s expected %s given\n",
            get_type_name(types[i]), get_type_name(obj->type));
        return -1;
      }
    }
    ret[i] = obj;
    args = _cdr(args);
    ++i;
  }
  int to_many_args = 0;
  while (is_pair(args)) {
    ++to_many_args;
    args = _cdr(args);
  }
  if (to_many_args) {
    printf("ERROR: to many arguments %d expected %d given\n",
        nr, nr + to_many_args);
    return -1;
  }
  return 0;
}

cell_t *car(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[1];
  int types[1] = {CELL_T_PAIR};
  if (get_args(args, 1, types, arg)) {
    return ctx->NIL;
  }
  return _car(arg[0]);
}

cell_t *cdr(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[1];
  int types[1] = {CELL_T_PAIR};
  if (get_args(args, 1, types, arg)) {
    return ctx->NIL;
  }
  return _cdr(arg[0]);
}

static void plus_cb(int *ret, int in) { *ret += in; }
static void minus_cb(int *ret, int in) { *ret -= in; }
static void mul_cb(int *ret, int in) { *ret *= in; }
static void div_cb(int *ret, int in) { *ret /= in; }

cell_t *do_integer_math(
    scheme_ctx_t *ctx,
    cell_t *args,
    void (*cb)(int *ret, int in))
{
  int ret = 0;
  int i = 0;
  while (!is_null(ctx, args)) {
    if (is_integer(_car(args))) {
      if (i == 0) {
        ret = _car(args)->u.integer;
      } else {
        (*cb)(&ret, _car(args)->u.integer);
      }
    } else {
      printf("ERROR: integer expected %s given\n", get_type_name(_car(args)->type));
      return ctx->NIL;
    }
    ++i;
    args = _cdr(args);
  }
  return mk_integer(ctx, ret);
}


cell_t *op_minus(scheme_ctx_t *ctx, cell_t *args) {
  return do_integer_math(ctx, args, minus_cb);
}
cell_t *op_plus(scheme_ctx_t *ctx, cell_t *args) {
  return do_integer_math(ctx, args, plus_cb);
}
cell_t *op_mul(scheme_ctx_t *ctx, cell_t *args) {
  return do_integer_math(ctx, args, mul_cb);
}
cell_t *op_div(scheme_ctx_t *ctx, cell_t *args) {
  return do_integer_math(ctx, args, div_cb);
}

cell_t *op_gt(scheme_ctx_t *ctx, cell_t *args) {
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return arg[0]->u.integer > arg[1]->u.integer ? ctx->TRUE : ctx->FALSE;
}

cell_t *op_gt_eq(scheme_ctx_t *ctx, cell_t *args) {
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return arg[0]->u.integer >= arg[1]->u.integer ? ctx->TRUE : ctx->FALSE;
}

cell_t *op_lt(scheme_ctx_t *ctx, cell_t *args) {
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return arg[0]->u.integer < arg[1]->u.integer ? ctx->TRUE : ctx->FALSE;
}

cell_t *op_lt_eq(scheme_ctx_t *ctx, cell_t *args) {
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return arg[0]->u.integer <= arg[1]->u.integer ? ctx->TRUE : ctx->FALSE;
}

cell_t *write_primop(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg_array[1];
  int arg_types[] = {CELL_T_EMPTY};
  if (!get_args(args, 1, arg_types, arg_array)) {
    print_obj(ctx, arg_array[0]);
  }
  return ctx->NIL;
}

cell_t *display(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg_array[1];
  int arg_types[] = {CELL_T_EMPTY};
  if (!get_args(args, 1, arg_types, arg_array)) {
    if (arg_array[0]->type == CELL_T_STRING) {
      printf("%s", arg_array[0]->u.string);
    } else {
      print_obj(ctx, arg_array[0]);
    }
  }
  return ctx->NIL;
}

cell_t *newline(scheme_ctx_t *ctx, cell_t *args)
{
  printf("\n");
  return ctx->NIL;
}

cell_t *flush_output(scheme_ctx_t *ctx, cell_t *args)
{
  /* ignore args for now */
  fflush(stdout);
  return ctx->NIL;
}

cell_t *eq(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[2];
  int arg_types[2] = {CELL_T_EMPTY, CELL_T_EMPTY};
  if (get_args(args, 2, arg_types, arg)) {
    return ctx->FALSE;
  }
  return (arg[0] == arg[1]) ? ctx->TRUE : ctx->FALSE;
}

cell_t *integer_eq(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return arg[0]->u.integer == arg[1]->u.integer ? ctx->TRUE : ctx->FALSE;
}

cell_t *modulo(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[2];
  int types[2] = {CELL_T_INTEGER, CELL_T_INTEGER};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return mk_integer(ctx, arg[0]->u.integer % arg[1]->u.integer);
}

cell_t *eqv(scheme_ctx_t *ctx, cell_t *args)
{
  return ctx->FALSE;
}

cell_t *apply_primop(scheme_ctx_t *ctx, cell_t *primop, cell_t *args)
{
  return primop->u.primop(ctx, args);
}

int list_length(cell_t *args)
{
  int ret = 0;
  while (is_pair(args)) {
    ++ret;
    args = _cdr(args);
  }
  return ret;
}

cell_t *primop_length(scheme_ctx_t *ctx, cell_t *args)
{
  int ret = 0;
  cell_t *arg[1];
  int types[1] = {CELL_T_EMPTY};
  if (get_args(args, 1, types, arg)) {
    return ctx->NIL;
  }
  for(cell_t *c = arg[0]; is_pair(c); c = _cdr(c)) {
    ++ret;
  }
  return mk_integer(ctx, ret);
}

cell_t *primop_cons(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[2];
  int types[2] = {CELL_T_EMPTY, CELL_T_EMPTY};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  return cons(ctx, arg[0], arg[1]);
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

cell_t *eval_primop(scheme_ctx_t *ctx, cell_t *obj)
{
  if (list_length( obj )!= 1) {
    printf("eval only has/needs one argument\n");
    return ctx->NIL;
  }
  return eval(ctx, _car(obj));
}

cell_t *eval_list(scheme_ctx_t *ctx, cell_t *list)
{
  if (ctx->NIL == list) {
    return ctx->NIL;
  }
  /* XXX the result of eval is not in sink ... only in ctx->result but
   * this will be overwritten by any other eval */
  return cons(ctx, add_to_sink(ctx, eval(ctx, _car(list))), eval_list(ctx, _cdr(list)));
}

cell_t *eval_quasiquote(scheme_ctx_t *ctx, cell_t *list)
{
  if (!is_pair(list)) {
    return list;
  }
  cell_t *obj;
  if (ctx->SYMBOL_UNQUOTE == _car(list)) {
    return eval (ctx, _car(_cdr(list)));
  } else if (is_pair(_car(list)) && ctx->SYMBOL_UNQUOTE == _car(_car(list))) {
    obj = eval(ctx, _car(_cdr(_car(list))));
  } else {
    obj = _car(list);
  }
  return (cons(ctx, add_to_sink(ctx, obj), eval_quasiquote(ctx, _cdr(list))));
}

cell_t *apply_macro( scheme_ctx_t *ctx, cell_t *macro, cell_t *args)
{
  cell_t *ret = ctx->NIL;
  cell_t *old_env = ctx->env;

  env_define(ctx, macro->u.macro.arg_name, args);
  ret = eval(ctx, eval(ctx, macro->u.macro.body));

  ctx->env = old_env;

  return ret;
}

cell_t *apply_lambda(
    scheme_ctx_t *ctx,
    cell_t *lambda,
    cell_t *args,
    cell_t *last_lambda,
    cell_t **tail_recursion_args);

cell_t *apply(scheme_ctx_t *ctx, cell_t *args)
{
  cell_t *arg[2];
  int types[2] = {CELL_T_EMPTY, CELL_T_EMPTY};
  if (get_args(args, 2, types, arg)) {
    return ctx->NIL;
  }
  /* XXX check if arg[1] is either NULL or PAIR */
  if (!is_null(ctx, arg[1]) && !is_pair(arg[1])) {
    printf("argument 1 must be pair (or null)\n");
  } else if (is_primop(arg[0])) {
    return apply_primop(ctx, arg[0], arg[1]);
  } else if (is_lambda(arg[0])) {
    return apply_lambda(ctx, arg[0], arg[1], NULL, NULL);
  } else {
    printf("error applying\n");
  }
  return ctx->NIL;
}

cell_t *eval_ex(scheme_ctx_t *ctx,
    cell_t *obj,
    cell_t *last_lambda,
    cell_t **tail_recursion_args);


cell_t *apply_lambda(
    scheme_ctx_t *ctx,
    cell_t *lambda,
    cell_t *args,
    cell_t *last_lambda,
    cell_t **tail_recursion_args)
{
  if (lambda == last_lambda && tail_recursion_args) {
    /* TAIL RECURSION: */
    /* evaluate arguments and return to caller */
    *tail_recursion_args = eval_list(ctx, args);
    return ctx->NIL;
  }
  cell_t *old_env = ctx->env;  /* XXX */
  int old_sink_pos = ctx->sink_pos;
  cell_t *rec = NULL;
  cell_t *vars  = args;
  cell_t *ret;

  do {
    cell_t *names = lambda->u.lambda.names;
    cell_t *body  = lambda->u.lambda.body;
    if (is_pair(names)) {
      for( ;
          !is_null(ctx, names) && !is_null(ctx, vars);
          names = _cdr(names), vars = _cdr(vars)) {
        /* when in TAIL RECURSION arguments are already evaluated ... */
        if (rec) {
          env_define(ctx, _car(names), _car(vars));
        } else {
          env_define(ctx, _car(names), eval(ctx, _car(vars)));
        }
      }
    } else {
      if (rec) {
        env_define(ctx, names, vars);
      } else {
        env_define(ctx, names, eval_list(ctx, vars));
      }
    }
    rec = NULL;
    ret = eval_ex(ctx, body, lambda, &rec);
    if (rec) {
      /* TAIL RECURSION */
      vars = rec;
      ctx->args = vars; /* for gc */
      ctx->result = ret; /* for gc */
    } else {
      ctx->args = ctx->NIL;
    }
    ctx->env = old_env;  /* XXX */
    ctx->sink_pos = old_sink_pos; /*XXX  */
  } while(rec);
  return ret;
}


cell_t *eval(scheme_ctx_t *ctx, cell_t *obj)
{
  return eval_ex(ctx, obj, NULL, NULL);
}
cell_t *eval_ex(
    scheme_ctx_t *ctx,
    cell_t       *obj,
    cell_t       *last_lambda,
    cell_t      **tail_recursion_args)
{
  cell_t *ret = ctx->NIL;
  int old_sink_pos = ctx->sink_pos;

  if (is_null(ctx, obj)) {
    printf("error try to apply NULL\n");
  } else if (!is_pair(obj)) {
    if (is_sym(obj)) {
      ret = env_resolve(ctx, obj);
    } else {
      ret = obj;
    }
  } else if (is_sym(_car(obj))) {
    cell_t *cmd = _car(obj);
    cell_t *args = _cdr(obj);
    if (cmd == ctx->SYMBOL_IF) {
      /* (if a b c) */
      if (list_length(args) != 3) {
        printf("ERROR if requires 3 arguments\n");
      } else {
        cell_t *a = _car(args);
        cell_t *b = _car(_cdr(args));
        cell_t *c = _car(_cdr(_cdr(args)));
        if (is_true(ctx, eval(ctx, a))) {
          ret = eval_ex(ctx, b, last_lambda, tail_recursion_args);
        } else {
          ret = eval_ex(ctx, c, last_lambda, tail_recursion_args);
        }
      }
    } else if (cmd == ctx->SYMBOL_MACRO) {
      cell_t *arg0 = _car(args); /* name + arg */
      cell_t *macro_body = _car(_cdr(args)); /* body */
      cell_t *macro_name = _car(arg0);
      cell_t *macro_arg = _car(_cdr(arg0));
      ret = env_define(ctx, macro_name,
          mk_macro(ctx, macro_arg, macro_body));
    } else if (cmd == ctx->SYMBOL_DEFINE) {
      if (list_length(args) != 2) {
        printf("ERROR: define requites 2 arguments\n");
      } else {
        cell_t *name = _car(args);
        cell_t *value = _car(_cdr(args));
        if (!is_sym(name)) {
          printf("define: name is not a symbol\n");
        } else {
          ret = env_define(ctx, name, eval(ctx, value));
        }
      }
    } else if (cmd == ctx->SYMBOL_LAMBDA) {
      ret = mk_lambda(ctx, args);
    } else if (cmd == ctx->SYMBOL_QUOTE) {
      ret = _car(args);
    } else if (cmd == ctx->SYMBOL_QUASIQUOTE) {
      ret = eval_quasiquote(ctx, args);
    } else if (cmd == ctx->SYMBOL_BEGIN) {
      while( !is_null(ctx, args)) {
        if (is_null(ctx, _cdr(args))) {
          ret = eval_ex(ctx, _car(args), last_lambda, tail_recursion_args);
        } else {
          ret = eval(ctx, _car(args));
        }
        args = _cdr(args);
      }
    } else {
      /* try to resolve symbol */
      cell_t *resolved_cmd = env_resolve(ctx, cmd);
      if (is_null(ctx, resolved_cmd)) {
        /* ERROR */
      } else if (is_primop(resolved_cmd)) {
        ret = apply_primop(ctx, resolved_cmd, eval_list(ctx, args));
      } else if (is_lambda(resolved_cmd)) {
        ret = apply_lambda(ctx, resolved_cmd, args, last_lambda, tail_recursion_args);
      } else if (is_macro(resolved_cmd)) {
        ret = apply_macro(ctx, resolved_cmd, cons(ctx, resolved_cmd, args));
      }
    }
  } else if(is_pair(_car(obj))) {
    ret = eval_ex (ctx, _car(obj), last_lambda, tail_recursion_args);
    if (is_lambda(ret)) {
      ret = apply_lambda(ctx, ret, _cdr(obj), last_lambda, tail_recursion_args);
    } else if (is_macro(ret)) {
      ret = apply_macro(ctx, ret, obj);
    }
  } else if(is_lambda(_car(obj))){
    cell_t *cmd = _car(obj);
    cell_t *args = _cdr(obj);
    ret = apply_lambda(ctx, cmd, args, last_lambda, tail_recursion_args);
  } else if(is_macro(_car(obj))) {
    ret = apply_macro(ctx, _car(obj), obj);
  } else if(is_primop(_car(obj))) {
    ret = apply_primop(ctx, _car(obj), _cdr(obj));
  } else {
    printf("cannot apply\n");
    print_obj(ctx, obj);
    printf("\n");
  }
  ctx->result = ret; /* keep result from being GCed */
  ctx->sink_pos = old_sink_pos;
  return ret;
}

/* ---------------t main .. */
void scheme_init(scheme_ctx_t *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->NIL = &ctx->NIL_VALUE;
  ctx->TRUE = &ctx->TRUE_VALUE;
  ctx->FALSE = &ctx->FALSE_VALUE;
  ctx->sink_pos = 0;
  ctx->syms = ctx->NIL;
  ctx->env = ctx->NIL;
  ctx->code = ctx->NIL;
  ctx->result = ctx->NIL;
  ctx->args = ctx->NIL;
  ctx->memory = calloc(1, sizeof(cell_t) * MAX_MEMORY);
  /* init tokenizer for stdin */
  tokenizer_init_stdio(&ctx->tokenizer_ctx, stdin);

  ctx->PARENTHESIS_OPEN = mk_symbol(ctx, "(");
  ctx->PARENTHESIS_CLOSE = mk_symbol(ctx, ")");
  ctx->SYMBOL_IF = mk_symbol(ctx, "if");
  ctx->SYMBOL_BEGIN = mk_symbol(ctx, "begin");
  ctx->SYMBOL_LAMBDA = mk_symbol(ctx, "lambda");
  ctx->SYMBOL_DOT = mk_symbol(ctx, ".");
  ctx->SYMBOL_DEFINE = mk_symbol(ctx, "define");
  ctx->SYMBOL_QUOTE_ALIAS = mk_symbol(ctx, "'");
  ctx->SYMBOL_QUOTE = mk_symbol(ctx,"quote");
  ctx->SYMBOL_QUASIQUOTE = mk_symbol(ctx, "quasiquote");
  ctx->SYMBOL_QUASIQUOTE_ALIAS = mk_symbol(ctx, "`");
  ctx->SYMBOL_UNQUOTE = mk_symbol(ctx, "unquote");
  ctx->SYMBOL_UNQUOTE_ALIAS = mk_symbol(ctx, ",");
  ctx->SYMBOL_UNQUOTE_SPLICE = mk_symbol(ctx, "unquote-splice");
  ctx->SYMBOL_UNQUOTE_SPLICE_ALIAS = mk_symbol(ctx, ",@");
  ctx->SYMBOL_MACRO = mk_symbol(ctx, "macro");

  env_define(ctx, mk_symbol(ctx, "#t"), ctx->TRUE);
  env_define(ctx, mk_symbol(ctx, "#f"), ctx->FALSE);
  env_define(ctx, mk_symbol(ctx, "eq?"), mk_primop(ctx, &eq));
  env_define(ctx, mk_symbol(ctx, "apply"), mk_primop(ctx, &apply));
  env_define(ctx, mk_symbol(ctx, "eval"), mk_primop(ctx, &eval_primop));
  env_define(ctx, mk_symbol(ctx, "write"), mk_primop(ctx, &write_primop));
  env_define(ctx, mk_symbol(ctx, "display"), mk_primop(ctx, &display));
  env_define(ctx, mk_symbol(ctx, "newline"), mk_primop(ctx, &newline));
  env_define(ctx, mk_symbol(ctx, "flush-output"), mk_primop(ctx, &flush_output));
  env_define(ctx, mk_symbol(ctx, "cons"), mk_primop(ctx, &primop_cons));
  env_define(ctx, mk_symbol(ctx, "length"), mk_primop(ctx, &primop_length));
  env_define(ctx, mk_symbol(ctx, "car"), mk_primop(ctx, &car));
  env_define(ctx, mk_symbol(ctx, "cdr"), mk_primop(ctx, &cdr));

  env_define(ctx, mk_symbol(ctx, "+"), mk_primop(ctx, &op_plus));
  env_define(ctx, mk_symbol(ctx, "-"), mk_primop(ctx, &op_minus));
  env_define(ctx, mk_symbol(ctx, "*"), mk_primop(ctx, &op_mul));
  env_define(ctx, mk_symbol(ctx, "/"), mk_primop(ctx, &op_div));
  env_define(ctx, mk_symbol(ctx, "modulo"), mk_primop(ctx, &modulo));

  env_define(ctx, mk_symbol(ctx, "="), mk_primop(ctx, &integer_eq));
  env_define(ctx, mk_symbol(ctx, ">"), mk_primop(ctx, &op_gt));
  env_define(ctx, mk_symbol(ctx, "<"), mk_primop(ctx, &op_lt));
  env_define(ctx, mk_symbol(ctx, ">="), mk_primop(ctx, &op_gt_eq));
  env_define(ctx, mk_symbol(ctx, "<="), mk_primop(ctx, &op_lt_eq));
  ctx->sink_pos = 0;
  gc_collect(ctx, ctx->NIL, ctx->NIL);
  /* debug output */
  gc_info(ctx);
}

#if 0
void scheme_load_memory(scheme_ctx_t *ctx, char *memory, size_t len)
{
  struct memory_get_char_data_s data;
  data.len = len;
  data.pos = 0;
  data.memory = memory;
  tokenizer_init(&ctx->tokenizer_ctx, memory_get_char, &data);
  for (cell_t *obj = get_object(ctx); obj; obj=get_object(ctx)) {
    eval(ctx, obj);
  }
}
#endif

void scheme_load_file(scheme_ctx_t *ctx, char *filename)
{
  FILE *fd = fopen(filename, "r");
  if (!fd) {
    printf("error open file\n");
    return;
  }
  tokenizer_init_stdio(&ctx->tokenizer_ctx, fd);

  for (cell_t *obj = get_object(ctx); obj; obj=get_object(ctx)) {
    eval(ctx, obj);
  }
  fclose(fd);
}

int main(int argc, char *argv[])
{
  scheme_ctx_t ctx;
  scheme_init(&ctx);

  if (argc == 2) {
    scheme_load_file(&ctx, argv[1]);
  } else {
    cell_t *obj;
    while((obj = get_object(&ctx))) {
      print_obj(&ctx, eval(&ctx, obj));
      printf("\n");
    }
  }

#if 0
  char *str = "(display (+ 1 1)) (newline)";
  scheme_load_memory(&ctx, str, strlen(str));
    for (cell_t *t = ctx.sink; is_pair(t); t = _cdr(t)) {
      printf("sink: %s ", get_type_name(_car(t)->type));
      if (is_sym(_car(t))) {
        printf("%s", _car(t)->u.symbol);
      }
      printf("\n");
    }
#endif
    gc_info(&ctx);
  return 0;
}
