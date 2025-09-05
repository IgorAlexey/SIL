/*
** $Id: ldo.c $
** Stack and Call structure of SIL
** See Copyright Notice in sil.h
*/

#define ldo_c
#define SIL_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "sil.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > SIL_YIELD)


/*
** these macros allow user-specific actions when a thread is
** resumed/yielded.
*/
#if !defined(sili_userstateresume)
#define sili_userstateresume(L,n)	((void)L)
#endif

#if !defined(sili_userstateyield)
#define sili_userstateyield(L,n)	((void)L)
#endif


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** SILI_THROW/SILI_TRY define how SIL does exception handling. By
** default, SIL handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(SILI_THROW)				/* { */

#if defined(__cplusplus) && !defined(SIL_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define SILI_THROW(L,c)		throw(c)
#define SILI_TRY(L,c,f,ud) \
    try { (f)(L, ud); } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define sili_jmpbuf		int  /* dummy field */

#elif defined(SIL_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define SILI_THROW(L,c)		_longjmp((c)->b, 1)
#define SILI_TRY(L,c,f,ud)	if (_setjmp((c)->b) == 0) ((f)(L, ud))
#define sili_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define SILI_THROW(L,c)		longjmp((c)->b, 1)
#define SILI_TRY(L,c,f,ud)	if (setjmp((c)->b) == 0) ((f)(L, ud))
#define sili_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct sil_longjmp {
  struct sil_longjmp *previous;
  sili_jmpbuf b;
  volatile TStatus status;  /* error code */
};


void silD_seterrorobj (sil_State *L, TStatus errcode, StkId oldtop) {
  if (errcode == SIL_ERRMEM) {  /* memory error? */
    setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
  }
  else {
    sil_assert(errorstatus(errcode));  /* must be a real error */
    sil_assert(!ttisnil(s2v(L->top.p - 1)));  /* with a non-nil object */
    setobjs2s(L, oldtop, L->top.p - 1);  /* move it to 'oldtop' */
  }
  L->top.p = oldtop + 1;  /* top goes back to old top plus error object */
}


