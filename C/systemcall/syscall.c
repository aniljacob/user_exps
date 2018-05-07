#include <stdio.h>
#include <elf.h>

/*
position            content                     size (bytes) + comment
------------------------------------------------------------------------
	stack pointer ->  	[ argc = number of args ]     4
						[ argv[0] (pointer) ]         4   (program name)
						[ argv[1] (pointer) ]         4
						[ argv[..] (pointer) ]        4 * x
						[ argv[n - 1] (pointer) ]     4
						[ argv[n] (pointer) ]         4   (= NULL)

						[ envp[0] (pointer) ]         4
						[ envp[1] (pointer) ]         4
						[ envp[..] (pointer) ]        4
						[ envp[term] (pointer) ]      4   (= NULL)

						[ auxv[0] (Elf32_auxv_t) ]    8
						[ auxv[1] (Elf32_auxv_t) ]    8
						[ auxv[..] (Elf32_auxv_t) ]   8
						[ auxv[term] (Elf32_auxv_t) ] 8   (= AT_NULL vector)

						[ padding ]                   0 - 16

						[ argument ASCIIZ strings ]   >= 0
						[ environment ASCIIZ str. ]   >= 0

						(0xbffffffc)      [ end marker ]                4   (= NULL)

						(0xc0000000)      < bottom of stack >           0   (virtual)
	------------------------------------------------------------------------
*/

/*this program works in 32 bit machine*/
int main(int argc, char *argv[], char *envp[])
{
	int syscall_nr = 60, exit_status=69;
	asm volatile (
			"movq %0, %%rax;"
			"movq %1, %%rdi;"
			"syscall"
			:
			:"m"(syscall_nr), "m"(exit_status)
			:"rax", "rdi"
			);
	return 0;
}
