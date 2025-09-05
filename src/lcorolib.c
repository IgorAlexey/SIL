/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in sil.h
*/

#define lcorolib_c
#define SIL_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "sil.h"

#include "lauxlib.h"
#include "sillib.h"
#include "llimits.h"


static sil_State *getco (sil_State *L) {
  sil_State *co = sil_tothread(L, 1);
  silL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (sil_State *L, sil_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!sil_checkstack(co, narg))) {
    sil_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  sil_xmove(L, co, narg);
  status = sil_resume(co, L, narg, &nres);
  if (l_likely(status == SIL_OK || status == SIL_YIELD)) {
    if (l_unlikely(!sil_checkstack(L, nres + 1))) {
      sil_pop(co, nres);  /* remove results anyway */
      sil_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    sil_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    sil_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int silB_coresume (sil_State *L) {
  sil_State *co = getco(L);
  int r;
  r = auxresume(L, co, sil_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    sil_pushboolean(L, 0);
    sil_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    sil_pushboolean(L, 1);
    sil_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int silB_auxwrap (sil_State *L) {
  sil_State *co = sil_tothread(L, sil_upvalueindex(1));
  int r = auxresume(L, co, sil_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = sil_status(co);
    if (stat != SIL_OK && stat != SIL_YIELD) {  /* error in the coroutine? */
      stat = sil_closethread(co, L);  /* close its tbc variables */
      sil_assert(stat != SIL_OK);
      sil_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != SIL_ERRMEM &&  /* not a memory error and ... */
        sil_type(L, -1) == SIL_TSTRING) {  /* ... error object is a string? */
      silL_where(L, 1);  /* add extra info, if available */
      sil_insert(L, -2);
      sil_concat(L, 2);
    }
    return sil_error(L);  /* propagate error */
  }
  return r;
}


static int silB_cocreate (sil_State *L) {
  sil_State *NL;
  silL_checktype(L, 1, SIL_TFUNCTION);
  NL = sil_newthread(L);
  sil_pushvalue(L, 1);  /* move function to top */
  sil_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int silB_cowrap (sil_State *L) {
  silB_cocreate(L);
  sil_pushcclosure(L, silB_auxwrap, 1);
  return 1;
}


static int silB_yield (sil_State *L) {
  return sil_yield(L, sil_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (sil_State *L, sil_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (sil_status(co)) {
      case SIL_YIELD:
        return COS_YIELD;
      case SIL_OK: {
        sil_Debug ar;
        if (sil_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (sil_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int silB_costatus (sil_State *L) {
  sil_State *co = getco(L);
  sil_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static sil_State *getoptco (sil_State *L) {
  return (sil_isnone(L, 1) ? L : getco(L));
}


static int silB_yieldable (sil_State *L) {
  sil_State *co = getoptco(L);
  sil_pushboolean(L, sil_isyieldable(co));
  return 1;
}


static int silB_corunning (sil_State *L) {
  int ismain = sil_pushthread(L);
  sil_pushboolean(L, ismain);
  return 2;
}


static int silB_close (sil_State *L) {
  sil_State *co = getoptco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = sil_closethread(co, L);
      if (status == SIL_OK) {
        sil_pushboolean(L, 1);
        return 1;
      }
      else {
        sil_pushboolean(L, 0);
        sil_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    case COS_RUN:  /* running coroutine? */
      sil_geti(L, SIL_REGISTRYINDEX, SIL_RIDX_MAINTHREAD);  /* get main */
      if (sil_tothread(L, -1) == co)
        return silL_error(L, "cannot close main thread");
      sil_closethread(co, L);  /* close itself */
      sil_assert(0);  /* previous call does not return */
      return 0;
    default:  /* normal or running coroutine */
      return silL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const silL_Reg co_funcs[] = {
  {"create", silB_cocreate},
  {"resume", silB_coresume},
  {"running", silB_corunning},
  {"status", silB_costatus},
  {"wrap", silB_cowrap},
  {"yield", silB_yield},
  {"isyieldable", silB_yieldable},
  {"close", silB_close},
  {NULL, NULL}
};



SILMOD_API int silopen_coroutine (sil_State *L) {
  silL_newlib(L, co_funcs);
  return 1;
}