l_noret silD_throw (sil_State *L, TStatus errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    SILI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    sil_State *mainth = mainthread(g);
    errcode = silE_resetthread(L, errcode);  /* close all upvalues */
    L->status = errcode;
    if (mainth->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, mainth->top.p++, L->top.p - 1);  /* copy error obj. */
      silD_throw(mainth, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        sil_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


l_noret silD_throwbaselevel (sil_State *L, TStatus errcode) {
  if (L->errorJmp) {
    /* unroll error entries up to the first level */
    while (L->errorJmp->previous != NULL)
      L->errorJmp = L->errorJmp->previous;
  }
  silD_throw(L, errcode);
}


TStatus silD_rawrunprotected (sil_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct sil_longjmp lj;
  lj.status = SIL_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  SILI_TRY(L, &lj, f, ud);  /* call 'f' catching errors */
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/

/* some stack space for error handling */
#define STACKERRSPACE	200


/* maximum stack size that respects size_t */
#define MAXSTACK_BYSIZET  ((MAX_SIZET / sizeof(StackValue)) - STACKERRSPACE)

/*
** Minimum between SILI_MAXSTACK and MAXSTACK_BYSIZET
** (Maximum size for the stack must respect size_t.)
*/
#define MAXSTACK	cast_int(SILI_MAXSTACK < MAXSTACK_BYSIZET  \
			        ? SILI_MAXSTACK : MAXSTACK_BYSIZET)


/* stack size with extra space for error handling */
#define ERRORSTACKSIZE	(MAXSTACK + STACKERRSPACE)


/* raise an error while running the message handler */
l_noret silD_errerr (sil_State *L) {
  TString *msg = silS_newliteral(L, "error in error handling");
  setsvalue2s(L, L->top.p, msg);
  L->top.p++;  /* assume EXTRA_STACK */
  silD_throw(L, SIL_ERRERR);
}


/*
** In ISO C, any pointer use after the pointer has been deallocated is
** undefined behavior. So, before a stack reallocation, all pointers
** should be changed to offsets, and after the reallocation they should
** be changed back to pointers. As during the reallocation the pointers
** are invalid, the reallocation cannot run emergency collections.
** Alternatively, we can use the old address after the deallocation.
** That is not strict ISO C, but seems to work fine everywhere.
** The following macro chooses how strict is the code.
*/
#if !defined(SILI_STRICT_ADDRESS)
#define SILI_STRICT_ADDRESS	1
#endif

#if SILI_STRICT_ADDRESS
/*
** Change all pointers to the stack into offsets.
*/
static void relstack (sil_State *L) {
  CallInfo *ci;
  UpVal *up;
  L->top.offset = savestack(L, L->top.p);
  L->tbclist.offset = savestack(L, L->tbclist.p);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.offset = savestack(L, uplevel(up));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.offset = savestack(L, ci->top.p);
    ci->func.offset = savestack(L, ci->func.p);
  }
}


/*
** Change back all offsets into pointers.
*/
static void correctstack (sil_State *L, StkId oldstack) {
  CallInfo *ci;
  UpVal *up;
  UNUSED(oldstack);
  L->top.p = restorestack(L, L->top.offset);
  L->tbclist.p = restorestack(L, L->tbclist.offset);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.p = s2v(restorestack(L, up->v.offset));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.p = restorestack(L, ci->top.offset);
    ci->func.p = restorestack(L, ci->func.offset);
    if (isSil(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'silV_execute' */
  }
}

#else
/*
** Assume that it is fine to use an address after its deallocation,
** as long as we do not dereference it.
*/

static void relstack (sil_State *L) { UNUSED(L); }  /* do nothing */


/*
** Correct pointers into 'oldstack' to point into 'L->stack'.
*/
static void correctstack (sil_State *L, StkId oldstack) {
  CallInfo *ci;
  UpVal *up;
  StkId newstack = L->stack.p;
  if (oldstack == newstack)
    return;
  L->top.p = L->top.p - oldstack + newstack;
  L->tbclist.p = L->tbclist.p - oldstack + newstack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.p = s2v(uplevel(up) - oldstack + newstack);
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.p = ci->top.p - oldstack + newstack;
    ci->func.p = ci->func.p - oldstack + newstack;
    if (isSil(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'silV_execute' */
  }
}
#endif


/*
** Reallocate the stack to a new size, correcting all pointers into it.
** In case of allocation error, raise an error or return false according
** to 'raiseerror'.
*/
int silD_reallocstack (sil_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack;
  StkId oldstack = L->stack.p;
  lu_byte oldgcstop = G(L)->gcstopem;
  sil_assert(newsize <= MAXSTACK || newsize == ERRORSTACKSIZE);
  relstack(L);  /* change pointers to offsets */
  G(L)->gcstopem = 1;  /* stop emergency collection */
  newstack = silM_reallocvector(L, oldstack, oldsize + EXTRA_STACK,
                                   newsize + EXTRA_STACK, StackValue);
  G(L)->gcstopem = oldgcstop;  /* restore emergency collection */
  if (l_unlikely(newstack == NULL)) {  /* reallocation failed? */
    correctstack(L, oldstack);  /* change offsets back to pointers */
    if (raiseerror)
      silM_error(L);
    else return 0;  /* do not raise an error */
  }
  L->stack.p = newstack;
  correctstack(L, oldstack);  /* change offsets back to pointers */
  L->stack_last.p = L->stack.p + newsize;
  for (i = oldsize + EXTRA_STACK; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i)); /* erase new segment */
  return 1;
}


/*
** Try to grow the stack by at least 'n' elements. When 'raiseerror'
** is true, raises any error; otherwise, return 0 in case of errors.
*/
int silD_growstack (sil_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    sil_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      silD_errerr(L);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  }
  else if (n < MAXSTACK) {  /* avoids arithmetic overflows */
    int newsize = size + (size >> 1);  /* tentative new size (size * 1.5) */
    int needed = cast_int(L->top.p - L->stack.p) + n;
    if (newsize > MAXSTACK)  /* cannot cross the limit */
      newsize = MAXSTACK;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (l_likely(newsize <= MAXSTACK))
      return silD_reallocstack(L, newsize, raiseerror);
  }
  /* else stack overflow */
  /* add extra size to be able to handle the error message */
  silD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
  if (raiseerror)
    silG_runerror(L, "stack overflow");
  return 0;
}


/*
** Compute how much of the stack is being used, by computing the
** maximum top of all call frames in the stack and the current top.
*/
static int stackinuse (sil_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top.p;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top.p) lim = ci->top.p;
  }
  sil_assert(lim <= L->stack_last.p + EXTRA_STACK);
  res = cast_int(lim - L->stack.p) + 1;  /* part of stack in use */
  if (res < SIL_MINSTACK)
    res = SIL_MINSTACK;  /* ensure a minimum size */
  return res;
}


