#include <unistd.h>
#include "libbase.h"

int main(int argc, char *argv[])
{
	int size;

	size = getpagesize();
	pr_debug("PAGE SIZE: 0x%x, %u\n", size, size);

	return 0;
}
