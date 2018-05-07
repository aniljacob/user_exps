#include <stdio.h>

typedef unsigned char u8;

struct txdesc{
	unsigned int len;
	union{
		u8 cmd;
		struct {
			u8 eop:1;
			u8 ifcs:1;
			u8 ic:1;
			u8 rs:1;
			u8 rsvd2:1;
			u8 dext:1;
			u8 vle:1;
			u8 rsvd1:1;
		};
	};
};

int main(int argc, char *argv[])
{
	struct txdesc txd;

	txd.cmd = 0x3c;
	printf("%x\n", txd.ic);
	printf("%x\n", txd.vle);
	printf("%x\n", txd.cmd);
	txd.rsvd1 = 1;
	printf("%x\n", txd.cmd);

	return 0;
}

