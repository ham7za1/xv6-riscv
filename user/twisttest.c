#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int
main()
{
	printf("\n--- Starting Twist Test ---\n");
	// Ask for 2 pages with lazy sbrk.
	char *mem = sbrklazy(8192);

	printf("1. Touching first page...\n");
	mem[0] = 'A';

	printf("2. Touching second page...\n");
	mem[4096] = 'B';

	printf("--- Twist Test Complete ---\n");
	printf("You should see only one page fault for this test.\n\n");
	exit(0);
}