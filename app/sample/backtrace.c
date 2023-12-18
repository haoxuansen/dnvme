#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <errno.h>
#include "libbase.h"

void func_depth3(void)
{
	dump_stack(1);
}

static void func_depth2(void)
{
	func_depth3();
}

void func_depth1(void)
{
	func_depth2();
}

int main(int argc, char *argv[])
{
	func_depth1();
	return 0;
}
