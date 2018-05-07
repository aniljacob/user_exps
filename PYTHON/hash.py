#!/usr/bin/python

import hashlib
import sys

def usage():
	print("Invalid usage, try : "+ sys.argv[0] + " <data to hash>")
	return

if len(sys.argv) < 2:
	usage()
	exit()	

data = sys.argv[1]

hash_block = hashlib.sha256()
hash_block.update(data)

print(hash_block.hexdigest())
