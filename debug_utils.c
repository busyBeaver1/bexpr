#include <stdarg.h>

void log_tokens(b_vec_t *tokens) {
    printf("=== Number of tokens: %zu ===\n", tokens->len);
    for(int i = 0; i < tokens->len; i ++) {
        be_token_t *token = b_vec_get(tokens, i);
        printf("%.*s ", (int)token->len, token->name);
    }
    printf("\n");
}

void log_tlines(b_vec_t *tlines) {
    printf("=== Number of tlines: %zu ===\n", tlines->len);
    for(int i = 0; i < tlines->len; i ++) {
        be_tline_t *line = b_vec_get(tlines, i);
        printf("[");
        for(int j = 0; j < line->len; j ++) {
            printf("%.*s", (int)line->ptr[j].len, line->ptr[j].name);
            if(j < line->len - 1) printf(" ");
        }
        printf("]\n");
    }
}

// void log_parsed_expr(be_expr_t *expr) {
//     printf("Expression:\n");
//     for(int i = 0; i < expr->values.len; i ++) {
//         be_value_t *value = b_vec_get(&expr->values, i);
//         if(value->flags == BE_VT_TOKEN) {
//             be_token_t *token = value->token;
//             printf("token %.*s\n", (int)token->len, token->name);
//         } else {
//             printf("depth: %i; ", value->brackets_depth);
//             if((value->flags & BE_VT) == BE_VT_CONST) printf("const (%g:%g); ", value->a_const, value->b_const);
//             if((value->flags & BE_VT) == BE_VT_VAR) printf("var %zu; ", value->var);
//             printf("operation: %X; functions: ", value->flags & BE_OP);
//             for(int j = 0; j < value->functions.len; j ++) {
//                 be_fn_t *f = b_vec_get(&value->functions, j);
//                 printf("%s%s%X[%i] ", f->fn & BE_ADDINV ? "-" : "", f->fn & BE_MULINV ? "/" : "", f->fn & BE_OP, (int)f->depth);
//             }
//             printf("\n");
//         }
//     }
//     printf("Order:\n");
//     for(int i = 0; i < expr->order.len; i ++) {
//         struct { size_t a, b; } *pair = b_vec_get(&expr->order, i);
//         printf("%zu %zu\n", pair->a, pair->b);
//     }
// }

const char *lt2str(int lt) {
    switch(lt) {
        case BE_LT_EXPR     : return "EXPR"     ;
        case BE_LT_IF       : return "IF"       ;
        case BE_LT_ELIF     : return "ELIF"     ;
        case BE_LT_ELSE     : return "ELSE"     ;
        case BE_LT_SCOPE    : return "SCOPE"    ;
        case BE_LT_ASSIGN   : return "ASSIGN"   ;
        case BE_LT_OUT      : return "OUT"      ;
        case BE_LT_EMPTY_OUT: return "EMPTY_OUT";
        case BE_LT_BREAK    : return "BREAK"    ;
        case BE_LT_LOOP     : return "LOOP"     ;
    }
    return "?";
}

void log_lines(b_vec_t *lines, int depth) {
    if(depth == 1) printf("=== Lines: ===\n");
    for(size_t i = 0; i < lines->len; i ++) {
        be_line_t *line = b_vec_get(lines, i);
        for(size_t k = 0; k < depth - 1; k ++) printf("    ");
        printf("%s", lt2str(line->type));
        if(line->type & BE_EXPR) {
            be_tline_t *texpr = &line->texpr;
            for(size_t j = 0; j < texpr->len; j ++) printf(" %.*s", (int)texpr->ptr[j].len, texpr->ptr[j].name);
        }
        if(line->type & BE_INNER_LINES) printf(":");
        else if(line->type == BE_LT_ASSIGN) printf(" -> var%zu", line->var);
        else if(line->type & BE_DEPTH) printf(" -> depth%i", line->depth);
        if(line->breaks != NULL && line->loops != NULL) {
        printf(" [breaks=");
        for(size_t j = 0; j <= depth; j ++) printf("%i", (int)line->breaks[j]);
        printf(" loops=");
        for(size_t j = 0; j <= depth; j ++) printf("%i", (int)line->loops[j]);
        printf("]\n");
        } else printf("\n");
        if(line->type & BE_INNER_LINES) log_lines(&line->inner_lines, depth + 1);
    }
}

void _iterate_exprs(b_vec_t *lines, b_vec_t *exprs) {
    for(int i = 0; i < lines->len; i ++) {
        be_line_t *line = b_vec_get(lines, i);
        if(line->type & BE_EXPR) b_vec_push(exprs, &line->expr);
        if(line->type & BE_INNER_LINES) _iterate_exprs(&line->inner_lines, exprs);
    }
}

char *b_dupprintf(int overhead, const char *s, ...) {
    char *buf = malloc(strlen(s) + 1 + overhead);
    if(buf == NULL) return buf;
    va_list args;
    va_start(args, s);
    vsprintf(buf, s, args);
    va_end(args);
    return buf;
}

