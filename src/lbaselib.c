/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in sil.h
*/

#define lbaselib_c
#define SIL_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sil.h"

#include "lauxlib.h"
#include "sillib.h"
#include "llimits.h"


static int silB_print (sil_State *L) {
  int n = sil_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = silL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      sil_writestring("\t", 1);  /* add a tab before it */
    sil_writestring(s, l);  /* print it */
    sil_pop(L, 1);  /* pop result */
  }
  sil_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int silB_warn (sil_State *L) {
  int n = sil_gettop(L);  /* number of arguments */
  int i;
  silL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    silL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    sil_warning(L, sil_tostring(L, i), 1);
  sil_warning(L, sil_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, sil_Integer *pn) {
  sil_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum(cast_uchar(*s)))  /* no digit? */
    return NULL;
  do {
    unsigned digit = cast_uint(isdigit(cast_uchar(*s))
                               ? *s - '0'
                               : (toupper(cast_uchar(*s)) - 'A') + 10);
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum(cast_uchar(*s)));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (sil_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int silB_tonumber (sil_State *L) {
  if (sil_isnoneornil(L, 2)) {  /* standard conversion? */
    if (sil_type(L, 1) == SIL_TNUMBER) {  /* already a number? */
      sil_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = sil_tolstring(L, 1, &l);
      if (s != NULL && sil_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      silL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    sil_Integer n = 0;  /* to avoid warnings */
    sil_Integer base = silL_checkinteger(L, 2);
    silL_checktype(L, 1, SIL_TSTRING);  /* no numbers as strings */
    s = sil_tolstring(L, 1, &l);
    silL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      sil_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  silL_pushfail(L);  /* not a number */
  return 1;
}


static int silB_error (sil_State *L) {
  int level = (int)silL_optinteger(L, 2, 1);
  sil_settop(L, 1);
  if (sil_type(L, 1) == SIL_TSTRING && level > 0) {
    silL_where(L, level);   /* add extra information */
    sil_pushvalue(L, 1);
    sil_concat(L, 2);
  }
  return sil_error(L);
}


static int silB_getmetatable (sil_State *L) {
  silL_checkany(L, 1);
  if (!sil_getmetatable(L, 1)) {
    sil_pushnil(L);
    return 1;  /* no metatable */
  }
  silL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int silB_setmetatable (sil_State *L) {
  int t = sil_type(L, 2);
  silL_checktype(L, 1, SIL_TTABLE);
  silL_argexpected(L, t == SIL_TNIL || t == SIL_TTABLE, 2, "nil or table");
  if (l_unlikely(silL_getmetafield(L, 1, "__metatable") != SIL_TNIL))
    return silL_error(L, "cannot change a protected metatable");
  sil_settop(L, 2);
  sil_setmetatable(L, 1);
  return 1;
}


static int silB_rawequal (sil_State *L) {
  silL_checkany(L, 1);
  silL_checkany(L, 2);
  sil_pushboolean(L, sil_rawequal(L, 1, 2));
  return 1;
}


static int silB_rawlen (sil_State *L) {
  int t = sil_type(L, 1);
  silL_argexpected(L, t == SIL_TTABLE || t == SIL_TSTRING, 1,
                      "table or string");
  sil_pushinteger(L, l_castU2S(sil_rawlen(L, 1)));
  return 1;
}


static int silB_rawget (sil_State *L) {
  silL_checktype(L, 1, SIL_TTABLE);
  silL_checkany(L, 2);
  sil_settop(L, 2);
  sil_rawget(L, 1);
  return 1;
}

static int silB_rawset (sil_State *L) {
  silL_checktype(L, 1, SIL_TTABLE);
  silL_checkany(L, 2);
  silL_checkany(L, 3);
  sil_settop(L, 3);
  sil_rawset(L, 1);
  return 1;
}


static int pushmode (sil_State *L, int oldmode) {
  if (oldmode == -1)
    silL_pushfail(L);  /* invalid call to 'sil_gc' */
  else
    sil_pushstring(L, (oldmode == SIL_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'sil_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int silB_collectgarbage (sil_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {SIL_GCSTOP, SIL_GCRESTART, SIL_GCCOLLECT,
    SIL_GCCOUNT, SIL_GCSTEP, SIL_GCISRUNNING, SIL_GCGEN, SIL_GCINC,
    SIL_GCPARAM};
  int o = optsnum[silL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case SIL_GCCOUNT: {
      int k = sil_gc(L, o);
      int b = sil_gc(L, SIL_GCCOUNTB);
      checkvalres(k);
      sil_pushnumber(L, (sil_Number)k + ((sil_Number)b/1024));
      return 1;
    }
    case SIL_GCSTEP: {
      sil_Integer n = silL_optinteger(L, 2, 0);
      int res = sil_gc(L, o, cast_sizet(n));
      checkvalres(res);
      sil_pushboolean(L, res);
      return 1;
    }
    case SIL_GCISRUNNING: {
      int res = sil_gc(L, o);
      checkvalres(res);
      sil_pushboolean(L, res);
      return 1;
    }
    case SIL_GCGEN: {
      return pushmode(L, sil_gc(L, o));
    }
    case SIL_GCINC: {
      return pushmode(L, sil_gc(L, o));
    }
    case SIL_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        SIL_GCPMINORMUL, SIL_GCPMAJORMINOR, SIL_GCPMINORMAJOR,
        SIL_GCPPAUSE, SIL_GCPSTEPMUL, SIL_GCPSTEPSIZE};
      int p = pnum[silL_checkoption(L, 2, NULL, params)];
      sil_Integer value = silL_optinteger(L, 3, -1);
      sil_pushinteger(L, sil_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = sil_gc(L, o);
      checkvalres(res);
      sil_pushinteger(L, res);
      return 1;
    }
  }
  silL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int silB_type (sil_State *L) {
  int t = sil_type(L, 1);
  silL_argcheck(L, t != SIL_TNONE, 1, "value expected");
  sil_pushstring(L, sil_typename(L, t));
  return 1;
}


static int silB_next (sil_State *L) {
  silL_checktype(L, 1, SIL_TTABLE);
  sil_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (sil_next(L, 1))
    return 2;
  else {
    sil_pushnil(L);
    return 1;
  }
}


static int pairscont (sil_State *L, int status, sil_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int silB_pairs (sil_State *L) {
  silL_checkany(L, 1);
  if (silL_getmetafield(L, 1, "__pairs") == SIL_TNIL) {  /* no metamethod? */
    sil_pushcfunction(L, silB_next);  /* will return generator, */
    sil_pushvalue(L, 1);  /* state, */
    sil_pushnil(L);  /* and initial value */
  }
  else {
    sil_pushvalue(L, 1);  /* argument 'self' to metamethod */
    sil_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (sil_State *L) {
  sil_Integer i = silL_checkinteger(L, 2);
  i = silL_intop(+, i, 1);
  sil_pushinteger(L, i);
  return (sil_geti(L, 1, i) == SIL_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int silB_ipairs (sil_State *L) {
  silL_checkany(L, 1);
  sil_pushcfunction(L, ipairsaux);  /* iteration function */
  sil_pushvalue(L, 1);  /* state */
  sil_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (sil_State *L, int status, int envidx) {
  if (l_likely(status == SIL_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      sil_pushvalue(L, envidx);  /* environment for loaded function */
      if (!sil_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        sil_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    silL_pushfail(L);
    sil_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (sil_State *L, int idx) {
  const char *mode = silL_optstring(L, idx, "bt");
  if (strchr(mode, 'B') != NULL)  /* SIL code cannot use fixed buffers */
    silL_argerror(L, idx, "invalid mode");
  return mode;
}


static int silB_loadfile (sil_State *L) {
  const char *fname = silL_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!sil_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = silL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'sil_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (sil_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  silL_checkstack(L, 2, "too many nested functions");
  sil_pushvalue(L, 1);  /* get function */
  sil_call(L, 0, 1);  /* call it */
  if (sil_isnil(L, -1)) {
    sil_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!sil_isstring(L, -1)))
    silL_error(L, "reader function must return a string");
  sil_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return sil_tolstring(L, RESERVEDSLOT, size);
}


static int silB_load (sil_State *L) {
  int status;
  size_t l;
  const char *s = sil_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!sil_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = silL_optstring(L, 2, s);
    status = silL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = silL_optstring(L, 2, "=(load)");
    silL_checktype(L, 1, SIL_TFUNCTION);
    sil_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = sil_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (sil_State *L, int d1, sil_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'sil_Kfunction' prototype */
  return sil_gettop(L) - 1;
}


static int silB_dofile (sil_State *L) {
  const char *fname = silL_optstring(L, 1, NULL);
  sil_settop(L, 1);
  if (l_unlikely(silL_loadfile(L, fname) != SIL_OK))
    return sil_error(L);
  sil_callk(L, 0, SIL_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int silB_assert (sil_State *L) {
  if (l_likely(sil_toboolean(L, 1)))  /* condition is true? */
    return sil_gettop(L);  /* return all arguments */
  else {  /* error */
    silL_checkany(L, 1);  /* there must be a condition */
    sil_remove(L, 1);  /* remove it */
    sil_pushliteral(L, "assertion failed!");  /* default message */
    sil_settop(L, 1);  /* leave only message (default if no other one) */
    return silB_error(L);  /* call 'error' */
  }
}


static int silB_select (sil_State *L) {
  int n = sil_gettop(L);
  if (sil_type(L, 1) == SIL_TSTRING && *sil_tostring(L, 1) == '#') {
    sil_pushinteger(L, n-1);
    return 1;
  }
  else {
    sil_Integer i = silL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    silL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (sil_State *L, int status, sil_KContext extra) {
  if (l_unlikely(status != SIL_OK && status != SIL_YIELD)) {  /* error? */
    sil_pushboolean(L, 0);  /* first result (false) */
    sil_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return sil_gettop(L) - (int)extra;  /* return all results */
}


static int silB_pcall (sil_State *L) {
  int status;
  silL_checkany(L, 1);
  sil_pushboolean(L, 1);  /* first result if no errors */
  sil_insert(L, 1);  /* put it in place */
  status = sil_pcallk(L, sil_gettop(L) - 2, SIL_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'sil_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int silB_xpcall (sil_State *L) {
  int status;
  int n = sil_gettop(L);
  silL_checktype(L, 2, SIL_TFUNCTION);  /* check error function */
  sil_pushboolean(L, 1);  /* first result */
  sil_pushvalue(L, 1);  /* function */
  sil_rotate(L, 3, 2);  /* move them below function's arguments */
  status = sil_pcallk(L, n - 2, SIL_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int silB_tostring (sil_State *L) {
  silL_checkany(L, 1);
  silL_tolstring(L, 1, NULL);
  return 1;
}


static const silL_Reg base_funcs[] = {
  {"assert", silB_assert},
  {"collectgarbage", silB_collectgarbage},
  {"dofile", silB_dofile},
  {"error", silB_error},
  {"getmetatable", silB_getmetatable},
  {"ipairs", silB_ipairs},
  {"loadfile", silB_loadfile},
  {"load", silB_load},
  {"next", silB_next},
  {"pairs", silB_pairs},
  {"pcall", silB_pcall},
  {"print", silB_print},
  {"warn", silB_warn},
  {"rawequal", silB_rawequal},
  {"rawlen", silB_rawlen},
  {"rawget", silB_rawget},
  {"rawset", silB_rawset},
  {"select", silB_select},
  {"setmetatable", silB_setmetatable},
  {"tonumber", silB_tonumber},
  {"tostring", silB_tostring},
  {"type", silB_type},
  {"xpcall", silB_xpcall},
  /* placeholders */
  {SIL_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


SILMOD_API int silopen_base (sil_State *L) {
  /* open lib into global table */
  sil_pushglobaltable(L);
  silL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  sil_pushvalue(L, -1);
  sil_setfield(L, -2, SIL_GNAME);
  /* set global _VERSION */
  sil_pushliteral(L, SIL_VERSION);
  sil_setfield(L, -2, "_VERSION");
  return 1;
}

