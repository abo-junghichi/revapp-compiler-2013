#ifndef INTERP_C
#define INTERP_C
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
typedef enum {
    address_done, address_todo, address_empty, address_embedmem
} address_t;
typedef struct {
    size_t applen;
    union {
	size_t one /* applen == 0 */ ;
	size_t *cont /* applen >= 1 */ ;
    } u;
} closure;
typedef unsigned char embedmem[sizeof(closure)];
typedef struct {
    size_t share;
    union {
	closure c;
	embedmem m;
    } u;
} using_address;
static size_t size_t_max(void)
{
    return ~0;
}
typedef struct {
    /* skew heap */
    /* terminal is size_t_max(). */
    size_t child[2];
} empty_address;
typedef struct {
    size_t type;
    union {
	empty_address e;
	using_address f;
    } u;
} address;
typedef struct {
    size_t empty /* if nothing, empty == size_t_max() */ ;
    size_t need, room;
    address *cont;
} address_array;
static address_array address_init()
{
    address_array rtn;
    rtn.room = rtn.need = 0;
    rtn.empty = size_t_max();
    rtn.cont = NULL;
    return rtn;
}
static void address_setnode(address * cont, size_t curp, size_t young,
			    size_t elder)
{
    address *cur = &cont[curp];
    empty_address e;
    assert(cur->type == address_empty);
    e.child[0] = young;
    e.child[1] = elder;
    cur->u.e = e;
}
typedef struct {
    bool /* next is */ terminal;
    size_t top, next, top_young, top_elder;
} address_mergenode_step_rtn;
static address_mergenode_step_rtn address_mergenode_step(address * cont,
							 size_t addra,
							 size_t addrb)
{
    address_mergenode_step_rtn rtn;
    if (addra > addrb) {
	rtn.top = addrb;
	rtn.next = addra;
    } else {
	rtn.top = addra;
	rtn.next = addrb;
    }
    if (size_t_max() == rtn.next)
	rtn.terminal = true;
    else {
	rtn.terminal = false;
	rtn.top_young = cont[rtn.top].u.e.child[0];
	rtn.top_elder = cont[rtn.top].u.e.child[1];
    }
    return rtn;
}
static size_t address_mergenode(address * cont, size_t addra, size_t addrb)
{
    size_t rtn;
    address_mergenode_step_rtn local;
    local = address_mergenode_step(cont, addra, addrb);
    rtn = local.top;
    while (!local.terminal) {
	size_t cur = local.top, cur_young = local.top_young;
	local = address_mergenode_step(cont, local.next, local.top_elder);
	address_setnode(cont, cur, local.top, cur_young);
    }
    return rtn;
}
static size_t address_alloc(address_array * a)
{
    size_t i;
    size_t rtn;
    if (size_t_max() == a->empty) {
	size_t oldroom = a->room, newroom;
	address *cont;
	a->empty = oldroom;
	if (0 == oldroom)
	    newroom = 1;
	else
	    newroom = oldroom * 2;
	cont = realloc(a->cont, sizeof(address) * newroom);
	a->cont = cont;
	a->room = newroom;
	for (i = oldroom; i < newroom; i++) {
	    cont[i].type = address_empty;
	    address_setnode(cont, i, i + 1, size_t_max());
	}
	cont[i - 1].u.e.child[0] = size_t_max();
    }
    rtn = a->empty;
    {
	address *cont = a->cont;
	empty_address oldtop = cont[rtn].u.e;
	if (size_t_max() == oldtop.child[1])
	    a->empty = oldtop.child[0];
	else
	    a->empty =
		address_mergenode(cont, oldtop.child[0], oldtop.child[1]);
    }
    if (rtn >= a->need)
	a->need = rtn + 1;
    return rtn;
}
static void address_free(address_array * a, size_t tofree)
{
    size_t i;
    address *cont = a->cont;
    cont[tofree].type = address_empty;
    address_setnode(cont, tofree, size_t_max(), size_t_max());
    if (size_t_max() == a->empty)
	a->empty = tofree;
    else
	a->empty = address_mergenode(cont, a->empty, tofree);
    if (tofree + 1 == a->need) {
	i = tofree;
	while (true) {
	    if (0 >= i)
		break;
	    i--;
	    if (address_empty != cont[i].type) {
		i++;
		break;
	    }
	}
	a->need = i;
    }
    assert(a->room > 0 && a->empty != size_t_max());
    if (a->need * 4 < a->room) {
	size_t tail = size_t_max(), newroom = a->need * 2, empty =
	    a->empty;
	cont = realloc(cont, sizeof(address) * newroom);
	i = newroom;
	while (i > empty) {
	    i--;
	    if (address_empty == cont[i].type) {
		address_setnode(cont, i, tail, size_t_max());
		tail = i;
	    }
	}
	a->room = newroom;
	a->cont = cont;
	if (0 == newroom)
	    a->empty = size_t_max();
    }
}
static bool checksize(size_t need, size_t * room)
{
    if (*room < need || need * 4 < *room) {
	*room = need * 2;
	return true;
    } else
	return false;
}
typedef struct {
    size_t base, used, room, *cont;
} datastack;
static datastack datastack_init()
{
    datastack rtn;
    rtn.base = rtn.used = rtn.room = 0;
    rtn.cont = NULL;
    return rtn;
}
static size_t *dataspace(datastack * d, size_t need)
{
    if (checksize(need, &d->room))
	d->cont = realloc(d->cont, sizeof(size_t) * d->room);
    d->used = need;
    return d->cont;
}
static const size_t combinum;
static void embedmemfree(datastack * d, size_t type, embedmem m);
static void release_core(datastack * d, address_array * a)
{
    /* release addresses which stacked above base of the datastack */
    size_t i;
    size_t base = d->base;
    while (base < d->used) {
	size_t id = d->cont[d->used - 1], addr, type;
	using_address *holder;
	d->used--;
	if (combinum > id)
	    continue;
	addr = id - combinum;
	holder = &a->cont[addr].u.f;
	if (holder->share > 0) {
	    holder->share--;
	    continue;
	}
	type = a->cont[addr].type;
	switch (type) {
	case address_empty:
	    break;
	case address_todo:
	case address_done:
	    {
		closure cl = holder->u.c;
		if (0 < cl.applen) {
		    size_t head = d->used, *dcont =
			dataspace(d, head + cl.applen + 1) + head;
		    for (i = 0; i < cl.applen + 1; i++)
			dcont[i] = cl.u.cont[i];
		    free(cl.u.cont);
		} else {
		    d->cont[d->used] = cl.u.one;
		    assert(d->room > d->used);
		    d->used++;
		}
	    }
	    break;
	case address_embedmem:
	default:
	    embedmemfree(d, type - address_embedmem, holder->u.m);
	    break;
	}
	address_free(a, addr);
    }
}
static void release(datastack * d, address_array * a, size_t len,
		    size_t * cont)
{
    size_t i;
    size_t baseback = d->base, tail = d->used, *dcont;
    d->base = tail;
    dcont = dataspace(d, tail + len) + tail;
    for (i = 0; i < len; i++)
	dcont[i] = cont[i];
    release_core(d, a);
    d->base = baseback;
}
static void retain(address_array * a, size_t id, size_t up)
{
    if (combinum <= id) {
	size_t addr = id - combinum;
	assert(address_empty != a->cont[addr].type);
	a->cont[addr].u.f.share += up;
    }
}
typedef struct {
    size_t addr, base;
} frame;
typedef struct {
    size_t used, room;
    frame *cont;
} framestack;
static frame *expandframe(framestack * f, size_t nmemb)
{
    size_t used = f->used;
    f->used = f->used + nmemb;
    if (checksize(f->used, &f->room))
	f->cont = realloc(f->cont, sizeof(frame) * f->room);
    return f->cont + used;
}
static bool docombi(size_t * top, datastack * d, address_array * a,
		    framestack * f);
