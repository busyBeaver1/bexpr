import os
import time

with open("tests.txt", "r") as f:
    s = f.read()

for test in s.split("~~")[1:]:
    name = test.split("\n")[0].strip()
    # print(name)
    code = test[test.index("\n"): test.index("\n~")]
    for i, call in enumerate(test.split("\n~")[1:]):
        cmd = "echo -e " + repr(code) + " | ./bexpr " + call.split("\n")[0].strip() + " > test_output"
        os.system(cmd)
        with open("test_output", "r") as f:
            output = f.read()
        if output.strip() != call[call.index("\n"):].strip():
            print("~ expected:")
            print(call[call.index("\n"):].strip())
            print("~ got:")
            print(output.strip())
            print("test `" + name + "` failed at call " + str(i))
            exit(0)

print("all tests successful")
