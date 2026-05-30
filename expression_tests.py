from math import *
import subprocess
from random import random, seed
import os

old_sqrt = sqrt

def sqrt(x):
    if x >= 0: return old_sqrt(x)
    return float("nan")

def get_varnames(expression):
    varnames = set()
    while True:
        try:
            eval(expression.replace("^", "**"), locals={v: float("nan") for v in varnames})
        except NameError as e:
            varnames.add(e.name)
            continue
        break
    return varnames

seed(2)

def bexpr_eval(expression, vars):
    program = f'''
    _i := 0;
    result := 0;
    while _i < 2 {{
        if _i {{@:result}}
        result := {expression};
        _i := _i + 1
    }}
    result
    '''
    # print(repr(program))
    r = subprocess.run(
        ["./bexpr"] + [f"{v}={vars[v]}" for v in vars], 
        input=program,
        capture_output=True, 
        text=True,
        check=True
    )
    # print(repr(r.stdout))
    r1, r2 = map(lambda s : float("nan") if s == "" else float(s), r.stdout.splitlines())
    assert str(r1) == str(r2)
    return r1

# print(get_varnames("sin(a * b)"))
def test(expression):
    print(expression)
    varnames = get_varnames(expression)
    vars = {v: random() for v in varnames}
    print(vars)
    result_python = float(eval(expression.replace("^", "**"), locals=vars))
    print("python:", result_python, end="", flush=True)
    result_bexpr = bexpr_eval(expression, vars)
    assert str(result_bexpr) == str(bexpr_eval(expression, vars))
    print("; bexpr:", result_bexpr)
    if result_python != result_bexpr:
        print("!inequality")
    if isnan(result_python): assert isnan(result_bexpr)
    elif result_python: assert abs(result_python - result_bexpr) / abs(result_python) < 1e-14
    else: assert result_python == result_bexpr

for fname in sorted(os.listdir("expression_tests")):
    print(fname)
    with open("expression_tests/" + fname, "r") as f:
        s = f.read()
    
    for expression in s.splitlines():
        expression = expression.strip()
        if expression.startswith("#") or expression == "": continue
        test(expression)
