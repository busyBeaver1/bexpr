# Beaver-expr

This is a relatively fast and light-weight math expression parser and evaluator written in C, with performance comparable and sometimes even better than that of a well-known library [exprtk](https://github.com/ArashPartow/exprtk) while being much more light-weight. Apart from parsing expressions a basic execution flow is supported, namely defining variables and conditional execution with `if`, `elif`, `else` and loops. It is not turing-complete though because arrays are not supported thus making for a hard limit on memory available at runtime. Custom functions are not supported for now.

## API overview

You only need the `bexpr.c` file or `bexpr.h` additionally if you want to use a header. You should either `#include "bexpr.h"` or `#include "bexpr.c"` directly but not both. The `.c` is self-sustained and `.h` is only provided for separate compilation and for including into C++. The basic workflow is as follows:

- Create a bexpr program by defining an instance of `be_code_t` structure and calling `be_code_compile` on it;
- Check the return value of `be_code_compile` for a compilation error (this includes out-of-memory error `BE_ERR_ALLOC_FAIL`). If an error occurred `be_strerr` or `be_strerr_to` can be invoked for a human-readable explanation and further steps shall not be executed (not even `be_code_free`);
- Optionally set the `print` field of the program to a function to be invoked for printing out values during execution flow on `@` lines (if not set nothing will be done on a `@` line);
- Use your program by invoking `be_code_eval` on it;
- Deallocate all allocated fields of the program by calling `be_code_free`.

Here is a basic example:

```C
#include "bexpr.c"
#include <stdio.h>
#include <math.h>

void print(const char *label, double value) {
    if(isnan(value)) printf("%s\n", label);
    else printf("%s %g\n", label, value);
}

int main() {
    be_code_t program;
    const char *varnames[] = {"a", "b"};
    // this demonstrates the output feature (`@`) though a simple "sin(a) + cos(b)" also works
    be_err_t e = be_code_compile(&program, "@ Hello from the bexpr program!; @a =: a; @b =: b; sin(a) + cos(b)", varnames, sizeof(varnames)/sizeof(char*));
    if(e.err) {
        printf("Error: %s\n", be_strerr(&e));
        return 1;
    }
    program.print = print;
    double a = 1, b = 2;
    double vars[] = {a, b};
    double result = be_code_eval(&program, vars, 0);
    printf("sin(a) + cos(b) = %g\n", result);
    be_code_free(&program);
}
```

Output:
```
Hello from the bexpr program!
a = 1
b = 2
sin(a) + cos(b) = 0.425324
```
For further clarification reference comments

When compiling you should link against standard `math.h` library, e.g.
```bash
$ gcc -c -O3 -o bexpr.o bexpr.c
```

There are 6 user-facing definitions: `_BE_USE_GCC_LABEL_POINTERS`, `_BE_OPTIMIZE_POW`, `_BE_INTPOW_LIMIT`, `_BE_UNLIMITED_CASE`, `_BE_STACK_LIMIT`, that can slightly alter bexpr's behavior (for explanation reference comments in `bexpr.c`).

## Syntax

A bexpr program (code) is a sequence of code lines that are executed sequentially. Lines can be of different types with the most important being expression lines. An expression is a mathematical formula made up of variables, constants, operators and functions. An expression evaluates to a numeric value at runtime. Here are examples of expressions: `1`, `1 ? 2 : 3`, `sin 3 + cos 4`, `x % 5 = 0 & x % 2 = 0`. An expression can take part in a line like in assignation `var := <expression>` or after `if` but can also serve as a line on its own. In the latter case when it is encountered the value it evaluates to becomes the return value of the whole program and the program halts. The program must either return a value or execute indefinitely (guaranteed at compile time)

The code can be structured with scopes `{}` optionally preceded by `if <expression>`, `elif <expression>`, `else` or `while <expression>` in which case it's a conditional scope. The global space (not enclosed in curly braces) is called global scope. A scope is also a line of code in it's parent scope but also has inner lines

Lines are separated either explicitly by `;` or implicitly when any line is placed after a scope line. I.e. `a := 2; 3` must be separated with `;` as well as `a := 2; {b := 3}` but `{b := 3} a := 2` (without `;`) is fine. The very last line as well as the last line within a scope may not have `;` at the end. Empty lines are ignored. Newlines have no special meaning other than being a whitespace and terminating comments

1-line comments are supported, starting with `#` (a line of code may precede a comment as well), like in python

Here are all types of lines supported:

- Expression (returning a value)
- Variable definition/assignation. The syntax is `varname := <expression>` or with one of shortcuts `+=`, `-=`, `*=`, `/=`, `%=`, `^=`, `&=`, `|=` in place of `:=` (only for assigning to an existing variable). A shortcut gets expanded as `varname <operation>= <expression>` -> `varname := varname <operation> <expression>`. Variable names may be pretty much anything that gets parsed as a single token except for `;`, `{`, `}` and `#`. Though a safer option is to use letters `a-z`, `A-Z`, numbers `0-9`, underscore `_`, dot `.` and any non-ASCII characters. Those are always parsed as a single token. Variable name may start with a number or even overwrite a numeric literal. For example the program `1 := 2; 1` will return 2.
- Scope (bare/unconditional). The syntax is `{ <inner line 1>; <inner line 2>; ... }`
- `while` scope. The syntax is `while <expression> { <inner line 1>; <inner line 2>; ... }`
- `if` scope. The syntax is `if <expression> { <inner line 1>; <inner line 2>; ... }`
- `elif` scope. The syntax is `elif <expression> { <inner line 1>; <inner line 2>; ... }`. An `elif` line must only appear after an `if`, `elif` or `while` line (optionally with empty lines in between)
- `else` scope. The syntax is `else { <inner line 1>; <inner line 2>; ... }`. An `else` line must only appear after an `if`, `elif` or `while` line (optionally with empty lines in between)
- `break`. The syntax is `break<n>` (no whitespace) with `<n>` being a whole positive number, for example `break1`. `break<n>` exits exactly `n` enclosing scopes and jumps to the scope end
- `loop`. The syntax is `loop<n>` (no whitespace) with `<n>` being a whole positive number, for example `loop1`. `break<n>` exits exactly `n` enclosing scopes and jumps to the scope beginning
- Output. The syntax is either `@ <label> : <expression>` or `@ <label>`. `<label>` is any string not containing `;`, `{`, `}`, `:` or `#`, possibly empty. When such a line is encountered a user-defined function `print` (if it's defined) is called with the first argument being null-terminated label and the second being the value of the expression or NaN in case of the variant with no expression.

A note on keyword vs variable name collision: Variable name can be `if`, `elif`, `else`, `@`, `break<n>` or `loop<n>`. `if`, `else` and `elif` keywords are detected when finding `}` at the end of the line so there is no conflict with variables. When encountering `@` at the start of a line or `break<n>` or `loop<n>` as the single token of a line it is parsed as usual and not as an expression containing a variable even if one with such name exists. To change that one can wrap it in parentheses.

Variables are scope-specific. Defining a variable within a scope will make it inaccessible from outside. For example `{ a := 2 } a` will produce a compilation error, namely `BE_ERR_ILL_LAST_TOKEN`. However variables can be assigned to from within scopes. For example `a := 1; { a := 2 } a` will return 2.

Conditional scopes should be chained as follows. First goes an `if` or a `while`. Then an arbitrary number of `elif`-s and optionally an `else` at the end. A conditional scope when executed checks the value of it's expression (except for `else` which does not have an expression) and executes if that value is anything but `0`. A subsequent scope in a chain is executed if and **only** if the previous scope's condition has evaluated to `0` (not on a `break` from the previous scope). `while` as the name suggests reexecutes itself while the condition is not `0`.

Both `loop<n>` and `break<n>` lines move the program counter (i.e. where the execution is currently happening) out of exactly `n` nested scopes first placing it before the n-th scope and the latter placing it after. For example
```
{
    # <- execution will jump here from loop2
    if 1 {{
        loop2
    }}
}
0
```

```
{
    if 1 {{
        break2
    }}
    # <- execution will jump here from break2
}
0
```
`break1` has no other meaning than preventing subsequent lines in a scope from executing. `break<n>` within less than `n` nested scopes (not counting global) is not allowed. `loop<n>` within `n-1` scopes brings the program counter to the beginning of the entire program. `loop<n>` within less than `n-1` scopes is not allowed.

An `if` scope having `loop1` as the last line behaves exactly like a `while` scope. Btw that's how it's realized internally.

---

When checking that a program always returns a value all-branch checks for `if-elif-else` chains are performed, e.g. `if a {1} elif b {2} else {3}` is a valid program (if `a` and `b` are defined from outside) because a value is returned in each branch. The compiler also does constant folding but does not fold `if` and `elif` on constants. Thus it does not know that in `if 1 {2}` the condition is always satisfied, so this program will not compile. Presumably you don't call `if` on constants in normal usage anyways. If execution never reaches the end due to a proven infinite loop there may not be a returning expression, e.g. `loop1` and `{ if 1 {loop3} else {loop2} }` are valid programs both hanging indefinitely.

## Expression syntax

All values in expressions are floating-point numbers, namely of type `double`. Logical values are represented by `0` for `false` and `1` for `true`. When checking a value as logical anything but `0` is treated as `true`.

When parsing an expression each token is first checked against the list of known variable names thus a variable can overwrite the meaning of any operator or function of even a parenthesis.

Constants (numeric literals) are parsed by `strtod` C function which defines the format, including `nan`, `inf` (case-insensitive) and scientific notation. In addition to that `pi` and `e` (case-insensitive) literals are supported meaning the area of the unit circle and the Euler's number respectively.

Grouping parentheses `()` and `[]` are supported. Both have the same meaning but should be paired accordingly, e.g. `(1]` is invalid. Implicit multiplication is supported, i.e. writing 2 values adjacent to each other multiplies them as if they were separated by `*`, e.g. `2 (-12) = -24`. A number of 1-argument (unary) and 2-argument functions is supported. Technically 2-argument functions are also unary but acting on a pair of values which should be constructed with the syntax `(value1, value2)`. Functions are parsed almost exactly like unary operators with a small difference in precedence (see table below). This means a function can be called without parentheses, e.g. `sin 1` instead of `sin(1)`. Here is a comprehensive list of operators with precedence (higher means applied first):

| Operator                           | Precedence                                     | Description                                                                                                 |
| ---------------------------------- | ---------------------------------------------  | ----------------------------------------------------------------------------------------------------------- |
| `^` (binary)                       | 9                                              | exponentiation                                                                                              |
| `+`, `*` (unary)                   | 8                                              | do nothing                                                                                                  |
| `/` (unary)                        | 8                                              | reciprocal, i.e. `1/x`                                                                                      |
| `-` (unary)                        | 8                                              | negation                                                                                                    |
| `!` (unary)                        | 8                                              | logical NOT                                                                                                 |
| all functions (as unary operators) | 10 when directly* followed by `(`, 8 otherwise | conditional precedence makes `sin 2^2` parsed as `sin(2^2)` and `sin(2)^2` as `(sin(2))^2`                  |
| `/` (binary)                       | 7                                              | division                                                                                                    |
| `*` (binary)                       | 7                                              | multiplication                                                                                              |
| `%` (binary)                       | 6                                              | remainder (same behavior for negative numbers as in python)                                                 |
| `-` (binary)                       | 5                                              | subtraction                                                                                                 |
| `+` (binary)                       | 5                                              | addition                                                                                                    |
| `<`, `>`, `<=`, `>=` (binary)      | 4                                              | comparisons                                                                                                 |
| `=` (not `==`), `!=` (binary)      | 3                                              | comparisons, for `NaN` act like in C, e.g. `NaN != NaN` is `1`                                              |
| `&` (binary)                       | 2                                              | logical AND                                                                                                 |
| `\|` (binary)                      | 1                                              | logical OR                                                                                                  |
| `?` (binary)                       | 0                                              | swaps the values in the right operand (as pair) when left operand is `0`, otherwise returns right unchanged |
| `:`, `,` (binary)                  | 0                                              | same meaning, create a pair of 2 values                                                                     |

*to be clear, "directly followed" allows whitespaces, e.g. `sin (2)^2` is same thing as `sin(2)^2`.

`+`, `-` and `*`, `/` are left-associative and operations at all other precedence levels are right-associative.

### Pair architecture and ternary operator

What I explain in this subsection is not necessary for the regular use. You can apply ternary operator and 2-argument functions as usual without thinking about this.

Actually all values and variables are not single `double` values but pairs of those. All usual arithmetic and logical operations are applied to pairs elementwise. When a single values is needed, namely when returning, when outputting with `@`, when checking `if` and `elif` conditions and the left operand of `?`, the first element of the pair is used disregarding the second, thus rendering the second one effectless almost always. When parsing a numerical constant the first element of the pair is filled with it's value and the second is always filled with `0`.

Second elements of pairs come into play when using `?` and `:` (equivalently `,`) operators. Let's denote pairs with `(first,second)` syntax. Here is comprehensive list of examples for how this works:

- `(1,2) : (3,4)` = `(1,2), (3,4)` = `(1,3)`
- `(1,0) ? (2,3)` = `(2,3)`
- `(0,0) ? (2,3)` = `(3,2)`
- `(1,0) ? (2,3) : (4,5)` = `(1,0) ? (2,4)` = `(2,4)`
- `(0,0) ? (2,3) : (4,5)` = `(0,0) ? (2,4)` = `(4,2)`

When these operations are applied with the regular ternary operator syntax and second pair elements are ignored elsewhere this makes for exactly the regular ternary operator behavior. But it also enables things like `x := b:c; a ? x` which is equivalent to `a ? b : c`.

All 1-argument functions are applied to pairs elementwise. 2-argument functions act on pairs as on their 2 arguments writing the primary output to the first value and something else to the second value, namely:
- `min` writes maximum of its arguments to the second value of the output pair;
- `max` writes minimum of its arguments to the second value of the output pair;
- `atan2` writes the result of `atan2` on arguments swapped to the second value of the output pair, i.e. `atan2(a,b)` = `(atan2(a,b),atan2(b,a))`.

To avoid a lot of unnecessary operations, dead code elimination algorithm is used, usually stripping away all operations on second pair elements.

---

Here is the list of all functions supported:

`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` (function of 2 arguments), `floor`, `ceil`, `round`, `ln` (natural logarithm), `log` (also natural logarithm, just different alias), `exp`, `sqrt`, `cbrt` (cube root), `gamma` (gamma function, gamma(n) = (n-1)!), `sinh` (hyperbolic sin), `cosh`, `tanh`, `asinh` (inverse hyperbolic sin), `acosh`, `atanh`, `erf` (`erf(x) = 2/sqrt(pi) int_0^x e^(-t^2) dt`), `erfinv` (inverse function for `erf`, `erfinv(-1) = -inf`, `erfinv(1) = inf`, `erf(x) = nan` for `x` not in `[-1, 1]`), `sigmoid` (`sigmoid(x) = 1 / (1 + exp(-x))`), `max`, `min`, `sign` (returns `nan` on `nan`), `abs`.

## Examples

Calculating normal distribution density from `sigma`, `mu` and `x`:
```
/(sigma sqrt(2 pi)) * exp(-(x - mu)^2 / (2 sigma^2))
```

Searching and counting all prime numbers up to 100:
```
N := 100;
@ Printing and counting all prime numbers up to: N;
n := 0; # prime counter
p := 2;
while p <= N {
    divisor := 2;
    r := sqrt p;
    while divisor <= r {
        if p % divisor = 0 {break2}
        divisor += 1
    } else { # `else` executes when the condition for `while` is not satisfied but not on `break2`
        @ prime: p;
        n += 1
    }
    p += 1
}
@ Overall prime fraction: n / N;
n
```

Estimating golden ratio from the Fibonacci sequence:
```
x := 1;
y := 1;
while x < 1e100 {
    x := x + y;
    y := x + y;
}
y / x
```

## CLI

The `bexpr_cli.c` file can be compiled into a simple cli tool that can be used to run and debug bexpr. It reads program from stdin until EOF and either runs it or outputs a bunch of internal info about compilation process. Examples:

```bash
$ ./bexpr -h
Usage: ./bexpr [<var1>=<value1>] [<var2>=<value2>] ... (run program from stdin)
       ./bexpr c [<var1>] [<var2>] ... (print compilation result from stdin)
$ echo "1 + 2 + x^1.5" | ./bexpr x=2
5.82842712474619
$ echo "1 + 2 + x^1.5" | ./bexpr c x
=== Number of tokens: 7 ===
1 + 2 + x ^ 1.5 
=== Number of tlines: 1 ===
[1 + 2 + x ^ 1.5]
=== Lines: ===
EXPR 1 + 2 + x ^ 1.5 [breaks=10 loops=00]
=== Number of vars: 1 ===
Raw operations:
0: expr0-value2.temp := SQRT(var0.a); goto 1
1: expr0-value2.a := MUL(var0.a, expr0-value2.temp); goto 2
2: expr0-value2.b := COPY(1); goto 3
3: expr0-value0.a := ADD(expr0-value0.a, expr0-value2.a); goto 4
4: expr0-value0.b := ADD(expr0-value0.b, expr0-value2.b); goto 5
5: RETURN(expr0-value0.a)
=== After dead-end elimination: ===
Raw operations:
0: expr0-value2.temp := SQRT(var0.a); goto 1
1: expr0-value2.a := MUL(var0.a, expr0-value2.temp); goto 2
2: expr0-value0.a := ADD(expr0-value0.a, expr0-value2.a); goto 3
3: RETURN(expr0-value0.a)
```

Compilation:
```bash
gcc -O3 -lm bexpr_cli.c -o bexpr
```

---
The example above also demonstrates const power optimization: instead of calling `POW(x, 1.5)` it does `temp := SQRT(x); temp * x` (pseudo-code). Such optimizations are performed for powers of: 0, 1/4, 1/3, 1/2, 2/3, 1.5 and negations of those and all integer powers that can be done in no more than 8 (`_BE_INTPOW_LIMIT`) multiplications. Note that the optimization may create a small difference from a direct `pow` call, e.g. `pow(0.33333, 3) = 0.03703592593703701` (in C) but in bexpr `0.33333^3 = 0.037035925937037` (optimized). For small powers optimized version is more accurate but for large powers it is less accurate than `pow`. The optimizations can be disabled by `#define _BE_OPTIMIZE_POW 0` before `bexpr.c` or via compiler flag `-D_BE_OPTIMIZE_POW=0`.

## Performance

Here I mostly do a performance comparison with exprtk. I was initially inspired to create this project after trying out exprtk and seeing how just a basic compilation spans a several minecraft cpvp duels in time (and they're shipping it as single header wtf) producing ginormous file of several MB in size (specifically `exprtk_bridge.cpp` used in this benchmark compiles to 17.2 MB with `g++ -O3 -c exprtk_bridge.cpp -o exprtk_bridge.o` on my machine). Now `gcc -lm -c -O3 -o bexpr.o bexpr.c` takes ~1.8 seconds and produces 79 KB, or ~0.2 seconds and 60 KB without `-O3`.

For my benchmarks I use [`bench_expr_all.txt` file](https://github.com/ArashPartow/math-parser-benchmark-project/blob/master/bench_expr_all.txt) from [math-parser-benchmark-project](https://github.com/arashpartow/math-parser-benchmark-project). Did not bother to add my parser to their benchmark though that'd be cool.

Specs:
- CPU: Xeon E3 1230 v2
- RAM: ddr3 at 1600 mhz (probably in 2 channel)
- disk: Kingston SA400S37480G, SATA 3.2, 6.0 Gb/s (current: 3.0 Gb/s)
- gcc version: 16.1.1 20260430
- clang version: 22.1.3
- OS: Arch

Bexpr settings:

| definition                 | value |
| -------------------------- | ----- |
| _BE_USE_GCC_LABEL_POINTERS | 1     |
| _BE_OPTIMIZE_POW           | 1     |
| _BE_INTPOW_LIMIT           | 8     |
| _BE_UNLIMITED_CASE         | 1     |

All tests performed with `-O3` compiler flag.

### Compilation benchmark

This test measures the time it takes to compile an expression string and evaluate it just once. Basically black box eating a string and variables and spitting out a value. Each of 210 expressions from `bench_expr_all.txt` is (compiled and evaluated) 500 times with random variable values.

Results:

| parser        | total time | per test      |
| ------------- | ---------- | ------------- |
| bexpr, gcc    | 0.751442 s | 7.15659 &mu;s |
| bexpr, clang  | 0.793852 s | 7.56049 &mu;s |
| exprtk, gcc   | 13.793 s   | 131.362 &mu;s |
| exprtk, clang | 13.2247 s  | 125.949 &mu;s |

The difference between gcc and clang is negligible. On this test bexpr performs ~17 times faster than exprtk.

### Evaluation benchmark

This test measures evaluation time for a precompiled expression. Each of 210 expressions from `bench_expr_all.txt` is compiled once and evaluated 300000 times.

Results:

| parser        | total time | per test   |
| ------------- | ---------- | ---------- |
| bexpr, gcc    | 1.76691 s  | 28.0462 ns |
| bexpr, clang  | 1.88762 s  | 29.9623 ns |
| exprtk, gcc   | 1.31951 s  | 20.9446 ns |
| exprtk, clang | 1.40015 s  | 22.2246 ns |

The difference between gcc and clang is again small. On this test exprtk performs ~1.34 times faster than bexpr.

### Busy loop benchmark

This test measures performance of running a busy loop written within the expression syntax. I chose to use Fibonacci sequence algorithm with the modification of normalizing numbers not to get overflow. The test does 2*10^8 iterations of Fibonacci sequence and exits. Additionally I compare this to pure C implementation and python.

Bexpr code:
```
i := 0;
x := 1;
y := 1;
while i < 2e8 {
    x += y;
    y += x;
    if x > 1e8 { x *= 1e-8; y *= 1e-8 }
    i += 1
}
y / x
```

Exprtk code:
```
var x := 1;
var y := 1;
for(var i := 0; i < 2e8; i += 1) {
    x += y;
    y += x;
    if(x > 1e8) { x *= 1e-8; y *= 1e-8; }
};
y / x
```

C code:
```C
double x = 1, y = 1;
for(size_t i = 0; i < 2e8; i ++) {
    x += y;
    y += x;
    if(x > 1e8) { x *= 1e-8; y *= 1e-8; }
}
return y / x;
```

Python code:
```python
from time import time
def main():
    t = time()
    x = 1.
    y = 1.
    for i in range(int(2e8)):
        x += y
        y += x
        if x > 1e8:
            x *= 1e-8
            y *= 1e-8
    print(time() - t)
    print(y / x)
main()
```

Results:

| parser        | total time | per iteration |
| ------------- | ---------- | ------------- |
| bexpr, gcc    | 2.62243 s  | 13.11215 ns   |
| bexpr, clang  | 6.72462 s  | 33.6231 ns    |
| exprtk, gcc   | 3.40481 s  | 17.02405 ns   |
| exprtk, clang | 3.55375 s  | 17.76875 ns   |
| C, gcc        | 0.415883 s | 2.079415 ns   |
| C, clang      | 0.591416 s | 2.95708 ns    |
| python        | 19.18705 s | 95.93526 ns   |

Here bexpr works much faster with gcc than with clang for some reason, not sure why. With gcc, bexpr works 1.3 times faster than exprtk, but with clang it's 1.9 times slower than exprtk.

---
A known problem is that the pointers that bexpr operates on during evaluation end up being sparse in memory, often even coming from different malloc calls. This may promote cache misses for large programs, but for small ones it's fine. A fix could be to update all pointers to point to a single contiguous buffer after compilation, though not implemented yet

### How to run benchmark

First, get [`exprtk.hpp`](https://github.com/ArashPartow/exprtk/blob/master/exprtk.hpp) and [`bench_expr_all.txt`](https://github.com/ArashPartow/math-parser-benchmark-project/blob/master/bench_expr_all.txt) files. Then compile and run:
```bash
$ g++ -O3 -c exprtk_bridge.cpp -o exprtk_bridge.o
$ gcc -O3 -c bexpr.c -o bexpr.o
$ gcc -O3 benchmarks.c -lm -lstdc++ bexpr.o exprtk_bridge.o -o benchmarks
$ ./benchmarks
```
same thing with clang

## AI disclosure

AI was used for consulting, especially during optimization, but not directly for writing code. Everything is written by hand (not even AI autocompletion), except for `exprtk_bridge.cpp` (only used for benchmark) which is written almost entirely by AI.
