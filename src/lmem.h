/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in sil.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "sil.h"


#define silM_error(L)	silD_throw(L, SIL_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define silM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define silM_checksize(L,n,e)  \
	(silM_testsize(n,e) ? silM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' and that 'int' is not larger than 'size_t'.)
*/
#define silM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_int((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define silM_reallocvchar(L,b,on,n)  \
  cast_charp(silM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define silM_freemem(L, b, s)	silM_free_(L, (b), (s))
#define silM_free(L, b)		silM_free_(L, (b), sizeof(*(b)))
#define silM_freearray(L, b, n)   silM_free_(L, (b), (n)*sizeof(*(b)))

#define silM_new(L,t)		cast(t*, silM_malloc_(L, sizeof(t), 0))
#define silM_newvector(L,n,t)  \
	cast(t*, silM_malloc_(L, cast_sizet(n)*sizeof(t), 0))
#define silM_newvectorchecked(L,n,t) \
  (silM_checksize(L,n,sizeof(t)), silM_newvector(L,n,t))

#define silM_newobject(L,tag,s)	silM_malloc_(L, (s), tag)

#define silM_newblock(L, size)	silM_newvector(L, size, char)

#define silM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, silM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         silM_limitN(limit,t),e)))

#define silM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, silM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define silM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, silM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

SILI_FUNC l_noret silM_toobig (sil_State *L);

/* not to be called directly */
SILI_FUNC void *silM_realloc_ (sil_State *L, void *block, size_t oldsize,
                                                          size_t size);
SILI_FUNC void *silM_saferealloc_ (sil_State *L, void *block, size_t oldsize,
                                                              size_t size);
SILI_FUNC void silM_free_ (sil_State *L, void *block, size_t osize);
SILI_FUNC void *silM_growaux_ (sil_State *L, void *block, int nelems,
                               int *size, unsigned size_elem, int limit,
                               const char *what);
SILI_FUNC void *silM_shrinkvector_ (sil_State *L, void *block, int *nelem,
                                    int final_n, unsigned size_elem);
SILI_FUNC void *silM_malloc_ (sil_State *L, size_t size, int tag);

#endif

