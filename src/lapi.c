/*
** $Id: lapi.c $
** SIL API
** See Copyright Notice in sil.h
*/

#define lapi_c
#define SIL_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "sil.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char sil_ident[] =
  "$SilVersion: " SIL_COPYRIGHT " $"
  "$SilAuthors: " SIL_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
*/
#define isvalid(L, o)	((o) != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= SIL_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < SIL_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (sil_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, idx <= ci->top.p - (ci->func.p + 1), "unacceptable index");
    if (o >= L->top.p) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    return s2v(L->top.p + idx);
  }
  else if (idx == SIL_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = SIL_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or SIL function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
static StkId index2stack (sil_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, o < L->top.p, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top.p + idx;
  }
}


SIL_API int sil_checkstack (sil_State *L, int n) {
  int res;
  CallInfo *ci;
  sil_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = silD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  sil_unlock(L);
  return res;
}


SIL_API void sil_xmove (sil_State *from, sil_State *to, int n) {
  int i;
  if (from == to) return;
  sil_lock(to);
  api_checkpop(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top.p, from->top.p + i);
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  sil_unlock(to);
}


SIL_API sil_CFunction sil_atpanic (sil_State *L, sil_CFunction panicf) {
  sil_CFunction old;
  sil_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  sil_unlock(L);
  return old;
}


SIL_API sil_Number sil_version (sil_State *L) {
  UNUSED(L);
  return SIL_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
SIL_API int sil_absindex (sil_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top.p - L->ci->func.p) + idx;
}


SIL_API int sil_gettop (sil_State *L) {
  return cast_int(L->top.p - (L->ci->func.p + 1));
}


