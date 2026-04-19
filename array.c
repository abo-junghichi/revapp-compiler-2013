#ifndef ARRAY_C
#define ARRAY_C
#include <stdlib.h>
typedef struct {
    size_t cur, max;
    void *buf;
} array;
static array arrayinit()
{
    array rtn;
    rtn.cur = rtn.max = 0;
    rtn.buf = NULL;
    return rtn;
}
static void arrayfree(array * a)
{
    free(a->buf);
    *a = arrayinit();
}
static array *array_resize_help(array * a, size_t size, size_t maxnmemb,
				size_t curnmemb)
{
    void *newbuf = realloc(a->buf, maxnmemb * size);
    a->max = maxnmemb;
    a->cur = curnmemb;
    a->buf = newbuf;
    return a;
}
static array *arrayresize(array * a, size_t size, size_t nmemb)
{
    size_t enough = nmemb * 2;
    if (a->max < nmemb || enough < a->max)
	return array_resize_help(a, size, enough, nmemb);
    else {
	a->cur = nmemb;
	return a;
    }
}
static array *arraytruncate(array * a, size_t size, size_t nmemb)
{
    return array_resize_help(a, size, nmemb, nmemb);
}
#define ARRAY_HELPER3(Name,F,T) \
static T Name(F a)\
{\
    T rtn;\
    rtn.cur = a.cur;\
    rtn.max = a.max;\
    rtn.buf = a.buf;\
    return rtn;\
}
#define ARRAY_HELPER2(Array,Size,Rapper,Rappee) \
static Array * Rapper(Array * a, size_t nmemb)\
{\
    return (Array *) Rappee((array *) a, Size, nmemb);\
}
#define ARRAY_HELPER1(Base,Size,Array,T2a,A2t) \
typedef struct {\
    size_t cur, max;\
    Base *buf;\
} Array;\
ARRAY_HELPER3(A2t, array, Array)\
ARRAY_HELPER3(T2a, Array, array)\
static Array Array##_init()\
{\
    return A2t(arrayinit());\
}\
static void Array##_free(Array * a)\
{\
    arrayfree((array *) a);\
}\
ARRAY_HELPER2(Array, Size, Array##_resize, arrayresize)\
ARRAY_HELPER2(Array, Size, Array##_truncate, arraytruncate)\
static Array *Array##_addlast(Array * a, Base memb)\
{\
    size_t size = a->cur;\
    Array##_resize(a, size + 1)->buf[size] = memb;\
    return a;\
}
#define ARRAY(Base,Type) \
ARRAY_HELPER1(Base, sizeof(Base), Type, Type##2array, array2##Type)
#endif				/*  ARRAY_C */
