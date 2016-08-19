/*
 * cache-parallel.c
 *
 * build with:
 * gcc -DCACHELINE_SIZE=$(getconf LEVEL1_DCACHE_LINESIZE) -lpthread -Wall \
 * -g -ocache-parallel cache-parallel.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define likely(expr) __builtin_expect(!!(expr), 1)

#define __cacheline_aligned__ __attribute__((__aligned__(CACHELINE_SIZE)))

#define LOOPS_MAX 2000000000
#define STACK_SIZE 4096

/*
 * From GETTID(2):
 *
 *   Glibc does not provide a wrapper for this system call; call it using
 *   syscall(2).
 *
 */
static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

/* XXX: comment this to see the effect of the cache line bouncing */
#define DISTINCT_CACHE_LINES
struct shared_data_struct {
	unsigned int data1;
#ifdef DISTINCT_CACHE_LINES
	unsigned char pad[CACHELINE_SIZE - sizeof(unsigned int)];
#endif
	unsigned int data2;
};

static struct shared_data_struct shared_data __cacheline_aligned__;

static void dump_schedstats(pid_t pid, pid_t tid)
{
	char buffer[256];
	char filename[64];
	FILE *f;

	snprintf(filename, sizeof(filename),
			"/proc/%d/task/%d/status", pid, tid);
	f = fopen(filename, "r");
	if (unlikely(f == NULL)) {
		perror("could not read scheduler statistics");
		exit(1);
	}
	while (fgets(buffer, sizeof(buffer), f))
		fprintf(stdout, "[%d:%d] %s", pid, tid, buffer);
	fclose(f);
}

static void *inc_first(void *arg)
{
	struct shared_data_struct *sd = (struct shared_data_struct *)arg;
	pid_t pid = getpid(), tid = gettid();
	cpu_set_t cmask;
	register long i;

	/* set affinity */
	CPU_ZERO(&cmask);
	CPU_SET(0, &cmask);
	if (unlikely(sched_setaffinity(pid, sizeof(cmask), &cmask) < 0)) {
		perror("could not set cpu affinity for the child.");
		exit(1);
	}
	/* periodically increment first member of shared struct */
	for (i = 0; i < LOOPS_MAX; i++)
		sd->data1++;
	dump_schedstats(pid, tid);

	return NULL;
}

static void *inc_second(void *arg)
{
	struct shared_data_struct *sd = (struct shared_data_struct *)arg;
	pid_t pid = getpid(), tid = gettid();
	cpu_set_t cmask;
	register long i;

	/* set affinity */
	CPU_ZERO(&cmask);
	CPU_SET(1, &cmask);
	if (unlikely(sched_setaffinity(0, sizeof(cmask), &cmask) < 0)) {
		perror("could not set cpu affinity for current process.");
		exit(1);
	}
	/* periodically increment second member of shared struct */
	for (i = 0; i < LOOPS_MAX; i++)
		sd->data2++;
	dump_schedstats(pid, tid);

	return NULL;
}

int main(int argc, char **argv)
{
	void *child_stack;
	pthread_t child_thr;

	/* allocate memory for other process to execute in */
	if (unlikely((child_stack = malloc(STACK_SIZE)) == NULL)) {
		perror("cannot allocate stack for child");
		exit(1);
	}

	/* create the child */
	if (unlikely(pthread_create(&child_thr, NULL,
					&inc_second, &shared_data) < 0)) {
		perror("pthread_create failed");
		exit(1);
	}
	inc_first((void *)&shared_data);
	pthread_join(child_thr, NULL);

	return 0;
}