/*
** If stack size is more than 3 times the current use, reduce that size
** to twice the current use. (So, the final stack size is at most 2/3 the
** previous size, and half of its entries are empty.)
** As a particular case, if stack was handling a stack overflow and now
** it is not, 'max' (limited by MAXSTACK) will be smaller than
** stacksize (equal to ERRORSTACKSIZE in this case), and so the stack
** will be reduced to a "regular" size.
*/
void silD_shrinkstack (sil_State *L) {
  int inuse = stackinuse(L);
  int max = (inuse > MAXSTACK / 3) ? MAXSTACK : inuse * 3;
  /* if thread is currently not handling a stack overflow and its
     size is larger than maximum "reasonable" size, shrink it */
  if (inuse <= MAXSTACK && stacksize(L) > max) {
    int nsize = (inuse > MAXSTACK / 2) ? MAXSTACK : inuse * 2;
    silD_reallocstack(L, nsize, 0);  /* ok if that fails */
  }
  else  /* don't change stack */
    condmovestack(L,(void)0,(void)0);  /* (change only for debugging) */
  silE_shrinkCI(L);  /* shrink CI list */
}


void silD_inctop (sil_State *L) {
  L->top.p++;
  silD_checkstack(L, 1);
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which trigger this
** function, can be changed asynchronously by signals.)
*/
void silD_hook (sil_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  sil_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top.p);  /* preserve original 'top' */
    ptrdiff_t ci_top = savestack(L, ci->top.p);  /* idem for 'ci->top' */
    sil_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    L->transferinfo.ftransfer = ftransfer;
    L->transferinfo.ntransfer = ntransfer;
    if (isSil(ci) && L->top.p < ci->top.p)
      L->top.p = ci->top.p;  /* protect entire activation register */
    silD_checkstack(L, SIL_MINSTACK);  /* ensure minimum stack size */
    if (ci->top.p < L->top.p + SIL_MINSTACK)
      ci->top.p = L->top.p + SIL_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= CIST_HOOKED;
    sil_unlock(L);
    (*hook)(L, &ar);
    sil_lock(L);
    sil_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top.p = restorestack(L, ci_top);
    L->top.p = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


/*
** Executes a call hook for SIL functions. This function is called
** whenever 'hookmask' is not zero, so it checks whether call hooks are
** active.
*/
void silD_hookcall (sil_State *L, CallInfo *ci) {
  L->oldpc = 0;  /* set 'oldpc' for new function */
  if (L->hookmask & SIL_MASKCALL) {  /* is call hook on? */
    int event = (ci->callstatus & CIST_TAIL) ? SIL_HOOKTAILCALL
                                             : SIL_HOOKCALL;
    Proto *p = ci_func(ci)->p;
    ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
    silD_hook(L, event, -1, 1, p->numparams);
    ci->u.l.savedpc--;  /* correct 'pc' */
  }
}


