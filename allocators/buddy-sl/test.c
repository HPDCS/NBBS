#include "buddy.h"
#include <stdio.h>

static void *
test_alloc(int sz) {
	void * r = bd_xx_alloc(sz);
	printf("alloc %p (sz= %d)\n",r,sz);
	return r;
}

static void
test_free(void * addr) {
	printf("free %p\n",addr);
	bd_xx_free(addr);
}


int
main() {
	buddy_dump();
	void * m1 = test_alloc(4);
	void * m2 = test_alloc(9);
	void * m3 = test_alloc(3);
	void * m4 = test_alloc(7);
	test_free(m3);
	test_free(m1);
	test_free(m4);
	test_free(m2);

	void * m5 = test_alloc(32);
	test_free(m5);

	void * m6 = test_alloc(0);
	test_free(m6);

	return 0;
}
