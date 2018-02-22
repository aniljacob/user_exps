#include <stdio.h>

int main(int argc, char *argv[])
{
	int syscall_nr = 1, exit_status=33;
	asm volatile (
			"mov %0, %%eax;"
			"mov %1, %%ebx;"
			"int $0x80"
			:
			:"m"(syscall_nr), "m"(exit_status)
			:"eax", "ebx"
			);

	return 0;
}
