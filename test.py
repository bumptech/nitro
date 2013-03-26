import struct
import time


body = struct.pack("<BBBBI", 
    1, 0, 0, 0, 6) + "hello\0"

body = body * 10000

want = len(body)

#print repr(body[:50])

import socket
s = socket.socket()
s.connect(("localhost", 4444))

t = time.time()
s.sendall(body)

r = 0
while r < want:
    some = s.recv(65536)
    r += len(some)
print time.time() - t
