/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in sil.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)  \
	(offsetof(CClosure, upvalue) + sizeof(TValue) * cast_uint(n))

#define sizeLclosure(n)  \
	(offsetof(LClosure, upvals) + sizeof(UpVal *) * cast_uint(n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and SIL). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v.p != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v.p))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(SIL_ERRERR + 1)


SILI_FUNC Proto *silF_newproto (sil_State *L);
SILI_FUNC CClosure *silF_newCclosure (sil_State *L, int nupvals);
SILI_FUNC LClosure *silF_newLclosure (sil_State *L, int nupvals);
SILI_FUNC void silF_initupvals (sil_State *L, LClosure *cl);
SILI_FUNC UpVal *silF_findupval (sil_State *L, StkId level);
SILI_FUNC void silF_newtbcupval (sil_State *L, StkId level);
SILI_FUNC void silF_closeupval (sil_State *L, StkId level);
SILI_FUNC StkId silF_close (sil_State *L, StkId level, TStatus status, int yy);
SILI_FUNC void silF_unlinkupval (UpVal *uv);
SILI_FUNC lu_mem silF_protosize (Proto *p);
SILI_FUNC void silF_freeproto (sil_State *L, Proto *f);
SILI_FUNC const char *silF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
