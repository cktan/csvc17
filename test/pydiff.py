import sys

try:
    with open(sys.argv[1]) as f1:
        f1 = eval(f1.read())
except FileNotFoundError:
    sys.exit(f"File not found: {sys.argv[1]}")

try:
    with open(sys.argv[2]) as f2:
        f2 = eval(f2.read())
except FileNotFoundError:
    sys.exit(f"File not found: {sys.argv[2]}")

if f1 != f2:
    sys.exit(1)
