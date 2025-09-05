/*
** $Id: ldo.h $
** Stack and Call structure of SIL
** See Copyright Notice in sil.h
*/

#ifndef ldo_h
#define ldo_h


#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/

#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; silD_reallocstack((L), sz_, 0); pos; }
#endif

#define silD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
	  { pre; silD_growstack(L, n, 1); pos; } \
	else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define silD_checkstack(L,n)	silD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,pt)		(cast_charp(pt) - cast_charp(L->stack.p))
#define restorestack(L,n)	cast(StkId, cast_charp(L->stack.p) + (n))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  silD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(SILI_MAXCCALLS)
#define SILI_MAXCCALLS		200
#endif


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (sil_State *L, void *ud);

SILI_FUNC l_noret silD_errerr (sil_State *L);
SILI_FUNC void silD_seterrorobj (sil_State *L, TStatus errcode, StkId oldtop);
SILI_FUNC TStatus silD_protectedparser (sil_State *L, ZIO *z,
                                                  const char *name,
                                                  const char *mode);
SILI_FUNC void silD_hook (sil_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
SILI_FUNC void silD_hookcall (sil_State *L, CallInfo *ci);
SILI_FUNC int silD_pretailcall (sil_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
SILI_FUNC CallInfo *silD_precall (sil_State *L, StkId func, int nResults);
SILI_FUNC void silD_call (sil_State *L, StkId func, int nResults);
SILI_FUNC void silD_callnoyield (sil_State *L, StkId func, int nResults);
SILI_FUNC TStatus silD_closeprotected (sil_State *L, ptrdiff_t level,
                                                     TStatus status);
SILI_FUNC TStatus silD_pcall (sil_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
SILI_FUNC void silD_poscall (sil_State *L, CallInfo *ci, int nres);
SILI_FUNC int silD_reallocstack (sil_State *L, int newsize, int raiseerror);
SILI_FUNC int silD_growstack (sil_State *L, int n, int raiseerror);
SILI_FUNC void silD_shrinkstack (sil_State *L);
SILI_FUNC void silD_inctop (sil_State *L);

SILI_FUNC l_noret silD_throw (sil_State *L, TStatus errcode);
SILI_FUNC l_noret silD_throwbaselevel (sil_State *L, TStatus errcode);
SILI_FUNC TStatus silD_rawrunprotected (sil_State *L, Pfunc f, void *ud);

#endif

