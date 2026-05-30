#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

// #define _BE_USE_GCC_LABEL_POINTERS 1

#include "bexpr.c"
#include "debug_utils.c"

char *read_stdin() {
    b_vec_t s;
    b_vec_alloc(&s, 1, 256);
    for(int c = fgetc(stdin); c != EOF; c = fgetc(stdin)) {
        char _c = (char)c;
        b_vec_push(&s, &_c);
    }
    char zero = '\0';
    b_vec_push(&s, &zero);
    return s.ptr;
}

char _be_print_buf[64];

char *be_double2str(double x) {
    for(int prec = 15; prec <= 17; prec ++) {
        sprintf(_be_print_buf, "%.*g", prec, x);
        if(atof(_be_print_buf) == x) return _be_print_buf;
    }
    return _be_print_buf;
}

void print(const char *label, dtype x) {
    if(isnan(x)) printf("%s\n", label);
    else printf("%s%s%s\n", label, *label ? ": " : "", be_double2str(x));
}

int main(int argc, char **argv) {
    int k = 1, i = 0;
    bool debug_mode = false;
    if(argc >= 2 && strcmp(argv[1], "c") == 0) { k = 2; debug_mode = true; }
    int n_vars = argc - k;
    dtype *vars = debug_mode ? NULL : malloc(sizeof(dtype) * n_vars);
    char **varnames = malloc(sizeof(char*) * n_vars);
    for(; k < argc; k ++, i ++) {
        char *arg = argv[k];
        char *c;
        for(c = arg; *c; c ++) if(*c == '=' && !debug_mode) break;
        if(!debug_mode && *c != '=') goto err;
        if(!debug_mode && c[1] == '\0') goto err;
        varnames[i] = malloc(c - arg + 1);
        memcpy(varnames[i], arg, c - arg);
        varnames[i][c - arg] = '\0';
        if(debug_mode) continue;
        be_token_t num = { .name = c + 1, .len = strlen(c + 1) };
        int retval = be_check_literal(&num, vars + i);
        if(retval == -1) { i ++; goto err; }
        if(retval == 1) printf("Warning: numerical constant out of range: %s\n", c + 1);
    }
    goto no_err;
        err:
        for(int j = 0; j < i; j ++) free(varnames[j]);
        if(!debug_mode) free(vars);
        free(varnames);
        printf("Usage: ./bexpr [<var1>=<value1>] [<var2>=<value2>] ... (run program from stdin)\n"
               "       ./bexpr c [<var1>] [<var2>] ... (print compilation result from stdin)\n");
        return 1;
    no_err:;
    char *program = read_stdin();
    be_err_t err;
    if(debug_mode) {
        b_vec_t tokens; b_vec_alloc(&tokens, sizeof(be_token_t), _BE_DEFAULT_VEC);
        err = be_tokenize(&tokens, program);
        if(err.err) goto tokens;
        log_tokens(&tokens);

        b_vec_t tlines; b_vec_alloc(&tlines, sizeof(be_tline_t), _BE_DEFAULT_VEC);
        err = be_tlines(&tlines, &tokens);
        if(err.err) goto tlines;
        log_tlines(&tlines);

        b_vec_t _varnames; b_vec_alloc(&_varnames, sizeof(char*), n_vars + 1);
        for(size_t i = 0; i < n_vars; i ++) b_vec_push(&_varnames, varnames + i);
        if(_varnames.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto varnames; }

        b_vec_t lines; b_vec_alloc(&lines, sizeof(be_line_t), _BE_DEFAULT_VEC);
        if(lines.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto varnames; }
        bool breaks[2];
        bool loops[2];
        err = be_parse_tokens(&lines, &tokens, &_varnames, 1, breaks, loops);
        if(err.err) goto lines;
        log_lines(&lines, 1);
        size_t _n_vars = be_n_vars(&lines);
        // if(_n_vars < n_vars) _n_vars = n_vars;
        printf("=== Number of vars: %zu ===\n", _n_vars);

        be_var_t *vars = malloc(sizeof(be_var_t) * _n_vars);
        if(vars == NULL) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto vars; }
        b_vec_t entering_stack;  b_vec_alloc(&entering_stack,  sizeof(size_t),    _BE_DEFAULT_VEC);
        b_vec_t exiting_stack;   b_vec_alloc(&exiting_stack,   sizeof(be_exit_t), _BE_DEFAULT_VEC);
        b_vec_t end_jumping_ops; b_vec_alloc(&end_jumping_ops, sizeof(size_t),    _BE_DEFAULT_VEC);
        b_vec_t ops;             b_vec_alloc(&ops,             sizeof(be_op_t),   _BE_DEFAULT_VEC);
        err = be_scope_bake(&ops, &lines, vars, &entering_stack, &exiting_stack, &end_jumping_ops, 0);
        b_vec_free(&entering_stack); b_vec_free(&exiting_stack); b_vec_free(&end_jumping_ops);
        if(err.err) goto ops;
        _be_skip_rawop_shift = true;
        // be_op_prep(&ops);
        log_rawops(&ops, &lines, vars);

        err = be_optimize_dead_ends(&ops);
        if(err.err) goto ops;
        printf("=== After dead-end elimination: ===\n");
        log_rawops(&ops, &lines, vars);

        ops: b_vec_free(&ops);
        vars: free(vars);
        for(size_t i = 0; i < lines.len; i ++) be_line_free(b_vec_get(&lines, i));
        lines:
        b_vec_free(&lines);
        varnames: b_vec_free(&_varnames);
        tlines: b_vec_free(&tlines);
        tokens: b_vec_free(&tokens);
    } else {
        be_code_t code;
        err = be_code_compile(&code, program, (const char**)varnames, n_vars);
        if(err.err) { goto ret; }
        code.print = print;
        printf("%s\n", be_double2str(be_code_eval(&code, vars, 0)));
        be_code_free(&code);
    }
    ret:
    free(program);
    for(int j = 0; j < n_vars; j ++) free(varnames[j]);
    if(!debug_mode) free(vars);
    free(varnames);
    if(err.err) fprintf(stderr, "%s\n", be_strerr(&err));
    return err.err;
}
