// todo: stack limit

#ifndef _BE_BEXPR_INCLUDED
#define _BE_BEXPR_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>

// _BE_USE_GCC_LABEL_POINTERS tells whether to use gcc's label pointer extension (also supported in clang) for jumping to operations during evaluation, otherwise switch-case is used
// increases performance by ~1.2-1.3 times
#define _BE_USE_GCC_LABEL_POINTERS 0
#if defined(__GNUC__) && !defined(_BE_USE_GCC_LABEL_POINTERS)
#define _BE_USE_GCC_LABEL_POINTERS 1
#elif !defined(_BE_USE_GCC_LABEL_POINTERS)
#define _BE_USE_GCC_LABEL_POINTERS 0
#endif

#ifndef _BE_DEFAULT_VEC
#define _BE_DEFAULT_VEC 24 // default init size when allocating a vector
#endif

#define _B_VEC_GROWTH 2 // reallocation coefficient upon runnig out of capacity

#ifndef _BE_OPTIMIZE_POW
#define _BE_OPTIMIZE_POW 1 // wether to optimize integer & special powers into other operations than stdlib's `pow`
#endif

#ifndef _BE_INTPOW_LIMIT
#define _BE_INTPOW_LIMIT 8 // how many multiplications can be generated when optimizing integer power (if `_BE_OPTIMIZE_POW`=1), up to around 24-32 gives performance benefit, but lots of multiplications become inaccurate
#endif

#ifndef _BE_UNLIMITED_CASE
#define _BE_UNLIMITED_CASE 1 // whether to create separate clause for evaluation in case `max_operations`=0 avoiding operation count check, gives <+5% performance benefit but bloats the executable by 6.6 KB
#endif

#ifndef _BE_STACK_LIMIT
#define _BE_STACK_LIMIT 256 // how many nested scopes are allowed, for preventing stack overflow, -1 for no limit
#endif
// expression parser is not recursive (not even with parentheses) so it can't cause stack overflow, but scope parser is recursive
// from my measurements with gcc -O3, each scope eats 864 bytes of stack

#ifdef _MSC_VER
#define _BE_MULTICHAR   \
__pragma(warning(push)) \
__pragma(warning(disable : 4066))
#else
#define _BE_MULTICHAR          \
_Pragma("GCC diagnostic push") \
_Pragma("GCC diagnostic ignored \"-Wmultichar\"")
#endif

#ifdef _MSC_VER
#define _BE_DIAGNOSTIC_POP \
__pragma(warning(pop))
#else
#define _BE_DIAGNOSTIC_POP \
_Pragma("GCC diagnostic pop")
#endif

typedef struct {
    size_t len;
    size_t step; // element size
    size_t cap; // capacity
    void *ptr;
    bool fail; // flag indicating that a malloc/calloc call returned NULL
} b_vec_t;

void b_vec_alloc(b_vec_t *vec, size_t step, size_t cap) {
    vec->len = 0;
    vec->step = step;
    vec->cap = cap;
    vec->ptr = malloc(cap * step);
    vec->fail = (vec->ptr == NULL);
}

void b_vec_reserve(b_vec_t *vec, size_t new_cap) {
    if(vec->cap >= new_cap) return;
    void *ptr = malloc(new_cap * vec->step);
    if(ptr == NULL) { vec->fail = true; return; }
    memcpy(ptr, vec->ptr, vec->len * vec->step);
    free(vec->ptr);
    vec->ptr = ptr;
    vec->cap = new_cap;
}

// an element is pushed once even in case of a fail to reallocate occures (but not afterwards)
void b_vec_push(b_vec_t *vec, void *elt) {
    if(vec->fail) return;
    if(vec->len >= vec->cap - 1) b_vec_reserve(vec, vec->cap * _B_VEC_GROWTH);
    memcpy((char*)vec->ptr + vec->len * vec->step, elt, vec->step);
    vec->len ++;
}

void b_vec_set(b_vec_t *vec, size_t k, void *elt) {
    memcpy((char*)vec->ptr + k * vec->step, elt, vec->step);
}

void *b_vec_get(b_vec_t *vec, size_t k) {
    return (char*)vec->ptr + k * vec->step;
}

void b_vec_free(b_vec_t *vec) {
    free(vec->ptr);
}

void b_vec_cut(b_vec_t *vec, size_t k, size_t amount) {
    vec->len -= amount;
    size_t s = vec->len * vec->step;
    k *= vec->step;
    amount *= vec->step;
    // not using memcpy for we might have overlapping dst and src
    for(size_t i = k; i < s; i ++) ((char*)vec->ptr)[i] = ((char*)vec->ptr)[i + amount];
}

void b_vec_concat(b_vec_t *dst, b_vec_t *src) {
    if(dst->cap <= dst->len + src->len) {
        b_vec_reserve(dst, dst->len + src->len + 1);
        if(dst->fail) return;
    }
    memcpy((char*)dst->ptr + dst->len * dst->step, src->ptr, src->len * src->step);
    dst->len += src->len;
}

typedef double dtype;

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

#define _BE_STRERR_LEN      256 // should be sufficient capacity
#define _BE_STRERR_MESSAGES   8 // how many messages can live at a time (at least)
char _be_strerr[_BE_STRERR_LEN * _BE_STRERR_MESSAGES];
int _be_strerr_k = 0;

// same thing as `be_strerr` but writing to a provided destination `dst` rather than preallocated memory. thread-safe unlike `be_strerr`.
// `dst` should point to preallocated memory of at least 193 bytes (192 chartacters is the most possible with 64-bit `size_t` + 1 for null terminator)
// though 256 (=`_BE_STRERR_LEN`) is recommended because I might forget to update this comment
void be_strerr_to(char *dst, be_err_t *err) {
    const char *name;
    dst[0] = '\0';
    switch(err->err) {
        case 0:                          name = "No error";                                                                       break;
        case BE_ERR_ILL_CHAR:            name = "Unsupported character";                                                          break;
        case BE_ERR_SCOPE_EXTRA_CLOSE:   name = "Extra closing `}` with no respective `{`";                                       break;
        case BE_ERR_SCOPE_UNCLOSED:      name = "Unclosed `{`";                                                                   break;
        case BE_ERR_ILL_LAST_TOKEN:      name = "Last non-bracket token must be a variable or a constant";                        break;
        case BE_ERR_ILL_TOKEN:           name = "Unknown token";                                                                  break;
        case BE_ERR_CONSTANT_RANGE:      name = "Numerical constant out of range";                                                break;
        case BE_ERR_BRACKETS_MISMATCH:   name = "Brackets mismatch";                                                              break;
        case BE_ERR_BRACKET_UNCLOSED:    name = "Bracket not closed";                                                             break;
        case BE_ERR_BRACKET_OVERCLOSED:  name = "More closing brackets than opening ones";                                        break;
        case BE_ERR_EMPTY_EXPR:          name = "Empty expression";                                                               break;
        case BE_ERR_SCOPE_START:         name = "A line which is a scope should start with `{`, `if`, `elif`, `else` or `while`"; break;
        case BE_ERR_EMPTY_IF:            name = "An expression expected between `if` and `{`";                                    break;
        case BE_ERR_EMPTY_WHILE:         name = "An expression expected between `while` and `{`";                                 break;
        case BE_ERR_EMPTY_ELIF:          name = "An expression expected between `elif` and `{`";                                  break;
        case BE_ERR_MISFOLLOWUP:         name = "An `elif` or `else` line can only be placed directly after an `if` or `elif`";   break;
        case BE_ERR_NONEMPTY_ELSE:       name = "`else` should be followed by `{`";                                               break;
        case BE_ERR_BREAK_DEPTH:         name = "`break` depth is more than allowed";                                             break;
        case BE_ERR_LOOP_DEPTH:          name = "`loop` depth is more than allowed";                                              break;
        case BE_ERR_NO_EXPR:             name = "Compile-time guarantee that a value is returned failed. Try adding a number to the and as a default return value"; break;
        case BE_ERR_EMPTY_ASSIGN:        name = "Empty expression after `:=`";                                                    break;
        // case BE_ERR_TRAP:                name = "Unconditional infinite loop";                                                    break;
        case BE_ERR_EMPTY_OUT:           name = "Empty expression after `:` in a line starting with `@`";                         break;
        case BE_ERR_ALLOC_FAIL:          name = "Out of memory during compilation; malloc or calloc returned NULL";               break;
        case BE_ERR_STACK_DEPTH:         name = "Nested scope depth exceeded";                                                    break;
        default:                         name = "Unknown error";                                                                  goto end;
    }
    if(err->err && err->line_start) {
        if(err->line_start == err->line_end) sprintf(dst, "line %zu", err->line_start);
        else sprintf(dst, "lines %zu-%zu", err->line_start, err->line_end);
        if(err->char_n) sprintf(dst + strlen(dst), " character %zu: ", err->char_n);
        else sprintf(dst + strlen(dst), ": ");
    }
    end:
    sprintf(dst + strlen(dst), "%s", name);
    _be_strerr_k += strlen(dst) + 1;
}

// convert `be_err_t` to a humal-readible string (null-terminated)
// up to _BE_STRERR_MESSAGES messages are guarateed to safely coexist at a time (possibly more), then overwriting the oldest ones
// not thread-safe because it uses global variables to avoid heap-allocation
char *be_strerr(be_err_t *err) {
    if(sizeof(_be_strerr) - _be_strerr_k < _BE_STRERR_LEN) _be_strerr_k = 0;
    char *s = _be_strerr + _be_strerr_k;
    be_strerr_to(s, err);
    return s;
}

// #define BE_ERR_ALLOC_FAIL (*(volatile int*)NULL = 1)

typedef struct {
    const char *name;
    size_t len;
    size_t text_line;
    size_t text_char;
    int depth; // depth in brackets (only used during expression parsing)
    bool as_function; // for tokens like `sin` in `sin 2` or `sin(2)` indicates whether we have an `(` directly after the token (only used during expression parsing)
    // structure for tokens of code, `len` is character count, `name` is pointer the the start of the token
    // used as a view over existing string rather than separate allocated memory pieces
    // `text_line` and `text_char` are location in the original program string (counting from 1)
} be_token_t;

#define _BE_TOKEN2ERR(e, token, code) (e).err = code; (e).line_start = (token)->text_line; (e).line_end = (token)->text_line; (e).char_n = (token)->text_char;

#define _BE_TOKENCOMP(token, s) (((be_token_t*)(token))->len == sizeof(s) - 1 && strncmp(s, ((be_token_t*)(token))->name, sizeof(s) - 1) == 0)

// split a null-terminated string into tokens
// `*tokens` is dst, should be a preallocated vector of be_token_t
be_err_t be_tokenize(b_vec_t *tokens, const char *str) {
    be_token_t token;
    token.name = NULL;
    bool token_alphadotnumer_ = false;
    int non_number_chars;
    #define _BE_TOKEN_PUSH if(token.name != NULL) { b_vec_push(tokens, &token); token.name = NULL; token_alphadotnumer_ = false; }
    #define _BE_TOKEN_ASSIGN { token.text_line = text_line; token.text_char = text_char; token.name = str + i; token.len = char_len; non_number_chars = 0; }
    #define _BE_ILL_CHAR return (be_err_t){ .err = BE_ERR_ILL_CHAR, .line_start = text_line, .line_end = text_line, .char_n = text_char };
    size_t text_line = 1;
    size_t text_char = 1;
    bool comment = false;
    for(size_t i = 0; str[i];) {
        char c = str[i];
        int char_len;
        if((c & 0x80) == 0) char_len = 1;
        else if((c & 0xE0) == 0xC0) char_len = 2;
        else if((c & 0xF0) == 0xE0) char_len = 3;
        else if((c & 0xF8) == 0xF0) char_len = 4;
        else _BE_ILL_CHAR
        for(size_t j = i + 1; j < i + char_len; j ++) if((str[j] & 0b11000000) != 0x80) _BE_ILL_CHAR
        if(c == '#') { comment = true; _BE_TOKEN_PUSH }
        if(comment) goto past_token_check;
        bool space = (9 <= c && c <= 13) || c == ' ';
        bool dotnumer = ('0' <= c && c <= '9') || c == '.';
        bool alphadotnumer_ = !space && ((c & 0x80) || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || dotnumer || c == '_');
        if(space) { _BE_TOKEN_PUSH }
        else if(alphadotnumer_) {
            if(token_alphadotnumer_) token.len += char_len;
            else { _BE_TOKEN_PUSH _BE_TOKEN_ASSIGN token_alphadotnumer_ = true; }
        } else {
            bool comparison_operator = false;
            if(token.name != NULL) {
                char _c = token.name[token.len - 1];
                comparison_operator = _c == ':' || _c == '!' || _c == '=' || _c == '<' || _c == '>' || _c == '+' || _c == '-' || _c == '*' || _c == '/' || _c == '%' || _c == '&' || _c == '|' || _c == '^';
            }
            if(comparison_operator && c == '=') token.len ++;
            else if(token.name != NULL && token.len >= 2 && non_number_chars == 1 && str[i - 1] == 'e' && (c == '-' || c == '+')) token.len ++; // keeping numbers in exponential notation integral (not to break like 1e|+|10)
            else { _BE_TOKEN_PUSH _BE_TOKEN_ASSIGN }
        }
        past_token_check:
        if(str[i] == '\n') { text_line ++; text_char = 1; comment = false; }
        else if(str[i] == '\v' || str[i] == '\f') text_line ++;
        else if(str[i] == '\r') { text_char = 1; comment = false; }
        else text_char ++;
        i += char_len;
        non_number_chars += !dotnumer;
    }
    _BE_TOKEN_PUSH
    if(tokens->fail) return (be_err_t){ .err = BE_ERR_ALLOC_FAIL, .line_start = 0 };
    be_err_t err;
    err.err = 0;
    return err;
}

