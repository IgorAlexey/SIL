/*
** $Id: lauxlib.c $
** Auxiliary functions for building SIL libraries
** See Copyright Notice in sil.h
*/

#define lauxlib_c
#define SIL_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of SIL.
** Any function declared here could be written as an application function.
*/

#include "sil.h"

#include "lauxlib.h"
#include "llimits.h"


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (sil_State *L, int objidx, int level) {
  if (level == 0 || !sil_istable(L, -1))
    return 0;  /* not found */
  sil_pushnil(L);  /* start 'next' loop */
  while (sil_next(L, -2)) {  /* for each pair in table */
    if (sil_type(L, -2) == SIL_TSTRING) {  /* ignore non-string keys */
      if (sil_rawequal(L, objidx, -1)) {  /* found object? */
        sil_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        sil_pushliteral(L, ".");  /* place '.' between the two names */
        sil_replace(L, -3);  /* (in the slot occupied by table) */
        sil_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    sil_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (sil_State *L, sil_Debug *ar) {
  int top = sil_gettop(L);
  sil_getinfo(L, "f", ar);  /* push function */
  sil_getfield(L, SIL_REGISTRYINDEX, SIL_LOADED_TABLE);
  silL_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
  if (findfield(L, top + 1, 2)) {
    const char *name = sil_tostring(L, -1);
    if (strncmp(name, SIL_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      sil_pushstring(L, name + 3);  /* push name without prefix */
      sil_remove(L, -2);  /* remove original name */
    }
    sil_copy(L, -1, top + 1);  /* copy name to proper place */
    sil_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    sil_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (sil_State *L, sil_Debug *ar) {
  if (*ar->namewhat != '\0')  /* is there a name from code? */
    sil_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      sil_pushliteral(L, "main chunk");
  else if (pushglobalfuncname(L, ar)) {  /* try a global name */
    sil_pushfstring(L, "function '%s'", sil_tostring(L, -1));
    sil_remove(L, -2);  /* remove name */
  }
  else if (*ar->what != 'C')  /* for SIL functions, use <file:line> */
    sil_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    sil_pushliteral(L, "?");
}


static int lastlevel (sil_State *L) {
  sil_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (sil_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (sil_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


SILLIB_API void silL_traceback (sil_State *L, sil_State *L1,
                                const char *msg, int level) {
  silL_Buffer b;
  sil_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  silL_buffinit(L, &b);
  if (msg) {
    silL_addstring(&b, msg);
    silL_addchar(&b, '\n');
  }
  silL_addstring(&b, "stack traceback:");
  while (sil_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      sil_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      silL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      sil_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        sil_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        sil_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      silL_addvalue(&b);
      pushfuncname(L, &ar);
      silL_addvalue(&b);
      if (ar.istailcall)
        silL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  silL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

SILLIB_API int silL_argerror (sil_State *L, int arg, const char *extramsg) {
  sil_Debug ar;
  const char *argword;
  if (!sil_getstack(L, 0, &ar))  /* no stack frame? */
    return silL_error(L, "bad argument #%d (%s)", arg, extramsg);
  sil_getinfo(L, "nt", &ar);
  if (arg <= ar.extraargs)  /* error in an extra argument? */
    argword =  "extra argument";
  else {
    arg -= ar.extraargs;  /* do not count extra arguments */
    if (strcmp(ar.namewhat, "method") == 0) {  /* colon syntax? */
      arg--;  /* do not count (extra) self argument */
      if (arg == 0)  /* error in self argument? */
        return silL_error(L, "calling '%s' on bad self (%s)",
                               ar.name, extramsg);
      /* else go through; error in a regular argument */
    }
    argword = "argument";
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? sil_tostring(L, -1) : "?";
  return silL_error(L, "bad %s #%d to '%s' (%s)",
                       argword, arg, ar.name, extramsg);
}


SILLIB_API int silL_typeerror (sil_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (silL_getmetafield(L, arg, "__name") == SIL_TSTRING)
    typearg = sil_tostring(L, -1);  /* use the given type name */
  else if (sil_type(L, arg) == SIL_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = silL_typename(L, arg);  /* standard name */
  msg = sil_pushfstring(L, "%s expected, got %s", tname, typearg);
  return silL_argerror(L, arg, msg);
}


static void tag_error (sil_State *L, int arg, int tag) {
  silL_typeerror(L, arg, sil_typename(L, tag));
}


/*
** The use of 'sil_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
SILLIB_API void silL_where (sil_State *L, int level) {
  sil_Debug ar;
  if (sil_getstack(L, level, &ar)) {  /* check function at level */
    sil_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      sil_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  sil_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'sil_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** a memory error instead of the given message.)
*/
SILLIB_API int silL_error (sil_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  silL_where(L, 1);
  sil_pushvfstring(L, fmt, argp);
  va_end(argp);
  sil_concat(L, 2);
  return sil_error(L);
}


SILLIB_API int silL_fileresult (sil_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to SIL API may change this value */
  if (stat) {
    sil_pushboolean(L, 1);
    return 1;
  }
  else {
    const char *msg;
    silL_pushfail(L);
    msg = (en != 0) ? strerror(en) : "(no extra info)";
    if (fname)
      sil_pushfstring(L, "%s: %s", fname, msg);
    else
      sil_pushstring(L, msg);
    sil_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(SIL_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


SILLIB_API int silL_execresult (sil_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return silL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      sil_pushboolean(L, 1);
    else
      silL_pushfail(L);
    sil_pushstring(L, what);
    sil_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

SILLIB_API int silL_newmetatable (sil_State *L, const char *tname) {
  if (silL_getmetatable(L, tname) != SIL_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  sil_pop(L, 1);
  sil_createtable(L, 0, 2);  /* create metatable */
  sil_pushstring(L, tname);
  sil_setfield(L, -2, "__name");  /* metatable.__name = tname */
  sil_pushvalue(L, -1);
  sil_setfield(L, SIL_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


SILLIB_API void silL_setmetatable (sil_State *L, const char *tname) {
  silL_getmetatable(L, tname);
  sil_setmetatable(L, -2);
}


SILLIB_API void *silL_testudata (sil_State *L, int ud, const char *tname) {
  void *p = sil_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (sil_getmetatable(L, ud)) {  /* does it have a metatable? */
      silL_getmetatable(L, tname);  /* get correct metatable */
      if (!sil_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      sil_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


SILLIB_API void *silL_checkudata (sil_State *L, int ud, const char *tname) {
  void *p = silL_testudata(L, ud, tname);
  silL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

SILLIB_API int silL_checkoption (sil_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? silL_optstring(L, arg, def) :
                             silL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return silL_argerror(L, arg,
                       sil_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, SIL will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
SILLIB_API void silL_checkstack (sil_State *L, int space, const char *msg) {
  if (l_unlikely(!sil_checkstack(L, space))) {
    if (msg)
      silL_error(L, "stack overflow (%s)", msg);
    else
      silL_error(L, "stack overflow");
  }
}


SILLIB_API void silL_checktype (sil_State *L, int arg, int t) {
  if (l_unlikely(sil_type(L, arg) != t))
    tag_error(L, arg, t);
}


SILLIB_API void silL_checkany (sil_State *L, int arg) {
  if (l_unlikely(sil_type(L, arg) == SIL_TNONE))
    silL_argerror(L, arg, "value expected");
}


SILLIB_API const char *silL_checklstring (sil_State *L, int arg, size_t *len) {
  const char *s = sil_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, SIL_TSTRING);
  return s;
}


SILLIB_API const char *silL_optlstring (sil_State *L, int arg,
                                        const char *def, size_t *len) {
  if (sil_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return silL_checklstring(L, arg, len);
}


SILLIB_API sil_Number silL_checknumber (sil_State *L, int arg) {
  int isnum;
  sil_Number d = sil_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, SIL_TNUMBER);
  return d;
}


SILLIB_API sil_Number silL_optnumber (sil_State *L, int arg, sil_Number def) {
  return silL_opt(L, silL_checknumber, arg, def);
}


static void interror (sil_State *L, int arg) {
  if (sil_isnumber(L, arg))
    silL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, SIL_TNUMBER);
}


SILLIB_API sil_Integer silL_checkinteger (sil_State *L, int arg) {
  int isnum;
  sil_Integer d = sil_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


SILLIB_API sil_Integer silL_optinteger (sil_State *L, int arg,
                                                      sil_Integer def) {
  return silL_opt(L, silL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


/* Resize the buffer used by a box. Optimize for the common case of
** resizing to the old size. (For instance, __gc will resize the box
** to 0 even after it was closed. 'pushresult' may also resize it to a
** final size that is equal to the one set when the buffer was created.)
*/
static void *resizebox (sil_State *L, int idx, size_t newsize) {
  UBox *box = (UBox *)sil_touserdata(L, idx);
  if (box->bsize == newsize)  /* not changing size? */
    return box->box;  /* keep the buffer */
  else {
    void *ud;
    sil_Alloc allocf = sil_getallocf(L, &ud);
    void *temp = allocf(ud, box->box, box->bsize, newsize);
    if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
      sil_pushliteral(L, "not enough memory");
      sil_error(L);  /* raise a memory error */
    }
    box->box = temp;
    box->bsize = newsize;
    return temp;
  }
}


static int boxgc (sil_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const silL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (sil_State *L) {
  UBox *box = (UBox *)sil_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (silL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    silL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  sil_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  sil_assert(buffonstack(B) ? sil_touserdata(B->L, idx) != NULL  \
                            : sil_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes plus one for a terminating zero.
*/
static size_t newbuffsize (silL_Buffer *B, size_t sz) {
  size_t newsize = B->size;
  if (l_unlikely(sz >= MAX_SIZE - B->n))
    return cast_sizet(silL_error(B->L, "resulting string too large"));
  /* else  B->n + sz + 1 <= MAX_SIZE */
  if (newsize <= MAX_SIZE/3 * 2)  /* no overflow? */
    newsize += (newsize >> 1);  /* new size *= 1.5 */
  if (newsize < B->n + sz + 1)  /* not big enough? */
    newsize = B->n + sz + 1;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (silL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    sil_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      sil_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      sil_insert(L, boxidx);  /* move box to its intended position */
      sil_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
SILLIB_API char *silL_prepbuffsize (silL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


SILLIB_API void silL_addlstring (silL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    silL_addsize(B, l);
  }
}


SILLIB_API void silL_addstring (silL_Buffer *B, const char *s) {
  silL_addlstring(B, s, strlen(s));
}


SILLIB_API void silL_pushresult (silL_Buffer *B) {
  sil_State *L = B->L;
  checkbufferlevel(B, -1);
  if (!buffonstack(B))  /* using static buffer? */
    sil_pushlstring(L, B->b, B->n);  /* save result as regular string */
  else {  /* reuse buffer already allocated */
    UBox *box = (UBox *)sil_touserdata(L, -1);
    void *ud;
    sil_Alloc allocf = sil_getallocf(L, &ud);  /* function to free buffer */
    size_t len = B->n;  /* final string length */
    char *s;
    resizebox(L, -1, len + 1);  /* adjust box size to content size */
    s = (char*)box->box;  /* final buffer address */
    s[len] = '\0';  /* add ending zero */
    /* clear box, as SIL will take control of the buffer */
    box->bsize = 0;  box->box = NULL;
    sil_pushexternalstring(L, s, len, allocf, ud);
    sil_closeslot(L, -2);  /* close the box */
    sil_gc(L, SIL_GCSTEP, len);
  }
  sil_remove(L, -2);  /* remove box or placeholder from the stack */
}


SILLIB_API void silL_pushresultsize (silL_Buffer *B, size_t sz) {
  silL_addsize(B, sz);
  silL_pushresult(B);
}


/*
** 'silL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'silL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
SILLIB_API void silL_addvalue (silL_Buffer *B) {
  sil_State *L = B->L;
  size_t len;
  const char *s = sil_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  silL_addsize(B, len);
  sil_pop(L, 1);  /* pop string */
}


SILLIB_API void silL_buffinit (sil_State *L, silL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = SILL_BUFFERSIZE;
  sil_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


SILLIB_API char *silL_buffinitsize (sil_State *L, silL_Buffer *B, size_t sz) {
  silL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/*
** The previously freed references form a linked list: t[1] is the index
** of a first free index, t[t[1]] is the index of the second element,
** etc. A zero signals the end of the list.
*/
SILLIB_API int silL_ref (sil_State *L, int t) {
  int ref;
  if (sil_isnil(L, -1)) {
    sil_pop(L, 1);  /* remove from stack */
    return SIL_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = sil_absindex(L, t);
  if (sil_rawgeti(L, t, 1) == SIL_TNUMBER)  /* already initialized? */
    ref = (int)sil_tointeger(L, -1);  /* ref = t[1] */
  else {  /* first access */
    sil_assert(!sil_toboolean(L, -1));  /* must be nil or false */
    ref = 0;  /* list is empty */
    sil_pushinteger(L, 0);  /* initialize as an empty list */
    sil_rawseti(L, t, 1);  /* ref = t[1] = 0 */
  }
  sil_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    sil_rawgeti(L, t, ref);  /* remove it from list */
    sil_rawseti(L, t, 1);  /* (t[1] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)sil_rawlen(L, t) + 1;  /* get a new reference */
  sil_rawseti(L, t, ref);
  return ref;
}


SILLIB_API void silL_unref (sil_State *L, int t, int ref) {
  if (ref >= 0) {
    t = sil_absindex(L, t);
    sil_rawgeti(L, t, 1);
    sil_assert(sil_isinteger(L, -1));
    sil_rawseti(L, t, ref);  /* t[ref] = t[1] */
    sil_pushinteger(L, ref);
    sil_rawseti(L, t, 1);  /* t[1] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  unsigned n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (sil_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (sil_State *L, const char *what, int fnameindex) {
  int err = errno;
  const char *filename = sil_tostring(L, fnameindex) + 1;
  if (err != 0)
    sil_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
  else
    sil_pushfstring(L, "cannot %s %s", what, filename);
  sil_remove(L, fnameindex);
  return SIL_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


SILLIB_API int silL_loadfilex (sil_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = sil_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    sil_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    sil_pushfstring(L, "@%s", filename);
    errno = 0;
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == SIL_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      errno = 0;
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = cast_char(c);  /* 'c' is the first character */
  status = sil_load(L, getF, &lf, sil_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  errno = 0;  /* no useful error number until here */
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    sil_settop(L, fnameindex);  /* ignore results from 'sil_load' */
    return errfile(L, "read", fnameindex);
  }
  sil_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (sil_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


SILLIB_API int silL_loadbufferx (sil_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return sil_load(L, getS, &ls, name, mode);
}


SILLIB_API int silL_loadstring (sil_State *L, const char *s) {
  return silL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



SILLIB_API int silL_getmetafield (sil_State *L, int obj, const char *event) {
  if (!sil_getmetatable(L, obj))  /* no metatable? */
    return SIL_TNIL;
  else {
    int tt;
    sil_pushstring(L, event);
    tt = sil_rawget(L, -2);
    if (tt == SIL_TNIL)  /* is metafield nil? */
      sil_pop(L, 2);  /* remove metatable and metafield */
    else
      sil_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


SILLIB_API int silL_callmeta (sil_State *L, int obj, const char *event) {
  obj = sil_absindex(L, obj);
  if (silL_getmetafield(L, obj, event) == SIL_TNIL)  /* no metafield? */
    return 0;
  sil_pushvalue(L, obj);
  sil_call(L, 1, 1);
  return 1;
}


SILLIB_API sil_Integer silL_len (sil_State *L, int idx) {
  sil_Integer l;
  int isnum;
  sil_len(L, idx);
  l = sil_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    silL_error(L, "object length is not an integer");
  sil_pop(L, 1);  /* remove object */
  return l;
}


SILLIB_API const char *silL_tolstring (sil_State *L, int idx, size_t *len) {
  idx = sil_absindex(L,idx);
  if (silL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!sil_isstring(L, -1))
      silL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (sil_type(L, idx)) {
      case SIL_TNUMBER: {
        char buff[SIL_N2SBUFFSZ];
        sil_numbertocstring(L, idx, buff);
        sil_pushstring(L, buff);
        break;
      }
      case SIL_TSTRING:
        sil_pushvalue(L, idx);
        break;
      case SIL_TBOOLEAN:
        sil_pushstring(L, (sil_toboolean(L, idx) ? "true" : "false"));
        break;
      case SIL_TNIL:
        sil_pushliteral(L, "nil");
        break;
      default: {
        int tt = silL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == SIL_TSTRING) ? sil_tostring(L, -1) :
                                                 silL_typename(L, idx);
        sil_pushfstring(L, "%s: %p", kind, sil_topointer(L, idx));
        if (tt != SIL_TNIL)
          sil_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return sil_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
SILLIB_API void silL_setfuncs (sil_State *L, const silL_Reg *l, int nup) {
  silL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* placeholder? */
      sil_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        sil_pushvalue(L, -nup);
      sil_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    sil_setfield(L, -(nup + 2), l->name);
  }
  sil_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
SILLIB_API int silL_getsubtable (sil_State *L, int idx, const char *fname) {
  if (sil_getfield(L, idx, fname) == SIL_TTABLE)
    return 1;  /* table already there */
  else {
    sil_pop(L, 1);  /* remove previous result */
    idx = sil_absindex(L, idx);
    sil_newtable(L);
    sil_pushvalue(L, -1);  /* copy to be left at top */
    sil_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
SILLIB_API void silL_requiref (sil_State *L, const char *modname,
                               sil_CFunction openf, int glb) {
  silL_getsubtable(L, SIL_REGISTRYINDEX, SIL_LOADED_TABLE);
  sil_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!sil_toboolean(L, -1)) {  /* package not already loaded? */
    sil_pop(L, 1);  /* remove field */
    sil_pushcfunction(L, openf);
    sil_pushstring(L, modname);  /* argument to open function */
    sil_call(L, 1, 1);  /* call 'openf' to open module */
    sil_pushvalue(L, -1);  /* make copy of module (call result) */
    sil_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  sil_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    sil_pushvalue(L, -1);  /* copy of module */
    sil_setglobal(L, modname);  /* _G[modname] = module */
  }
}


SILLIB_API void silL_addgsub (silL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    silL_addlstring(b, s, ct_diff2sz(wild - s));  /* push prefix */
    silL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  silL_addstring(b, s);  /* push last suffix */
}


SILLIB_API const char *silL_gsub (sil_State *L, const char *s,
                                  const char *p, const char *r) {
  silL_Buffer b;
  silL_buffinit(L, &b);
  silL_addgsub(&b, s, p, r);
  silL_pushresult(&b);
  return sil_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/*
** Standard panic function just prints an error message. The test
** with 'sil_type' avoids possible memory errors in 'sil_tostring'.
*/
static int panic (sil_State *L) {
  const char *msg = (sil_type(L, -1) == SIL_TSTRING)
                  ? sil_tostring(L, -1)
                  : "error object is not a string";
  sil_writestringerror("PANIC: unprotected error in call to SIL API (%s)\n",
                        msg);
  return 0;  /* return to SIL to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (sil_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      sil_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      sil_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((sil_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  sil_State *L = (sil_State *)ud;
  sil_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    sil_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    sil_writestringerror("%s", "\n");  /* finish message with end-of-line */
    sil_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((sil_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  sil_writestringerror("%s", "Sil warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}



/*
** A function to compute an unsigned int with some level of
** randomness. Rely on Address Space Layout Randomization (if present)
** and the current time.
*/
#if !defined(sili_makeseed)

#include <time.h>


/* Size for the buffer, in bytes */
#define BUFSEEDB	(sizeof(void*) + sizeof(time_t))

/* Size for the buffer in int's, rounded up */
#define BUFSEED		((BUFSEEDB + sizeof(int) - 1) / sizeof(int))

/*
** Copy the contents of variable 'v' into the buffer pointed by 'b'.
** (The '&b[0]' disguises 'b' to fix an absurd warning from clang.)
*/
#define addbuff(b,v)	(memcpy(&b[0], &(v), sizeof(v)), b += sizeof(v))


static unsigned int sili_makeseed (void) {
  unsigned int buff[BUFSEED];
  unsigned int res;
  unsigned int i;
  time_t t = time(NULL);
  char *b = (char*)buff;
  addbuff(b, b);  /* local variable's address */
  addbuff(b, t);  /* time */
  /* fill (rare but possible) remain of the buffer with zeros */
  memset(b, 0, sizeof(buff) - BUFSEEDB);
  res = buff[0];
  for (i = 1; i < BUFSEED; i++)
    res ^= (res >> 3) + (res << 7) + buff[i];
  return res;
}

#endif


SILLIB_API unsigned int silL_makeseed (sil_State *L) {
  (void)L;  /* unused */
  return sili_makeseed();
}


SILLIB_API sil_State *silL_newstate (void) {
  sil_State *L = sil_newstate(l_alloc, NULL, sili_makeseed());
  if (l_likely(L)) {
    sil_atpanic(L, &panic);
    sil_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


SILLIB_API void silL_checkversion_ (sil_State *L, sil_Number ver, size_t sz) {
  sil_Number v = sil_version(L);
  if (sz != SILL_NUMSIZES)  /* check numeric types */
    silL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    silL_error(L, "version mismatch: app. needs %f, SIL core provides %f",
                  (SILI_UACNUMBER)ver, (SILI_UACNUMBER)v);
}

