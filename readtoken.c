#ifndef READTOKEN_C
#define READTOKEN_C
#include <stdio.h>
#include <ctype.h>
#include "array.c"
typedef struct {
    FILE *stream;
    size_t cha, line;
    int peek;
} source;
static source *readahead(source * src)
{
    if ('\n' == src->peek) {
	src->cha = 0;
	src->line++;
    } else
	src->cha++;
    src->peek = fgetc(src->stream);
    return src;
}
static source source_init(FILE * stream)
{
    source rtn;
    rtn.stream = stream;
    rtn.cha = rtn.line = 0;
    rtn.peek = fgetc(stream);
    return rtn;
}
ARRAY(char, char_buf);
typedef enum {
    token_ref, token_def, token_open, token_close, token_eof
} token_t;
static char_buf readtoken_stem(source * src)
{
    char_buf rtn = char_buf_init();
    int peek = src->peek;
    while (!(isspace(peek)
	     || '(' == peek || ')' == peek || '=' == peek || EOF == peek)) {
	char_buf_addlast(&rtn, peek);
	peek = readahead(src)->peek;
    }
    char_buf_truncate(&rtn, rtn.cur);
    return rtn;
}
static token_t readtoken(source * src, char_buf * stem)
{
    token_t rtn;
    while (isspace(src->peek))
	readahead(src);
    switch (src->peek) {
    case '(':
	rtn = token_open;
	readahead(src);
	break;
    case ')':
	rtn = token_close;
	readahead(src);
	break;
    case '=':
	rtn = token_def;
	readahead(src);
	*stem = readtoken_stem(src);
	break;
    case EOF:
	rtn = token_eof;
	break;
    default:
	rtn = token_ref;
	*stem = readtoken_stem(src);
	break;
    }
    return rtn;
}
#endif				/* READTOKEN_C */
