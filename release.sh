#!/usr/bin/env python
import readline
import os
import hashlib
import sys

assert len(sys.argv) == 3, "need CHANGESET VERSION"
cset = sys.argv[1]
version = sys.argv[2]

assert not os.system("mkdir -p downloads")
os.chdir("downloads")
assert not os.system("git clone ../ nitro-" + version)
os.chdir("nitro-" + version)
assert not os.system("git checkout " + cset)
assert not os.system("rm -rf .git")
os.chdir("..")
assert not os.system("tar cf nitro-%s.tar nitro-%s" % (version, version))
assert not os.system("gzip nitro-%s.tar" % version)
hash = hashlib.sha1(open("nitro-%s.tar.gz" % version).read())
open("nitro-%s.tar.gz.sha1" % version, 'wb').write(hash.hexdigest())
