import sys

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

    def disable(self):
        self.HEADER = ''
        self.OKBLUE = ''
        self.OKGREEN = ''
        self.WARNING = ''
        self.FAIL = ''
        self.ENDC = ''

colors = bcolors()

if not sys.stderr.isatty():
    colors.disable

from contextlib import contextmanager
@contextmanager
def term_color(attr):
    sys.stderr.write(attr)
    yield
    sys.stderr.write(colors.ENDC)

line = sys.stdin.readline()
while line:
    if line.startswith("!"):
        last = line.split()[-1]
        if line.split()[-1] == 'ok':
            with term_color(bcolors.OKGREEN):
                sys.stderr.write(line)
        elif line.split()[-1] == 'FAILED':
            with term_color(bcolors.FAIL):
                sys.stderr.write(line)
    elif line.startswith("WvTest:") or line.startswith("Testing \""):
        clr = bcolors.FAIL if ("failure" in line and '0 failures' not in line) else bcolors.OKBLUE
        with term_color(clr):
            sys.stderr.write(line)
    else:
        sys.stderr.write(line)
    line = sys.stdin.readline()
