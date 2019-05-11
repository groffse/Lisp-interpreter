#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Parser Declariations */

mpc_parser_t* Number; 
mpc_parser_t* Integer; 
mpc_parser_t* Double; 
mpc_parser_t* Symbol; 
mpc_parser_t* String; 
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;  
mpc_parser_t* Qexpr;  
mpc_parser_t* Expr; 
mpc_parser_t* Lispy;

/* Forward Declarations */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
/*typedef union dataType {
    double d; long l;
}number;*/

typedef struct dataType{
    enum {
      typLong,
      typDouble,
    } nType;

    union
    {
        long l;
        double d;
    } value;
}number;

/* Lisp Value */

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_STR, 
       LVAL_FUN, LVAL_OBJ, LVAL_INST, LVAL_SEXPR, LVAL_QEXPR };
       
typedef lval*(*lbuiltin)(lenv*, lval*);

typedef int bool;
#define true 1;
#define false 0;

typedef struct{
	bool bFound;
	int iPosition;
}tTuple;

struct lval {
  int type;
  int subType; // Number is either INT or DOUBLE
  
  /* Basic */
  number num;
  
  char* err;
  char* sym;
  char* str;
  
  /* Function */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;
  
  /* Object */
  lbuiltin objBuiltin;
  lenv* objEnv;
  lval* objSlots;
  
  /* Object instance */
  lbuiltin instBuiltin;
  lenv* instEnv;
  lval* memberVariables;
  char* name;
  
  /* Expression */
  int count;
  lval** cell;
};

lval* lval_num(number x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	if(x.nType == typLong){
		v->num.value.d = 0;
		v->num.value.l = x.value.l;
		v->num.nType = typLong;
	}
		
	else{
		v->num.value.l = 0;
		v->num.value.d = x.value.d;
		v->num.nType = typDouble;
	}
	return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;  
  va_list va;
  va_start(va, fmt);  
  v->err = malloc(512);  
  vsnprintf(v->err, 511, fmt, va);  
  v->err = realloc(v->err, strlen(v->err)+1);
  va_end(va);  
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

// OBJECT IN THE MAKING
lval* lval_objBuiltin(lbuiltin obj)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_OBJ;
	v->objBuiltin = obj;
	return v;
}

lval* lval_str(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_STR;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);
  return v;
}