char _be_name[100];
const char *_name_ptr(void *ptr, b_vec_t *known_ptrs, b_vec_t *known_ptr_names) {
    if(known_ptrs == NULL) return "?";
    for(int j = 0; j < known_ptr_names->len; j ++) {
        if(ptr == *(dtype**)b_vec_get(known_ptrs, j))
            return *(char**)b_vec_get(known_ptr_names, j);
    }
    sprintf(_be_name, "%p", ptr);
    return _be_name;
}

const char *op2str(int op) {
    // printf("\n%X\n", op);
    switch(op) {
        case BE_OP_PAIR    : return "PAIR"    ;
        case BE_OP_SWAP    : return "SWAP"    ;
        case BE_OP_OR      : return "OR"      ;
        case BE_OP_AND     : return "AND"     ;
        case BE_OP_EQUALS  : return "EQUALS"  ;
        case BE_OP_NEQUALS : return "NEQUALS" ;
        case BE_OP_GREATER : return "GREATER" ;
        case BE_OP_LESS    : return "LESS"    ;
        case BE_OP_GEQ     : return "GEQ"     ;
        case BE_OP_LEQ     : return "LEQ"     ;
        case BE_OP_ADD     : return "ADD"     ;
        case BE_OP_SUB     : return "SUB"     ;
        case BE_OP_MOD     : return "MOD"     ;
        case BE_OP_MUL     : return "MUL"     ;
        case BE_OP_DIV     : return "DIV"     ;
        case BE_OP_POW     : return "POW"     ;
        case BE_OP_OUT     : return "OUT"     ;
        case BE_OP_RETURN  : return "RETURN"  ;
        case BE_OP_CONDJUMP: return "CONDJUMP";
        case BE_FN_NOT     : return "NOT"     ;
        case BE_FN_SIN     : return "SIN"     ;
        case BE_FN_COS     : return "COS"     ;
        case BE_FN_TAN     : return "TAN"     ;
        case BE_FN_ASIN    : return "ASIN"    ;
        case BE_FN_ACOS    : return "ACOS"    ;
        case BE_FN_ATAN    : return "ATAN"    ;
        case BE_FN_ATAN2   : return "ATAN2"   ;
        case BE_FN_FLOOR   : return "FLOOR"   ;
        case BE_FN_CEIL    : return "CEIL"    ;
        case BE_FN_ROUND   : return "ROUND"   ;
        case BE_FN_LN      : return "LN"      ;
        case BE_FN_EXP     : return "EXP"     ;
        case BE_FN_SQRT    : return "SQRT"    ;
        case BE_FN_CBRT    : return "CBRT"    ;
        case BE_FN_GAMMA   : return "GAMMA"   ;
        case BE_FN_SINH    : return "SINH"    ;
        case BE_FN_COSH    : return "COSH"    ;
        case BE_FN_TANH    : return "TANH"    ;
        case BE_FN_ASINH   : return "ASINH"   ;
        case BE_FN_ACOSH   : return "ACOSH"   ;
        case BE_FN_ATANH   : return "ATANH"   ;
        case BE_FN_ERF     : return "ERF"     ;
        case BE_FN_ERFINV  : return "ERFINV"  ;
        case BE_FN_SIGMOID : return "SIGMOID" ;
        case BE_FN_MAX     : return "MAX"     ;
        case BE_FN_MIN     : return "MIN"     ;
        case BE_FN_NEG     : return "NEG"     ;
        case BE_FN_RECIPR  : return "RECIPR"  ;
        case BE_FN_SIGN    : return "SIGN"    ;
        case BE_FN_ABS     : return "ABS"     ;
        case BE_OP_INTPOW  : return "INTPOW"  ;
        case BE_FN_COPY    : return "COPY"    ;
    }
    return "?";
}

void log_rawop(b_vec_t *ops, be_op_t *rawop, b_vec_t *known_ptrs, b_vec_t *known_ptr_names) {
    if(BE_DST & rawop->op) printf("%s := ", _name_ptr(rawop->dst, known_ptrs, known_ptr_names));
    printf("%s(", op2str(rawop->op & BE_OP));
    if(rawop->op == BE_OP_OUT) printf("\"%s\", ", (char*)rawop->src2);
    int bits[] = { BE_SRC1, BE_SRC2, BE_SRC3 };
    dtype *ptrs[] = { rawop->src1, rawop->src2, rawop->src3 };
    int args_printed = 0;
    for(int k = 0; k < 3; k ++) {
        if((bits[k] & rawop->op) == 0) continue;
        printf("%s%s", args_printed ++ ? ", " : "", _name_ptr(ptrs[k], known_ptrs, known_ptr_names));
    }
    if(rawop->op == BE_OP_CONDJUMP) printf("); goto %zd:%zd", (ssize_t)rawop->next, (ssize_t)rawop->src2);
    else if(rawop->op == BE_OP_RETURN) printf(")");
    else printf("); goto %zd", (size_t)rawop->next);
    // if(importants != NULL) {
    //     printf("; important ptrs: ");
    //     b_vec_t *imps = importants + i;
    //     // printf("%i", imps->len);
    //     for(size_t k = 0; k < imps->len; k ++) {
    //         printf("%s; ", _name_ptr(*(dtype**)b_vec_get(imps, k), &known_ptrs, &known_ptr_names));
    //     }
    //     if((rawop->op & BE_DST) == 0 || be_vec_contains_ptr(imps, rawop->dst)) printf("!!!");
    // }
    printf("\n");
}