static bool checkinterp(size_t top, datastack d, address_array a,
			framestack f)
{
    size_t i, n;
    fprintf(stderr, "<datastack> base=%4zx\n", d.base);
    for (i = 0; i < d.used; i++) {
	int c;
	if (0 == i % 16)
	    fprintf(stderr, "%4zx:", i);
	fprintf(stderr, "%4zx", d.cont[i]);
	if (0 == (i + 1) % 16)
	    c = '\n';
	else
	    c = ' ';
	fputc(c, stderr);
    }
    if (0 == i % 16)
	fprintf(stderr, "%4zx:", i);
    fprintf(stderr, "%4zx(top)\n" "<framestack>\n", top);
    for (i = 0; i < f.used; i++) {
	frame cur = f.cont[i];
	fprintf(stderr, "[addr=%4zx,base=%4zx]\n", cur.addr + combinum,
		cur.base);
    }
    fprintf(stderr, "<address_array>\n");
    for (i = 0; i < a.need; i++) {
	address ad = a.cont[i];
	if (address_empty == ad.type)
	    continue;
	fprintf(stderr, "%4zx:(%4zx) ", i + combinum, ad.u.f.share);
	if (ad.type < address_empty) {
	    closure c = ad.u.f.u.c;
	    char *name[] = { "done", "todo" };
	    bool forcing = false;
	    fprintf(stderr, "%s ", name[ad.type]);
	    if (address_todo == ad.type)
		for (n = 0; n < f.used; n++) {
		    frame cur = f.cont[n];
		    if (i == cur.addr) {
			forcing = true;
			break;
		    }
		}
	    if (forcing)
		fputs("forcing\n", stderr);
	    else if (c.applen > 0) {
		for (n = 0; n < c.applen; n++)
		    fprintf(stderr, "%4zx ", c.u.cont[n]);
		fprintf(stderr, "%4zx\n", c.u.cont[c.applen]);
	    } else
		fprintf(stderr, "%4zx\n", c.u.one);
	} else if (ad.type >= address_embedmem) {
	    fprintf(stderr, "embed[%4zx]", ad.type - address_embedmem);
	    for (n = 0; n < sizeof(embedmem); n++)
		fprintf(stderr, " %02x", ad.u.f.u.m[n]);
	    fputc('\n', stderr);
	} else {
	    assert(address_empty == ad.type);
	    fprintf(stderr, "leaking empty\n");
	}
    }
    fprintf(stderr, "---\n");
    return true;
}
static void interp_retainclosure(address_array * a, closure c, size_t top)
{
    size_t i;
    if (c.applen > 0)
	for (i = 0; i < c.applen; i++)
	    retain(a, c.u.cont[i], 1);
    retain(a, top, 1);
}
static size_t interp(datastack * d, address_array * a, size_t top)
{
    size_t i;
    framestack f;
    f.room = f.used = 0;
    f.cont = NULL;
    while (true) {
	while (true) {
	    size_t addr, type, *share;
	    closure c;
	    assert(checkinterp(top, *d, *a, f));
	    if (top < combinum) {
		if (docombi(&top, d, a, &f))
		    break;
		else
		    continue;
	    }
	    addr = top - combinum;
	    type = a->cont[addr].type;
	    if (type >= address_empty)
		break;
	    c = a->cont[addr].u.f.u.c;
	    if (c.applen > 0) {
		size_t begin = d->used, *dcont =
		    dataspace(d, begin + c.applen) + begin;
		for (i = 0; i < c.applen; i++)
		    dcont[i] = c.u.cont[i];
		top = c.u.cont[c.applen];
	    } else
		top = c.u.one;
	    share = &a->cont[addr].u.f.share;
	    if (0 >= *share) {
		if (c.applen > 0)
		    free(c.u.cont);
		address_free(a, addr);
		continue;
	    }
	    (*share)--;
	    if (address_done == type) {
		interp_retainclosure(a, c, top);
		continue;
	    }
	    assert(address_todo == type);
	    if (c.applen > 0)
		free(c.u.cont);
	    {
		frame *cur = expandframe(&f, 1) + 0;
		cur->addr = addr;
		cur->base = d->base;
		d->base = d->used - c.applen;
	    }
	}
	if (f.used <= 0)
	    break;
	f.used--;
	{
	    frame cur = f.cont[f.used];
	    closure c;
	    size_t addr = cur.addr;
	    c.applen = d->used - d->base;
	    if (c.applen > 0) {
		size_t *dcont = d->cont + d->base;
		c.u.cont = malloc(sizeof(size_t) * (c.applen + 1));
		for (i = 0; i < c.applen; i++)
		    c.u.cont[i] = dcont[i];
		c.u.cont[c.applen] = top;
	    } else
		c.u.one = top;
	    a->cont[addr].type = address_done;
	    a->cont[addr].u.f.u.c = c;
	    interp_retainclosure(a, c, top);
	    d->base = cur.base;
	}
    }
    free(f.cont);
    return top;
}
static size_t address_closure(address_array * a, size_t share, size_t len,
			      size_t * cont)
{
    size_t rtn = address_alloc(a);
    address *ad = &a->cont[rtn];
    assert(len > 0);
    ad->type = address_todo;
    ad->u.f.share = share;
    ad->u.f.u.c.applen = len - 1;
    if (len - 1 > 0) {
	size_t i;
	size_t *ncont = malloc(sizeof(size_t) * len);
	for (i = 0; i < len; i++)
	    ncont[i] = cont[i];
	ad->u.f.u.c.u.cont = ncont;
    } else
	ad->u.f.u.c.u.one = cont[0];
    return rtn + combinum;
}
static bool primitive_nop(size_t * top, datastack * d, address_array * a,
			  framestack * f)
{
    if (d->used < d->base + 1)
	return true;
    else {
	size_t *dcont = d->cont + d->used - 1;
	*top = dcont[0];
	d->used -= 1;
	return false;
    }
}
#endif				/* INTERP_C */
