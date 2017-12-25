#include "mpc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#ifdef __linux__
#include <editline/history.h>
#endif

/* possible error types */
enum { GERR_DIV_ZERO, GERR_BAD_OP, GERR_BAD_NUM };

/* possible geta val types */
enum { GVAL_NUM, GVAL_ERR };

typedef struct {
    int type;
    long num;
    int err;
} gval;

/* new number type gval */
gval gval_num(long x) {
    gval v;
    v.type = GVAL_NUM;
    v.num = x;
    return v;
}

/* new error type gval */
gval gval_err(int x) {
    gval v;
    v.type = GVAL_ERR;
    v.err = x;
    return v;
}

void gval_print(gval v) {
    switch (v.type) {
        case GVAL_NUM:
            printf("%li", v.num);
            break;
        case GVAL_ERR:
            if (v.err == GERR_DIV_ZERO) {
                printf("Error: Division By Zero!");
            }
            if (v.err == GERR_BAD_OP) {
                printf("Error: Invalid Operator!");
            }
            if (v.err == GERR_BAD_NUM) {
                printf("Error: Invalid Number!");
            }
            break;
    }
}

/* print gval followed by newline */
void gval_println(gval v) { gval_print(v); putchar('\n'); }

/* Use operator string to see which operation to perform */
gval eval_op(gval x, char* op, gval y) {

    /* If either value is an error return it */
    if (x.type == GVAL_ERR) { return x; }
    if (y.type == GVAL_ERR) { return y; }

    /* Otherwise do maths on the number values */

    if (strcmp(op, "+") == 0) { return gval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return gval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return gval_num(x.num * y.num); }
    if (strcmp(op, "^") == 0) { return gval_num(pow(x.num, y.num)); }
    if (strcmp(op, "min") == 0) {
        if (x.num <= y.num) { return gval_num(x.num); }
        return gval_num(y.num);
    }
    if (strcmp(op, "max") == 0) {
        if (x.num >= y.num) { return gval_num(x.num); }
        return gval_num(y.num);
    }
    if (strcmp(op, "/") == 0) {
        return y.num == 0
            ? gval_err(GERR_DIV_ZERO)
            : gval_num(x.num / y.num);
    }
    return gval_err(GERR_BAD_OP);
}

gval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        /* Check if there is some error in conversion */
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? gval_num(x) : gval_err(GERR_BAD_NUM);
    }

    char* op = t->children[1]->contents;
    gval x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv) {
    /* Create Some Parsers */
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Geta     = mpc_new("geta");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number   : /-?[0-9]+/ ;                             \
            operator : '+' | '-' | '*' | '/' | '^'              \
                     | \"min\" | \"max\" ;                      \
            expr     : <number> | '(' <operator> <expr>+ ')' ;  \
            geta     : /^/ <operator> <expr>+ /$/ ;             \
            ",
            Number, Operator, Expr, Geta);

    puts("Press Ctrl+c to Exit\n");


    while (1) {
        /* Output our prompt and get input */
        char* input = readline("geta > ");
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Geta, &r)) {
            gval result = eval(r.output);
            gval_println(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print and delete the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    mpc_cleanup(4, Number, Operator, Expr, Geta);
    return 0;
}
