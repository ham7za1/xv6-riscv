#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main() {
printf("\n--- Starting Twist Test ---\n");
// Ask the kernel for 8192 bytes (exactly 2 pages).
// Because of lazy allocation, NO memory is actually allocated here.
char *p = sbrk(8192);
printf("1. Touching first page...\n");
// This WILL cause a page fault.
// A correct prefetcher will catch this, allocate Page 1, AND prefetch Page 2.
p[0] = 'A';
printf("2. Touching second page...\n");
// If prefetching works, this will NOT cause a page fault, because
// the trap handler already mapped it during the previous step!
p[4096] = 'B';
printf("--- Twist Test Complete ---\n");
printf("Check your console output. You should only see ONE page fault reported for this test.\n\n");
exit(0);
}