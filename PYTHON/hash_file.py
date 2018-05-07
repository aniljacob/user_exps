#!/usr/bin/python

import sys
import hashlib

filename=""

def usage():
	print("Invalid usage " + sys.argv[0] + " <file to hash>");
	exit()

if len(sys.argv) < 2:
	usage();

filename = sys.argv[1]

sha256_hasher = hashlib.sha256();

with open(filename, 'rb') as hash_file:
	buf = hash_file.read()
	sha256_hasher.update(buf)

print("Hexdigest = " + str(sha256_hasher.hexdigest()))