typedef struct {
    size_t len;
    be_token_t *ptr;
    // a sequence of tokens, used to hold lines of code and expressions
    // used as a view over existing vector
} be_tline_t;

// split a vector of tokens into lines of code, delimited by ';' and closing scopes ('}' at top level)
// each scope is treated as a single line and processed recursively
// `*tlines` is dst, should be a preallocated vector of `be_tline_t`, tlines->fail=true is allowed
be_err_t be_tlines(b_vec_t *tlines, b_vec_t *tokens) {
    be_tline_t line;
    line.ptr = NULL;
    int scope_depth = 0;
    for(int i = 0; i < tokens->len; i ++) {
        be_token_t *token = b_vec_get(tokens, i);
        if((*token->name == ';' && scope_depth == 0) || (*token->name == '}' && (-- scope_depth == 0))) {
            if(*token->name != ';') line.len ++;
            if(line.ptr != NULL) b_vec_push(tlines, &line);
            line.ptr = NULL;
        } else if(line.ptr == NULL) {
            line.ptr = token;
            line.len = 1;
        } else line.len ++;
        if(*token->name == '{') scope_depth ++;
        if(scope_depth < 0) {
            be_err_t err;
            _BE_TOKEN2ERR(err, token, BE_ERR_SCOPE_EXTRA_CLOSE)
            return err;
        }
    }
    if(scope_depth != 0) return (be_err_t){ .err = BE_ERR_SCOPE_UNCLOSED, .line_start = 0 };
    if(line.ptr != NULL) b_vec_push(tlines, &line);
    if(tlines->fail) return (be_err_t){ .err = BE_ERR_ALLOC_FAIL, .line_start = 0 };
    return (be_err_t){ .err = 0 };
}

#define BE_DST          0x0010
#define BE_SRC1         0x0020
#define BE_SRC2         0x0040
#define BE_SRC3         0x0080

// types of operations (joining 2 subsequent values), last 4 bits are priority (higher executed first)
// also used as type of raw operation, 4 bits at location 0x00F0 denote which dst and src fields of `be_op_t` are used (as indicated just above)
#define BE_OP           0xFFFF
#define BE_PRIO         0x000F
#define BE_OP_PAIR      0x0030
#define BE_OP_SWAP      0x01F0
#define BE_OP_OR        0x0271
#define BE_OP_AND       0x0372
#define BE_OP_EQUALS    0x0473
#define BE_OP_NEQUALS   0x0573
#define BE_OP_GREATER   0x0674
#define BE_OP_LESS      0x0774
#define BE_OP_GEQ       0x0874
#define BE_OP_LEQ       0x0974
#define BE_OP_ADD       0x0A75
#define BE_OP_SUB       0x0B75
#define BE_OP_MOD       0x0C76
#define BE_OP_MUL       0x0D77
#define BE_OP_DIV       0x0E77
#define BE_OP_POW       0x0F79
#define BE_UN_PRIO           8
#define BE_FN_PRIO          10
#define BE_ADD_PRIO (BE_PRIO & BE_OP_ADD)
#define BE_MUL_PRIO (BE_PRIO & BE_OP_MUL)
#define BE_N_PRIOS  (BE_FN_PRIO + 1)

// only used as raw operations
#define BE_OP_OUT       0x1020
#define BE_OP_RETURN    0x1120
#define BE_OP_CONDJUMP  0x1220
#define BE_FN_COPY      0x1330
#define BE_OP_INTPOW    0x1470

// function types (unary operations), except for atan2, min and max which use both `a` and `b` fields still of single value
// also used as type of raw operation, bits 0x00F0 indicate used srd and dsp fields as for operations
#define BE_FN_NOT       0x153F
#define BE_FN_SIN       0x163F
#define BE_FN_COS       0x173F
#define BE_FN_TAN       0x183F
#define BE_FN_ASIN      0x193F
#define BE_FN_ACOS      0x1A3F
#define BE_FN_ATAN      0x1B3F
#define BE_FN_ATAN2     0x1C7F
#define BE_FN_FLOOR     0x1D3F
#define BE_FN_CEIL      0x1E3F
#define BE_FN_ROUND     0x1F3F
#define BE_FN_LN        0x203F
#define BE_FN_EXP       0x213F
#define BE_FN_SQRT      0x223F
#define BE_FN_CBRT      0x233E
#define BE_FN_GAMMA     0x243F
#define BE_FN_SINH      0x253E
#define BE_FN_COSH      0x263E
#define BE_FN_TANH      0x273E
#define BE_FN_ASINH     0x283E
#define BE_FN_ACOSH     0x293E
#define BE_FN_ATANH     0x2A3E
#define BE_FN_ERF       0x2B3E
#define BE_FN_ERFINV    0x2C3E
#define BE_FN_SIGMOID   0x2D3E
#define BE_FN_MAX       0x2E7E
#define BE_FN_MIN       0x2F7E
#define BE_FN_NEG       0x303D
#define BE_FN_RECIPR    0x313D
#define BE_FN_SIGN      0x323D
#define BE_FN_ABS       0x333D

#define BE_RAWOP_SHIFT       8

// value types (value is a constant of a variable within an expression)
#define BE_VT          0xC0000
#define BE_VT_CONST    0x40000
#define BE_VT_VAR      0x80000

// buts or-ed with value type to indicate whether this value is a contant or derived from constants (in process of baking expression) and can be caculated in compile time
#define BE_A_CONST     0x10000
#define BE_B_CONST     0x20000

typedef struct {
    int n;
    bool choices[_BE_INTPOW_LIMIT]; // true to square, false to multiply by original value
    bool neg;
} be_intpow_t;

dtype be_intpow(be_intpow_t *p, dtype x) {
    dtype s = x;
    for(int i = 0; i < p->n; i ++) s *= p->choices[i] ? s : x;
    return p->neg ? 1/s : s;
}

// _p should be not 0
void be_intpow_bake(be_intpow_t *p, int _p) {
    if((p->neg = (_p < 0))) _p = -_p;
    int i = 0;
    while(_p >> i) i ++;
    int k = 0;
    for(i -= 2; i >= 0; i --) {
        if(k >= _BE_INTPOW_LIMIT) { p->n = -1; return; }
        p->choices[k] = true; k ++;
        if((_p >> i) & 1) {
            if(k >= _BE_INTPOW_LIMIT) { p->n = -1; return; }
            p->choices[k] = false; k ++;
        }
    }
    p->n = k;
}

typedef struct {
    dtype a, b; // destinations
    dtype temp; // temporary destination
    dtype a_const, b_const; // const value in case of const (literal)
    dtype *a_ptr, *b_ptr; // current sources (possibly &a, &b, or links to variables, or other places)
    b_vec_t functions; // vector of `be_fn_t`, subsequent functions applied to this value
    int curr_function; // the indes of next function to apply to this value in process of baking, i.e. sin(-a + b) first applies `BE_FN_NEG` to `a`, storing `curr_function=1`, then writes `a+b -> a` and then applies `BE_FN_SIN` to `a`
    int flags; // `B_VT_*`|`BE_OP_*` (meaning bitwise or) possibly or-ed with `BE_A_CONST` and/or `BE_B_CONST`
    int depth; // least bracket depth the appears between this value and one to the left of it, i.e. in `(([b] [[a]]))` `a` will have `depth` of 2 (only round brackets are counted)
    size_t var; // index of variable when `flags&BE_VT = BE_VT_VAR`
    be_intpow_t ip; // for values that are integer constants used as powers
    // value in an expression
    // when evaluating an expression, the operations between values are performed according to priority of right-hand value of operation calculated as `depth*BE_N_PRIOS + (flags&BE_PRIO)`
    // but right to left when priorities are matching. Left-hand value is used as temporary dst for operation result
    // i.e. `a*b*c + d` is evaluated as `b*c -> b` then `a*b -> a` then `a+d -> a` with `b` and `c` determining multiplicative type and priority of `*` operations
    // `a`, `b` are temporary dst for result of operation, `a_const`, `b_const` hold constant value for values that are constants (i.e. numeric literals), `a_ptr`, `b_ptr` indicate curent value source,
    // i.e. one of `&a`, `&b`, `&a_const`, `&b_const` or values of a variable, used in process of baking
} be_value_t;

typedef struct {
    int fn; // `BE_FN_*`
    int prio; // global priority relative to binary operations; a function is applied before a binary operation if prio >= value.depth * BE_N_PRIOS + (value.flags & BE_PRIO)
} be_fn_t;

typedef struct {
    dtype a, b;
    // a variable, i.e. provided by user or defined with `:=` within code
} be_var_t;

typedef struct be_op_s {
    int op; // operation type, `BE_OP_*` or `BE_FN_*`
    dtype *dst, *src1, *src2, *src3; // destination and sources (some types of operations only use part of those as indicated by `BE_DST` and `BE_SRC*` bits of `op`)
    struct be_op_s *next; // next operaion to execute (unused for `BE_OP_RETURN`), before calling `be_op_prep` contains indices (explicitly casted to pointer type) instead of actual pointers
    #if _BE_USE_GCC_LABEL_POINTERS
    void *gnulabel;
    #if _BE_UNLIMITED_CASE
    void *_gnulabel; // for `max_operations`=0 case
    #endif
    #endif
    // raw operation, what is executed by interpreter at run time
    // for `BE_OP_CONDJUMP` `*next` is where to jump if condition is satisfied, otherwise it jumps to `be_op_t*` pointer written to `src2`
} be_op_t;

typedef struct {
    b_vec_t values; // array of be_value_t
    b_vec_t order; // order of applying operations (array of pairs of size_t, indexing values in expression)
    // expression
} be_expr_t;

// 0 if valid number, 1 if overflow, -1 if invalid, -2 on malloc returning NULL
int be_check_literal(be_token_t *token, dtype *dst) {
    if(token->len == 2 && (token->name[0] == 'p' || token->name[0] == 'P') && (token->name[1] == 'i' || token->name[1] == 'I')) {
        if(dst) *dst = 3.141592653589793;
        return 0;
    }
    if(token->len == 1 && (token->name[0] == 'e' || token->name[0] == 'E')) {
        if(dst) *dst = 2.718281828459045;
        return 0;
    }
    char *name = malloc(token->len + 1);
    if(name == NULL) return -2;
    memcpy(name, token->name, token->len);
    name[token->len] = '\0';
    char *end;
    errno = 0;
    if(dst) *dst = strtod(name, &end);
    else strtod(name, &end);
    int ret = (end == name + token->len ? (errno == ERANGE ? 1 : 0) : -1); // solely to avoid gcc's use-after-free warning
    free(name);
    return ret;
}

// free all alocated fields of a `be_expr_t`
void be_expr_free(be_expr_t *expr) {
    b_vec_t *values = &expr->values;
    for(size_t i = 0; i < values->len; i ++) {
        be_value_t *value = b_vec_get(values, i);
        b_vec_free(&value->functions);
    }
    b_vec_free(values);
    b_vec_free(&expr->order);
}