lval* lval_builtin(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

lenv* lenv_new(void);

// INSTANCE IN THE MAKING
lval* lval_instance(lval* memberVariables, char* n){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_INST;
	v->memberVariables = memberVariables;
	
	// We will need to copy the object into the instance
	v->instEnv = lenv_new();
	v->name = malloc(strlen(n) + 1);
	strcpy(v->name, n);
	return v;
}

// OBJECT IN THE MAKING
lval* lval_object(lval* objSlots)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_OBJ;
	
	v->objBuiltin = NULL;
	
	v->objEnv = lenv_new();
	
	v->objSlots = objSlots;
	return v;
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;  
  v->builtin = NULL;  
  v->env = lenv_new();  
  v->formals = formals;
  v->body = body;
  return v;  
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lenv_del(lenv* e);

void lval_del(lval* v) {

  switch (v->type) {
    case LVAL_NUM: break;
    case LVAL_FUN: 
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
    break;
	case LVAL_OBJ:
		if(!v->objBuiltin){
			lenv_del(v->objEnv);
			lval_del(v->objSlots);
		}
	break;
	case LVAL_INST:
		lenv_del(v->instEnv);
		lval_del(v->memberVariables);
		free(v->name);
	break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_STR: free(v->str); break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
    break;
  }
  
  free(v);
}

lenv* lenv_copy(lenv* e);

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;
  switch (v->type) {
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
    break;
	case LVAL_OBJ:
		if(v->objBuiltin) {
			x->objBuiltin = v->objBuiltin;
			
		} else {
			x->objBuiltin = NULL;
			x->objEnv = lenv_copy(v->objEnv);
			x->objSlots = lval_copy(v->objSlots);
		}
	break;
	case LVAL_INST:
		x->instEnv = lenv_copy(v->instEnv);
		x->memberVariables = lval_copy(v->memberVariables);
		x->name = malloc(strlen(v->name) + 1);
		strcpy(x->name, v->name);
	break;
    case LVAL_NUM:
		if(v->num.nType == typLong) {
			x->num.value.l = v->num.value.l;
			x->num.nType = typLong;
			break;
		}
		else {
			x->num.value.d = v->num.value.d;
			x->num.nType = typDouble;
			break;
		}
	break;
    case LVAL_ERR: x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
    break;
    case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
    break;
    case LVAL_STR: x->str = malloc(strlen(v->str) + 1);
      strcpy(x->str, v->str);
    break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }
  return x;
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_join(lval* x, lval* y) {  
  for (int i = 0; i < y->count; i++) {
    x = lval_add(x, y->cell[i]);
  }
  free(y->cell);
  free(y);  
  return x;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];  
  memmove(&v->cell[i],
    &v->cell[i+1], sizeof(lval*) * (v->count-i-1));  
  v->count--;  
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

void lval_print(lval* v);

void lval_print_expr(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);    
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print_str(lval* v) {
  /* Make a Copy of the string */
  char* escaped = malloc(strlen(v->str)+1);
  strcpy(escaped, v->str);
  /* Pass it through the escape function */
  escaped = mpcf_escape(escaped);
  /* Print it between " characters */
  printf("\"%s\"", escaped);
  /* free the copied string */
  free(escaped);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_FUN:
      if (v->builtin) {
        printf("<builtin>");
      } else {
        printf("(\\ ");
        lval_print(v->formals);
        putchar(' ');
        lval_print(v->body);
        putchar(')');
      }
    break;
	case LVAL_OBJ:
		printf("Object: ");
		lval_print(v->objSlots);
	break;
    case LVAL_NUM:
		if(v->num.nType == typLong)
		{
			printf("%li", v->num.value.l);
			break;
		}
			 
		else
		{
			printf("%lf", v->num.value.d); break;
		}
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_STR:   lval_print_str(v); break;
    case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
    case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

int NUM_EQ(lval* x, lval* y){
	if(x->num.nType == typLong && y->num.nType == typLong)
		return ((x->num.value.l == y->num.value.l) ? 1 : 0);
	else if(x->num.nType == typLong && y->num.nType == typDouble)
		return ((x->num.value.l == y->num.value.d) ? 1 : 0);
	else if(x->num.nType == typDouble && y->num.nType == typLong)
		return ((x->num.value.d == y->num.value.l) ? 1 : 0);
	else if(x->num.nType == typDouble && y->num.nType == typDouble)
		return ((x->num.value.d == y->num.value.d) ? 1 : 0);		
}

int lval_eq(lval* x, lval* y) {
  if (x->type != y->type) { return 0; }
  
  switch (x->type) {
    case LVAL_NUM: return ( NUM_EQ(x,y)/*x->num.value.l == y->num.value.l*/ );    
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);    
    case LVAL_STR: return (strcmp(x->str, y->str) == 0);    
    case LVAL_FUN: 
      if (x->builtin || y->builtin) {
        return x->builtin == y->builtin;
      } else {
        return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
      }    
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      if (x->count != y->count) { return 0; }
      for (int i = 0; i < x->count; i++) {
        if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
      }
      return 1;
    break;
  }
  return 0;
}

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: 		return "Function";
	case LVAL_OBJ: 		return "Object";
	case LVAL_INST: 	return "Instance";
    case LVAL_NUM: 		return "Number";
    case LVAL_ERR: 		return "Error";
    case LVAL_SYM: 		return "Symbol";
    case LVAL_STR: 		return "String";
    case LVAL_SEXPR: 	return "S-Expression";
    case LVAL_QEXPR: 	return "Q-Expression";
    default: 			return "Unknown";
  }
}

/* Lisp Environment */

struct lenv {
  lenv* par;
  int count;
  char** syms;
  lval** vals;
};

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }  
  free(e->syms);
  free(e->vals);
  free(e);
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);
  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

lval* lenv_get_local(lenv* e, lval* k){
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("Unbound Symbol '%s'", k->sym);
}

lval* lenv_get(lenv* e, lval* k) {
  
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
  }
  
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("Unbound Symbol '%s'", k->sym);
  }
}

void lenv_put(lenv* e, lval* k, lval* v) {
  
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }
  
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);  
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->par) { e = e->par; }
  lenv_put(e, k, v);
}

/* Builtins */

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);

lval* lval_eval(lenv* e, lval* v);

lval* builtin_member(lenv* e, lval* a){
	return lval_err("We need to do more!");
}

