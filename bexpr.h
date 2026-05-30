#ifndef _BE_BEXPR_H_INCLUDED
#define _BE_BEXPR_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    size_t len;
    size_t step; // element size
    size_t cap; // capacity
    void *ptr;
    bool fail; // flag indicating that a malloc/calloc call returned NULL
} b_vec_t;

typedef struct {
    double a, b;
    // a variable, i.e. provided by user or defined with `:=` within code
} be_var_t;

// === user-facing structures and functions: ===

typedef struct {
    int err;
    size_t line_start;
    size_t line_end;
    size_t char_n;
    // structure for reporting compilaton errors
    // if `err`=0 there is no error and other fields are meaningless
    // `line_start`, `line_end` - range (inclusive) of text lines (counting from 1) that contain the cause of the error
    // if `line_start`=0, location is unknown and `line_end` and `char_n` are meaningless
    // `char_n` is the number of character (counting from 1) in a line (when `line_start`=`line_end`) that point to the cause of the error, if 0 then the location within line is unknown
} be_err_t;

// the only user-facing field is `print` which may be set AFTER calling `be_code_compile` (by default set to `NULL` in `be_code_compile`), leaving it NULL is fine, output just won't happen
typedef struct {
    b_vec_t ops; // vector of `be_op_t` raw operations
    b_vec_t lines; // lines of code
    be_var_t *vars; // array of variables, user-provided variables go to beginning of this array, rest is for holding values of variables defined in code of the program
    void (*print)(const char*, double); // function that gets called on `@` operations, if set to NULL it is not called
    size_t n_vars; // number of user-defined variables
    // #if _BE_USE_GCC_LABEL_POINTERS
    bool _be_gnulabel_filler_call;
    // #endif

    // structure for a compiled program
} be_code_t;

// compile a program
// `*code` is dst for copiled program
// if an error occures, all fields of `*code` are deallocated automatically, but on success `be_code_free` should be called at some pointe after to deallocate allocated fields of `*code`
// `program` is the null-terminated string containing program code encoded in utf-8 or pure ascii
// `varnames` is an array of null-terminated strings indicating names of user-defined variables that will have to be passed into `be_code_eval` on each evaluation
// `n_vars` is the number of user-defined variables. If `n_vars`=0, `varnames` is not read and can be set to NULL or anything
be_err_t be_code_compile(be_code_t *code, const char *program, const char **varnames, size_t n_vars);

// free all allocated fields of `*code`
void be_code_free(be_code_t *code);

// evaluate (run) a compiled program
// `*code` is the compiled program
// `vars` is an array containing values for user-defined variables, same number as number of variables specified at compilation (`n_vars`)
// `max_operations` is cap for how much raw operations may be performed (not to hang in case of infinite loop), 0 means no limit; when the limit is reached NAN is returned
double be_code_eval(const be_code_t *code, const double *vars, unsigned long long max_operations);

// convert `be_err_t` to a humal-readible string (null-terminated)
// up to _BE_STRERR_MESSAGES messages are guarateed to safely coexist at a time (possibly more), then overwriting the oldest ones
// not thread-safe because it uses global variables to avoid heap-allocation
char *be_strerr(be_err_t *err);

// same thing as `be_strerr` but writing to a provided destination `dst` rather than preallocated memory. thread-safe unlike `be_strerr`.
// `dst` should point to preallocated memory of at least 197 bytes (196 chartacters is the most possible with 64-bit `size_t` + 1 for null terminator) though 256 (=`_BE_STRERR_LEN`) is recommended
void be_strerr_to(char *dst, be_err_t *err);

// error codes, 0 is for success
#define BE_ERR_ILL_CHAR             1
#define BE_ERR_SCOPE_EXTRA_CLOSE    2
#define BE_ERR_SCOPE_UNCLOSED       3
#define BE_ERR_ILL_LAST_TOKEN       4
#define BE_ERR_ILL_TOKEN            5
#define BE_ERR_CONSTANT_RANGE       6
#define BE_ERR_BRACKETS_MISMATCH    7
#define BE_ERR_BRACKET_UNCLOSED     8
#define BE_ERR_BRACKET_OVERCLOSED   9
#define BE_ERR_EMPTY_EXPR          10
#define BE_ERR_SCOPE_START         11
#define BE_ERR_EMPTY_IF            12
#define BE_ERR_EMPTY_WHILE         13
#define BE_ERR_EMPTY_ELIF          14
#define BE_ERR_MISFOLLOWUP         15
#define BE_ERR_NONEMPTY_ELSE       16
#define BE_ERR_BREAK_DEPTH         17
#define BE_ERR_LOOP_DEPTH          18
#define BE_ERR_NO_EXPR             19
#define BE_ERR_EMPTY_ASSIGN        20
// #define BE_ERR_TRAP                21
#define BE_ERR_EMPTY_OUT           22
#define BE_ERR_ALLOC_FAIL          23
#define BE_ERR_STACK_DEPTH         24

#endif