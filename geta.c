#include "mpc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#ifdef __linux__
#include <editline/history.h>
#endif

/* Add SYM and SEXPR as possible gval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

typedef struct gval {
    int type;
    long num;
    /* Error and Symbol types have some string data */
    char* err;
    char* sym;
    /* Count and Pointer to a list of "gval*"; */
    int count;
    struct gval** cell;
} gval;

/* Construct a pointer to a new Number gval */
gval* gval_num(long x) {
    gval* v = malloc(sizeof(gval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error gval */
gval* gval_err(char* m) {
    gval* v = malloc(sizeof(gval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Construct a pointer to a new Symbol gval */
gval* gval_sym(char* s) {
    gval* v = malloc(sizeof(gval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr gval */
gval* gval_sexpr(void) {
    gval* v = malloc(sizeof(gval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void gval_del(gval* v) {
    switch (v->type) {
        /* Do nothing special for number type */
        case LVAL_NUM: break;

        /* For Err or Sym free the string data */
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        /* If Sexpr then delete all elements inside */
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                gval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }

    /* Free the memory allocated for the "gval" struct itself */
    free(v);
}

gval* gval_add(gval* v, gval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(gval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

gval* gval_pop(gval* v, int i) {
    /* Find the item at "i" */
    gval* x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(gval*) * (v->count-i-1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(gval*) * v->count);
    return x;
}

gval* gval_take(gval* v, int i) {
    gval* x = gval_pop(v, i);
    gval_del(v);
    return x;
}

void gval_print(gval* v);

void gval_expr_print(gval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        /* Print Value contained within */
        gval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void gval_print(gval* v) {
    switch (v->type) {
        case LVAL_NUM:   printf("%li", v->num); break;
        case LVAL_ERR:   printf("Error: %s", v->err); break;
        case LVAL_SYM:   printf("%s", v->sym); break;
        case LVAL_SEXPR: gval_expr_print(v, '(', ')'); break;
    }
}

void gval_println(gval* v) { gval_print(v); putchar('\n'); }

gval* builtin_op(gval* a, char* op) {

    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            gval_del(a);
            return gval_err("Cannot operate on non-number!");
        }
    }

    /* Pop the first element */
    gval* x = gval_pop(a, 0);

    /* If no arguments and sub then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        gval* y = gval_pop(a, 0);

        /* Perform operation */
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                gval_del(x); gval_del(y);
                x = gval_err("Division By Zero.");
                break;
            }
            x->num /= y->num;
        }

        /* Delete element now finished with */
        gval_del(y);
    }

    /* Delete input expression and return result */
    gval_del(a);
    return x;
}

gval* gval_eval(gval* v);

gval* gval_eval_sexpr(gval* v) {

    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = gval_eval(v->cell[i]);
    }

    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return gval_take(v, i); }
    }

    /* Empty Expression */
    if (v->count == 0) { return v; }

    /* Single Expression */
    if (v->count == 1) { return gval_take(v, 0); }

    /* Ensure First Element is Symbol */
    gval* f = gval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        gval_del(f); gval_del(v);
        return gval_err("S-expression Does not start with symbol.");
    }

    /* Call builtin with operator */
    gval* result = builtin_op(v, f->sym);
    gval_del(f);
    return result;
}

gval* gval_eval(gval* v) {
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) { return gval_eval_sexpr(v); }
    /* All other gval types remain the same */
    return v;
}

gval* gval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        gval_num(x) : gval_err("invalid number");
}

gval* gval_read(mpc_ast_t* t) {

    /* If Symbol or Number return conversion to that type */
    if (strstr(t->tag, "number")) { return gval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return gval_sym(t->contents); }

    /* If root (>) or sexpr then create empty list */
    gval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = gval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = gval_sexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = gval_add(x, gval_read(t->children[i]));
    }

    return x;
}

int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Geta   = mpc_new("geta");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                        \
            number : /-?[0-9]+/ ;                    \
            symbol : '+' | '-' | '*' | '/' ;         \
            sexpr  : '(' <expr>* ')' ;               \
            expr   : <number> | <symbol> | <sexpr> ; \
            geta   : /^/ <expr>* /$/ ;               \
            ",
            Number, Symbol, Sexpr, Expr, Geta);

    puts("Press Ctrl+c to Exit\n");

    while (1) {
        char* input = readline("geta> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Geta, &r)) {
            gval* x = gval_eval(gval_read(r.output));
            gval_println(x);
            gval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Geta);

    return 0;
}