lval* lval_eval_sexpr(lenv* e, lval* v);
//lval* builtin_eval(lenv* e, lval* a);

lval* builtin_instance(lenv* e, lval* a) {
	
	// Check that we get object name and instance name only
	if(a->count != 2)
	{
		return lval_err("Passed incorrect number of arguments. Expected '2', got '%i'.", a->count);
	}
	
	// Check that the object name passed by user actually is an object
	if(a->cell[0]->type != LVAL_OBJ)
	{
		return lval_err("Expected '%s', got '%s'", ltype_name(LVAL_OBJ), ltype_name(a->cell[0]->type));
	}
	
	// Checking the last argument is a Q-Expression
	if(a->cell[1]->type != LVAL_QEXPR)
	{
		return lval_err("Expected '%s', got '%s'", ltype_name(LVAL_QEXPR), ltype_name(a->cell[1]->type));
	}
	
	lval* inst_name = lval_pop(a,1);
	
	if(inst_name->count == 0)
	{
		return lval_err("Need more than than '0' instance names");
	}
	 
	
	
	for(int i = 0; i < inst_name->count; i++)
	{
		lval* inst = lval_instance(a->cell[0]->objSlots, inst_name->cell[i]->sym);

		inst->instEnv = lenv_copy(e);
		//inst->instEnv->par = e;
		// Evaluate every objectslot and put into instance environment
		for(int j = 0; j < a->cell[0]->objSlots->count; j++)
		{
			lval* result = lval_eval_sexpr(inst->instEnv, a->cell[0]->objSlots->cell[j]);

			lval_del(result);
		}

		lenv_def(e, inst_name->cell[i], inst);

		lval_del(a);
		lval_del(inst);
		
	}
	
	return lval_sexpr();
	
	/*
	// Now we need to execute the object slots and put them in the instance environment
	
	
	//lenv_def(e, inst_name->cell[0], inst);
	inst->instEnv = lenv_copy(e);
	
	// Evaluate every objectslot and put into instance environment
	for(int i = 0; i < a->cell[0]->objSlots->count; i++)
	{
		lval* result = lval_eval_sexpr(inst->instEnv, a->cell[0]->objSlots->cell[i]);
		lval_del(result);
	}
	
	lenv_def(e, inst_name->cell[0], inst);

	lval_del(a);
	lval_del(inst);
	
	return lval_sexpr();
	*/
}



// OBJECT IN THE MAKING
lval* builtin_object(lenv* e, lval* a)
{
	LASSERT_NUM("obj", a, 1);
	LASSERT_TYPE("obj", a, 0, LVAL_QEXPR);
	/* Check first Q-Expression contains only Symbols */
	/* This checks the object body */
	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SEXPR),
		" Got %s, Expected %s.",
		ltype_name(a->cell[0]->cell[i]->type),ltype_name(LVAL_SEXPR));
	}
	
	lval* slots = lval_pop(a, 0);
	lval_del(a);
	return lval_object(slots);
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);
  
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
      "Cannot define non-symbol. Got %s, Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }
  
  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);
  
  return lval_lambda(formals, body);
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_head(lenv* e, lval* a) {
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);
  
  lval* v = lval_take(a, 0);  
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_NUM("tail", a, 1);
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail", a, 0);

  lval* v = lval_take(a, 0);  
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_NUM("eval", a, 1);
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
  
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
  }
  
  lval* x = lval_pop(a, 0);
  
  while (a->count) {
    lval* y = lval_pop(a, 0);
    x = lval_join(x, y);
  }
  
  lval_del(a);
  return x;
}

void ADD_NUM(lval* x, lval* y){	

	if(x->num.nType == typLong && y->num.nType == typLong) {x->num.value.l += y->num.value.l; }
	else if(x->num.nType == typLong && y->num.nType == typDouble) {
		
		x->num.value.d += y->num.value.d + x->num.value.l;
		x->num.nType = typDouble;
	}
	else if(x->num.nType == typDouble && y->num.nType == typLong) {x->num.value.d += y->num.value.l;}
	else if(x->num.nType == typDouble && y->num.nType == typDouble) {x->num.value.d += y->num.value.d;}
}

