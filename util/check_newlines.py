import sys
old = None

line = sys.stdin.readline()
while line:
    num = int(line.split(':')[1])
    if old and num - old == 1:
        sys.stdout.write("Double Blank:" + line)
    old = num
    line = sys.stdin.readline()
