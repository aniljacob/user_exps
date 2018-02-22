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

/*this program works in 32 bit machine
 * will give a segfault in 64 bit machines
 * */
int main(int argc, char *argv[], char *envp[])
{
	int syscall_nr = 1, exit_status=69;
	Elf32_auxv_t *auxv;

	/*from stack diagram above: *envp = NULL marks end of envp*/
	while(*envp++ != NULL);

	/* auxv->a_type = AT_NULL marks the end of auxv */
	for (auxv = (Elf32_auxv_t *)envp; auxv->a_type != AT_NULL; auxv++){
		if(auxv->a_type == AT_SYSINFO){
			printf("AT_SYSINFO is: type = %d, 0x%x\n", auxv->a_un.a_val, auxv->a_type);
			break;
		}
	}
	asm volatile (
			"mov %0, %%eax;"
			"mov %1, %%ebx;"
			"call *%2"
			:
			:"m"(syscall_nr), "m"(exit_status), "m"(auxv->a_un.a_val)
			:"eax", "ebx"
			);
	return 0;
}