void SUB_NUM(lval* x, lval* y){	

	if(x->num.nType == typLong && y->num.nType == typLong) {x->num.value.l -= y->num.value.l; }
	else if(x->num.nType == typLong && y->num.nType == typDouble) {
		x->num.value.d += x->num.value.l - y->num.value.d;
		x->num.nType = typDouble;
	}
	else if(x->num.nType == typDouble && y->num.nType == typLong) {x->num.value.d -= y->num.value.l;}
	else if(x->num.nType == typDouble && y->num.nType == typDouble) {x->num.value.d -= y->num.value.d;}
}

void MUL_NUM(lval* x, lval* y){	

	if(x->num.nType == typLong && y->num.nType == typLong) {x->num.value.l *= y->num.value.l; }
	else if(x->num.nType == typLong && y->num.nType == typDouble) {
		x->num.value.d += y->num.value.d * x->num.value.l;
		x->num.nType = typDouble;
	}
	else if(x->num.nType == typDouble && y->num.nType == typLong) {x->num.value.d *= y->num.value.l;}
	else if(x->num.nType == typDouble && y->num.nType == typDouble) {x->num.value.d *= y->num.value.d;}
}

void DIV_NUM(lval* x, lval* y){	

	if(x->num.nType == typLong && y->num.nType == typLong) {x->num.value.l /= y->num.value.l; }
	else if(x->num.nType == typLong && y->num.nType == typDouble) {
		x->num.value.d += x->num.value.d / y->num.value.l;
		x->num.nType = typDouble;
	}
	else if(x->num.nType == typDouble && y->num.nType == typLong) {x->num.value.d /= y->num.value.l;}
	else if(x->num.nType == typDouble && y->num.nType == typDouble) {x->num.value.d /= y->num.value.d;}
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(op, a, i, LVAL_NUM);
  }
  
  lval* x = lval_pop(a, 0);
  
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		if(x->num.nType == typLong)
			x->num.value.l = -x->num.value.l;
		else
			x->num.value.d = -x->num.value.d;
	}
  
  while (a->count > 0) {  
    lval* y = lval_pop(a, 0);
    if (strcmp(op, "+") == 0) { ADD_NUM(x,y); }
    if (strcmp(op, "-") == 0) { SUB_NUM(x,y); }
    if (strcmp(op, "*") == 0) { MUL_NUM(x,y); }
    if (strcmp(op, "/") == 0) {
      if (y->num.value.l == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero.");
        break;
      }
      DIV_NUM(x,y);
    }
    
    lval_del(y);
  }
  
  lval_del(a);
  return x;
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);
  
  lval* syms = a->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
      "Function '%s' cannot define non-symbol. "
      "Got %s, Expected %s.",
      func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }
  
  LASSERT(a, (syms->count == a->count-1),
    "Function '%s' passed too many arguments for symbols. "
    "Got %i, Expected %i.",
    func, syms->count, a->count-1);
    
  for (int i = 0; i < syms->count; i++) {
    if (strcmp(func, "def") == 0) { lenv_def(e, syms->cell[i], a->cell[i+1]); }
    if (strcmp(func, "=")   == 0) { lenv_put(e, syms->cell[i], a->cell[i+1]); } 
  }
  
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) { return builtin_var(e, a, "def"); }
lval* builtin_put(lenv* e, lval* a) { return builtin_var(e, a, "="); }

number GREATER(lval* a, number r){
	if(a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.l >  a->cell[1]->num.value.l);
	else if (a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typDouble)
		r.value.l = (a->cell[0]->num.value.l >  a->cell[1]->num.value.d);
	else if (a->cell[0]->num.nType == typDouble && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.d >  a->cell[1]->num.value.l);
	else
		r.value.l = (a->cell[0]->num.value.d >  a->cell[1]->num.value.d);
	return r;
}

number LESS(lval* a, number r){
	if(a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.l <  a->cell[1]->num.value.l);
	else if (a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typDouble)
		r.value.l = (a->cell[0]->num.value.l <  a->cell[1]->num.value.d);
	else if (a->cell[0]->num.nType == typDouble && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.d <  a->cell[1]->num.value.l);
	else
		r.value.l = (a->cell[0]->num.value.d <  a->cell[1]->num.value.d);
	return r;
}

number GREATER_OR_EQUAL(lval* a, number r){
	if(a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.l >=  a->cell[1]->num.value.l);
	else if (a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typDouble)
		r.value.l = (a->cell[0]->num.value.l >=  a->cell[1]->num.value.d);
	else if (a->cell[0]->num.nType == typDouble && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.d >=  a->cell[1]->num.value.l);
	else
		r.value.l = (a->cell[0]->num.value.d >=  a->cell[1]->num.value.d);
	return r;
}