int be_token2int(be_token_t *token) {
    int _token;
    _BE_MULTICHAR
    if(('AB' & 0xF) == 'A') { // endianness of multy-char constants check
        _token = token->name[0];
        if(token->len >= 2) _token |= (int)token->name[1] << 8;
        if(token->len >= 3) _token |= (int)token->name[2] << 16;
        if(token->len >= 4) _token |= (int)token->name[3] << 24;
    } else {
        _token = token->name[token->len - 1];
        if(token->len >= 2) _token |= (int)token->name[token->len - 2] << 8;
        if(token->len >= 3) _token |= (int)token->name[token->len - 3] << 16;
        if(token->len >= 4) _token |= (int)token->name[token->len - 4] << 24;
    }
    _BE_DIAGNOSTIC_POP
    return _token;
}

// parse an expression held in `*line` and write it to `*expr`, allocating it's fields
// if an error accures all fields are deallocated before returning, otherwise `be_expr_free` shall be called at some point later
// `*varnames` is a vector of `char*` (not modified in this function) containing names of all variables existing to this point in code
be_err_t be_expr_parse(be_expr_t *expr, be_tline_t *line, b_vec_t *varnames) {
    be_err_t err;
    err.err = 0;
    b_vec_t brackets;  b_vec_alloc(&brackets , sizeof(be_token_t), _BE_DEFAULT_VEC);
    b_vec_t modifiers; b_vec_alloc(&modifiers, sizeof(be_token_t), _BE_DEFAULT_VEC);
    b_vec_t *order  = &expr->order;  b_vec_alloc(order , sizeof(struct { size_t a, b; }), _BE_DEFAULT_VEC);
    b_vec_t *values = &expr->values; b_vec_alloc(values, sizeof(be_value_t), _BE_DEFAULT_VEC);
    if(values->fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
    int depth = 0;
    int max_depth = 0;
    bool brackets_up = false;
    bool outer_modifier;
    for(size_t i = 0; i < line->len; i ++) {
        be_value_t value;
        be_token_t *token = line->ptr + i;
        for(size_t j = 0; j < varnames->len; j ++) {
            char *varname = *(char**)b_vec_get(varnames, j);
            if(strlen(varname) == token->len && strncmp(varname, token->name, token->len) == 0) {
                value.flags = BE_VT_VAR | BE_OP_MUL;
                value.var = j;
                b_vec_alloc(&value.functions, sizeof(be_fn_t), _BE_DEFAULT_VEC);
                goto got_value;
            }
        }
        dtype _const;
        int lit = be_check_literal(token, &_const);
        if(lit == -2) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
        if(lit == 1) { _BE_TOKEN2ERR(err, token, BE_ERR_CONSTANT_RANGE) goto ret; }
        if(lit == 0) {
            value.a_const = _const;
            value.b_const = 0;
            value.flags = BE_VT_CONST | BE_OP_MUL;
            b_vec_alloc(&value.functions, sizeof(be_fn_t), _BE_DEFAULT_VEC);
            goto got_value;
        }
        if(token->len == 1 && (*token->name == ')' || *token->name == ']')) {
            if(brackets_up) { _BE_TOKEN2ERR(err, token, BE_ERR_EMPTY_EXPR) goto ret; }
            if(brackets.len == 0) { _BE_TOKEN2ERR(err, token, BE_ERR_BRACKET_OVERCLOSED) goto ret; }
            if(**(char**)b_vec_get(&brackets, brackets.len - 1) != (*token->name == ')' ? '(' : '[')) { _BE_TOKEN2ERR(err, token, BE_ERR_BRACKETS_MISMATCH) goto ret; }
            brackets.len --; depth --; continue;
        }
        brackets_up = true;
        if(token->len == 1 && (*token->name == '[' || *token->name == '(')) {
            if(modifiers.len) ((be_token_t*)b_vec_get(&modifiers, modifiers.len - 1))->as_function = true;
            b_vec_push(&brackets, token);
            if(brackets.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
            continue;
        }
        if(modifiers.len == 0) outer_modifier = depth == brackets.len;
        token->depth = brackets.len;
        token->as_function = false;
        b_vec_push(&modifiers, token);
        if(modifiers.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
        continue;
        got_value:
        value.depth = depth;
        depth = brackets.len;
        if(depth > max_depth) max_depth = depth;
        brackets_up = false;
        bool operation_found = false;
        _BE_MULTICHAR
        if(outer_modifier && values->len > 0 && modifiers.len > 0 && ((be_token_t*)modifiers.ptr)->len <= 2) {
            operation_found = true;
            switch(be_token2int(modifiers.ptr)) {
                case '*' : value.flags = (value.flags & ~BE_OP) | BE_OP_MUL    ; break;
                case '/' : value.flags = (value.flags & ~BE_OP) | BE_OP_DIV    ; break;
                case '+' : value.flags = (value.flags & ~BE_OP) | BE_OP_ADD    ; break;
                case '-' : value.flags = (value.flags & ~BE_OP) | BE_OP_SUB    ; break;
                case '%' : value.flags = (value.flags & ~BE_OP) | BE_OP_MOD    ; break;
                case '^' : value.flags = (value.flags & ~BE_OP) | BE_OP_POW    ; break;
                case ':' : value.flags = (value.flags & ~BE_OP) | BE_OP_PAIR   ; break;
                case ',' : value.flags = (value.flags & ~BE_OP) | BE_OP_PAIR   ; break;
                case '?' : value.flags = (value.flags & ~BE_OP) | BE_OP_SWAP   ; break;
                case '|' : value.flags = (value.flags & ~BE_OP) | BE_OP_OR     ; break;
                case '&' : value.flags = (value.flags & ~BE_OP) | BE_OP_AND    ; break;
                case '=' : value.flags = (value.flags & ~BE_OP) | BE_OP_EQUALS ; break;
                case '!=': value.flags = (value.flags & ~BE_OP) | BE_OP_NEQUALS; break;
                case '>=': value.flags = (value.flags & ~BE_OP) | BE_OP_GEQ    ; break;
                case '<=': value.flags = (value.flags & ~BE_OP) | BE_OP_LEQ    ; break;
                case '<' : value.flags = (value.flags & ~BE_OP) | BE_OP_LESS   ; break;
                case '>' : value.flags = (value.flags & ~BE_OP) | BE_OP_GREATER; break;
                default: operation_found = false;
            }
        }
        for(size_t k = 0; k < modifiers.len - operation_found; k ++) {
            be_token_t *mod = b_vec_get(&modifiers, modifiers.len - 1 - k);
            int prio0 = mod->depth * BE_N_PRIOS + BE_UN_PRIO;
            int prio = mod->depth * BE_N_PRIOS + (mod->as_function ? BE_FN_PRIO : BE_UN_PRIO);
            if(mod->len <= 4) switch(be_token2int(mod)) {
                case '*': case '+': break;
                case '-'   : { be_fn_t f = { .prio = prio0, .fn = BE_FN_NEG    }; b_vec_push(&value.functions, &f); } break;
                case '/'   : { be_fn_t f = { .prio = prio0, .fn = BE_FN_RECIPR }; b_vec_push(&value.functions, &f); } break;
                case '!'   : { be_fn_t f = { .prio = prio0, .fn = BE_FN_NOT    }; b_vec_push(&value.functions, &f); } break;
                case 'sin' : { be_fn_t f = { .prio = prio , .fn = BE_FN_SIN    }; b_vec_push(&value.functions, &f); } break;
                case 'cos' : { be_fn_t f = { .prio = prio , .fn = BE_FN_COS    }; b_vec_push(&value.functions, &f); } break;
                case 'tan' : { be_fn_t f = { .prio = prio , .fn = BE_FN_TAN    }; b_vec_push(&value.functions, &f); } break;
                case 'asin': { be_fn_t f = { .prio = prio , .fn = BE_FN_ASIN   }; b_vec_push(&value.functions, &f); } break;
                case 'acos': { be_fn_t f = { .prio = prio , .fn = BE_FN_ACOS   }; b_vec_push(&value.functions, &f); } break;
                case 'atan': { be_fn_t f = { .prio = prio , .fn = BE_FN_ATAN   }; b_vec_push(&value.functions, &f); } break;
                case 'ceil': { be_fn_t f = { .prio = prio , .fn = BE_FN_CEIL   }; b_vec_push(&value.functions, &f); } break;
                case 'ln'  : { be_fn_t f = { .prio = prio , .fn = BE_FN_LN     }; b_vec_push(&value.functions, &f); } break;
                case 'log' : { be_fn_t f = { .prio = prio , .fn = BE_FN_LN     }; b_vec_push(&value.functions, &f); } break;
                case 'exp' : { be_fn_t f = { .prio = prio , .fn = BE_FN_EXP    }; b_vec_push(&value.functions, &f); } break;
                case 'sqrt': { be_fn_t f = { .prio = prio , .fn = BE_FN_SQRT   }; b_vec_push(&value.functions, &f); } break;
                case 'cbrt': { be_fn_t f = { .prio = prio , .fn = BE_FN_CBRT   }; b_vec_push(&value.functions, &f); } break;
                case 'sinh': { be_fn_t f = { .prio = prio , .fn = BE_FN_SINH   }; b_vec_push(&value.functions, &f); } break;
                case 'cosh': { be_fn_t f = { .prio = prio , .fn = BE_FN_COSH   }; b_vec_push(&value.functions, &f); } break;
                case 'tanh': { be_fn_t f = { .prio = prio , .fn = BE_FN_TANH   }; b_vec_push(&value.functions, &f); } break;
                case 'max' : { be_fn_t f = { .prio = prio , .fn = BE_FN_MAX    }; b_vec_push(&value.functions, &f); } break;
                case 'min' : { be_fn_t f = { .prio = prio , .fn = BE_FN_MIN    }; b_vec_push(&value.functions, &f); } break;
                case 'sign': { be_fn_t f = { .prio = prio , .fn = BE_FN_SIGN   }; b_vec_push(&value.functions, &f); } break;
                case 'abs' : { be_fn_t f = { .prio = prio , .fn = BE_FN_ABS    }; b_vec_push(&value.functions, &f); } break;
                default: { _BE_TOKEN2ERR(err, mod, BE_ERR_ILL_TOKEN) }
            }
            else if(_BE_TOKENCOMP(mod, "atan2"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_ATAN2   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "floor"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_FLOOR   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "round"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_ROUND   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "gamma"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_GAMMA   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "asinh"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_ASINH   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "acosh"  )) { be_fn_t f = { .prio = prio, .fn = BE_FN_ACOSH   }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "erfinv" )) { be_fn_t f = { .prio = prio, .fn = BE_FN_ERFINV  }; b_vec_push(&value.functions, &f); }
            else if(_BE_TOKENCOMP(mod, "sigmoid")) { be_fn_t f = { .prio = prio, .fn = BE_FN_SIGMOID }; b_vec_push(&value.functions, &f); }
            else { _BE_TOKEN2ERR(err, mod, BE_ERR_ILL_TOKEN) }
        }
        _BE_DIAGNOSTIC_POP
        if(err.err) { b_vec_free(&value.functions); goto ret; }
        b_vec_push(values, &value);
        if(values->fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
        modifiers.len = 0;
    }
    if(modifiers.len) {
        be_token_t *mod = b_vec_get(&modifiers, modifiers.len - 1);
        _BE_TOKEN2ERR(err, mod, BE_ERR_ILL_LAST_TOKEN) goto ret;
    }
    if(brackets.len) {
        be_token_t *br = b_vec_get(&brackets, brackets.len - 1);
        _BE_TOKEN2ERR(err, br, BE_ERR_BRACKET_UNCLOSED)
        goto ret;
    }
    for(int prio = (max_depth + 1) * BE_N_PRIOS; prio --> 0;) {
        size_t k = 0;
        if(prio % BE_N_PRIOS == BE_MUL_PRIO || prio % BE_N_PRIOS == BE_ADD_PRIO) // for we go left to right to avoid problems like 2 / 2 * 2 = 2 / (2 * 2)
        for(size_t i = 0; i < values->len; i ++) {
            be_value_t *value = b_vec_get(values, i);
            int p = (value->flags & BE_PRIO) + value->depth * BE_N_PRIOS;
            if(p == prio && i) {
                struct { size_t a, b; } p;
                p.a = k; p.b = i;
                b_vec_push(order, &p);
                if(order->fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
            }
            if(p < prio) k = i;
        }
        else
        for(size_t i = values->len; i --> 0;) {
            be_value_t *value = b_vec_get(values, i);
            int p = (value->flags & BE_PRIO) + value->depth * BE_N_PRIOS;
            if((p <= prio || i == 0) && k) {
                struct { size_t a, b; } p;
                p.a = i; p.b = k;
                b_vec_push(order, &p);
                if(order->fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
                k = 0;
            }
            if(p == prio) k = i;
        }
    }
    ret:
    b_vec_free(&brackets);
    b_vec_free(&modifiers);
    if(err.err) be_expr_free(expr);
    return err;
}

// a % b operation, behaving like one in python
dtype be_fmod(dtype a, dtype b) {
    if(b == 0) return NAN;
    dtype r = fmod(a, b);
    if(r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// invers erf function, calculated via using newton algorythm on an approximation
dtype be_erfinv(dtype p) {
    if(p < -1 || p > 1 || isnan(p)) return NAN;
    dtype sign = p >= 0 ? 1 : -1;
    p *= sign;
    if(p == 1) return sign * INFINITY;
    dtype x2;
    if(p < 0.95) { // simple hand-picked polynomial approximation for low values
        dtype p4 = p * p; p4 *= p4;
        dtype p16 = p4 * p4; p16 *= p16;
        x2 = 0.9 * p + 0.43 * p4 + 0.4 * p16;
    } else { // asymptotic approximation, approximatelly inverting 1 - exp(-x^2) / (sqrt(pi) x) ~ erf(x) (at x -> inf) in 2 iterations
        // iterations of solving 1 - exp(-x^2) / (sqrt(pi) x) = p are as follows: x_{n+1} = sqrt(-log(x_n * c)), x_0 = 1
        // for reason unknown to me, doing exactly 2 iterations yealds much better approximation for erfinv than true solution, there should be a mathematical explanation for this
        dtype c = 1.7724538509055159 * (1 - p); // root pi
        dtype x1 = sqrt(-log(c));
        x2 = sqrt(-log(x1 * c));
    }
    for(int i = 0; i < 4; i ++) { // newtonian algorythm, 4 iters seem to always be sufficient for as perfect convergence as it gets
        dtype x3 = x2 - (erf(x2) - p) / (1.1283791670955126 * exp(-x2*x2)); // 2/root pi
        if(x2 == x3) break;
        x2 = x3;
    }
    return x2 * sign;
}

// part of code to execute raw operation, used for both runtime and compile-time execution
#define _BE_OP_CASES(prefix, joined, shift, postfix)                                                                                \
    prefix joined##BE_OP_SWAP    shift: *rawop->dst = *rawop->src1 != 0 ? *rawop->src2 : *rawop->src3;                     postfix; \
    prefix joined##BE_OP_OR      shift: *rawop->dst = *rawop->src1 != 0 || *rawop->src2 != 0 ? 1 : 0;                      postfix; \
    prefix joined##BE_OP_AND     shift: *rawop->dst = *rawop->src1 != 0 && *rawop->src2 != 0 ? 1 : 0;                      postfix; \
    prefix joined##BE_OP_EQUALS  shift: *rawop->dst = *rawop->src1 == *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_NEQUALS shift: *rawop->dst = *rawop->src1 != *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_GREATER shift: *rawop->dst = *rawop->src1 >  *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_LESS    shift: *rawop->dst = *rawop->src1 <  *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_GEQ     shift: *rawop->dst = *rawop->src1 >= *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_LEQ     shift: *rawop->dst = *rawop->src1 <= *rawop->src2 ? 1 : 0;                                postfix; \
    prefix joined##BE_OP_ADD     shift: *rawop->dst = *rawop->src1 + *rawop->src2;                                         postfix; \
    prefix joined##BE_OP_SUB     shift: *rawop->dst = *rawop->src1 - *rawop->src2;                                         postfix; \
    prefix joined##BE_OP_MOD     shift: *rawop->dst = be_fmod(*rawop->src1, *rawop->src2);                                 postfix; \
    prefix joined##BE_OP_MUL     shift: *rawop->dst = *rawop->src1 * *rawop->src2;                                         postfix; \
    prefix joined##BE_OP_DIV     shift: *rawop->dst = *rawop->src1 / *rawop->src2;                                         postfix; \
    prefix joined##BE_OP_POW     shift: *rawop->dst = pow(*rawop->src1, *rawop->src2);                                     postfix; \
    prefix joined##BE_FN_NOT     shift: *rawop->dst = *rawop->src1 != 0 ? 0 : 1;                                           postfix; \
    prefix joined##BE_FN_SIN     shift: *rawop->dst = sin(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_COS     shift: *rawop->dst = cos(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_TAN     shift: *rawop->dst = tan(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_ASIN    shift: *rawop->dst = asin(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_ACOS    shift: *rawop->dst = acos(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_ATAN    shift: *rawop->dst = atan(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_ATAN2   shift: *rawop->dst = atan2(*rawop->src1, *rawop->src2);                                   postfix; \
    prefix joined##BE_FN_FLOOR   shift: *rawop->dst = floor(*rawop->src1);                                                 postfix; \
    prefix joined##BE_FN_CEIL    shift: *rawop->dst = ceil(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_ROUND   shift: *rawop->dst = round(*rawop->src1);                                                 postfix; \
    prefix joined##BE_FN_LN      shift: *rawop->dst = log(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_EXP     shift: *rawop->dst = exp(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_SQRT    shift: *rawop->dst = sqrt(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_CBRT    shift: *rawop->dst = cbrt(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_GAMMA   shift: *rawop->dst = tgamma(*rawop->src1);                                                postfix; \
    prefix joined##BE_FN_SINH    shift: *rawop->dst = sinh(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_COSH    shift: *rawop->dst = cosh(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_TANH    shift: *rawop->dst = tanh(*rawop->src1);                                                  postfix; \
    prefix joined##BE_FN_ASINH   shift: *rawop->dst = asinh(*rawop->src1);                                                 postfix; \
    prefix joined##BE_FN_ACOSH   shift: *rawop->dst = acosh(*rawop->src1);                                                 postfix; \
    prefix joined##BE_FN_ATANH   shift: *rawop->dst = atanh(*rawop->src1);                                                 postfix; \
    prefix joined##BE_FN_ERF     shift: *rawop->dst = erf(*rawop->src1);                                                   postfix; \
    prefix joined##BE_FN_ERFINV  shift: *rawop->dst = be_erfinv(*rawop->src1);                                             postfix; \
    prefix joined##BE_FN_SIGMOID shift: *rawop->dst = 1 / (1 + exp(-*rawop->src1));                                        postfix; \
    prefix joined##BE_FN_MAX     shift: *rawop->dst = fmax(*rawop->src1, *rawop->src2);                                    postfix; \
    prefix joined##BE_FN_MIN     shift: *rawop->dst = fmin(*rawop->src1, *rawop->src2);                                    postfix; \
    prefix joined##BE_FN_NEG     shift: *rawop->dst = -*rawop->src1;                                                       postfix; \
    prefix joined##BE_FN_RECIPR  shift: *rawop->dst = 1 / *rawop->src1;                                                    postfix; \
    prefix joined##BE_FN_SIGN    shift: *rawop->dst = isnan(*rawop->src1) ? NAN : (*rawop->src1 > 0) - (*rawop->src1 < 0); postfix; \
    prefix joined##BE_FN_ABS     shift: *rawop->dst = fabs(*rawop->src1);                                                  postfix; \
    prefix joined##BE_OP_INTPOW  shift: *rawop->dst = be_intpow((void*)rawop->src2, *rawop->src1);                         postfix; \
    prefix joined##BE_FN_COPY    shift: *rawop->dst = *rawop->src1;                                                        postfix;

#if _BE_USE_GCC_LABEL_POINTERS
#define _BE_CHECK_LABEL(pref, name) case name >> BE_RAWOP_SHIFT: rawop->pref##gnulabel = &&pref##_##name; break;
#define _BE_LABEL2PTR(pref)                                                                                                                                                                                          \
switch(rawop->op) {                                                                                                                                                                                                  \
    _BE_CHECK_LABEL(pref, BE_OP_SWAP) _BE_CHECK_LABEL(pref, BE_OP_OR) _BE_CHECK_LABEL(pref, BE_OP_AND) _BE_CHECK_LABEL(pref, BE_OP_EQUALS) _BE_CHECK_LABEL(pref, BE_OP_NEQUALS) _BE_CHECK_LABEL(pref, BE_OP_GREATER) \
    _BE_CHECK_LABEL(pref, BE_OP_LESS) _BE_CHECK_LABEL(pref, BE_OP_GEQ) _BE_CHECK_LABEL(pref, BE_OP_LEQ) _BE_CHECK_LABEL(pref, BE_OP_ADD) _BE_CHECK_LABEL(pref, BE_OP_SUB) _BE_CHECK_LABEL(pref, BE_OP_MOD)           \
    _BE_CHECK_LABEL(pref, BE_OP_MUL) _BE_CHECK_LABEL(pref, BE_OP_DIV) _BE_CHECK_LABEL(pref, BE_OP_POW) _BE_CHECK_LABEL(pref, BE_FN_NOT) _BE_CHECK_LABEL(pref, BE_FN_SIN) _BE_CHECK_LABEL(pref, BE_FN_COS)            \
    _BE_CHECK_LABEL(pref, BE_FN_TAN) _BE_CHECK_LABEL(pref, BE_FN_ASIN) _BE_CHECK_LABEL(pref, BE_FN_ACOS) _BE_CHECK_LABEL(pref, BE_FN_ATAN) _BE_CHECK_LABEL(pref, BE_FN_ATAN2) _BE_CHECK_LABEL(pref, BE_FN_FLOOR)     \
    _BE_CHECK_LABEL(pref, BE_FN_CEIL) _BE_CHECK_LABEL(pref, BE_FN_ROUND) _BE_CHECK_LABEL(pref, BE_FN_LN) _BE_CHECK_LABEL(pref, BE_FN_EXP) _BE_CHECK_LABEL(pref, BE_FN_SQRT) _BE_CHECK_LABEL(pref, BE_FN_CBRT)        \
    _BE_CHECK_LABEL(pref, BE_FN_GAMMA) _BE_CHECK_LABEL(pref, BE_FN_SINH) _BE_CHECK_LABEL(pref, BE_FN_COSH) _BE_CHECK_LABEL(pref, BE_FN_TANH) _BE_CHECK_LABEL(pref, BE_FN_ASINH) _BE_CHECK_LABEL(pref, BE_FN_ACOSH)   \
    _BE_CHECK_LABEL(pref, BE_FN_ATANH) _BE_CHECK_LABEL(pref, BE_FN_ERF) _BE_CHECK_LABEL(pref, BE_FN_ERFINV) _BE_CHECK_LABEL(pref, BE_FN_SIGMOID) _BE_CHECK_LABEL(pref, BE_FN_MAX) _BE_CHECK_LABEL(pref, BE_FN_MIN)   \
    _BE_CHECK_LABEL(pref, BE_FN_NEG) _BE_CHECK_LABEL(pref, BE_FN_RECIPR) _BE_CHECK_LABEL(pref, BE_FN_SIGN) _BE_CHECK_LABEL(pref, BE_FN_ABS) _BE_CHECK_LABEL(pref, BE_OP_INTPOW) _BE_CHECK_LABEL(pref, BE_FN_COPY)    \
    _BE_CHECK_LABEL(pref, BE_OP_CONDJUMP) _BE_CHECK_LABEL(pref, BE_OP_RETURN) _BE_CHECK_LABEL(pref, BE_OP_OUT)                                                                                                       \
}
#endif

// execute raw operation in compile time
be_op_t *be_rawop_exec(be_op_t *rawop) {
    switch(rawop->op) {
        _BE_OP_CASES(case,,, break)
    }
    return rawop->next;
}

dtype _be_const_1 = 1; // do not change this, it is the src for copying from to implement zero power

// turn operation/function on 2 values into raw operation(s) or execute in compile time
// new operation(s) get push onto `*ops` vector of be_op_t, it should be preallocated and may already contain operations; this finction is `ops->fail`-agnostic
// does not set `next` field of the operations
// for unary functions `left` and `right` should be the same
// op is operation type, either `BE_OP_*` or `BE_FN_*`
void be_bake_op(b_vec_t *ops, be_value_t *left, be_value_t *right, int op) {
    #define _BE_TRANSFER_CONSTNESS(dst, dst_flag, src, src_flag) if((src)->flags & src_flag) (dst)->flags |= dst_flag; else (dst)->flags &= ~dst_flag;
    if(op == BE_FN_COPY) return; // empty function, just copy
    be_op_t rawop;
    if(op == BE_OP_PAIR) {
        left->b_ptr = right->a_ptr;
        _BE_TRANSFER_CONSTNESS(left, BE_B_CONST, right, BE_A_CONST)
    } else if(op == BE_OP_SWAP) {
        if(left->flags & BE_A_CONST) {
            if(*left->a_ptr != 0) {
                left->a_ptr = right->a_ptr;
                left->b_ptr = right->b_ptr;
                _BE_TRANSFER_CONSTNESS(left, BE_A_CONST, right, BE_A_CONST)
                _BE_TRANSFER_CONSTNESS(left, BE_B_CONST, right, BE_B_CONST)
            } else {
                left->a_ptr = right->b_ptr;
                left->b_ptr = right->a_ptr;
                _BE_TRANSFER_CONSTNESS(left, BE_A_CONST, right, BE_B_CONST)
                _BE_TRANSFER_CONSTNESS(left, BE_B_CONST, right, BE_A_CONST)
            }
        } else {
            rawop.op = op;
            rawop.dst = &left->b;
            rawop.src1 = left->a_ptr;
            rawop.src2 = right->b_ptr;
            rawop.src3 = right->a_ptr;
            b_vec_push(ops, &rawop);
            rawop.dst = &left->a;
            rawop.src2 = right->a_ptr;
            rawop.src3 = right->b_ptr;
            b_vec_push(ops, &rawop);
            left->a_ptr = &left->a;
            left->b_ptr = &left->b;
            left->flags &= ~(BE_A_CONST | BE_B_CONST);
        }
    } else if(op == BE_FN_MIN || op == BE_FN_MAX || op == BE_FN_ATAN2) {
        rawop.op = op;
        rawop.src1 = right->a_ptr;
        rawop.src2 = right->b_ptr;
        if((right->flags & BE_A_CONST) && (right->flags & BE_B_CONST)) {
            left->a_ptr = rawop.dst = &left->a_const;
            be_rawop_exec(&rawop);
            left->flags |= BE_A_CONST;
        } else {
            left->a_ptr = rawop.dst = &left->a;
            b_vec_push(ops, &rawop);
            left->flags &= ~BE_A_CONST;
        }
        if(op == BE_FN_ATAN2) {
            rawop.src1 = right->b_ptr;
            rawop.src2 = right->a_ptr;
        }
        rawop.op = op == BE_FN_MIN ? BE_FN_MAX : op == BE_FN_MAX ? BE_FN_MIN : op;
        if((right->flags & BE_A_CONST) && (right->flags & BE_B_CONST)) {
            left->b_ptr = rawop.dst = &left->b_const;
            be_rawop_exec(&rawop);
            left->flags |= BE_B_CONST;
        } else {
            left->b_ptr = rawop.dst = &left->b;
            b_vec_push(ops, &rawop);
            left->flags &= ~BE_B_CONST;
        }
    } else {
        #define _BE_POW_CASES                                                                                                                                       \
        dtype _p = p > 0 ? p : -p;                                                                                                                                  \
             if(_p ==    0) { rawop.src1 = &_be_const_1; rawop.op = BE_FN_COPY; b_vec_push(ops, &rawop); }                                                          \
        else if(_p ==  .25) { rawop.op = BE_FN_SQRT; b_vec_push(ops, &rawop); rawop.src1 = rawop.dst; b_vec_push(ops, &rawop); }                                    \
        else if(_p == 1./3) { rawop.op = BE_FN_CBRT; b_vec_push(ops, &rawop); }                                                                                     \
        else if(_p == 2./3) { rawop.op = BE_FN_CBRT; b_vec_push(ops, &rawop); rawop.src2 = rawop.src1 = rawop.dst; rawop.op = BE_OP_MUL; b_vec_push(ops, &rawop); } \
        else if(_p ==   .5) { rawop.op = BE_FN_SQRT; b_vec_push(ops, &rawop); }                                                                                     \
        else if(_p ==  1.5) { rawop.op = BE_FN_SQRT; rawop.dst = &left->temp; b_vec_push(ops, &rawop);                                                              \
                              rawop.src2 = rawop.dst; rawop.dst = _dst; rawop.op = BE_OP_MUL; b_vec_push(ops, &rawop); }                                            \
        else if(_p ==    2) { rawop.src2 = rawop.src1; rawop.op = BE_OP_MUL; b_vec_push(ops, &rawop); }                                                             \
        else if(_p > 2 && p == (int)p) { be_intpow_bake(&right->ip, p); if(right->ip.n < 0) special_case = false;                                                   \
                                         else { rawop.op = BE_OP_INTPOW; rawop.src2 = (void*)&right->ip; b_vec_push(ops, &rawop); } }                               \
        else { special_case = false; }                                                                                                                              \
        if(special_case && p < 0 && rawop.op != BE_OP_INTPOW) { rawop.src1 = rawop.dst; rawop.op = BE_FN_RECIPR; b_vec_push(ops, &rawop); }                         \
        if(p == -1) { rawop.op = BE_FN_RECIPR; b_vec_push(ops, &rawop); special_case = true; }                                                                      \
        else if(p == 1) { power1 = special_case = true; }
        for(int k = 0; k < 2; k ++) {
            rawop.op = op;
            int const_flag = (k ? BE_B_CONST : BE_A_CONST);
            rawop.src1 = k ? left->b_ptr : left->a_ptr;
            rawop.src2 = k ? right->b_ptr : right->a_ptr;
            bool const_fold = (right->flags & const_flag) && (left->flags & const_flag);
            size_t _l = ops->len;
            dtype *_dst = rawop.dst = const_fold ? (k ? &left->b_const : &left->a_const) : (k ? &left->b : &left->a);
            bool power1 = false;
            bool special_case = false;
            #if _BE_OPTIMIZE_POW
            if(op == BE_OP_POW && (right->flags & const_flag)) {
                special_case = true;
                dtype p = (k ? *right->b_ptr : *right->a_ptr);
                _BE_POW_CASES
            }
            #endif
            if(!special_case) { b_vec_push(ops, &rawop); left->flags &= ~const_flag; }
            if(!power1) *(k ? &left->b_ptr : &left->a_ptr) = _dst;
            if(const_fold) {
                for(size_t i = _l; i < ops->len; i ++) be_rawop_exec(b_vec_get(ops, i));
                ops->len = _l;
                left->flags |= const_flag;
            }
        }
    }
}

// bake expression into raw operations
// be_expr_parse should be called on `*expr` before this function
// new operation(s) get push onto `*ops` vector of be_op_t, it should be preallocated and may already contain operations; this finction is `ops->fail`-agnostic
// `vars` (array) should be final placement of vars in memory, with sufficient number of elements to cover all indices pointed to by `var` field ob `be_value_t`
void be_expr_bake(b_vec_t *ops, be_expr_t *expr, be_var_t *vars) {
    size_t _ops_len = ops->len;
    b_vec_t *values = &expr->values;
    for(size_t i = 0; i < values->len; i ++) { // populating values
        be_value_t *value = b_vec_get(values, i);
        value->a_ptr = (value->flags & BE_VT) == BE_VT_CONST ? &value->a_const : &vars[value->var].a;
        value->b_ptr = (value->flags & BE_VT) == BE_VT_CONST ? &value->b_const : &vars[value->var].b;
        if((value->flags & BE_VT) == BE_VT_CONST) value->flags |= BE_A_CONST | BE_B_CONST;
        value->curr_function = 0;
    }
    b_vec_t *order = &expr->order;
    for(size_t i = 0; i < order->len; i ++) { // evaluating
        struct { size_t a, b; } *pair = b_vec_get(order, i);
        be_value_t *left = b_vec_get(values, pair->a);
        be_value_t *right = b_vec_get(values, pair->b);
        while(left->curr_function < left->functions.len) {
            be_fn_t *f = b_vec_get(&left->functions, left->curr_function);
            // if(f->depth < right->depth) break;
            // if(f->depth * BE_N_PRIOS + BE_FN_PRIO < right->depth * BE_N_PRIOS + (right->flags & BE_PRIO)) break;
            if(f->prio < right->depth * BE_N_PRIOS + (right->flags & BE_PRIO)) break;
            be_bake_op(ops, left, left, f->fn);
            left->curr_function ++;
        }
        while(right->curr_function < right->functions.len)
            be_bake_op(ops, right, right, ((be_fn_t*)b_vec_get(&right->functions, right->curr_function ++))->fn);
        be_bake_op(ops, left, right, right->flags & BE_OP);
    }
    be_value_t *v = values->ptr;
    while(v->curr_function < v->functions.len) be_bake_op(ops, v, v, ((be_fn_t*)b_vec_get(&v->functions, v->curr_function ++))->fn);
    for(size_t i = _ops_len; i < ops->len; i ++)
        ((be_op_t*)b_vec_get(ops, i))->next = (void*)(i + 1);
}

// line types
#define BE_LT_EXPR      0x011
#define BE_LT_IF        0x132
#define BE_LT_ELIF      0x1B3
#define BE_LT_ELSE      0x1A4
#define BE_LT_SCOPE     0x025
#define BE_LT_ASSIGN    0x016
#define BE_LT_OUT       0x017
#define BE_LT_EMPTY_OUT 0x008
#define BE_LT_BREAK     0x049
#define BE_LT_LOOP      0x04A
#define BE_EXPR         0x010 // bit showing whether line type has `expr` defined except
#define BE_INNER_LINES  0x020 // bit showing whether line type has inner lines (for scopes andd if-s and while-s (represented as if-s))
#define BE_FOLLOWUP     0x080 // elif or else
#define BE_CONDITIONAL  0x100
#define BE_DEPTH        0x040

typedef struct {
    // line of code; scopes and if-s are also lines, containing inner lines
    int type; // line type, `BE_LT_*`
    size_t var; // index or dst var for assign line type
    be_expr_t expr; // main expression of this line (condition to check in case of `if` line type)
    be_tline_t texpr; // for debug purpaces
    b_vec_t inner_lines; // inner line for scpes and if-s
    int depth; // how many scopes to jump through (for loop and break line types)
    bool *loops, *breaks;
    char *label; // for BE_LT_OUT
} be_line_t;

// free allocatable fields of `be_line_t`
void be_line_free(be_line_t *line) {
    if(line->type & BE_EXPR)
        be_expr_free(&line->expr);
    if(line->type & BE_INNER_LINES) {
        for(size_t i = 0; i < line->inner_lines.len; i ++)
            be_line_free(b_vec_get(&line->inner_lines, i));
        b_vec_free(&line->inner_lines);
    }
    if(line->type == BE_LT_OUT || line->type == BE_LT_EMPTY_OUT) free(line->label);
    free(line->breaks);
    free(line->loops);
}

be_err_t be_line_parse(be_line_t *line, be_tline_t *tokens, b_vec_t *varnames, size_t depth, bool if_chain);

// pars tokens of code into `*lines` vector of be_line_t (should be succesfully preallocated)
// `depth` is scope depth we are at (1 for global scope, the whole program)
// `*varnames` is a vector of `char*`, existing variable names.
// It gets modified within the function but in the end it has same length and contents
// but possibly at different ptr and cap and possibly a fail due to reallocations; must be non-failed at call time
// `conn_depth` is such scope depth that a `loop` to that depth always returns control to the current point of parsing (unless it gets stuck in an infinite loop); initially 0 for global scope
// all lines pushed onto `*lines` should be freed later with `be_line_free` if success, on error they are automatically freed (though still pushed into `*lines`)
be_err_t be_parse_tokens(b_vec_t *lines, b_vec_t *tokens, b_vec_t *varnames, size_t depth, bool *breaks, bool *loops) {
    be_err_t err;
    err.err = 0;
    if(_BE_STACK_LIMIT > 0 && depth > _BE_STACK_LIMIT + 1) { err.err = BE_ERR_STACK_DEPTH; err.line_start = 0; return err; }
    size_t n_vars = varnames->len;
    b_vec_t tlines;
    b_vec_alloc(&tlines, sizeof(be_tline_t), _BE_DEFAULT_VEC);
    err = be_tlines(&tlines, tokens); // this does malloc check on tlines
    if(err.err) goto ret;
    bool reached = true;
    bool if_chain = false;
    bool trap = false;
    memset(breaks, 0, sizeof(bool) * (depth + 1));
    memset(loops, 0, sizeof(bool) * (depth + 1));
    for(size_t i = 0; i < tlines.len; i ++) {
        be_tline_t *tline = b_vec_get(&tlines, i);
        be_line_t line;
        err = be_line_parse(&line, tline, varnames, depth, if_chain);
        if(err.err) goto ret;
        if(line.type == BE_LT_IF) if_chain = true;
        else if(line.type != BE_LT_ELIF) if_chain = false;
        b_vec_push(lines, &line);
        if(lines->fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
        if(reached) {
            for(size_t i = 0; i <= depth; i ++) { loops[i] |= line.loops[i]; breaks[i] |= line.breaks[i]; }
            if((line.type & BE_CONDITIONAL) == 0) reached = line.breaks[depth];
        }
        if(reached && line.type == BE_LT_ELSE) {
            reached = false;
            for(size_t j = i + 1; j --> 0;) {
                be_line_t *l = b_vec_get(lines, j);
                reached |= l->breaks[depth];
                if(l->type == BE_LT_IF) break;
            }
        }
    }
    
    breaks[depth] = reached;
    ret:
    b_vec_free(&tlines);
    if(!err.err && depth == 1 && reached) {
        err.line_start = 0;
        err.err = BE_ERR_NO_EXPR;
    }
    if(err.err) {
        for(size_t i = 0; i < lines->len; i ++) be_line_free(b_vec_get(lines, i));
    }
    for(size_t i = n_vars; i < varnames->len; i ++) free(*(void**)b_vec_get(varnames, i));
    varnames->len = n_vars;
    return err;
}

// check tokens like `break1` and `loop1` (with any positive whole number in place of `1`) and return the number in the end
// cmd is `break` or `loop`
// returns 0 if the cmd is not found or overflow or any other problem
int be_jump_depth(be_token_t *token, const char *cmd) {
    size_t l = strlen(cmd);
    if(token->len <= l) return 0;
    if(strncmp(cmd, token->name, l) != 0) return 0;
    if(token->name[l] == '0') return 0;
    if(token->len - l > 9) return 0; // guaranteed fit into 32-bit signed int
    int n = 0, k = 1;
    for(size_t i = token->len; i --> l;) {
        if(token->name[i] < '0' || token->name[i] > '9') return 0;
        n += (token->name[i] - '0') * k; k *= 10;
    }
    return n;
}

// parse `*tokens` representing a line of code into `be_line_t` structure
// `*varnames`, `depth` and `conn_depth` arguments are like those for `be_parse_tokens`
// an assignation line can push new varname onto `varnames`
// `*line` should be freed with `be_line_free` afterwards if no error occurs, on error it is freed automatically
// fields `alw_reached` and `smt_reached` are not set by this function
be_err_t be_line_parse(be_line_t *line, be_tline_t *tokens, b_vec_t *varnames, size_t depth, bool if_chain) {
    be_err_t err = { .err = 0 };
    line->breaks = malloc(sizeof(bool) * (depth + 2)); // 1 more byte than needed allocated for calling be_parse_tokens on this directly
    line->loops = malloc(sizeof(bool) * (depth + 2));
    if(line->breaks == NULL || line->loops == NULL) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
    int break_depth = be_jump_depth(tokens->ptr, "break");
    if(break_depth >= depth) {
        _BE_TOKEN2ERR(err, tokens->ptr, BE_ERR_BREAK_DEPTH)
        err.char_n += 5;
        goto ret;
    }
    int loop_depth = be_jump_depth(tokens->ptr, "loop");
    if(loop_depth > depth) {
        _BE_TOKEN2ERR(err, tokens->ptr, BE_ERR_LOOP_DEPTH)
        err.char_n += 4;
        goto ret;
    }
    if(*tokens->ptr[tokens->len - 1].name == '}') {
        int k = 1;
        bool is_elif = _BE_TOKENCOMP(tokens->ptr, "elif");
        bool is_while = _BE_TOKENCOMP(tokens->ptr, "while");
        if(is_while || is_elif || _BE_TOKENCOMP(tokens->ptr, "if")) {
            line->type = is_elif ? BE_LT_ELIF : BE_LT_IF;
            while(k < tokens->len - 1 && *tokens->ptr[k].name != '{') k ++;
            line->texpr.ptr = tokens->ptr + 1;
            line->texpr.len = k - 1;
            if(line->texpr.len == 0) { _BE_TOKEN2ERR(err, tokens->ptr + k, is_elif ? BE_ERR_EMPTY_ELIF : is_while ? BE_ERR_EMPTY_WHILE : BE_ERR_EMPTY_IF) goto ret; }
            err = be_expr_parse(&line->expr, &line->texpr, varnames);
            if(err.err) goto ret;
            k ++;
        } else if(_BE_TOKENCOMP(tokens->ptr, "else")) {
            line->type = BE_LT_ELSE;
            if(*tokens->ptr[1].name != '{') { _BE_TOKEN2ERR(err, tokens->ptr + 1, BE_ERR_NONEMPTY_ELSE) goto ret; }
            k = 2;
        }
        else if(*tokens->ptr->name == '{') line->type = BE_LT_SCOPE;
        else { _BE_TOKEN2ERR(err, tokens->ptr, BE_ERR_SCOPE_START) goto ret; }
        if((line->type & BE_FOLLOWUP) && !if_chain) {
            if(line->type & BE_EXPR) be_expr_free(&line->expr);
            _BE_TOKEN2ERR(err, tokens->ptr, BE_ERR_MISFOLLOWUP) goto ret;
        }
        b_vec_t inner_tokens;
        inner_tokens.ptr = tokens->ptr + k;
        inner_tokens.len = tokens->len - 1 - k;
        inner_tokens.step = sizeof(be_token_t);
        b_vec_alloc(&line->inner_lines, sizeof(be_line_t), _BE_DEFAULT_VEC);
        if(line->inner_lines.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; }
        else err = be_parse_tokens(&line->inner_lines, &inner_tokens, varnames, depth + 1, line->breaks, line->loops);
        if(err.err) {
            b_vec_free(&line->inner_lines);
            if(line->type & BE_EXPR) be_expr_free(&line->expr);
            goto ret;
        }
        if(is_while) {
            be_line_t l = { .type = BE_LT_LOOP, .depth = 1, .breaks = NULL, .loops = NULL };
            b_vec_push(&line->inner_lines, &l); // always the last push so a fail here is fine
        } else line->breaks[depth] |= line->breaks[depth + 1]; // end reachability implies continuability of executions
    } else if(tokens->len == 1 && break_depth) {
        line->type = BE_LT_BREAK;
        line->depth = break_depth;
        memset(line->breaks, 0, depth + 1);
        memset(line->loops , 0, depth + 1);
        line->breaks[depth - break_depth] = true;
    } else if(tokens->len == 1 && loop_depth) {
        line->type = BE_LT_LOOP;
        line->depth = loop_depth;
        memset(line->breaks, 0, depth + 1);
        memset(line->loops , 0, depth + 1);
        line->loops[depth - loop_depth] = true;
    } else {
        memset(line->breaks, 0, depth);
        memset(line->loops , 0, depth + 1);
        line->breaks[depth] = true;
        size_t k;
        char *varname = NULL;
        // if(tokens->len >= 2 && _BE_TOKENCOMP(tokens->ptr + 1, ":=")) {
        char c;
        if(tokens->len >= 2) c = tokens->ptr[1].name[0];
        if(tokens->len >= 2 && tokens->ptr[1].len == 2 && tokens->ptr[1].name[1] == '=' && (c == ':' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|' || c == '^')) {
            if(tokens->len == 2) {
                _BE_TOKEN2ERR(err, tokens->ptr + 1, BE_ERR_EMPTY_ASSIGN)
                err.char_n += 2;
                goto ret;
            }
            for(size_t i = 0; i < varnames->len; i ++) {
                char *varname = *(char**)b_vec_get(varnames, i);
                if(tokens->ptr->len == strlen(varname) && strncmp(varname, tokens->ptr->name, tokens->ptr->len) == 0) {
                    line->var = i;
                    goto var_found;
                }
            }
            line->var = varnames->len;
            varname = malloc(tokens->ptr->len + 1);
            if(varname == NULL) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
            varname[tokens->ptr->len] = '\0';
            memcpy(varname, tokens->ptr->name, tokens->ptr->len);
            var_found:
            line->type = BE_LT_ASSIGN; k = c == ':' ? 2 : 0;
        } else if(*tokens->ptr->name == '@') {
            line->type = BE_LT_OUT;
            for(k = 1; k < tokens->len; k ++) if(tokens->ptr[k].len == 1 && *tokens->ptr[k].name == ':') break;
            if(k == tokens->len - 1) {
                err.line_start = tokens->ptr->text_line;
                err.line_end = tokens->ptr[tokens->len - 1].text_line;
                err.char_n = err.line_start == err.line_end ? tokens->ptr->text_char : 0;
                err.err = BE_ERR_EMPTY_OUT;
                goto ret;
            }
            if(k == tokens->len) line->type = BE_LT_EMPTY_OUT;
            k ++;
        } else {
            line->type = BE_LT_EXPR;
            k = 0;
            line->breaks[depth] = false;
            line->breaks[0] = true;
        }
        line->texpr.ptr = tokens->ptr + k;
        line->texpr.len = tokens->len - k;
        if(line->type & BE_EXPR) {
            if(line->type == BE_LT_ASSIGN && c != ':') tokens->ptr[1].len = 1;
            err = be_expr_parse(&line->expr, &line->texpr, varnames);
            if(line->type == BE_LT_ASSIGN && c != ':') tokens->ptr[1].len = 2;
        }
        if(varname != NULL) {
            b_vec_push(varnames, &varname);
            if(varnames->fail) {
                if(!err.err) be_expr_free(&line->expr);
                free(varname); varnames->len --; // poping last pushed varname assuming `varnames` was not pre-failed
                err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0;
                goto ret;
            }
        }
        if(err.err) goto ret;
        if(line->type == BE_LT_OUT || line->type == BE_LT_EMPTY_OUT) {
            size_t label_len = k == 2 ? 0 : (tokens->ptr[k - 2].name - tokens->ptr[1].name) + tokens->ptr[k - 2].len; // assuming tokens are pointing to a contigous string
            line->label = malloc(label_len + 1);
            if(line->label == NULL) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto ret; }
            memcpy(line->label, tokens->ptr[1].name, label_len);
            line->label[label_len] = '\0';
        }
    }
    ret:
    if(err.err) { free(line->breaks); free(line->loops); }
    return err;
}

typedef struct {
    b_vec_t ops; // vector of `be_op_t` raw operations
    b_vec_t lines; // lines of code
    be_var_t *vars; // array of variables, user-provided variables go to beginning of this array, rest is for holding values of variables defined in code of the program
    void (*print)(const char*, dtype); // function that gets called on `@` operations, if set to NULL it is not called
    size_t n_vars; // number of user-defined variables
    // #if _BE_USE_GCC_LABEL_POINTERS
    bool _be_gnulabel_filler_call;
    // #endif

    // structure for a compiled program
} be_code_t;

// iterate through `*lines` (vector of `be_line_t`) recursively ane return maximum var index found +1 (effectively how much variables we can have defined at a time in a deepest scope)
size_t be_n_vars(b_vec_t *lines) {
    size_t N = 1;
    for(size_t i = 0; i < lines->len; i ++) {
        be_line_t *line = b_vec_get(lines, i);
        if(line->type & BE_EXPR) {
            b_vec_t *values = &line->expr.values;
            for(size_t j = 0; j < values->len; j ++) {
                be_value_t *value = b_vec_get(values, j);
                if((value->flags & BE_VT_VAR) && value->var >= N) N = value->var + 1;
            }
        }
        if(line->type == BE_LT_ASSIGN && line->var >= N) N = line->var + 1;
        if(line->type & BE_INNER_LINES) {
            size_t _N = be_n_vars(&line->inner_lines);
            if(_N > N) N = _N;
        }
    }
    return N;
}

// iterates through `*ops` vector of `be_op_t` at the indeces from `*idx` vector of `size_t`
// redirects those `next` fields and `src2` of condjumps that pointed to `old_target` to `new_target`
void be_redirect_ops(b_vec_t *ops, b_vec_t *idx, size_t old_target, size_t new_target) {
    for(size_t i = 0; i < idx->len; i ++) {
        be_op_t *rawop = b_vec_get(ops, *(size_t*)b_vec_get(idx, i));
        if((size_t)rawop->next == old_target) rawop->next = (void*)new_target;
        if(rawop->op == BE_OP_CONDJUMP && (size_t)rawop->src2 == old_target) rawop->src2 = (void*)new_target;
    }
}

// bakes an assignation (line with `:=`) into raw operations pushed onto `*ops` vector of `be_op_t` by redirecting `dst` of operations when possible or by adding `BE_FN_COPY`
// `*line` is the assignation line whose `expr` should already be baked
// `line_first_op` is the index of first oparation the expr was baked into or `ops->len` when the expr has produced no operations
void be_bake_assign(b_vec_t *ops, be_line_t *line, be_var_t *vars, size_t line_first_op) {
    be_op_t rawop;
    dtype *a_ptr = ((be_value_t*)line->expr.values.ptr)->a_ptr;
    dtype *b_ptr = ((be_value_t*)line->expr.values.ptr)->b_ptr;
    dtype *var_a = &vars[line->var].a;
    dtype *var_b = &vars[line->var].b;
    bool a_optimized = a_ptr == var_a;
    bool b_optimized = b_ptr == var_b;
    bool a_unoptimized = b_ptr == var_a;
    bool b_unoptimized = a_ptr == var_b;
    for(size_t j = ops->len; j --> line_first_op;) {
        be_op_t *rawop = b_vec_get(ops, j);
        if(rawop->dst == a_ptr && !a_optimized && !a_unoptimized) { rawop->dst = var_a; a_optimized = true; if(a_ptr == b_ptr) b_unoptimized = true; }
        if(rawop->dst == b_ptr && !b_optimized && !b_unoptimized) { rawop->dst = var_b; b_optimized = true; }
        if(((rawop->op & BE_SRC1) && rawop->src1 == var_a) ||
            ((rawop->op & BE_SRC2) && rawop->src2 == var_a) ||
            ((rawop->op & BE_SRC3) && rawop->src3 == var_a)) a_unoptimized = true;
        if(((rawop->op & BE_SRC1) && rawop->src1 == var_b) ||
            ((rawop->op & BE_SRC2) && rawop->src2 == var_b) ||
            ((rawop->op & BE_SRC3) && rawop->src3 == var_b)) b_unoptimized = true;
    }
    rawop.op = BE_FN_COPY;
    #define _BE_COPY_A if(!a_optimized) { rawop.src1 = a_ptr; rawop.dst = var_a; rawop.next = (be_op_t*)(ops->len + 1); b_vec_push(ops, &rawop); }
    #define _BE_COPY_B if(!b_optimized) { rawop.src1 = b_ptr; rawop.dst = var_b; rawop.next = (be_op_t*)(ops->len + 1); b_vec_push(ops, &rawop); }
    if(b_ptr != var_a) {
        _BE_COPY_A
        _BE_COPY_B
    } else if(a_ptr != var_b) {
        _BE_COPY_B
        _BE_COPY_A
    } else { // swap
        dtype *temp = &((be_value_t*)line->expr.values.ptr)->a;
        rawop.src1 = var_a; rawop.dst = temp ; rawop.next = (be_op_t*)(ops->len + 1); b_vec_push(ops, &rawop);
        rawop.src1 = var_b; rawop.dst = var_a; rawop.next = (be_op_t*)(ops->len + 1); b_vec_push(ops, &rawop);
        rawop.src1 = temp ; rawop.dst = var_b; rawop.next = (be_op_t*)(ops->len + 1); b_vec_push(ops, &rawop);
    }
}

typedef struct {
    size_t i; // index of the operation
    size_t depth; // depth where the break arrives to with global scope being 1, i.e. 2 for `{{break1}}`, 1 for `{break1}`
    bool next, other; // wither to redirect next fiend and/or `src2` for a condjump
    // used for remembering where to redirect operations coming before a `break` line when the requested number of scopes is closed
} be_exit_t;

dtype _be_const_nan = NAN;

void be_push_nop(b_vec_t *ops) {
    be_op_t rawop = { .op = BE_OP_CONDJUMP, .src1 = &_be_const_nan, .next = (void*)(ops->len + 1), .src2 = (void*)(ops->len + 1) }; // used as NOP
    b_vec_push(ops, &rawop);
}

be_err_t be_if_ch_bake(b_vec_t *ops, b_vec_t *lines, be_var_t *vars, b_vec_t *entering_stack, b_vec_t *exiting_stack, b_vec_t *end_jumping_ops);

// bake a scope of lines (denoted with `{}`) into raw operations pushed onto `*ops` vector of `be_op_t`
// `*lines` is a vector of `be_line_t`, the lines of the scope
// `*entering_stack` is a vector of `size_t` holding indices of operations at scope entering points of the whole scope stack we'r in (for conditional scopes pointing to the beginning of condition evaluation)
// `*exiting_stack` is a vector of `be_exit_t` tracking redirection of operations coming before `break` lines
// `entering_op` is the index of first operation of the scope which can be different from `ops->len` for conditional scopes as it needs to point to the condition, not to the insides of scope itself
// `*ops`, `*entering_stack`, `*exiting_stack` and `*end_jumping_ops` are all allowed to be failed
be_err_t be_scope_bake(b_vec_t *ops, b_vec_t *lines, be_var_t *vars, b_vec_t *entering_stack, b_vec_t *exiting_stack, b_vec_t *end_jumping_ops, size_t entering_op) {
    if(entering_stack->fail) return (be_err_t){ .err = BE_ERR_ALLOC_FAIL, .line_start = 0 };
    be_err_t err = { .err = 0, .line_start = 0 };
    b_vec_push(entering_stack, &entering_op);
    #define _BE_RESET_END_JUMPS { end_jumping_ops->len = 0; size_t prelast = ops->len - 1; b_vec_push(end_jumping_ops, &prelast); }
    for(size_t i = 0; i < lines->len; i ++) {
        cnt:;
        be_line_t *line = b_vec_get(lines, i);
        size_t line_first_op = ops->len;
        if((line->type & BE_EXPR) && line->type != BE_LT_IF) {
            be_expr_bake(ops, &line->expr, vars);
            if(ops->len > line_first_op) _BE_RESET_END_JUMPS
        }
        if(line->type == BE_LT_ASSIGN) {
            size_t _ops_len = ops->len;
            be_bake_assign(ops, line, vars, line_first_op);
            if(ops->len > _ops_len) _BE_RESET_END_JUMPS
        } else if(line->type == BE_LT_BREAK) {
            if(line->depth == 1) goto ret;
            for(size_t i = 0; i < end_jumping_ops->len; i ++) {
                be_exit_t record = { .i = *(size_t*)b_vec_get(end_jumping_ops, i), .depth = entering_stack->len - line->depth };
                be_op_t *r = b_vec_get(ops, record.i);
                record.next = ((size_t)r->next == ops->len);
                if(record.next) { r->next = (void*)-3; } // setting to a placeholder to ensure `end_jumping_ops` contains ALL end jumpers
                record.other = (r->op == BE_OP_CONDJUMP && (size_t)r->src2 == ops->len);
                if(record.other) { r->src2 = (void*)-3; }
                b_vec_push(exiting_stack, &record);
            }
            end_jumping_ops->len = 0;
            goto ret;
        } else if(line->type == BE_LT_LOOP) {
            size_t dst = *(size_t*)b_vec_get(entering_stack, entering_stack->len - line->depth);
            if(dst == ops->len) { be_push_nop(ops); _BE_RESET_END_JUMPS }
            be_redirect_ops(ops, end_jumping_ops, ops->len, dst);
            end_jumping_ops->len = 0;
            goto ret;
        } else if(line->type == BE_LT_OUT || line->type == BE_LT_EMPTY_OUT) {
            be_op_t rawop = { .op = BE_OP_OUT, .src1 = line->type == BE_LT_EMPTY_OUT ? &_be_const_nan : ((be_value_t*)line->expr.values.ptr)->a_ptr, .src2 = (void*)line->label, .next = (void*)(ops->len + 1) };
            b_vec_push(ops, &rawop);
            _BE_RESET_END_JUMPS
        } else if(line->type == BE_LT_EXPR) {
            be_op_t rawop = { .op = BE_OP_RETURN, .src1 = ((be_value_t*)line->expr.values.ptr)->a_ptr, .next = (void*)(ops->len + 1) };
            b_vec_push(ops, &rawop);
            _BE_RESET_END_JUMPS
            goto ret;
        } else if(line->type == BE_LT_IF) {
            size_t j;
            for(j = i + 1; j < lines->len; j ++) if((((be_line_t*)b_vec_get(lines, j))->type & BE_FOLLOWUP) == 0) break;
            b_vec_t if_ch_lines = { .step = sizeof(be_line_t), .len = j - i, .ptr = b_vec_get(lines, i) };
            be_if_ch_bake(ops, &if_ch_lines, vars, entering_stack, exiting_stack, end_jumping_ops);
            i = j;
            if(j < lines->len) goto cnt;
        } else if(line->type == BE_LT_SCOPE) {
            err = be_scope_bake(ops, &line->inner_lines, vars, entering_stack, exiting_stack, end_jumping_ops, ops->len);
            if(err.err) goto ret;
        }
    }
    ret:
    if(!err.err && (ops->fail || end_jumping_ops->fail || exiting_stack->fail)) err.err = BE_ERR_ALLOC_FAIL;
    if(!err.err)
    for(size_t i = exiting_stack->len; i --> 0;) {
        be_exit_t *record = b_vec_get(exiting_stack, i);
        if(record->depth == entering_stack->len - 1) {
            be_op_t *r = b_vec_get(ops, record->i);
            bool was_end_jumping = (size_t)r->next == ops->len || (r->op == BE_OP_CONDJUMP && (size_t)r->src2 == ops->len);
            if(record->next) r->next = (void*)ops->len;
            if(record->other) r->src2 = (void*)ops->len;
            if(!was_end_jumping) b_vec_push(end_jumping_ops, &record->i);
            //exiting_stack->len --;
            b_vec_cut(exiting_stack, i, 1);
        }// else break;
    }
    entering_stack->len --;
    return err;
}

be_err_t be_if_ch_bake(b_vec_t *ops, b_vec_t *lines, be_var_t *vars, b_vec_t *entering_stack, b_vec_t *exiting_stack, b_vec_t *end_jumping_ops) {
    be_err_t err = { .err = 0, .line_start = 0 };
    b_vec_t chain_end_jumpers; b_vec_alloc(&chain_end_jumpers, sizeof(size_t), _BE_DEFAULT_VEC);
    for(size_t i = 0; i < lines->len; i ++) {
        size_t line_start_op = ops->len;
        be_line_t *line = b_vec_get(lines, i);
        size_t condjump;
        if(line->type & BE_EXPR) {
            be_expr_bake(ops, &line->expr, vars);
            be_op_t rawop = { .op = BE_OP_CONDJUMP, .src1 = ((be_value_t*)line->expr.values.ptr)->a_ptr, .next = (void*)(ops->len + 1), .src2 = (void*)-2 }; // -2 is placeholder for the next clause
            condjump = ops->len;
            b_vec_push(ops, &rawop);
            _BE_RESET_END_JUMPS
        }
        err = be_scope_bake(ops, &line->inner_lines, vars, entering_stack, exiting_stack, end_jumping_ops, line_start_op);
        if(err.err) goto ret;
        be_redirect_ops(ops, end_jumping_ops, ops->len, (size_t)-1); // -1 is placeholder for the if-chain end
        b_vec_concat(&chain_end_jumpers, end_jumping_ops);
        end_jumping_ops->len = 0;
        if(line->type & BE_EXPR) {
            be_op_t *rawop = b_vec_get(ops, condjump);
            if((size_t)rawop->next != ops->len && (rawop->op != BE_OP_CONDJUMP || (size_t)rawop->src2 != ops->len))
                b_vec_push(end_jumping_ops, &condjump);
            rawop->src2 = (void*)ops->len;
        }
    }
    be_redirect_ops(ops, &chain_end_jumpers, (size_t)-1, ops->len);
    b_vec_concat(end_jumping_ops, &chain_end_jumpers);
    ret:
    if(!err.err && (ops->fail || end_jumping_ops->fail || chain_end_jumpers.fail)) err.err = BE_ERR_ALLOC_FAIL;
    b_vec_free(&chain_end_jumpers);
    return err;
}

bool _be_skip_rawop_shift = false; // left for debug purpaces, used in cli

// change `next` fields from indeces to actual pointers and apply >>BE_RAWOP_SHIFT for a dense jump table
// `*ops` is a vector of `be_op_t`
void be_op_prep(b_vec_t *ops) {
    for(size_t i = 0; i < ops->len; i ++) {
        be_op_t *rawop = b_vec_get(ops, i);
        rawop->next = (be_op_t*)ops->ptr + (size_t)rawop->next;
        if(rawop->op == BE_OP_CONDJUMP) rawop->src2 = (void*)((be_op_t*)ops->ptr + (size_t)rawop->src2);
        if(!_be_skip_rawop_shift) rawop->op >>= BE_RAWOP_SHIFT;
    }
}

// evaluate (run) a compiled program
// `*code` is the compiled program
// `vars` is an array containing values for user-defined variables, same number as number of variables specified at compilation (`n_vars`)
// `max_operations` is cap for how much raw operations may be performed (not to hang in case of infinite loop), 0 means no limit; when the limit is reached NAN is returned
dtype be_code_eval(const be_code_t *code, const dtype *vars, unsigned long long max_operations) {
    #if _BE_USE_GCC_LABEL_POINTERS
    if(code->_be_gnulabel_filler_call) {
        for(size_t i = 0; i < code->ops.len; i ++) {
            be_op_t *rawop = b_vec_get((void*)&code->ops, i);
            _BE_LABEL2PTR()
            #if _BE_UNLIMITED_CASE
            _BE_LABEL2PTR(_)
            #endif
        }
        return 0;
    }
    #endif
    for(size_t i = 0; i < code->n_vars; i ++) {
        code->vars[i].a = vars[i];
        code->vars[i].b = 0;
    }
    be_op_t *rawop = code->ops.ptr;
    #if _BE_USE_GCC_LABEL_POINTERS
    if(!_BE_UNLIMITED_CASE || max_operations) {
        unsigned long long k = 0;
        goto *rawop->gnulabel;
        _BE_OP_CASES(, _,, if(max_operations && ++ k >= max_operations) return NAN; rawop = rawop->next; goto *rawop->gnulabel)
        _BE_OP_RETURN: return *rawop->src1;
        _BE_OP_CONDJUMP: rawop = *rawop->src1 != 0 ? rawop->next : (void*)rawop->src2; if(max_operations && ++ k >= max_operations) return NAN; goto *rawop->gnulabel;
        _BE_OP_OUT: if(code->print != NULL) code->print((char*)rawop->src2, *rawop->src1); if(max_operations && ++ k >= max_operations) return NAN; rawop = rawop->next; goto *rawop->gnulabel;
    }
    #if _BE_UNLIMITED_CASE
    else {
        goto *rawop->_gnulabel;
        _BE_OP_CASES(, __,, rawop = rawop->next; goto *rawop->_gnulabel)
        __BE_OP_RETURN: return *rawop->src1;
        __BE_OP_CONDJUMP: rawop = *rawop->src1 != 0 ? rawop->next : (void*)rawop->src2; goto *rawop->_gnulabel;
        __BE_OP_OUT: if(code->print != NULL) code->print((char*)rawop->src2, *rawop->src1); rawop = rawop->next; goto *rawop->_gnulabel;
    }
    #endif
    #else
    for(unsigned long long k = 0; k < max_operations || max_operations == 0; k ++) {
        switch(rawop->op) {
            _BE_OP_CASES(case,, >> BE_RAWOP_SHIFT, break)
            case BE_OP_RETURN   >> BE_RAWOP_SHIFT: return *rawop->src1;
            case BE_OP_CONDJUMP >> BE_RAWOP_SHIFT: rawop = *rawop->src1 != 0 ? rawop->next : (void*)rawop->src2; continue;
            case BE_OP_OUT      >> BE_RAWOP_SHIFT: if(code->print != NULL) code->print((char*)rawop->src2, *rawop->src1); break;
        }
        rawop = rawop->next;
    }
    #endif
    return NAN;
}

// checks whether a vector of `dtype*` contains a specific pointer as an element
bool be_vec_contains_ptr(b_vec_t *vec, dtype *ptr) {
    for(size_t i = 0; i < vec->len; i ++) {
        if(((dtype**)vec->ptr)[i] == ptr) return true;
    }
    return false;
}

size_t be_unskipped_descendant(b_vec_t *ops, size_t k, bool *unoptimized, size_t *skip_count) {
    while(!unoptimized[k]) k = (size_t)((be_op_t*)b_vec_get(ops, k))->next;
    return k - skip_count[k];
}

// remove all operations that write to pointers that are never read from or only used to calculate other pointers that are never read from or so on
// all CONDJUMP, OUT and RETURN operations are treated as valuable and never optimized out, even if actually unreachable (e.g. due to unsatisfiable condition in `if`)
be_err_t be_optimize_dead_ends(b_vec_t *ops) {
    be_err_t err; err.err = 0; err.line_start = 0;
    b_vec_t *importants = malloc(sizeof(b_vec_t) * ops->len); // "important" pointers per operation, i.e. those that hold values we care about
    // basically all sources of operation and of all its subsequent operations
    bool *unoptimized = calloc(ops->len, sizeof(bool)); // operations that will be left (not optimized away)
    ssize_t *loops = calloc(ops->len, sizeof(ssize_t)); // -1 in this array will mark operations in unconditional cycles (1 mark per cycle)
    if(importants == NULL || unoptimized == NULL || loops == NULL) {
        free(importants); free(unoptimized); free(loops);
        err.err = BE_ERR_ALLOC_FAIL;
        return err;
    }
    size_t unchecked = 0;
    ssize_t loop_idx = 1;
    while(unchecked < ops->len) {
        size_t k = unchecked;
        for(;;) {
            be_op_t *rawop = b_vec_get(ops, k);
            if(loops[k] == loop_idx) { loops[k] = -1; break; }
            loops[k] = loop_idx;
            while(unchecked < ops->len && loops[unchecked]) unchecked ++;
            if((rawop->op & BE_DST) == 0 || rawop->op == BE_OP_RETURN) break;
            k = (ssize_t)rawop->next;
        }
        loop_idx ++;
    }
    for(size_t i = 0; i < ops->len; i ++) {
        b_vec_alloc(importants + i, sizeof(dtype*), _BE_DEFAULT_VEC);
        if(importants[i].fail) {
            for(size_t j = 0; j < i; j ++) b_vec_free(importants + j);
            free(importants); free(unoptimized);
            err.err = BE_ERR_ALLOC_FAIL;
            return err;
        }
    }
    for(bool updated = true; updated;) {
        updated = false;
        for(size_t i = ops->len; i --> 0;) {
            be_op_t *rawop = b_vec_get(ops, i);
            b_vec_t *imps = importants + i;
            size_t ks[2] = { (size_t)rawop->next, (size_t)rawop->src2 };
            for(int l = 0; l < 1 + (rawop->op == BE_OP_CONDJUMP); l ++) {
                size_t k = ks[l];
                b_vec_t *next_imps = importants + k;
                if(k >= ops->len || rawop->op == BE_OP_RETURN) continue;
                for(size_t j = 0; j < next_imps->len; j ++) {
                    dtype *ptr = *(dtype**)b_vec_get(next_imps, j);
                    if(be_vec_contains_ptr(imps, ptr)) continue;
                    if((rawop->op & BE_DST) != 0 && ptr == rawop->dst) {
                        unoptimized[i] = true;
                    } else {
                        b_vec_push(imps, &ptr);
                        updated = true;
                    }
                }
            }
            if((rawop->op & BE_DST) == 0 || loops[i] == -1) { if(!unoptimized[i]) updated = true; unoptimized[i] = true; }
            if((rawop->op & BE_DST) == 0 || loops[i] == -1 || be_vec_contains_ptr(importants + (size_t)rawop->next, rawop->dst)) {
                if((rawop->op & BE_SRC1) && !be_vec_contains_ptr(imps, rawop->src1)) { b_vec_push(imps, &rawop->src1); updated = true; }
                if((rawop->op & BE_SRC2) && !be_vec_contains_ptr(imps, rawop->src2)) { b_vec_push(imps, &rawop->src2); updated = true; }
                if((rawop->op & BE_SRC3) && !be_vec_contains_ptr(imps, rawop->src3)) { b_vec_push(imps, &rawop->src3); updated = true; }
            }
            if(imps->fail) { err.err = BE_ERR_ALLOC_FAIL; goto ret1; }
        }
    }
    ret1:
    free(loops);
    for(size_t i = 0; i < ops->len; i ++) b_vec_free(importants + i);
    free(importants);
    if(err.err) goto ret2;
    size_t *skip_count = malloc(ops->len * sizeof(size_t));
    if(skip_count == NULL) { err.err = BE_ERR_ALLOC_FAIL; goto ret2; }
    size_t skipped = 0;
    for(size_t i = 0; i < ops->len; i ++) {
        if(!unoptimized[i]) skipped ++;
        skip_count[i] = skipped;
    }
    for(size_t i = 0; i < ops->len; i ++) {
        if(unoptimized[i]) {
            be_op_t *rawop = b_vec_get(ops, i);
            if((size_t)rawop->next < ops->len) rawop->next = (be_op_t*)be_unskipped_descendant(ops, (size_t)rawop->next, unoptimized, skip_count);
            if(rawop->op == BE_OP_CONDJUMP)
                rawop->src2 = (void*)be_unskipped_descendant(ops, (size_t)rawop->src2, unoptimized, skip_count);
            *(be_op_t*)b_vec_get(ops, i - skip_count[i]) = *rawop;
        }
    }
    ops->len -= skip_count[ops->len - 1];
    free(skip_count);
    ret2:
    free(unoptimized);
    return err;
}

bool _be_skip_dead_end_optimization = false; // left for debug purpaces, used in cli

// compile a program
// `*code` is dst for copiled program
// if an error occures, all fields of `*code` are deallocated automatically, but on success `be_code_free` should be called at some pointe after to deallocate allocated fields of `*code`
// `program` is the null-terminated string containing program code encoded in utf-8 or pure ascii
// `varnames` is an array of null-terminated strings indicating names of user-defined variables that will have to be passed into `be_code_eval` on each evaluation
// `n_vars` is the number of user-defined variables. If `n_vars`=0, `varnames` is not read and can be set to NULL or anything
be_err_t be_code_compile(be_code_t *code, const char *program, const char **varnames, size_t n_vars) {
    b_vec_t tokens; b_vec_alloc(&tokens, sizeof(be_token_t), _BE_DEFAULT_VEC);
    be_err_t err = be_tokenize(&tokens, program);
    if(err.err) goto end1;

    b_vec_t _varnames; b_vec_alloc(&_varnames, sizeof(char*), _BE_DEFAULT_VEC <= n_vars ? n_vars + 1 : _BE_DEFAULT_VEC);
    for(size_t i = 0; i < n_vars; i ++) b_vec_push(&_varnames, varnames + i);
    if(_varnames.fail) { b_vec_free(&_varnames); err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto end1; }

    b_vec_alloc(&code->lines, sizeof(be_line_t), _BE_DEFAULT_VEC);
    if(code->lines.fail) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto end2; }
    bool loops[2];
    bool breaks[2];
    err = be_parse_tokens(&code->lines, &tokens, &_varnames, 1, breaks, loops);
    if(err.err) goto end2;

    size_t _n_vars = be_n_vars(&code->lines);
    code->vars = malloc(_n_vars * sizeof(be_var_t));
    if(code->vars == NULL) { err.err = BE_ERR_ALLOC_FAIL; err.line_start = 0; goto end2; }
    
    b_vec_t entering_stack; b_vec_alloc(&entering_stack, sizeof(size_t), _BE_DEFAULT_VEC);
    b_vec_t exiting_stack; b_vec_alloc(&exiting_stack, sizeof(be_exit_t), _BE_DEFAULT_VEC);
    b_vec_t end_jumping_ops; b_vec_alloc(&end_jumping_ops, sizeof(size_t), _BE_DEFAULT_VEC);
    b_vec_alloc(&code->ops, sizeof(be_op_t), _BE_DEFAULT_VEC);
    // err = be_code_bake(code, &entering_stack, &end_jumping_ops);
    err = be_scope_bake(&code->ops, &code->lines, code->vars, &entering_stack, &exiting_stack, &end_jumping_ops, 0);
    b_vec_free(&entering_stack); b_vec_free(&exiting_stack); b_vec_free(&end_jumping_ops);
    if(err.err) goto end3;
    if(!_be_skip_dead_end_optimization) {
        err = be_optimize_dead_ends(&code->ops);
        if(err.err) goto end3;
    }
    be_op_prep(&code->ops);
    code->n_vars = _n_vars < n_vars ? _n_vars : n_vars;
    code->print = NULL;
    end3:
    if(err.err) free(code->vars);
    if(err.err) b_vec_free(&code->ops);
    end2:
    if(err.err) b_vec_free(&code->lines);
    b_vec_free(&_varnames);
    end1:
    b_vec_free(&tokens);
    #if _BE_USE_GCC_LABEL_POINTERS
    if(err.err == 0) {
        code->_be_gnulabel_filler_call = true;
        be_code_eval(code, NULL, 0);
        code->_be_gnulabel_filler_call = false;
    }
    #endif
    return err;
}

// free all allocated fields of `*code`
void be_code_free(be_code_t *code) {
    for(int i = 0; i < code->lines.len; i ++) be_line_free(b_vec_get(&code->lines, i));
    b_vec_free(&code->lines);
    b_vec_free(&code->ops);
    free(code->vars);
}

#endif