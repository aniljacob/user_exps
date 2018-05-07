CC=gcc

all:cache-parallel

clean:
	rm -f cache-parallel

cache-parallel:
	gcc -DCACHELINE_SIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE) -lpthread -Wall  -g -ocache-parallel cache-parallel.c
