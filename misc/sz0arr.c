/*
In C, the members of a struct are always allocated in the order they appear. So, my_pkt->data is a pointer "one past the end" 
of a pkt object. If initialized with

my_pkt = malloc( sizeof( struct pkt ) + 50 );
then my_pkt->data points to the beginning of a 50-byte general-purpose buffer.

Only the final member of a struct is allowed to be defined this way.

To be more compliant with C99, omit the 0 and write char data[];.

Refer http://gcc.gnu.org/onlinedocs/gcc-4.5.0/gcc/Zero-Length.html#Zero-Length for finding what gcc says
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mystruct{
	int i;
	char data[0];
};

int main(int argc, char *argv[])
{
	struct mystruct *mys_p50 = (struct mystruct *)malloc(sizeof(struct mystruct) + 50);

	mys_p50->data[39] = 10;

	printf("mys_p50 (%p) data pointer = %p, mys_p50->data[39] = %d\n", mys_p50, mys_p50->data, mys_p50->data[39]);
	
	return 0;
}