SIL_API void sil_settop (sil_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  sil_lock(L);
  ci = L->ci;
  func = ci->func.p;
  if (idx >= 0) {
    api_check(L, idx <= ci->top.p - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top.p;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top.p++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top.p - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  newtop = L->top.p + diff;
  if (diff < 0 && L->tbclist.p >= newtop) {
    sil_assert(ci->callstatus & CIST_TBC);
    newtop = silF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  sil_unlock(L);
}


SIL_API void sil_closeslot (sil_State *L, int idx) {
  StkId level;
  sil_lock(L);
  level = index2stack(L, idx);
  api_check(L, (L->ci->callstatus & CIST_TBC) && (L->tbclist.p == level),
     "no variable to close at given level");
  level = silF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  sil_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'sil_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (sil_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
SIL_API void sil_rotate (sil_State *L, int idx, int n) {
  StkId p, t, m;
  sil_lock(L);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, L->tbclist.p < p, "moving a to-be-closed slot");
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  sil_unlock(L);
}


SIL_API void sil_copy (sil_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  sil_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    silC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr);
  /* SIL_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  sil_unlock(L);
}


SIL_API void sil_pushvalue (sil_State *L, int idx) {
  sil_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx));
  api_incr_top(L);
  sil_unlock(L);
}



/*
** access functions (stack -> C)
*/


SIL_API int sil_type (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : SIL_TNONE);
}


SIL_API const char *sil_typename (sil_State *L, int t) {
  UNUSED(L);
  api_check(L, SIL_TNONE <= t && t < SIL_NUMTYPES, "invalid type");
  return ttypename(t);
}


SIL_API int sil_iscfunction (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


SIL_API int sil_isinteger (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


SIL_API int sil_isnumber (sil_State *L, int idx) {
  sil_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


SIL_API int sil_isstring (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


SIL_API int sil_isuserdata (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


SIL_API int sil_rawequal (sil_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? silV_rawequalobj(o1, o2) : 0;
}


SIL_API void sil_arith (sil_State *L, int op) {
  sil_lock(L);
  if (op != SIL_OPUNM && op != SIL_OPBNOT)
    api_checkpop(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checkpop(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  silO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--;  /* pop second operand */
  sil_unlock(L);
}


SIL_API int sil_compare (sil_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  sil_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case SIL_OPEQ: i = silV_equalobj(L, o1, o2); break;
      case SIL_OPLT: i = silV_lessthan(L, o1, o2); break;
      case SIL_OPLE: i = silV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  sil_unlock(L);
  return i;
}


SIL_API unsigned (sil_numbertocstring) (sil_State *L, int idx, char *buff) {
  const TValue *o = index2value(L, idx);
  if (ttisnumber(o)) {
    unsigned len = silO_tostringbuff(o, buff);
    buff[len++] = '\0';  /* add final zero */
    return len;
  }
  else
    return 0;
}


SIL_API size_t sil_stringtonumber (sil_State *L, const char *s) {
  size_t sz = silO_str2num(s, s2v(L->top.p));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


SIL_API sil_Number sil_tonumberx (sil_State *L, int idx, int *pisnum) {
  sil_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


SIL_API sil_Integer sil_tointegerx (sil_State *L, int idx, int *pisnum) {
  sil_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


SIL_API int sil_toboolean (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


SIL_API const char *sil_tolstring (sil_State *L, int idx, size_t *len) {
  TValue *o;
  sil_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      sil_unlock(L);
      return NULL;
    }
    silO_tostring(L, o);
    silC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  sil_unlock(L);
  if (len != NULL)
    return getlstr(tsvalue(o), *len);
  else
    return getstr(tsvalue(o));
}


SIL_API sil_Unsigned sil_rawlen (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case SIL_VSHRSTR: return cast(sil_Unsigned, tsvalue(o)->shrlen);
    case SIL_VLNGSTR: return cast(sil_Unsigned, tsvalue(o)->u.lnglen);
    case SIL_VUSERDATA: return cast(sil_Unsigned, uvalue(o)->len);
    case SIL_VTABLE: return silH_getn(hvalue(o));
    default: return 0;
  }
}


SIL_API sil_CFunction sil_tocfunction (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case SIL_TUSERDATA: return getudatamem(uvalue(o));
    case SIL_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


SIL_API void *sil_touserdata (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


SIL_API sil_State *sil_tothread (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
SIL_API const void *sil_topointer (sil_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case SIL_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case SIL_VUSERDATA: case SIL_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


SIL_API void sil_pushnil (sil_State *L) {
  sil_lock(L);
  setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  sil_unlock(L);
}


SIL_API void sil_pushnumber (sil_State *L, sil_Number n) {
  sil_lock(L);
  setfltvalue(s2v(L->top.p), n);
  api_incr_top(L);
  sil_unlock(L);
}


SIL_API void sil_pushinteger (sil_State *L, sil_Integer n) {
  sil_lock(L);
  setivalue(s2v(L->top.p), n);
  api_incr_top(L);
  sil_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
SIL_API const char *sil_pushlstring (sil_State *L, const char *s, size_t len) {
  TString *ts;
  sil_lock(L);
  ts = (len == 0) ? silS_new(L, "") : silS_newlstr(L, s, len);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  silC_checkGC(L);
  sil_unlock(L);
  return getstr(ts);
}


SIL_API const char *sil_pushexternalstring (sil_State *L,
	        const char *s, size_t len, sil_Alloc falloc, void *ud) {
  TString *ts;
  sil_lock(L);
  api_check(L, len <= MAX_SIZE, "string too large");
  api_check(L, s[len] == '\0', "string not ending with zero");
  ts = silS_newextlstr (L, s, len, falloc, ud);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  silC_checkGC(L);
  sil_unlock(L);
  return getstr(ts);
}


SIL_API const char *sil_pushstring (sil_State *L, const char *s) {
  sil_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top.p));
  else {
    TString *ts;
    ts = silS_new(L, s);
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  silC_checkGC(L);
  sil_unlock(L);
  return s;
}


SIL_API const char *sil_pushvfstring (sil_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  sil_lock(L);
  ret = silO_pushvfstring(L, fmt, argp);
  silC_checkGC(L);
  sil_unlock(L);
  return ret;
}


SIL_API const char *sil_pushfstring (sil_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  sil_lock(L);
  pushvfstring(L, argp, fmt, ret);
  silC_checkGC(L);
  sil_unlock(L);
  return ret;
}


SIL_API void sil_pushcclosure (sil_State *L, sil_CFunction fn, int n) {
  sil_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top.p), fn);
    api_incr_top(L);
  }
  else {
    int i;
    CClosure *cl;
    api_checkpop(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = silF_newCclosure(L, n);
    cl->f = fn;
    for (i = 0; i < n; i++) {
      setobj2n(L, &cl->upvalue[i], s2v(L->top.p - n + i));
      /* does not need barrier because closure is white */
      sil_assert(iswhite(cl));
    }
    L->top.p -= n;
    setclCvalue(L, s2v(L->top.p), cl);
    api_incr_top(L);
    silC_checkGC(L);
  }
  sil_unlock(L);
}


SIL_API void sil_pushboolean (sil_State *L, int b) {
  sil_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p));
  else
    setbfvalue(s2v(L->top.p));
  api_incr_top(L);
  sil_unlock(L);
}


SIL_API void sil_pushlightuserdata (sil_State *L, void *p) {
  sil_lock(L);
  setpvalue(s2v(L->top.p), p);
  api_incr_top(L);
  sil_unlock(L);
}


SIL_API int sil_pushthread (sil_State *L) {
  sil_lock(L);
  setthvalue(L, s2v(L->top.p), L);
  api_incr_top(L);
  sil_unlock(L);
  return (mainthread(G(L)) == L);
}



/*
** get functions (Sil -> stack)
*/


static int auxgetstr (sil_State *L, const TValue *t, const char *k) {
  lu_byte tag;
  TString *str = silS_new(L, k);
  silV_fastget(t, str, s2v(L->top.p), silH_getstr, tag);
  if (!tagisempty(tag))
    api_incr_top(L);
  else {
    setsvalue2s(L, L->top.p, str);
    api_incr_top(L);
    tag = silV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  }
  sil_unlock(L);
  return novariant(tag);
}


/*
** The following function assumes that the registry cannot be a weak
** table, so that en mergency collection while using the global table
** cannot collect it.
*/
static void getGlobalTable (sil_State *L, TValue *gt) {
  Table *registry = hvalue(&G(L)->l_registry);
  lu_byte tag = silH_getint(registry, SIL_RIDX_GLOBALS, gt);
  (void)tag;  /* avoid not-used warnings when checks are off */
  api_check(L, novariant(tag) == SIL_TTABLE, "global table must exist");
}


SIL_API int sil_getglobal (sil_State *L, const char *name) {
  TValue gt;
  sil_lock(L);
  getGlobalTable(L, &gt);
  return auxgetstr(L, &gt, name);
}


SIL_API int sil_gettable (sil_State *L, int idx) {
  lu_byte tag;
  TValue *t;
  sil_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  silV_fastget(t, s2v(L->top.p - 1), s2v(L->top.p - 1), silH_get, tag);
  if (tagisempty(tag))
    tag = silV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  sil_unlock(L);
  return novariant(tag);
}


SIL_API int sil_getfield (sil_State *L, int idx, const char *k) {
  sil_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


SIL_API int sil_geti (sil_State *L, int idx, sil_Integer n) {
  TValue *t;
  lu_byte tag;
  sil_lock(L);
  t = index2value(L, idx);
  silV_fastgeti(t, n, s2v(L->top.p), tag);
  if (tagisempty(tag)) {
    TValue key;
    setivalue(&key, n);
    tag = silV_finishget(L, t, &key, L->top.p, tag);
  }
  api_incr_top(L);
  sil_unlock(L);
  return novariant(tag);
}


static int finishrawget (sil_State *L, lu_byte tag) {
  if (tagisempty(tag))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  sil_unlock(L);
  return novariant(tag);
}


l_sinline Table *gettable (sil_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


SIL_API int sil_rawget (sil_State *L, int idx) {
  Table *t;
  lu_byte tag;
  sil_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  tag = silH_get(t, s2v(L->top.p - 1), s2v(L->top.p - 1));
  L->top.p--;  /* pop key */
  return finishrawget(L, tag);
}


SIL_API int sil_rawgeti (sil_State *L, int idx, sil_Integer n) {
  Table *t;
  lu_byte tag;
  sil_lock(L);
  t = gettable(L, idx);
  silH_fastgeti(t, n, s2v(L->top.p), tag);
  return finishrawget(L, tag);
}


SIL_API int sil_rawgetp (sil_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  sil_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, silH_get(t, &k, s2v(L->top.p)));
}


SIL_API void sil_createtable (sil_State *L, int narray, int nrec) {
  Table *t;
  sil_lock(L);
  t = silH_new(L);
  sethvalue2s(L, L->top.p, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    silH_resize(L, t, cast_uint(narray), cast_uint(nrec));
  silC_checkGC(L);
  sil_unlock(L);
}


SIL_API int sil_getmetatable (sil_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  sil_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case SIL_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case SIL_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top.p, mt);
    api_incr_top(L);
    res = 1;
  }
  sil_unlock(L);
  return res;
}


SIL_API int sil_getiuservalue (sil_State *L, int idx, int n) {
  TValue *o;
  int t;
  sil_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top.p));
    t = SIL_TNONE;
  }
  else {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  sil_unlock(L);
  return t;
}


/*
** set functions (stack -> SIL)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (sil_State *L, const TValue *t, const char *k) {
  int hres;
  TString *str = silS_new(L, k);
  api_checkpop(L, 1);
  silV_fastset(t, str, s2v(L->top.p - 1), hres, silH_psetstr);
  if (hres == HOK) {
    silV_finishfastset(L, t, s2v(L->top.p - 1));
    L->top.p--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top.p, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    silV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), hres);
    L->top.p -= 2;  /* pop value and key */
  }
  sil_unlock(L);  /* lock done by caller */
}


SIL_API void sil_setglobal (sil_State *L, const char *name) {
  TValue gt;
  sil_lock(L);  /* unlock done in 'auxsetstr' */
  getGlobalTable(L, &gt);
  auxsetstr(L, &gt, name);
}


SIL_API void sil_settable (sil_State *L, int idx) {
  TValue *t;
  int hres;
  sil_lock(L);
  api_checkpop(L, 2);
  t = index2value(L, idx);
  silV_fastset(t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres, silH_pset);
  if (hres == HOK)
    silV_finishfastset(L, t, s2v(L->top.p - 1));
  else
    silV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres);
  L->top.p -= 2;  /* pop index and value */
  sil_unlock(L);
}


SIL_API void sil_setfield (sil_State *L, int idx, const char *k) {
  sil_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


SIL_API void sil_seti (sil_State *L, int idx, sil_Integer n) {
  TValue *t;
  int hres;
  sil_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  silV_fastseti(t, n, s2v(L->top.p - 1), hres);
  if (hres == HOK)
    silV_finishfastset(L, t, s2v(L->top.p - 1));
  else {
    TValue temp;
    setivalue(&temp, n);
    silV_finishset(L, t, &temp, s2v(L->top.p - 1), hres);
  }
  L->top.p--;  /* pop value */
  sil_unlock(L);
}


static void aux_rawset (sil_State *L, int idx, TValue *key, int n) {
  Table *t;
  sil_lock(L);
  api_checkpop(L, n);
  t = gettable(L, idx);
  silH_set(L, t, key, s2v(L->top.p - 1));
  invalidateTMcache(t);
  silC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p -= n;
  sil_unlock(L);
}


SIL_API void sil_rawset (sil_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top.p - 2), 2);
}


SIL_API void sil_rawsetp (sil_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


SIL_API void sil_rawseti (sil_State *L, int idx, sil_Integer n) {
  Table *t;
  sil_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  silH_setint(L, t, n, s2v(L->top.p - 1));
  silC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  sil_unlock(L);
}


SIL_API int sil_setmetatable (sil_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  sil_lock(L);
  api_checkpop(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1));
  }
  switch (ttype(obj)) {
    case SIL_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        silC_objbarrier(L, gcvalue(obj), mt);
        silC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case SIL_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        silC_objbarrier(L, uvalue(obj), mt);
        silC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top.p--;
  sil_unlock(L);
  return 1;
}


SIL_API int sil_setiuservalue (sil_State *L, int idx, int n) {
  TValue *o;
  int res;
  sil_lock(L);
  api_checkpop(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1));
    silC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));
    res = 1;
  }
  L->top.p--;
  sil_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run SIL code)
*/


#define checkresults(L,na,nr) \
     (api_check(L, (nr) == SIL_MULTRET \
               || (L->ci->top.p - L->top.p >= (nr) - (na)), \
	"results from function overflow current stack size"), \
      api_check(L, SIL_MULTRET <= (nr) && (nr) <= MAXRESULTS,  \
                   "invalid number of results"))


SIL_API void sil_callk (sil_State *L, int nargs, int nresults,
                        sil_KContext ctx, sil_KFunction k) {
  StkId func;
  sil_lock(L);
  api_check(L, k == NULL || !isSil(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == SIL_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    silD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    silD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  sil_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (sil_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  silD_callnoyield(L, c->func, c->nresults);
}



SIL_API int sil_pcallk (sil_State *L, int nargs, int nresults, int errfunc,
                        sil_KContext ctx, sil_KFunction k) {
  struct CallS c;
  TStatus status;
  ptrdiff_t func;
  sil_lock(L);
  api_check(L, k == NULL || !isSil(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == SIL_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top.p - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = silD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    silD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = SIL_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  sil_unlock(L);
  return APIstatus(status);
}


SIL_API int sil_load (sil_State *L, sil_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  TStatus status;
  sil_lock(L);
  if (!chunkname) chunkname = "?";
  silZ_init(L, &z, reader, data);
  status = silD_protectedparser(L, &z, chunkname, mode);
  if (status == SIL_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top.p - 1));  /* get new function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      TValue gt;
      getGlobalTable(L, &gt);
      /* set global table as 1st upvalue of 'f' (may be SIL_ENV) */
      setobj(L, f->upvals[0]->v.p, &gt);
      silC_barrier(L, f->upvals[0], &gt);
    }
  }
  sil_unlock(L);
  return APIstatus(status);
}


/*
** Dump a SIL function, calling 'writer' to write its parts. Ensure
** the stack returns with its original size.
*/
SIL_API int sil_dump (sil_State *L, sil_Writer writer, void *data, int strip) {
  int status;
  ptrdiff_t otop = savestack(L, L->top.p);  /* original top */
  TValue *f = s2v(L->top.p - 1);  /* function to be dumped */
  sil_lock(L);
  api_checkpop(L, 1);
  api_check(L, isLfunction(f), "Sil function expected");
  status = silU_dump(L, clLvalue(f)->p, writer, data, strip);
  L->top.p = restorestack(L, otop);  /* restore top */
  sil_unlock(L);
  return status;
}


SIL_API int sil_status (sil_State *L) {
  return APIstatus(L->status);
}


/*
** Garbage-collection function
*/
SIL_API int sil_gc (sil_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & (GCSTPGC | GCSTPCLS))  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  sil_lock(L);
  va_start(argp, what);
  switch (what) {
    case SIL_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case SIL_GCRESTART: {
      silE_setdebt(g, 0);
      g->gcstp = 0;  /* (other bits must be zero here) */
      break;
    }
    case SIL_GCCOLLECT: {
      silC_fullgc(L, 0);
      break;
    }
    case SIL_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case SIL_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case SIL_GCSTEP: {
      lu_byte oldstp = g->gcstp;
      l_mem n = cast(l_mem, va_arg(argp, size_t));
      int work = 0;  /* true if GC did some work */
      g->gcstp = 0;  /* allow GC to run (other bits must be zero here) */
      if (n <= 0)
        n = g->GCdebt;  /* force to run one basic step */
      silE_setdebt(g, g->GCdebt - n);
      silC_condGC(L, (void)0, work = 1);
      if (work && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      g->gcstp = oldstp;  /* restore previous state */
      break;
    }
    case SIL_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case SIL_GCGEN: {
      res = (g->gckind == KGC_INC) ? SIL_GCINC : SIL_GCGEN;
      silC_changemode(L, KGC_GENMINOR);
      break;
    }
    case SIL_GCINC: {
      res = (g->gckind == KGC_INC) ? SIL_GCINC : SIL_GCGEN;
      silC_changemode(L, KGC_INC);
      break;
    }
    case SIL_GCPARAM: {
      int param = va_arg(argp, int);
      int value = va_arg(argp, int);
      api_check(L, 0 <= param && param < SIL_GCPN, "invalid parameter");
      res = cast_int(silO_applyparam(g->gcparams[param], 100));
      if (value >= 0)
        g->gcparams[param] = silO_codeparam(cast_uint(value));
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  sil_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


SIL_API int sil_error (sil_State *L) {
  TValue *errobj;
  sil_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checkpop(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    silM_error(L);  /* raise a memory error */
  else
    silG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


SIL_API int sil_next (sil_State *L, int idx) {
  Table *t;
  int more;
  sil_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  more = silH_next(L, t, L->top.p - 1);
  if (more)
    api_incr_top(L);
  else  /* no more elements */
    L->top.p--;  /* pop key */
  sil_unlock(L);
  return more;
}


SIL_API void sil_toclose (sil_State *L, int idx) {
  StkId o;
  sil_lock(L);
  o = index2stack(L, idx);
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  silF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  L->ci->callstatus |= CIST_TBC;  /* mark that function has TBC slots */
  sil_unlock(L);
}


SIL_API void sil_concat (sil_State *L, int n) {
  sil_lock(L);
  api_checknelems(L, n);
  if (n > 0) {
    silV_concat(L, n);
    silC_checkGC(L);
  }
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top.p, silS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  sil_unlock(L);
}


SIL_API void sil_len (sil_State *L, int idx) {
  TValue *t;
  sil_lock(L);
  t = index2value(L, idx);
  silV_objlen(L, L->top.p, t);
  api_incr_top(L);
  sil_unlock(L);
}


SIL_API sil_Alloc sil_getallocf (sil_State *L, void **ud) {
  sil_Alloc f;
  sil_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  sil_unlock(L);
  return f;
}


SIL_API void sil_setallocf (sil_State *L, sil_Alloc f, void *ud) {
  sil_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  sil_unlock(L);
}


void sil_setwarnf (sil_State *L, sil_WarnFunction f, void *ud) {
  sil_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  sil_unlock(L);
}


void sil_warning (sil_State *L, const char *msg, int tocont) {
  sil_lock(L);
  silE_warning(L, msg, tocont);
  sil_unlock(L);
}



SIL_API void *sil_newuserdatauv (sil_State *L, size_t size, int nuvalue) {
  Udata *u;
  sil_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = silS_newudata(L, size, cast(unsigned short, nuvalue));
  setuvalue(L, s2v(L->top.p), u);
  api_incr_top(L);
  silC_checkGC(L);
  sil_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case SIL_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case SIL_VLCL: {  /* SIL closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v.p;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


SIL_API const char *sil_getupvalue (sil_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  sil_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top.p, val);
    api_incr_top(L);
  }
  sil_unlock(L);
  return name;
}


SIL_API const char *sil_setupvalue (sil_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  sil_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    setobj(L, val, s2v(L->top.p));
    silC_barrier(L, owner, val);
  }
  sil_unlock(L);
  return name;
}


static UpVal **getupvalref (sil_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Sil function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


SIL_API void *sil_upvalueid (sil_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case SIL_VLCL: {  /* sil closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case SIL_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case SIL_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


SIL_API void sil_upvaluejoin (sil_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  silC_objbarrier(L, f1, *up1);
}


