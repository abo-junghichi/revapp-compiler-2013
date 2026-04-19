#include <stdio.h>
#include <mcheck.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "interp.c"
typedef enum { embed_world, embed_int } embed;
static void embedmemfree(datastack * d, size_t type, embedmem m)
{
    embed e = type;
    switch (e) {
    case embed_world:
	break;
    case embed_int:
	break;
    }
}
static bool findembedmem_core(embed type, size_t length, address_array * a,
			      size_t id, int *m, size_t * addrp,
			      size_t * share)
{
    address ad;
    size_t addr;
    if (combinum > id)
	return true;
    addr = id - combinum;
    ad = a->cont[addr];
    if (address_embedmem + type != ad.type)
	return true;
    assert(length <= sizeof(embedmem));
    memcpy(m, &ad.u.f.u.m, length);
    *addrp = addr;
    *share = ad.u.f.share;
    return false;
}
static size_t alloc_embedmem(address_array * a, embed type, void *src,
			     size_t length)
{
    address c;
    size_t caddr = address_alloc(a);
    c.type = address_embedmem + type;
    c.u.f.share = 0;
    assert(length <= sizeof(embedmem));
    memcpy(&c.u.f.u.m, src, length);
    a->cont[caddr] = c;
    return caddr + combinum;
}
static bool findint(address_array * a, size_t id, int *m)
{
    size_t addr, share;
    return findembedmem_core(embed_int, sizeof(int), a, id, m, &addr,
			     &share);
}
static size_t allocint(address_array * a, int m)
{
    return alloc_embedmem(a, embed_int, &m, sizeof(int));
}
static bool findworld(address_array * a, size_t id, int *m)
{
    size_t addr, share;
    return findembedmem_core(embed_world, 0, a, id, m, &addr, &share);
}
static size_t allocworld(address_array * a)
{
    return alloc_embedmem(a, embed_world, NULL, 0);
}
static bool primitive_startworld(size_t * top, datastack * d,
				 address_array * a, framestack * f)
{
    if (d->used < d->base + 1)
	return true;
    else {
	size_t head = d->used - 1, *dcont = d->cont + head;
	*top = dcont[0];
	dcont[0] = allocworld(a);
	return false;
    }
}
static bool primitive_forkworld(size_t * top, datastack * d,
				address_array * a, framestack * f)
{
    if (d->used < d->base + 2)
	return true;
    else {
	size_t head = d->used - 2, *dcont = d->cont + head, unlink[1];
	int pas;
	if (findworld(a, dcont[0], &pas)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[0];
	*top = dcont[1];
	release(d, a, 1, unlink);
	dcont = d->cont + head;
	dcont[0] = allocworld(a);
	dcont[1] = allocworld(a);
	return false;
    }
}
static bool primitive_joinworld(size_t * top, datastack * d,
				address_array * a, framestack * f)
{
    if (d->used < d->base + 3)
	return true;
    else {
	size_t head = d->used - 3, *dcont = d->cont + head, unlink[2];
	int pas, act;
	if (findworld(a, dcont[1], &act) || findworld(a, dcont[0], &pas)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[0];
	unlink[1] = dcont[1];
	*top = dcont[2];
	d->used -= 2;
	release(d, a, 2, unlink);
	dcont = d->cont + head;
	dcont[0] = allocworld(a);
	return false;
    }
}
static bool constantint(int value, size_t * top, datastack * d,
			address_array * a, framestack * f)
{
    if (d->used < d->base + 1)
	return true;
    else {
	size_t head = d->used - 1, *dcont = d->cont + head;
	*top = dcont[0];
	dcont[0] = allocint(a, value);
	return false;
    }
}
static bool binaryopeint(int (*ope) (int pas, int act), size_t * top,
			 datastack * d, address_array * a, framestack * f)
{
    if (d->used < d->base + 3)
	return true;
    else {
	size_t head = d->used - 3, *dcont = d->cont + head, unlink[2];
	int pas, act;
	if (findint(a, dcont[1], &act) || findint(a, dcont[0], &pas)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[0];
	unlink[1] = dcont[1];
	*top = dcont[2];
	d->used -= 2;
	release(d, a, 2, unlink);
	dcont = d->cont + head;
	dcont[0] = allocint(a, ope(pas, act));
	return false;
    }
}
static bool compareint(bool(*compar) (int target, int standard),
		       size_t * top, datastack * d, address_array * a,
		       framestack * f)
{
    if (d->used < d->base + 4)
	return true;
    else {
	size_t *dcont = d->cont + d->used - 4, unlink[3];
	int target, standard;
	if (findint(a, dcont[1], &standard)
	    || findint(a, dcont[0], &target)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[1];
	unlink[1] = dcont[0];
	if (compar(target, standard)) {
	    *top = dcont[3];
	    unlink[2] = dcont[2];
	} else {
	    *top = dcont[2];
	    unlink[2] = dcont[3];
	}
	d->used -= 4;
	release(d, a, 3, unlink);
	return false;
    }
}
static bool primitive_getc(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    if (d->used < d->base + 2)
	return true;
    else {
	size_t *dcont = d->cont + d->used - 2;
	int gotten = fgetc(stdin);
	*top = dcont[1];
	dcont[1] = allocint(a, gotten);
	return false;
    }
}
static bool prim_putc_core(FILE * stream, size_t * top, datastack * d,
			   address_array * a, framestack * f)
{
    if (d->used < d->base + 3)
	return true;
    else {
	size_t head = d->used - 3, *dcont = d->cont + head, unlink[1];
	int toput;
	if (findint(a, dcont[1], &toput)) {
	    assert(false);
	    return true;
	}
	fputc(toput, stream);
	unlink[0] = dcont[1];
	*top = dcont[2];
	d->used -= 2;
	release(d, a, 1, unlink);
	return false;
    }
}
static bool primitive_putc(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    return prim_putc_core(stdout, top, d, a, f);
}
static bool primitive_errc(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    return prim_putc_core(stderr, top, d, a, f);
}
static bool prim_equal_compar(int tgt, int std)
{
    return tgt == std;
}
static bool primitive_equal(size_t * top, datastack * d, address_array * a,
			    framestack * f)
{
    return compareint(prim_equal_compar, top, d, a, f);
}
static bool prim_eqbig_compar(int tgt, int std)
{
    return tgt >= std;
}
static bool primitive_eqbig(size_t * top, datastack * d, address_array * a,
			    framestack * f)
{
    return compareint(prim_eqbig_compar, top, d, a, f);
}
static bool prim_big_compar(int tgt, int std)
{
    return tgt > std;
}
static bool primitive_big(size_t * top, datastack * d, address_array * a,
			  framestack * f)
{
    return compareint(prim_big_compar, top, d, a, f);
}
static int prim_plus_ope(int pas, int act)
{
    return pas + act;
}
static bool primitive_plus(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    return binaryopeint(prim_plus_ope, top, d, a, f);
}
static int prim_minus_ope(int pas, int act)
{
    return pas - act;
}
static bool primitive_minus(size_t * top, datastack * d, address_array * a,
			    framestack * f)
{
    return binaryopeint(prim_minus_ope, top, d, a, f);
}
static int prim_mul_ope(int pas, int act)
{
    return pas * act;
}
static bool primitive_mul(size_t * top, datastack * d, address_array * a,
			  framestack * f)
{
    return binaryopeint(prim_mul_ope, top, d, a, f);
}
static int prim_shla_ope(int pas, int act)
{
/*** shift left arithmetic ***
Do shift-right with negative act-value.
Emulate sequence of "shift one",
which means returning zero for positive big act-value,
filled with MSB for negative big act-value. */
    unsigned int int_bit = sizeof(int) * CHAR_BIT, shift;
    if (0 <= act) {
	shift = act;
	if (int_bit <= shift)
	    return 0;
	else
	    return pas << shift;
    } else {
	unsigned int sign_bit = 0x1 << (int_bit - 1), rst;
	shift = -act;
	if (int_bit <= shift)
	    shift = int_bit - 1;
	rst =
	    ((((unsigned int) pas) ^ sign_bit) >> shift) -
	    (sign_bit >> shift);
	return rst;
    }
}
static bool primitive_shla(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    return binaryopeint(prim_shla_ope, top, d, a, f);
}
static int prim_bitand_ope(int pas, int act)
{
    return pas & act;
}
static bool primitive_bitand(size_t * top, datastack * d,
			     address_array * a, framestack * f)
{
    return binaryopeint(prim_bitand_ope, top, d, a, f);
}
static bool primitive_divmod(size_t * top, datastack * d,
			     address_array * a, framestack * f)
{
    if (d->used < d->base + 3)
	return true;
    else {
	size_t head = d->used - 3, *dcont = d->cont + head, unlink[2];
	int pas, act;
	if (findint(a, dcont[1], &act) || findint(a, dcont[0], &pas)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[1];
	unlink[1] = dcont[0];
	*top = dcont[2];
	d->used -= 1;
	release(d, a, 2, unlink);
	dcont = d->cont + head;
	dcont[1] = allocint(a, pas / act);
	dcont[0] = allocint(a, pas % act);
	return false;
    }
}
static bool primitive_zero(size_t * top, datastack * d, address_array * a,
			   framestack * f)
{
    return constantint(0, top, d, a, f);
}
static bool primitive_one(size_t * top, datastack * d, address_array * a,
			  framestack * f)
{
    return constantint(1, top, d, a, f);
}
static bool primitive_eof(size_t * top, datastack * d, address_array * a,
			  framestack * f)
{
    return constantint(EOF, top, d, a, f);
}
static bool primitive_undefined(size_t * top, datastack * d,
				address_array * a, framestack * f)
{
    return true;
}
static bool primitive_isspace(size_t * top, datastack * d,
			      address_array * a, framestack * f)
{
    if (d->used < d->base + 3)
	return true;
    else {
	size_t head = d->used - 3, *dcont = d->cont + head, unlink[2];
	int c;
	if (findint(a, dcont[0], &c)) {
	    assert(false);
	    return true;
	}
	unlink[0] = dcont[0];
	if (isspace(c)) {
	    *top = dcont[2];
	    unlink[1] = dcont[1];
	} else {
	    *top = dcont[1];
	    unlink[1] = dcont[2];
	}
	d->used -= 3;
	release(d, a, 2, unlink);
	return false;
    }
}
#include "viarevapp.c"
int main(void)
{
    datastack d = datastack_init();
    address_array a = address_init();
    mtrace();
    run_revapp(&d, &a);
    {
	size_t addrp, share;
	if (d.used != 1
	    || findembedmem_core(embed_world, 0, &a, d.cont[0], NULL,
				 &addrp, &share)) {
	    framestack f;
	    f.room = f.used = 0;
	    f.cont = NULL;
	    if (1)
		checkinterp(~0, d, address_init(), f);
	    assert(false);
	    fprintf(stderr, "normal abort.\n");
	    abort();
	}
    }
    free(d.cont);
    free(a.cont);
    muntrace();
    return 0;
}