number LESS_OR_EQUAL(lval* a, number r){
	if(a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.l <=  a->cell[1]->num.value.l);
	else if (a->cell[0]->num.nType == typLong && a->cell[1]->num.nType == typDouble)
		r.value.l = (a->cell[0]->num.value.l <=  a->cell[1]->num.value.d);
	else if (a->cell[0]->num.nType == typDouble && a->cell[1]->num.nType == typLong)
		r.value.l = (a->cell[0]->num.value.d <=  a->cell[1]->num.value.l);
	else
		r.value.l = (a->cell[0]->num.value.d <=  a->cell[1]->num.value.d);
	return r;
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);
  
  //int r;
  number r;
  r.nType = typLong;
  if (strcmp(op, ">")  == 0) { r = GREATER(a, r); }
  if (strcmp(op, "<")  == 0) { r = LESS(a, r); }
  if (strcmp(op, ">=") == 0) { r = GREATER_OR_EQUAL(a, r); }
  if (strcmp(op, "<=") == 0) { r = LESS_OR_EQUAL(a, r); }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_gt(lenv* e, lval* a) { return builtin_ord(e, a, ">");  }
lval* builtin_lt(lenv* e, lval* a) { return builtin_ord(e, a, "<");  }
lval* builtin_ge(lenv* e, lval* a) { return builtin_ord(e, a, ">="); }
lval* builtin_le(lenv* e, lval* a) { return builtin_ord(e, a, "<="); }

lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);
  //int r;
  number r;
  r.nType = typLong;
  if (strcmp(op, "==") == 0) { r.value.l =  lval_eq(a->cell[0], a->cell[1]); }
  if (strcmp(op, "!=") == 0) { r.value.l = !lval_eq(a->cell[0], a->cell[1]); }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) { return builtin_cmp(e, a, "=="); }
lval* builtin_ne(lenv* e, lval* a) { return builtin_cmp(e, a, "!="); }

lval* builtin_if(lenv* e, lval* a) {
  LASSERT_NUM("if", a, 3);
  LASSERT_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", a, 2, LVAL_QEXPR);
  
  lval* x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;
  
  if (a->cell[0]->num.value.l) {
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    x = lval_eval(e, lval_pop(a, 2));
  }
  
  lval_del(a);
  return x;
}

lval* lval_read(mpc_ast_t* t);

lval* builtin_load(lenv* e, lval* a) {
  LASSERT_NUM("load", a, 1);
  LASSERT_TYPE("load", a, 0, LVAL_STR);
  
  /* Parse File given by string name */
  mpc_result_t r;
  if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {
    
    /* Read contents */
    lval* expr = lval_read(r.output);
    mpc_ast_delete(r.output);

    /* Evaluate each Expression */
    while (expr->count) {
      lval* x = lval_eval(e, lval_pop(expr, 0));
      /* If Evaluation leads to error print it */
      if (x->type == LVAL_ERR) { lval_println(x); }
      lval_del(x);
    }
    
    /* Delete expressions and arguments */
    lval_del(expr);    
    lval_del(a);
    
    /* Return empty list */
    return lval_sexpr();
    
  } else {
    /* Get Parse Error as String */
    char* err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);
    
    /* Create new error message using it */
    lval* err = lval_err("Could not load Library %s", err_msg);
    free(err_msg);
    lval_del(a);
    
    /* Cleanup and return error */
    return err;
  }
}

