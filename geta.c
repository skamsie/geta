#include "mpc.h"
#include "math.h"

#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#ifdef __linux__
#include <editline/history.h>
#endif

/* Use operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    if (strcmp(op, "^") == 0) { return pow(x, y); }
    return 0;
}

long eval(mpc_ast_t* t) {
    /* If tagged as number return it directly. */
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    /* The operator is always second child. */
    char* op = t->children[1]->contents;

    /* We store the third child in `x` */
    long x = eval(t->children[2]);

    /* Iterate the remaining children and combining. */
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
        "                                                     \
          number   : /-?[0-9]+/ ;                             \
          operator : '+' | '-' | '*' | '/' | '^' | ;          \
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
        long result = eval(r.output);
        printf("%li\n", result);
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
