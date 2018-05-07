#!/usr/bin/python

import sys

if len(sys.argv) < 2:
	print("invalid usage, Aborting")
	exit()

filename=sys.argv[1]
print("filename: " + filename)

with open(filename, "r") as hashfile:
	buf =  hashfile.read()
	print(buf)