lval* builtin_print(lenv* e, lval* a) {
  
  /* Print each argument followed by a space */
  for (int i = 0; i < a->count; i++) {
    lval_print(a->cell[i]); putchar(' ');
  }
  
  /* Print a newline and delete arguments */
  putchar('\n');
  lval_del(a);
  
  return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
  LASSERT_NUM("error", a, 1);
  LASSERT_TYPE("error", a, 0, LVAL_STR);
  
  /* Construct Error from first argument */
  lval* err = lval_err(a->cell[0]->str);
  
  /* Delete arguments and return */
  lval_del(a);
  return err;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_builtin(func);
  lenv_put(e, k, v);
  lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* Variable Functions */
  lenv_add_builtin(e, "\\",  builtin_lambda);
  lenv_add_builtin(e, "obj", builtin_object); // OBJECT IN THE MAKING
  lenv_add_builtin(e, "instance", builtin_instance); // INSTANCE IN THE MAKING
  lenv_add_builtin(e, "->", builtin_member); 
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=",   builtin_put);
  
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  
  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  
  /* Comparison Functions */
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  lenv_add_builtin(e, ">",  builtin_gt);
  lenv_add_builtin(e, "<",  builtin_lt);
  lenv_add_builtin(e, ">=", builtin_ge);
  lenv_add_builtin(e, "<=", builtin_le);
  
  /* String Functions */
  lenv_add_builtin(e, "load",  builtin_load); 
  lenv_add_builtin(e, "error", builtin_error);
  lenv_add_builtin(e, "print", builtin_print);
}

tTuple find_member_variables(lval* a, lval* k)
{
	tTuple ans = {
		.bFound = 0,
		.iPosition = 0
	};
	for (int i = 0; i < a->memberVariables->count; i++) {
		if (strcmp(a->memberVariables->cell[i]->sym, k->sym) == 0) {
		  ans.bFound = true;
		  ans.iPosition = i;
		  return ans;
		}
	}
  return ans;
}

/* Evaluation */

lval* lval_call(lenv* e, lval* f, lval* a);

lval* lval_instance_call (lenv* e, lval* f, lval* a){
	
	lval* member_func = lval_pop(a, 0);
	lval* givenVariable = lval_pop(a, 0);
	lval* inst_var_vall = lenv_get_local(f->instEnv, givenVariable->cell[0]);
	if(inst_var_vall->type == LVAL_NUM){
		lval_del(a);
		return inst_var_vall;
	}
	else if(inst_var_vall->type == LVAL_FUN){
		lval* res = lval_call(f->instEnv, inst_var_vall, a);
		lval_del(a);
		return res;
	}
}

lval* lval_object_call (lenv* e, lval* f, lval* a){
	/* Record Argument Counts */
	int given = a->count;
	int total = f->objSlots->count;
	printf("%i", a->count);
	while (a->count) {
		/* If we've ran out of object slots to bind */
		if (f->objSlots->count == 0) {
		  lval_del(a);
		  return lval_err("To many arguments passed to object. "
			"Got %i, Expected %i.", given, total); 
		}
		
		/* Pop the first symbol from the slots */
		lval* sym = lval_pop(f->objSlots, 0);
		
		//printf("%c",f->objSlots->sym);
		
		/* Pop the next argument from the list */
		lval* val = lval_pop(a, 0);

		/* Bind a copy into the object's environment */
		lenv_put(f->objEnv, sym, val);

		/* Delete symbol and value */
		lval_del(sym); lval_del(val);
		
	}
	
	/* Argument list is now bound so can be cleaned up */
	lval_del(a);
	
	
    /* Otherwise return partially evaluated function */
	
    return lval_copy(f);
  }

lval* lval_call(lenv* e, lval* f, lval* a) {
  
  if (f->builtin) { return f->builtin(e, a); }
  
  int given = a->count;
  int total = f->formals->count;
  
  while (a->count) {
    
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err("Function passed too many arguments. "
        "Got %i, Expected %i.", given, total); 
    }
    
    lval* sym = lval_pop(f->formals, 0);
    
    if (strcmp(sym->sym, "&") == 0) {
      
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("Function format invalid. "
          "Symbol '&' not followed by single symbol.");
      }
      
      lval* nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym); lval_del(nsym);
      break;
    }
    
    lval* val = lval_pop(a, 0);    
    lenv_put(f->env, sym, val);    
    lval_del(sym); lval_del(val);
  }
  
  lval_del(a);
  
  if (f->formals->count > 0 &&
    strcmp(f->formals->cell[0]->sym, "&") == 0) {
    
    if (f->formals->count != 2) {
      return lval_err("Function format invalid. "
        "Symbol '&' not followed by single symbol.");
    }
    
    lval_del(lval_pop(f->formals, 0));
    
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();    
    lenv_put(f->env, sym, val);
    lval_del(sym); lval_del(val);
  }
  
  if (f->formals->count == 0) {  
    f->env->par = e;    
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    return lval_copy(f);
  }
  
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  
  lval* result;
  
  for (int i = 0; i < v->count; i++) { v->cell[i] = lval_eval(e, v->cell[i]); }
  for (int i = 0; i < v->count; i++) { if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); } }
  
  if (v->count == 0) { return v; }  
  if (v->count == 1) { return lval_eval(e, lval_take(v, 0)); }
  
  lval* f = lval_pop(v, 0);
  
  if(f->type == LVAL_OBJ)
  {
	  result = lval_object_call(e, f, v);
	  lval_del(f);
	  return result;
  }
  
  if(f->type == LVAL_INST)
  {
	  result = lval_instance_call(e, f, v);
	  lval_del(f);
	  return result;
  }
  
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
      "S-Expression starts with incorrect type. "
      "Got %s, Expected %s.",
      ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f); lval_del(v);
    return err;
  }
  result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