/*
** Executes a return hook for SIL and C functions and sets/corrects
** 'oldpc'. (Note that this correction is needed by the line hook, so it
** is done even when return hooks are off.)
*/
static void rethook (sil_State *L, CallInfo *ci, int nres) {
  if (L->hookmask & SIL_MASKRET) {  /* is return hook on? */
    StkId firstres = L->top.p - nres;  /* index of first result */
    int delta = 0;  /* correction for vararg functions */
    int ftransfer;
    if (isSil(ci)) {
      Proto *p = ci_func(ci)->p;
      if (p->flag & PF_ISVARARG)
        delta = ci->u.l.nextraargs + p->numparams + 1;
    }
    ci->func.p += delta;  /* if vararg, back to virtual 'func' */
    ftransfer = cast_int(firstres - ci->func.p);
    silD_hook(L, SIL_HOOKRET, -1, ftransfer, nres);  /* call it */
    ci->func.p -= delta;
  }
  if (isSil(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);  /* set 'oldpc' */
}


/*
** Check whether 'func' has a '__call' metafield. If so, put it in the
** stack, below original 'func', so that 'silD_precall' can call it.
** Raise an error if there is no '__call' metafield.
** Bits CIST_CCMT in status count how many _call metamethods were
** invoked and how many corresponding extra arguments were pushed.
** (This count will be saved in the 'callstatus' of the call).
**  Raise an error if this counter overflows.
*/
static unsigned tryfuncTM (sil_State *L, StkId func, unsigned status) {
  const TValue *tm;
  StkId p;
  tm = silT_gettmbyobj(L, s2v(func), TM_CALL);
  if (l_unlikely(ttisnil(tm)))  /* no metamethod? */
    silG_callerror(L, s2v(func));
  for (p = L->top.p; p > func; p--)  /* open space for metamethod */
    setobjs2s(L, p, p-1);
  L->top.p++;  /* stack space pre-allocated by the caller */
  setobj2s(L, func, tm);  /* metamethod is the new function to be called */
  if ((status & MAX_CCMT) == MAX_CCMT)  /* is counter full? */
    silG_runerror(L, "'__call' chain too long");
  return status + (1u << CIST_CCMT);  /* increment counter */
}


/* Generic case for 'moveresult' */
l_sinline void genmoveresults (sil_State *L, StkId res, int nres,
                                             int wanted) {
  StkId firstresult = L->top.p - nres;  /* index of first result */
  int i;
  if (nres > wanted)  /* extra results? */
    nres = wanted;  /* don't need them */
  for (i = 0; i < nres; i++)  /* move all results to correct place */
    setobjs2s(L, res + i, firstresult + i);
  for (; i < wanted; i++)  /* complete wanted number of results */
    setnilvalue(s2v(res + i));
  L->top.p = res + wanted;  /* top points after the last result */
}


/*
** Given 'nres' results at 'firstResult', move 'fwanted-1' of them
** to 'res'.  Handle most typical cases (zero results for commands,
** one result for expressions, multiple results for tail calls/single
** parameters) separated. The flag CIST_TBC in 'fwanted', if set,
** forces the switch to go to the default case.
*/
l_sinline void moveresults (sil_State *L, StkId res, int nres,
                                          l_uint32 fwanted) {
  switch (fwanted) {  /* handle typical cases separately */
    case 0 + 1:  /* no values needed */
      L->top.p = res;
      return;
    case 1 + 1:  /* one value needed */
      if (nres == 0)   /* no results? */
        setnilvalue(s2v(res));  /* adjust with nil */
      else  /* at least one result */
        setobjs2s(L, res, L->top.p - nres);  /* move it to proper place */
      L->top.p = res + 1;
      return;
    case SIL_MULTRET + 1:
      genmoveresults(L, res, nres, nres);  /* we want all results */
      break;
    default: {  /* two/more results and/or to-be-closed variables */
      int wanted = get_nresults(fwanted);
      if (fwanted & CIST_TBC) {  /* to-be-closed variables? */
        L->ci->u2.nres = nres;
        L->ci->callstatus |= CIST_CLSRET;  /* in case of yields */
        res = silF_close(L, res, CLOSEKTOP, 1);
        L->ci->callstatus &= ~CIST_CLSRET;
        if (L->hookmask) {  /* if needed, call hook after '__close's */
          ptrdiff_t savedres = savestack(L, res);
          rethook(L, L->ci, nres);
          res = restorestack(L, savedres);  /* hook can move stack */
        }
        if (wanted == SIL_MULTRET)
          wanted = nres;  /* we want all results */
      }
      genmoveresults(L, res, nres, wanted);
      break;
    }
  }
}


/*
** Finishes a function call: calls hook if necessary, moves current
** number of results to proper place, and returns to previous call
** info. If function has to close variables, hook must be called after
** that.
*/
void silD_poscall (sil_State *L, CallInfo *ci, int nres) {
  l_uint32 fwanted = ci->callstatus & (CIST_TBC | CIST_NRESULTS);
  if (l_unlikely(L->hookmask) && !(fwanted & CIST_TBC))
    rethook(L, ci, nres);
  /* move results to proper place */
  moveresults(L, ci->func.p, nres, fwanted);
  /* function cannot be in any of these cases when returning */
  sil_assert(!(ci->callstatus &
        (CIST_HOOKED | CIST_YPCALL | CIST_FIN | CIST_CLSRET)));
  L->ci = ci->previous;  /* back to caller (after closing variables) */
}



#define next_ci(L)  (L->ci->next ? L->ci->next : silE_extendCI(L))


/*
** Allocate and initialize CallInfo structure. At this point, the
** only valid fields in the call status are number of results,
** CIST_C (if it's a C function), and number of extra arguments.
** (All these bit-fields fit in 16-bit values.)
*/
l_sinline CallInfo *prepCallInfo (sil_State *L, StkId func, unsigned status,
                                                StkId top) {
  CallInfo *ci = L->ci = next_ci(L);  /* new frame */
  ci->func.p = func;
  sil_assert((status & ~(CIST_NRESULTS | CIST_C | MAX_CCMT)) == 0);
  ci->callstatus = status;
  ci->top.p = top;
  return ci;
}


/*
** precall for C functions
*/
l_sinline int precallC (sil_State *L, StkId func, unsigned status,
                                            sil_CFunction f) {
  int n;  /* number of returns */
  CallInfo *ci;
  checkstackp(L, SIL_MINSTACK, func);  /* ensure minimum stack size */
  L->ci = ci = prepCallInfo(L, func, status | CIST_C,
                               L->top.p + SIL_MINSTACK);
  sil_assert(ci->top.p <= L->stack_last.p);
  if (l_unlikely(L->hookmask & SIL_MASKCALL)) {
    int narg = cast_int(L->top.p - func) - 1;
    silD_hook(L, SIL_HOOKCALL, -1, 1, narg);
  }
  sil_unlock(L);
  n = (*f)(L);  /* do the actual call */
  sil_lock(L);
  api_checknelems(L, n);
  silD_poscall(L, ci, n);
  return n;
}


/*
** Prepare a function for a tail call, building its call info on top
** of the current call info. 'narg1' is the number of arguments plus 1
** (so that it includes the function itself). Return the number of
** results, if it was a C function, or -1 for a SIL function.
*/
int silD_pretailcall (sil_State *L, CallInfo *ci, StkId func,
                                    int narg1, int delta) {
  unsigned status = SIL_MULTRET + 1;
 retry:
  switch (ttypetag(s2v(func))) {
    case SIL_VCCL:  /* C closure */
      return precallC(L, func, status, clCvalue(s2v(func))->f);
    case SIL_VLCF:  /* light C function */
      return precallC(L, func, status, fvalue(s2v(func)));
    case SIL_VLCL: {  /* SIL function */
      Proto *p = clLvalue(s2v(func))->p;
      int fsize = p->maxstacksize;  /* frame size */
      int nfixparams = p->numparams;
      int i;
      checkstackp(L, fsize - delta, func);
      ci->func.p -= delta;  /* restore 'func' (if vararg) */
      for (i = 0; i < narg1; i++)  /* move down function and arguments */
        setobjs2s(L, ci->func.p + i, func + i);
      func = ci->func.p;  /* moved-down function */
      for (; narg1 <= nfixparams; narg1++)
        setnilvalue(s2v(func + narg1));  /* complete missing arguments */
      ci->top.p = func + 1 + fsize;  /* top for new function */
      sil_assert(ci->top.p <= L->stack_last.p);
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->callstatus |= CIST_TAIL;
      L->top.p = func + narg1;  /* set top */
      return -1;
    }
    default: {  /* not a function */
      checkstackp(L, 1, func);  /* space for metamethod */
      status = tryfuncTM(L, func, status);  /* try '__call' metamethod */
      narg1++;
      goto retry;  /* try again */
    }
  }
}


/*
** Prepares the call to a function (C or SIL). For C functions, also do
** the call. The function to be called is at '*func'.  The arguments
** are on the stack, right after the function.  Returns the CallInfo
** to be executed, if it was a SIL function. Otherwise (a C function)
** returns NULL, with all the results on the stack, starting at the
** original function position.
*/
CallInfo *silD_precall (sil_State *L, StkId func, int nresults) {
  unsigned status = cast_uint(nresults + 1);
  sil_assert(status <= MAXRESULTS + 1);
 retry:
  switch (ttypetag(s2v(func))) {
    case SIL_VCCL:  /* C closure */
      precallC(L, func, status, clCvalue(s2v(func))->f);
      return NULL;
    case SIL_VLCF:  /* light C function */
      precallC(L, func, status, fvalue(s2v(func)));
      return NULL;
    case SIL_VLCL: {  /* SIL function */
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top.p - func) - 1;  /* number of real arguments */
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;  /* frame size */
      checkstackp(L, fsize, func);
      L->ci = ci = prepCallInfo(L, func, status, func + 1 + fsize);
      ci->u.l.savedpc = p->code;  /* starting point */
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top.p++));  /* complete missing arguments */
      sil_assert(ci->top.p <= L->stack_last.p);
      return ci;
    }
    default: {  /* not a function */
      checkstackp(L, 1, func);  /* space for metamethod */
      status = tryfuncTM(L, func, status);  /* try '__call' metamethod */
      goto retry;  /* try again with metamethod */
    }
  }
}


