#!/usr/bin/python

import os

def find_file(filepath):
	if os.path.isfile(filepath):
		print "hello world"
	else:
	 	print "Not file"

find_file("/usr/bin/python");
find_file("/usr/bin/pyth");

