import struct


body = struct.pack("<BBBBI", 
    1, 0, 0, 0, 6) + "hello\0"

body = body * 10000

print repr(body[:50])

import socket
s = socket.socket()
s.connect(("localhost", 4444))
s.sendall(body)

print repr(s.recv(65536))
