/*
** $Id: ldblib.c $
** Interface from SIL to its debug API
** See Copyright Notice in sil.h
*/

#define ldblib_c
#define SIL_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sil.h"

#include "lauxlib.h"
#include "sillib.h"
#include "llimits.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (sil_State *L, sil_State *L1, int n) {
  if (l_unlikely(L != L1 && !sil_checkstack(L1, n)))
    silL_error(L, "stack overflow");
}


static int db_getregistry (sil_State *L) {
  sil_pushvalue(L, SIL_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (sil_State *L) {
  silL_checkany(L, 1);
  if (!sil_getmetatable(L, 1)) {
    sil_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (sil_State *L) {
  int t = sil_type(L, 2);
  silL_argexpected(L, t == SIL_TNIL || t == SIL_TTABLE, 2, "nil or table");
  sil_settop(L, 2);
  sil_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (sil_State *L) {
  int n = (int)silL_optinteger(L, 2, 1);
  if (sil_type(L, 1) != SIL_TUSERDATA)
    silL_pushfail(L);
  else if (sil_getiuservalue(L, 1, n) != SIL_TNONE) {
    sil_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (sil_State *L) {
  int n = (int)silL_optinteger(L, 3, 1);
  silL_checktype(L, 1, SIL_TUSERDATA);
  silL_checkany(L, 2);
  sil_settop(L, 2);
  if (!sil_setiuservalue(L, 1, n))
    silL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static sil_State *getthread (sil_State *L, int *arg) {
  if (sil_isthread(L, 1)) {
    *arg = 1;
    return sil_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'sil_settable', used by 'db_getinfo' to put results
** from 'sil_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (sil_State *L, const char *k, const char *v) {
  sil_pushstring(L, v);
  sil_setfield(L, -2, k);
}

static void settabsi (sil_State *L, const char *k, int v) {
  sil_pushinteger(L, v);
  sil_setfield(L, -2, k);
}

static void settabsb (sil_State *L, const char *k, int v) {
  sil_pushboolean(L, v);
  sil_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'sil_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'sil_getinfo' on top of the result table so that it can call
** 'sil_setfield'.
*/
static void treatstackoption (sil_State *L, sil_State *L1, const char *fname) {
  if (L == L1)
    sil_rotate(L, -2, 1);  /* exchange object and table */
  else
    sil_xmove(L1, L, 1);  /* move object to the "main" stack */
  sil_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'sil_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'sil_getinfo'.
*/
static int db_getinfo (sil_State *L) {
  sil_Debug ar;
  int arg;
  sil_State *L1 = getthread(L, &arg);
  const char *options = silL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  silL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (sil_isfunction(L, arg + 1)) {  /* info about a function? */
    options = sil_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    sil_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    sil_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!sil_getstack(L1, (int)silL_checkinteger(L, arg + 1), &ar)) {
      silL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!sil_getinfo(L1, options, &ar))
    return silL_argerror(L, arg+2, "invalid option");
  sil_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    sil_pushlstring(L, ar.source, ar.srclen);
    sil_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't')) {
    settabsb(L, "istailcall", ar.istailcall);
    settabsi(L, "extraargs", ar.extraargs);
  }
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (sil_State *L) {
  int arg;
  sil_State *L1 = getthread(L, &arg);
  int nvar = (int)silL_checkinteger(L, arg + 2);  /* local-variable index */
  if (sil_isfunction(L, arg + 1)) {  /* function argument? */
    sil_pushvalue(L, arg + 1);  /* push function */
    sil_pushstring(L, sil_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    sil_Debug ar;
    const char *name;
    int level = (int)silL_checkinteger(L, arg + 1);
    if (l_unlikely(!sil_getstack(L1, level, &ar)))  /* out of range? */
      return silL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = sil_getlocal(L1, &ar, nvar);
    if (name) {
      sil_xmove(L1, L, 1);  /* move local value */
      sil_pushstring(L, name);  /* push name */
      sil_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      silL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (sil_State *L) {
  int arg;
  const char *name;
  sil_State *L1 = getthread(L, &arg);
  sil_Debug ar;
  int level = (int)silL_checkinteger(L, arg + 1);
  int nvar = (int)silL_checkinteger(L, arg + 2);
  if (l_unlikely(!sil_getstack(L1, level, &ar)))  /* out of range? */
    return silL_argerror(L, arg+1, "level out of range");
  silL_checkany(L, arg+3);
  sil_settop(L, arg+3);
  checkstack(L, L1, 1);
  sil_xmove(L, L1, 1);
  name = sil_setlocal(L1, &ar, nvar);
  if (name == NULL)
    sil_pop(L1, 1);  /* pop value (if not popped by 'sil_setlocal') */
  sil_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (sil_State *L, int get) {
  const char *name;
  int n = (int)silL_checkinteger(L, 2);  /* upvalue index */
  silL_checktype(L, 1, SIL_TFUNCTION);  /* closure */
  name = get ? sil_getupvalue(L, 1, n) : sil_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  sil_pushstring(L, name);
  sil_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (sil_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (sil_State *L) {
  silL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (sil_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)silL_checkinteger(L, argnup);  /* upvalue index */
  silL_checktype(L, argf, SIL_TFUNCTION);  /* closure */
  id = sil_upvalueid(L, argf, nup);
  if (pnup) {
    silL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (sil_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    sil_pushlightuserdata(L, id);
  else
    silL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (sil_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  silL_argcheck(L, !sil_iscfunction(L, 1), 1, "Sil function expected");
  silL_argcheck(L, !sil_iscfunction(L, 3), 3, "Sil function expected");
  sil_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (sil_State *L, sil_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  sil_getfield(L, SIL_REGISTRYINDEX, HOOKKEY);
  sil_pushthread(L);
  if (sil_rawget(L, -2) == SIL_TFUNCTION) {  /* is there a hook function? */
    sil_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      sil_pushinteger(L, ar->currentline);  /* push current line */
    else sil_pushnil(L);
    sil_assert(sil_getinfo(L, "lS", ar));
    sil_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= SIL_MASKCALL;
  if (strchr(smask, 'r')) mask |= SIL_MASKRET;
  if (strchr(smask, 'l')) mask |= SIL_MASKLINE;
  if (count > 0) mask |= SIL_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & SIL_MASKCALL) smask[i++] = 'c';
  if (mask & SIL_MASKRET) smask[i++] = 'r';
  if (mask & SIL_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (sil_State *L) {
  int arg, mask, count;
  sil_Hook func;
  sil_State *L1 = getthread(L, &arg);
  if (sil_isnoneornil(L, arg+1)) {  /* no hook? */
    sil_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = silL_checkstring(L, arg+2);
    silL_checktype(L, arg+1, SIL_TFUNCTION);
    count = (int)silL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!silL_getsubtable(L, SIL_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    sil_pushliteral(L, "k");
    sil_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    sil_pushvalue(L, -1);
    sil_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  sil_pushthread(L1); sil_xmove(L1, L, 1);  /* key (thread) */
  sil_pushvalue(L, arg + 1);  /* value (hook function) */
  sil_rawset(L, -3);  /* hooktable[L1] = new SIL hook */
  sil_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (sil_State *L) {
  int arg;
  sil_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = sil_gethookmask(L1);
  sil_Hook hook = sil_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    silL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    sil_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    sil_getfield(L, SIL_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    sil_pushthread(L1); sil_xmove(L1, L, 1);
    sil_rawget(L, -2);   /* 1st result = hooktable[L1] */
    sil_remove(L, -2);  /* remove hook table */
  }
  sil_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  sil_pushinteger(L, sil_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (sil_State *L) {
  for (;;) {
    char buffer[250];
    sil_writestringerror("%s", "sil_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (silL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        sil_pcall(L, 0, 0, 0))
      sil_writestringerror("%s\n", silL_tolstring(L, -1, NULL));
    sil_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (sil_State *L) {
  int arg;
  sil_State *L1 = getthread(L, &arg);
  const char *msg = sil_tostring(L, arg + 1);
  if (msg == NULL && !sil_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    sil_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)silL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    silL_traceback(L, L1, msg, level);
  }
  return 1;
}


static const silL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {NULL, NULL}
};


SILMOD_API int silopen_debug (sil_State *L) {
  silL_newlib(L, dblib);
  return 1;
}