/*
** Call a function (C or SIL) through C. 'inc' can be 1 (increment
** number of recursive invocations in the C stack) or nyci (the same
** plus increment number of non-yieldable calls).
** This function can be called with some use of EXTRA_STACK, so it should
** check the stack before doing anything else. 'silD_precall' already
** does that.
*/
l_sinline void ccall (sil_State *L, StkId func, int nResults, l_uint32 inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (l_unlikely(getCcalls(L) >= SILI_MAXCCALLS)) {
    checkstackp(L, 0, func);  /* free any use of EXTRA_STACK */
    silE_checkcstack(L);
  }
  if ((ci = silD_precall(L, func, nResults)) != NULL) {  /* SIL function? */
    ci->callstatus |= CIST_FRESH;  /* mark that it is a "fresh" execute */
    silV_execute(L, ci);  /* call it */
  }
  L->nCcalls -= inc;
}


/*
** External interface for 'ccall'
*/
void silD_call (sil_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}


/*
** Similar to 'silD_call', but does not allow yields during the call.
*/
void silD_callnoyield (sil_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}


/*
** Finish the job of 'sil_pcallk' after it was interrupted by an yield.
** (The caller, 'finishCcall', does the final call to 'adjustresults'.)
** The main job is to complete the 'silD_pcall' called by 'sil_pcallk'.
** If a '__close' method yields here, eventually control will be back
** to 'finishCcall' (when that '__close' method finally returns) and
** 'finishpcallk' will run again and close any still pending '__close'
** methods. Similarly, if a '__close' method errs, 'precover' calls
** 'unroll' which calls ''finishCcall' and we are back here again, to
** close any pending '__close' methods.
** Note that, up to the call to 'silF_close', the corresponding
** 'CallInfo' is not modified, so that this repeated run works like the
** first one (except that it has at least one less '__close' to do). In
** particular, field CIST_RECST preserves the error status across these
** multiple runs, changing only if there is a new error.
*/
static TStatus finishpcallk (sil_State *L,  CallInfo *ci) {
  TStatus status = getcistrecst(ci);  /* get original status */
  if (l_likely(status == SIL_OK))  /* no error? */
    status = SIL_YIELD;  /* was interrupted by an yield */
  else {  /* error */
    StkId func = restorestack(L, ci->u2.funcidx);
    L->allowhook = getoah(ci);  /* restore 'allowhook' */
    func = silF_close(L, func, status, 1);  /* can yield or raise an error */
    silD_seterrorobj(L, status, func);
    silD_shrinkstack(L);   /* restore stack size in case of overflow */
    setcistrecst(ci, SIL_OK);  /* clear original status */
  }
  ci->callstatus &= ~CIST_YPCALL;
  L->errfunc = ci->u.c.old_errfunc;
  /* if it is here, there were errors or yields; unlike 'sil_pcallk',
     do not change status */
  return status;
}


