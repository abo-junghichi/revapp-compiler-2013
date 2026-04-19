#include <stdio.h>
#include <mcheck.h>
#include <stdbool.h>
#include "compile.c"
int main()
{
    bool rtn;
    source src = source_init(stdin);
    mtrace();
    rtn = compile(&src, stdout);
    if (rtn)
	fprintf(stderr,
		"compiling revapp failed in line=%zu,char=%zu."
		" wrong bracket?\n", src.line + 1, src.cha);
    muntrace();
    return rtn;
}
