#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int alloc_count = 0;
int alloc_call_count = 0;
int fail_on_alloc = -1;

void __sanitizer_set_report_fd(void *fd);

void stop_asan() {
    FILE *f = fopen("/dev/null", "w");
    __sanitizer_set_report_fd((void*)(uintptr_t)fileno(f));
}

void crash() { stop_asan(); *(volatile int*)NULL = 1; }

int alloc_fail_line;
const char *alloc_fail_file;

void *_malloc(size_t size, int line, const char *file) {
    if((alloc_call_count ++) == fail_on_alloc) {
        alloc_fail_line = line; alloc_fail_file = file;
        return NULL;
    }
    void *ptr = malloc(size);
    if(ptr == NULL) { fprintf(stderr, "oops; an actual malloc fail\n"); crash(); }
    alloc_count ++;
    return ptr;
}

void *_calloc(size_t num, size_t size, int line, const char *file) {
    if(alloc_call_count == fail_on_alloc) {
        alloc_fail_line = line; alloc_fail_file = file;
        return NULL;
    }
    alloc_call_count ++;
    void *ptr = calloc(num, size);
    if(ptr == NULL) { fprintf(stderr, "oops; an actual calloc fail\n"); crash(); }
    alloc_count ++;
    return ptr;
}

void _free(void *ptr) {
    if(ptr != NULL) alloc_count --;
    free(ptr);
}

#define malloc(size) _malloc(size, __LINE__, __FILE__)
#define calloc(num, size) _calloc(num, size, __LINE__, __FILE__)
#define free _free

#define _BE_STACK_LIMIT 3

#include "bexpr.c"

void test(const char *s, const char **varnames, int n_vars, int expected_err) {
    fail_on_alloc = -1;
    alloc_call_count = 0;
    be_code_t code;
    be_err_t err = be_code_compile(&code, s, varnames, n_vars);
    if(err.err != expected_err) {
        printf("%i %i %s\n", err.err, expected_err, be_strerr(&err));
        be_err_t _err = { .err = expected_err, .line_start = 0 };
        fprintf(stderr, "test failed on the following code:\n%s\nExpected error: `%s`, got: `%s`\nAlloc count: %i\n", s, be_strerr(&_err), be_strerr(&err), alloc_count);
        if(!err.err) {
            be_code_free(&code);
            fprintf(stderr, "Alloc count after free: %i\n", alloc_count);
        }
        crash();
    }
    if(alloc_count != 0) {
        fprintf(stderr, "test failed on the following code:\n%s\nAlloc count after free: %i\n", s, alloc_count);
        exit(0);
    }
    int N = alloc_call_count;
    // printf("%i\n", N);
    for(int n = 0; n < N; n ++) {
        // printf("%i\n", n);
        fail_on_alloc = n;
        alloc_call_count = 0;
        be_code_t code;
        be_err_t err = be_code_compile(&code, s, varnames, n_vars);
        if(err.err != expected_err && err.err != BE_ERR_ALLOC_FAIL) {
            be_err_t _err = { .err = expected_err, .line_start = 0 };
            fprintf(stderr, "test failed on the following code when returning NULL at alloc #%i at %s:%i:\n%s\nExpected error: `%s`, got: `%s`\nAlloc count: %i\n",
                    n, alloc_fail_file, alloc_fail_line, s, be_strerr(&_err), be_strerr(&err), alloc_count);
            if(!err.err) {
                be_code_free(&code);
                fprintf(stderr, "Alloc count after free: %i\n", alloc_count);
            }
            crash();
        }
        if(!err.err) {
            double vars[100];
            be_code_eval(&code, vars, 0);
            be_code_eval(&code, vars, 10);
            be_code_free(&code);
            if(alloc_count != 0) {
                fprintf(stderr, "test failed on the following code when returning NULL at alloc #%i at %s:%i:\n%s\nAlloc count after free: %i\n",
                        n, alloc_fail_file, alloc_fail_line, s, alloc_count);
                crash();
            }
        } else if(alloc_count != 0) {
            fprintf(stderr, "test failed on the following code when returning NULL at alloc #%i at %s:%i:\n%s\nAlloc count after compilation with error `%s`: %i\n",
                    n, alloc_fail_file, alloc_fail_line, s, be_strerr(&err), alloc_count);
            exit(0);
        }
    }
}

int main() {
    char s[] = "lorem ipsum etc";
    s[13] = 0xFF; // illegal char in utf-8
    test(s, NULL, 0, BE_ERR_ILL_CHAR);
    test("a := 2; a := a + h + 4; a", NULL, 0, BE_ERR_ILL_TOKEN);
    test("a := 2; if 0 { a := a + h + 4 } else {3} a", NULL, 0, BE_ERR_ILL_TOKEN);
    test("a := 2; if 0 { a := a + 4 } else {3}", NULL, 0, BE_ERR_NO_EXPR);
    test("a := 2; if 0 { a := a + 4 } else {3} }", NULL, 0, BE_ERR_SCOPE_EXTRA_CLOSE);
    test("a := 2; if 0 { a := a + 4 } else {3", NULL, 0, BE_ERR_SCOPE_UNCLOSED);
    test("a := 2; if 0 { a := a * 2 } else {3e400}", NULL, 0, BE_ERR_CONSTANT_RANGE);
    test("a := 2; if 0 { a := (a * 2] } else {3e400}", NULL, 0, BE_ERR_BRACKETS_MISMATCH);
    test("a := 2; if 0 { a := (a * 2 } else {3e400}", NULL, 0, BE_ERR_BRACKET_UNCLOSED);
    test("a := 2; if 0 { a := a * 2) } else {3e400}", NULL, 0, BE_ERR_BRACKET_OVERCLOSED);
    test("a := 2; if 0 { a := a * () } else {3e400}", NULL, 0, BE_ERR_EMPTY_EXPR);
    test("a := 2; fi 0 { a := a * () } else {3e400}", NULL, 0, BE_ERR_SCOPE_START);
    test("a := 2; if { a := a * () } else {3e400}", NULL, 0, BE_ERR_EMPTY_IF);
    test("a := 2; while { a := a * () } else {3e400}", NULL, 0, BE_ERR_EMPTY_WHILE);
    test("a := 2; elif { a := a * () } else {3e400}", NULL, 0, BE_ERR_EMPTY_ELIF);
    test("a := 2; elif 2 * 2 { a := a * () } else {3e400}", NULL, 0, BE_ERR_MISFOLLOWUP);
    test("a := 2; else { a := a * () } else {3e400}", NULL, 0, BE_ERR_MISFOLLOWUP);
    const char *a = "a";
    const char *_a[] = {a};
    test("if a {} else 2 {loop3} 2", _a, 1, BE_ERR_NONEMPTY_ELSE);
    test("if a {} else {break2} 2", _a, 1, BE_ERR_BREAK_DEPTH);
    test("if a {} else {loop3} 2", _a, 1, BE_ERR_LOOP_DEPTH);
    test("if a {1} else { if 0 {} else {2} }", _a, 1, BE_ERR_NO_EXPR);
    test("if a {1} else { if 0 {} else {2} {{{}}} }", _a, 1, BE_ERR_STACK_DEPTH);
    printf("all tests successful\n");
    return 0;
}