// void log_rawops(b_vec_t *ops, b_vec_t *lines, be_var_t *vars, b_vec_t *importants) {
void log_rawops(b_vec_t *ops, b_vec_t *lines, be_var_t *vars) {
    size_t n_vars = be_n_vars(lines);
    b_vec_t known_ptrs; b_vec_alloc(&known_ptrs, sizeof(dtype*), 8);
    b_vec_t known_ptr_names; b_vec_alloc(&known_ptr_names, sizeof(char*), 8);
    for(int i = 0; i < n_vars; i ++) {
        dtype *ptr;
        ptr = &(vars + i)->a; b_vec_push(&known_ptrs, &ptr);
        ptr = &(vars + i)->b; b_vec_push(&known_ptrs, &ptr);
        char *name_a = b_dupprintf(32, "var%i.a", i);
        char *name_b = b_dupprintf(32, "var%i.b", i);
        b_vec_push(&known_ptr_names, &name_a);
        b_vec_push(&known_ptr_names, &name_b);
    }
    b_vec_t exprs; b_vec_alloc(&exprs, sizeof(be_expr_t), 8);
    _iterate_exprs(lines, &exprs);
    if(exprs.fail) goto alloc_fail;
    for(int i = 0; i < exprs.len; i ++) {
        be_expr_t *expr = b_vec_get(&exprs, i);
        for(int j = 0; j < expr->values.len; j ++) {
            dtype *ptr;
            char *name;
            ptr = &((be_value_t*)b_vec_get(&expr->values, j))->a;            b_vec_push(&known_ptrs, &ptr);
            name = b_dupprintf(64, "expr%i-value%i.a", i, j);                b_vec_push(&known_ptr_names, &name);
            ptr = &((be_value_t*)b_vec_get(&expr->values, j))->b;            b_vec_push(&known_ptrs, &ptr);
            name = b_dupprintf(64, "expr%i-value%i.b", i, j);                b_vec_push(&known_ptr_names, &name);
            ptr = &((be_value_t*)b_vec_get(&expr->values, j))->temp;         b_vec_push(&known_ptrs, &ptr);
            name = b_dupprintf(64, "expr%i-value%i.temp", i, j);             b_vec_push(&known_ptr_names, &name);
            ptr = &((be_value_t*)b_vec_get(&expr->values, j))->a_const;      b_vec_push(&known_ptrs, &ptr);
            name = b_dupprintf(64, "expr%i-value%i.a_const=%g", i, j, *ptr); b_vec_push(&known_ptr_names, &name);
            ptr = &((be_value_t*)b_vec_get(&expr->values, j))->b_const;      b_vec_push(&known_ptrs, &ptr);
            name = b_dupprintf(64, "expr%i-value%i.b_const=%g", i, j, *ptr); b_vec_push(&known_ptr_names, &name);
            ptr = (void*)&((be_value_t*)b_vec_get(&expr->values, j))->ip;
            int m = ((be_intpow_t*)ptr)->n + ((be_intpow_t*)ptr)->neg;
            if(0 <= m && m <= _BE_INTPOW_LIMIT) {
                name = malloc(m + 1);
                name[0] = '-';
                for(int i = 0; i < ((be_intpow_t*)ptr)->n; i ++) name[i + ((be_intpow_t*)ptr)->neg] = ((be_intpow_t*)ptr)->choices[i] ? 's' : 'm';
                name[m] = '\0';
                b_vec_push(&known_ptr_names, &name);
                b_vec_push(&known_ptrs, &ptr);
            }
        }
    }
    dtype *ptr = &_be_const_1; b_vec_push(&known_ptrs, &ptr);
    char *const_1 = b_dupprintf(-1, "%s", "1"); b_vec_push(&known_ptr_names, &const_1);
    dtype *_nan_src = &_be_const_nan; b_vec_push(&known_ptrs, &_nan_src);
    char *_nan = b_dupprintf(1, "%s", "NaN"); b_vec_push(&known_ptr_names, &_nan);
    if(known_ptrs.fail || known_ptr_names.fail) goto alloc_fail;
    printf("Raw operations:\n");
    for(int i = 0; i < ops->len; i ++) {
        be_op_t *rawop = b_vec_get(ops, i);
        printf("%i: ", i);
        log_rawop(ops, rawop, &known_ptrs, &known_ptr_names);
    }
    goto no_fail;
        alloc_fail:
        fprintf(stderr, "malloc returned NULL while logging\n");
    no_fail:
    for(int i = 0; i < known_ptr_names.len; i ++) free(*(char**)b_vec_get(&known_ptr_names, i));
    b_vec_free(&exprs);
    b_vec_free(&known_ptr_names);
    b_vec_free(&known_ptrs);
}
