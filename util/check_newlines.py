import sys
import re
import os

fn = sys.argv[1]

data = open(fn).read()

newname = fn + '.tmp'
with open(newname, 'wb') as rw:
    rw.write(re.sub(r"\n\n\n", "\n\n", data, re.MULTILINE))

os.rename(newname, fn)