/* Reading */

lval* lval_read_num(mpc_ast_t* t) {
	number x;
	if(strstr(t->tag, "integer")){
		errno = 0;
		x.value.l = strtol(t->contents, NULL, 10);
		x.nType = typLong;
		return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number.");
	}
	else{
		errno = 0;
		x.value.d = strtod(t->contents, NULL);
		x.nType = typDouble;
		return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number.");
	}
  
}

lval* lval_read_str(mpc_ast_t* t) {
  /* Cut off the final quote character */
  t->contents[strlen(t->contents)-1] = '\0';
  /* Copy the string missing out the first quote character */
  char* unescaped = malloc(strlen(t->contents+1)+1);
  strcpy(unescaped, t->contents+1);
  /* Pass through the unescape function */
  unescaped = mpcf_unescape(unescaped);
  /* Construct a new lval using the string */
  lval* str = lval_str(unescaped);
  /* Free the string and return */
  free(unescaped);
  return str;
}

lval* lval_read(mpc_ast_t* t) {
  
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "string")) { return lval_read_str(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
  
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    if (strstr(t->children[i]->tag, "comment")) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  
  return x;
}

/* Main */

int main(int argc, char** argv) {
  
  Number  = mpc_new("number");
  Integer = mpc_new("integer");
  Double  = mpc_new("double");
  Symbol  = mpc_new("symbol");
  String  = mpc_new("string");
  Comment = mpc_new("comment");
  Sexpr   = mpc_new("sexpr");
  Qexpr   = mpc_new("qexpr");
  Expr    = mpc_new("expr");
  Lispy   = mpc_new("lispy");
  
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                              \
	  integer : /-?[0-9]+/ ;                       \
	  double  : /-?[0-9]+\\.[0-9]+/ ;			   \
      number  : <double> | <integer> ;             \
      symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
      string  : /\"(\\\\.|[^\"])*\"/ ;             \
      comment : /;[^\\r\\n]*/ ;                    \
      sexpr   : '(' <expr>* ')' ;                  \
      qexpr   : '{' <expr>* '}' ;                  \
      expr    : <number>  | <symbol> | <string>    \
              | <comment> | <sexpr>  | <qexpr>;    \
      lispy   : /^/ <expr>* /$/ ;                  \
    ",
    Number, Integer, Double, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
  
  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  /* Interactive Prompt */
  if (argc == 1) {
  
    puts("Lispy Version 0.0.1.1");
    puts("Press Ctrl+c to Exit\n");
  
    while (1) {
    
      char* input = readline("altLisp> ");
      add_history(input);
      
      mpc_result_t r;
      if (mpc_parse("<stdin>", input, Lispy, &r)) {
        
        lval* x = lval_eval(e, lval_read(r.output));
        lval_println(x);
        lval_del(x);
        
        mpc_ast_delete(r.output);
      } else {    
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
      }
      
      free(input);
      
    }
  }
  
  /* Supplied with list of files */
  if (argc >= 2) {
  
    /* loop over each supplied filename (starting from 1) */
    for (int i = 1; i < argc; i++) {
      
      /* Argument list with a single argument, the filename */
      lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
      
      /* Pass to builtin load and get the result */
      lval* x = builtin_load(e, args);
      
      /* If the result is an error be sure to print it */
      if (x->type == LVAL_ERR) { lval_println(x); }
      lval_del(x);
    }
  }
  
  lenv_del(e);
  
  mpc_cleanup(10, 
    Number, Integer, Double, Symbol, String, 
	Comment, Sexpr,  Qexpr,  Expr,   Lispy);
  
  return 0;
}