/*
** Completes the execution of a C function interrupted by an yield.
** The interruption must have happened while the function was either
** closing its tbc variables in 'moveresults' or executing
** 'sil_callk'/'sil_pcallk'. In the first case, it just redoes
** 'silD_poscall'. In the second case, the call to 'finishpcallk'
** finishes the interrupted execution of 'sil_pcallk'.  After that, it
** calls the continuation of the interrupted function and finally it
** completes the job of the 'silD_call' that called the function.  In
** the call to 'adjustresults', we do not know the number of results
** of the function called by 'sil_callk'/'sil_pcallk', so we are
** conservative and use SIL_MULTRET (always adjust).
*/
static void finishCcall (sil_State *L, CallInfo *ci) {
  int n;  /* actual number of results from C function */
  if (ci->callstatus & CIST_CLSRET) {  /* was closing TBC variable? */
    sil_assert(ci->callstatus & CIST_TBC);
    n = ci->u2.nres;  /* just redo 'silD_poscall' */
    /* don't need to reset CIST_CLSRET, as it will be set again anyway */
  }
  else {
    TStatus status = SIL_YIELD;  /* default if there were no errors */
    sil_KFunction kf = ci->u.c.k;  /* continuation function */
    /* must have a continuation and must be able to call it */
    sil_assert(kf != NULL && yieldable(L));
    if (ci->callstatus & CIST_YPCALL)   /* was inside a 'sil_pcallk'? */
      status = finishpcallk(L, ci);  /* finish it */
    adjustresults(L, SIL_MULTRET);  /* finish 'sil_callk' */
    sil_unlock(L);
    n = (*kf)(L, APIstatus(status), ci->u.c.ctx);  /* call continuation */
    sil_lock(L);
    api_checknelems(L, n);
  }
  silD_poscall(L, ci, n);  /* finish 'silD_call' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop).
*/
static void unroll (sil_State *L, void *ud) {
  CallInfo *ci;
  UNUSED(ud);
  while ((ci = L->ci) != &L->base_ci) {  /* something in the stack */
    if (!isSil(ci))  /* C function? */
      finishCcall(L, ci);  /* complete its execution */
    else {  /* SIL function */
      silV_finishOp(L);  /* finish interrupted instruction */
      silV_execute(L, ci);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (sil_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Signal an error in the call to 'sil_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (sil_State *L, const char *msg, int narg) {
  api_checkpop(L, narg);
  L->top.p -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top.p, silS_new(L, msg));  /* push error message */
  api_incr_top(L);
  sil_unlock(L);
  return SIL_ERRRUN;
}


/*
** Do the work for 'sil_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (sil_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top.p - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == SIL_OK)  /* starting a coroutine? */
    ccall(L, firstArg - 1, SIL_MULTRET, 0);  /* just call its body */
  else {  /* resuming from previous yield */
    sil_assert(L->status == SIL_YIELD);
    L->status = SIL_OK;  /* mark that it is running (again) */
    if (isSil(ci)) {  /* yielded inside a hook? */
      /* undo increment made by 'silG_traceexec': instruction was not
         executed yet */
      sil_assert(ci->callstatus & CIST_HOOKYIELD);
      ci->u.l.savedpc--;
      L->top.p = firstArg;  /* discard arguments */
      silV_execute(L, ci);  /* just continue running SIL code */
    }
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        sil_unlock(L);
        n = (*ci->u.c.k)(L, SIL_YIELD, ci->u.c.ctx); /* call continuation */
        sil_lock(L);
        api_checknelems(L, n);
      }
      silD_poscall(L, ci, n);  /* finish 'silD_call' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}


/*
** Unrolls a coroutine in protected mode while there are recoverable
** errors, that is, errors inside a protected call. (Any error
** interrupts 'unroll', and this loop protects it again so it can
** continue.) Stops with a normal end (status == SIL_OK), an yield
** (status == SIL_YIELD), or an unprotected error ('findpcall' doesn't
** find a recover point).
*/
static TStatus precover (sil_State *L, TStatus status) {
  CallInfo *ci;
  while (errorstatus(status) && (ci = findpcall(L)) != NULL) {
    L->ci = ci;  /* go down to recovery functions */
    setcistrecst(ci, status);  /* status to finish 'pcall' */
    status = silD_rawrunprotected(L, unroll, NULL);
  }
  return status;
}


SIL_API int sil_resume (sil_State *L, sil_State *from, int nargs,
                                      int *nresults) {
  TStatus status;
  sil_lock(L);
  if (L->status == SIL_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top.p - (L->ci->func.p + 1) == nargs)  /* no function? */
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != SIL_YIELD)  /* ended with errors? */
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  if (getCcalls(L) >= SILI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  L->nCcalls++;
  sili_userstateresume(L, nargs);
  api_checkpop(L, (L->status == SIL_OK) ? nargs + 1 : nargs);
  status = silD_rawrunprotected(L, resume, &nargs);
   /* continue running after recoverable errors */
  status = precover(L, status);
  if (l_likely(!errorstatus(status)))
    sil_assert(status == L->status);  /* normal end or yield */
  else {  /* unrecoverable error */
    L->status = status;  /* mark thread as 'dead' */
    silD_seterrorobj(L, status, L->top.p);  /* push error message */
    L->ci->top.p = L->top.p;
  }
  *nresults = (status == SIL_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top.p - (L->ci->func.p + 1));
  sil_unlock(L);
  return APIstatus(status);
}


SIL_API int sil_isyieldable (sil_State *L) {
  return yieldable(L);
}


SIL_API int sil_yieldk (sil_State *L, int nresults, sil_KContext ctx,
                        sil_KFunction k) {
  CallInfo *ci;
  sili_userstateyield(L, nresults);
  sil_lock(L);
  ci = L->ci;
  api_checkpop(L, nresults);
  if (l_unlikely(!yieldable(L))) {
    if (L != mainthread(G(L)))
      silG_runerror(L, "attempt to yield across a C-call boundary");
    else
      silG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = SIL_YIELD;
  ci->u2.nyield = nresults;  /* save number of results */
  if (isSil(ci)) {  /* inside a hook? */
    sil_assert(!isSilcode(ci));
    api_check(L, nresults == 0, "hooks cannot yield values");
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    silD_throw(L, SIL_YIELD);
  }
  sil_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  sil_unlock(L);
  return 0;  /* return to 'silD_hook' */
}


/*
** Auxiliary structure to call 'silF_close' in protected mode.
*/
struct CloseP {
  StkId level;
  TStatus status;
};


/*
** Auxiliary function to call 'silF_close' in protected mode.
*/
static void closepaux (sil_State *L, void *ud) {
  struct CloseP *pcl = cast(struct CloseP *, ud);
  silF_close(L, pcl->level, pcl->status, 0);
}


/*
** Calls 'silF_close' in protected mode. Return the original status
** or, in case of errors, the new status.
*/
TStatus silD_closeprotected (sil_State *L, ptrdiff_t level, TStatus status) {
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  for (;;) {  /* keep closing upvalues until no more errors */
    struct CloseP pcl;
    pcl.level = restorestack(L, level); pcl.status = status;
    status = silD_rawrunprotected(L, &closepaux, &pcl);
    if (l_likely(status == SIL_OK))  /* no more errors? */
      return pcl.status;
    else {  /* an error occurred; restore saved state and repeat */
      L->ci = old_ci;
      L->allowhook = old_allowhooks;
    }
  }
}


/*
** Call the C function 'func' in protected mode, restoring basic
** thread information ('allowhook', etc.) and in particular
** its stack level in case of errors.
*/
TStatus silD_pcall (sil_State *L, Pfunc func, void *u, ptrdiff_t old_top,
                                  ptrdiff_t ef) {
  TStatus status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = silD_rawrunprotected(L, func, u);
  if (l_unlikely(status != SIL_OK)) {  /* an error occurred? */
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = silD_closeprotected(L, old_top, status);
    silD_seterrorobj(L, status, restorestack(L, old_top));
    silD_shrinkstack(L);   /* restore stack size in case of overflow */
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


static void checkmode (sil_State *L, const char *mode, const char *x) {
  if (strchr(mode, x[0]) == NULL) {
    silO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    silD_throw(L, SIL_ERRSYNTAX);
  }
}


static void f_parser (sil_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  const char *mode = p->mode ? p->mode : "bt";
  int c = zgetc(p->z);  /* read first character */
  if (c == SIL_SIGNATURE[0]) {
    int fixed = 0;
    if (strchr(mode, 'B') != NULL)
      fixed = 1;
    else
      checkmode(L, mode, "binary");
    cl = silU_undump(L, p->z, p->name, fixed);
  }
  else {
    checkmode(L, mode, "text");
    cl = silY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  sil_assert(cl->nupvalues == cl->p->sizeupvalues);
  silF_initupvals(L, cl);
}


TStatus silD_protectedparser (sil_State *L, ZIO *z, const char *name,
                                            const char *mode) {
  struct SParser p;
  TStatus status;
  incnny(L);  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  silZ_initbuffer(L, &p.buff);
  status = silD_pcall(L, f_parser, &p, savestack(L, L->top.p), L->errfunc);
  silZ_freebuffer(L, &p.buff);
  silM_freearray(L, p.dyd.actvar.arr, cast_sizet(p.dyd.actvar.size));
  silM_freearray(L, p.dyd.gt.arr, cast_sizet(p.dyd.gt.size));
  silM_freearray(L, p.dyd.label.arr, cast_sizet(p.dyd.label.size));
  decnny(L);
  return status;
}


