#!/usr/bin/env python
import readline
import os
import hashlib
import sys

assert len(sys.argv) == 3, "need CHANGESET VERSION"
cset = sys.argv[1]
version = sys.argv[2]

os.system("mkdir -p downloads")
os.chdir("downloads")
os.system("git clone ../ nitro-" + version)
os.chdir("nitro-" + version)
os.system("git checkout " + cset)
os.chdir("..")
os.system("tar cf nitro-%s.tar nitro-%s" % (version, version))
os.system("gzip nitro-%s.tar" % version)
hash = hashlib.sha1(open("nitro-%s.tar.gz" % version).read())
open("nitro-%s.tar.gz.sha1" % version, 'wb').write(hash.hexdigest())
